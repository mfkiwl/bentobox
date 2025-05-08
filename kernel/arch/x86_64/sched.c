#include "kernel/arch/x86_64/vmm.h"
#include <stddef.h>
#include <stdatomic.h>
#include <kernel/arch/x86_64/tss.h>
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

// TODO: implement task threading

long max_pid = 0, next_cpu = 0;

void sched_lock(void) {
    lapic_eoi();
    lapic_oneshot(0x79, 5);
}

void sched_unlock(void) {
    lapic_stop_timer();
}

struct task *sched_new_task(void *entry, const char *name, int cpu) {
    struct cpu *core = cpu == -2 ? this_core() : get_core(cpu == -1 ? next_cpu : cpu);

    struct task *proc = (struct task *)kmalloc(sizeof(struct task));
    memset(proc, 0, sizeof(struct task));
    proc->pml4 = this_core()->pml4;

    uint64_t *stack = VIRTUAL(mmu_alloc(4));
    mmu_map_pages(4, (uintptr_t)PHYSICAL(stack), (uintptr_t)stack, PTE_PRESENT | PTE_WRITABLE);
    memset(stack, 0, 4 * PAGE_SIZE);

    proc->ctx.rdi = 0;
    proc->ctx.rsi = 0;
    proc->ctx.rbp = 0;
    proc->ctx.rsp = (uint64_t)stack + (4 * PAGE_SIZE) - 8;
    proc->ctx.rbx = 0;
    proc->ctx.rdx = 0;
    proc->ctx.rcx = 0;
    proc->ctx.rax = 0;
    proc->ctx.rip = (uint64_t)entry;
    proc->ctx.cs = 0x8;
    proc->ctx.ss = 0x10;
    proc->ctx.rflags = 0x202;
    proc->name = name;
    proc->stack = (uint64_t)stack + (4 * PAGE_SIZE);
    proc->stack_bottom = (uint64_t)stack;
    proc->gs = 0;
    proc->state = RUNNING;
    proc->pid = max_pid++;
    proc->user = false;
    proc->heap = heap_create();
    proc->fd_table[0] = fd_open(vfs_open(vfs_root, "/dev/keyboard"), 0);
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
    struct cpu *core = cpu == -2 ? this_core() : get_core(cpu == -1 ? next_cpu : cpu);

    struct task *proc = (struct task *)kmalloc(sizeof(struct task));
    memset(proc, 0, sizeof(struct task));
    proc->pml4 = mmu_create_user_pm(proc);

