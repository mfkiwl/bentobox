#include <stddef.h>
#include <stdatomic.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/hpet.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/mmu.h>
#include <kernel/vfs.h>
#include <kernel/acpi.h>
#include <kernel/heap.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/sys/sched.h>
#include <kernel/sys/spinlock.h>

long max_pid = 0, next_cpu = 0;

static void sched_stack_exit(void) {
    sched_kill(this_core()->current_proc);
}

void sched_lock(void) {
    acquire(&(this_core()->sched_lock));
}

void sched_unlock(void) {
    release(&(this_core()->sched_lock));
}

struct task *sched_new_task(void *entry, const char *name, int cpu) {
    struct task *proc = (struct task *)kmalloc(sizeof(struct task));
    proc->page_dir = kernel_pd;

    uint64_t *stack = VIRTUAL(mmu_alloc(4));
    mmu_map_pages(4, (uintptr_t)PHYSICAL(stack), (uintptr_t)stack, PTE_PRESENT | PTE_WRITABLE);
    memset(stack, 0, 4 * PAGE_SIZE);
    stack[2047] = (uint64_t)sched_stack_exit;

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
    proc->ring = 0;
    proc->name = name;
    proc->stack = stack;
    proc->state = RUNNING;
    proc->pid = max_pid++;
    proc->heap = heap_create();
    proc->fd_table[0] = vfs_open(vfs_root, "/dev/serial0");
    proc->fd_table[1] = vfs_open(vfs_root, "/dev/serial0");

    struct cpu *core = get_core(cpu == -1 ? next_cpu : cpu);

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
    lapic_stop_timer();

    struct cpu *this = this_core();

    if (this->current_proc) {
        memcpy(&(this->current_proc->ctx), r, sizeof(struct registers));
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

    while (this->current_proc->state != RUNNING) {
        if (this->current_proc->state == PAUSED
         && hpet_ticks >= this->current_proc->time.end) {
            this->current_proc->state = RUNNING;
            this->current_proc->time.last = this->current_proc->time.end - this->current_proc->time.start;
            break;
        } else if (this->current_proc->state == KILLED) {
            struct task *proc = this->current_proc;
            this->current_proc = this->current_proc->next;

            max_pid = proc->pid;
            proc->prev->next = proc->next;
            proc->next->prev = proc->prev;
            heap_delete(proc->heap);
            mmu_free(PHYSICAL(proc->stack), 4);
            mmu_unmap_pages(4, (uintptr_t)proc->stack);
            kfree(proc);
            break;
        }
        this->current_proc = this->current_proc->next;
    }

    this->current_proc->time.start = hpet_ticks;

    memcpy(r, &(this->current_proc->ctx), sizeof(struct registers));

    sched_unlock();
    lapic_eoi();
    lapic_oneshot(0x79, 5);
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

void sched_kill(struct task *proc) {
    proc->state = KILLED;
    sched_yield();
}

void sched_idle(void) {
    for (;;) {
        asm ("hlt");
    }
}

static void uptime_task(void) {
    int hours = 0, minutes = 0, seconds = 0;
    for (;;) {
        if (seconds >= 60) {
            seconds = 0;
            minutes++;
        }
        if (minutes >= 60) {
            minutes = 0;
            hours++;
        }

        printf("\rUptime: %dh, %dmin, %ds", hours, minutes, seconds++);
        sched_sleep(1000000);
    }
}

void sched_start_all_cores(void) {
    irq_register(0x79 - 32, sched_schedule);
    for (uint32_t i = 1; i < madt_lapics; i++) {
        sched_new_task(sched_idle, "System Idle Process", i);
        lapic_ipi(i, 0x79);
    }
    lapic_ipi(0, 0x79);
}

void sched_install(void) {
    sched_new_task(sched_idle, "System Idle Process", -1);
    sched_new_task(uptime_task, "Uptime Task", -1);
    //extern void debugger_task_entry(void);
    //sched_new_task(debugger_task_entry, "bentobox debug shell", -1);

    printf("\033[92m * \033[97mInitialized scheduler\033[0m\n");
}