#include <stddef.h>
#include <stdatomic.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/hpet.h>
#include <kernel/arch/x86_64/user.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/fd.h>
#include <kernel/mmu.h>
#include <kernel/vfs.h>
#include <kernel/acpi.h>
#include <kernel/sched.h>
#include <kernel/malloc.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>

long max_pid = 0, next_cpu = 0;

static void sched_task_entry(int (*entry)()) {
    sched_kill(this_core()->current_proc, entry());
}

void sched_lock(void) {
    acquire(&(this_core()->sched_lock));
}

void sched_unlock(void) {
    release(&(this_core()->sched_lock));
}

void sched_start_timer(void) {
    lapic_eoi();
    lapic_oneshot(0x79, 5);
}

void sched_stop_timer(void) {
    lapic_stop_timer();
}

struct task *sched_new_task(void *entry, const char *name, int cpu) {
    struct cpu *core = cpu == -2 ? this_core() : get_core(cpu == -1 ? next_cpu : cpu);

    struct task *proc = (struct task *)kmalloc(sizeof(struct task));
    proc->pml4 = this_core()->pml4;

    uint64_t *stack = VIRTUAL(mmu_alloc(4));
    mmu_map_pages(4, (uintptr_t)PHYSICAL(stack), (uintptr_t)stack, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    memset(stack, 0, 4 * PAGE_SIZE);

    uint64_t *kernel_stack = VIRTUAL(mmu_alloc(4));
    mmu_map_pages(4, (uintptr_t)PHYSICAL(stack), (uintptr_t)stack, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    memset(stack, 0, 4 * PAGE_SIZE);

    proc->ctx.rdi = (uint64_t)entry;
    proc->ctx.rsi = 0;
    proc->ctx.rbp = 0;
    proc->ctx.rsp = (uint64_t)stack + (4 * PAGE_SIZE) - 8;
    proc->ctx.rbx = 0;
    proc->ctx.rdx = 0;
    proc->ctx.rcx = 0;
    proc->ctx.rax = 0;
    proc->ctx.rip = (uint64_t)sched_task_entry;
    proc->ctx.cs = 0x8;
    proc->ctx.ss = 0x10;
    proc->ctx.rflags = 0x202;
    proc->name = name;
    proc->stack = (uint64_t)stack;
    proc->kernel_stack = (uint64_t)kernel_stack;
    proc->gs = 0;
    proc->state = RUNNING;
    proc->pid = max_pid++;
    proc->heap = heap_create();
    proc->fd_table[0] = fd_open(vfs_open(vfs_root, "/dev/serial0"), 0);
    proc->fd_table[1] = fd_open(vfs_open(vfs_root, "/dev/console"), 0);

    sched_lock();
    if (!core->processes) {
        proc->prev = proc;
        proc->next = proc;
        core->processes = proc;
    } else {
        proc->prev = core->processes->prev;
        core->processes->prev->next = proc;
        proc->next = core->processes;
        core->processes->prev = proc;
    }
    next_cpu++;
    if (next_cpu >= madt_lapics)
        next_cpu = 0;
    else if (next_cpu < 0)
        next_cpu = madt_lapics - 1;
    sched_unlock();

    dprintf("%s:%d: created task \"%s\" on CPU #%d\n", __FILE__, __LINE__, name, core->id);
    return proc;
}

struct task *sched_new_user_task(void *entry, const char *name, int cpu) {
    uintptr_t *pml4 = mmu_alloc(1);
    mmu_map((uintptr_t)VIRTUAL(pml4), (uintptr_t)pml4, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    mmu_create_user_pm(pml4);

    struct task *proc = sched_new_task(entry, name, cpu);
    proc->ctx.cs = 0x23;
    proc->ctx.ss = 0x1b;
    proc->pml4 = pml4;
    printf("elf64's heap @ 0x%lx\n", proc->heap);

    this_core()->pml4 = kernel_pd;
    return proc;
}

void sched_schedule(struct registers *r) {
    sched_stop_timer();
    vmm_switch_pm(kernel_pd);

    struct cpu *this = this_core();

    if (this->current_proc) {
        memcpy(&(this->current_proc->ctx), r, sizeof(struct registers));
        this->current_proc->gs = read_kernel_gs();
    } else {
        this->current_proc = this->processes;
    }

    size_t hpet_ticks = hpet_get_ticks();
    if (this->current_proc == RUNNING)
        this->current_proc->time.last = hpet_ticks - this->current_proc->time.start;

    if (!this->current_proc->next) {
        this->current_proc = this->processes;
    } else {
        this->current_proc = this->current_proc->next;
    }

    dprintf("context switch from %s\n", this->current_proc->name);

    while (this->current_proc->state != RUNNING) {
        if (this->current_proc->state == PAUSED
         && hpet_ticks >= this->current_proc->time.end) {
            this->current_proc->state = RUNNING;
            this->current_proc->time.last = this->current_proc->time.end - this->current_proc->time.start;
            break;
        } else if (this->current_proc->state == KILLED) {
            dprintf("killing %s\n", this->current_proc->name);
            vmm_switch_pm(kernel_pd);

            struct task *proc = this->current_proc;
            this->current_proc = this->current_proc->next;

            max_pid = proc->pid;
            proc->prev->next = proc->next;
            proc->next->prev = proc->prev;

            this_core()->pml4 = proc->pml4;
            for (size_t i = 0; i < sizeof(proc->sections) / sizeof(struct task_section); i++) {
                if (proc->sections[i].length == 0) continue;
                for (size_t page = 0; page < proc->sections[i].length / PAGE_SIZE; page++) {
                    uintptr_t vaddr = proc->sections[i].ptr + page * PAGE_SIZE;
                    uintptr_t paddr = mmu_get_physical(proc->pml4, vaddr);
                    mmu_free((void *)paddr, 1);
                    mmu_unmap(vaddr);
                }
            }
            this_core()->pml4 = kernel_pd;
            heap_delete(proc->heap);
            //mmu_free(PHYSICAL(proc->kernel_stack), 4);
            //mmu_unmap_pages(4, (uintptr_t)PHYSICAL(proc->kernel_stack));
            //mmu_free(PHYSICAL(proc->stack), 4);
            //mmu_unmap_pages(4, (uintptr_t)PHYSICAL(proc->stack));
            if (proc->pml4 != kernel_pd) {
                mmu_free(proc->pml4, 1);
                mmu_unmap((uintptr_t)VIRTUAL(proc->pml4));
            }
            kfree(proc);
            dprintf("current proc is now %s\n", this->current_proc->name);
            break;
        }

        this->current_proc = this->current_proc->next;
    }

    if (this->current_proc->state != RUNNING)
        this->current_proc = this->current_proc->next;

    this->current_proc->time.start = hpet_ticks;

    memcpy(r, &(this->current_proc->ctx), sizeof(struct registers));
    vmm_switch_pm(this->current_proc->pml4);
    write_kernel_gs((uint64_t)this->current_proc);

    sched_start_timer();
}

void sched_yield(void) {
    lapic_ipi(this_core()->lapic_id, 0x79);
}

void sched_block(enum task_state reason) {
    this_core()->current_proc->state = reason;
    sched_yield();
}

void sched_unblock(struct task *proc) {
    proc->state = RUNNING;
}

void sched_sleep(int us) {
    this_core()->current_proc->time.end = hpet_get_ticks() + us * (hpet_period / 1000000);
    sched_block(PAUSED);
}

void sched_kill(struct task *proc, int status) {
    proc->state = KILLED;
    sched_yield();
}

void sched_idle(void) {
    for (;;) {
        asm ("hlt");
    }
}

void sched_start_all_cores(void) {
    irq_register(0x79 - 32, sched_schedule);
    for (uint32_t i = 1; i < madt_lapics; i++) {
        sched_new_task(sched_idle, "System Idle Process", i);
    }
    for (uint32_t i = madt_lapics - 1; i >= 0; i--) {
        lapic_ipi(i, 0x79);
    }
}

void sched_install(void) {
    sched_new_task(sched_idle, "System Idle Process", -1);

    printf("\033[92m * \033[97mInitialized scheduler\033[0m\n");
}