    uint64_t *user_stack = VIRTUAL(mmu_alloc(4));
    uint64_t *kernel_stack = VIRTUAL(mmu_alloc(4));
    mmu_map_pages(4, (uintptr_t)PHYSICAL(user_stack), (uintptr_t)user_stack, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    mmu_map_pages(4, (uintptr_t)PHYSICAL(kernel_stack), (uintptr_t)kernel_stack, PTE_PRESENT | PTE_WRITABLE);
    
    proc->ctx.rdi = 0;
    proc->ctx.rsi = 0;
    proc->ctx.rbp = 0;
    proc->ctx.rsp = (uint64_t)user_stack + (4 * PAGE_SIZE) - 8;
    proc->ctx.rbx = 0;
    proc->ctx.rdx = 0;
    proc->ctx.rcx = 0;
    proc->ctx.rax = 0;
    proc->ctx.rip = (uint64_t)entry;
    proc->ctx.cs = 0x23;
    proc->ctx.ss = 0x1b;
    proc->ctx.rflags = 0x202;
    proc->name = name;
    proc->stack = (uint64_t)user_stack + (4 * PAGE_SIZE);
    proc->stack_bottom = (uint64_t)user_stack;
    proc->kernel_stack = (uint64_t)kernel_stack + (4 * PAGE_SIZE);
    proc->kernel_stack_bottom = (uint64_t)kernel_stack;
    proc->gs = 0;
    proc->state = RUNNING;
    proc->pid = max_pid++;
    proc->heap = NULL;
    proc->user = true;
    proc->fd_table[0] = fd_open(vfs_open(vfs_root, "/dev/keyboard"), 0);
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

void sched_schedule(struct registers *r) {
    sched_lock();
    vmm_switch_pm(kernel_pd);

    struct cpu *this = this_core();

    if (this->current_proc) {
        memcpy(&(this->current_proc->ctx), r, sizeof(struct registers));
        this->current_proc->gs = read_kernel_gs();
    } else {
        this->current_proc = this->processes;
    }

    size_t hpet_ticks = hpet_get_ticks();
    if (this->current_proc->state == RUNNING)
        this->current_proc->time.last = hpet_ticks - this->current_proc->time.start;

    if (!this->current_proc->next) {
        this->current_proc = this->processes;
    } else {
        this->current_proc = this->current_proc->next;
    }

    while (this->current_proc->state != RUNNING) {
        if (this->current_proc->state == PAUSED
         && hpet_ticks >= this->current_proc->time.end) {
            this->current_proc->state = RUNNING;
            this->current_proc->time.last = this->current_proc->time.end - this->current_proc->time.start;
            break;
        }

        this->current_proc = this->current_proc->next;
    }

    this->current_proc->time.start = hpet_ticks;

    memcpy(r, &(this->current_proc->ctx), sizeof(struct registers));
    if (this_core()->pml4 != this->current_proc->pml4)
        vmm_switch_pm(this->current_proc->pml4);
    write_kernel_gs((uint64_t)this->current_proc);
    set_kernel_stack(this->current_proc->kernel_stack);

    sched_unlock();
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
    sched_lock();

    max_pid = proc->pid;
    proc->prev->next = proc->next;
    proc->next->prev = proc->prev;
    proc->next = this_core()->terminated_processes;
    this_core()->terminated_processes = proc;

    proc->state = KILLED;
    
    bool is_current = proc == this_core()->current_proc;
    if (is_current) {
        this_core()->current_proc = proc->next;
    }
    
    sched_unblock(this_core()->cleaner_proc);
    sched_unlock();

    if (is_current) {
        sched_yield();
        __builtin_unreachable();
    }
}

void sched_cleaner(void) {
    for (;;) {
        sched_lock();
        
        struct task *proc = this_core()->terminated_processes;
        if (!proc) {
            sched_block(PAUSED);
            continue;
        }
        this_core()->terminated_processes = proc->next;
        
        if (proc->user) {
            this_core()->pml4 = proc->pml4;
            if (proc->sections[0].length > 0) {
                for (int i = 0; proc->sections[i].length; i++) {
                    mmu_free((void *)mmu_get_physical(proc->pml4, proc->sections[i].ptr), ALIGN_UP(proc->sections[i].length, PAGE_SIZE) / PAGE_SIZE);
                    mmu_unmap_pages(ALIGN_UP(proc->sections[i].length, PAGE_SIZE) / PAGE_SIZE, proc->sections[i].ptr);
                }
            }

            mmu_unmap_pages(4, proc->stack_bottom);
            mmu_unmap_pages(4, proc->kernel_stack_bottom);
            mmu_free(PHYSICAL(proc->stack_bottom), 4);
            mmu_free(PHYSICAL(proc->kernel_stack_bottom), 4);
            mmu_destroy_user_pm(proc->pml4);
        } else {
            mmu_unmap_pages(4, proc->stack_bottom);
            mmu_free(PHYSICAL(proc->stack_bottom), 4);
            heap_delete(proc->heap);
        }
        
        kfree(proc);
        sched_unlock();
    }
}

void sched_start_all_cores(void) {
    irq_register(0x79 - 32, sched_schedule);
    for (uint32_t i = madt_lapics - 1; i >= 0; i--) {
        lapic_ipi(i, 0x79);
    }
}

void sched_install(void) {
    for (uint32_t i = 0; i < madt_lapics; i++) {
        struct task *cleaner = sched_new_task(sched_cleaner, "System", i);
        cleaner->state = PAUSED;
        get_core(i)->cleaner_proc = cleaner;
    }

    printf("\033[92m * \033[97mInitialized scheduler\033[0m\n");
}