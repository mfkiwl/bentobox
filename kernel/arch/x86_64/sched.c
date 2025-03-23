#include <stddef.h>
#include <stdatomic.h>
#include <kernel/arch/x86_64/pit.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/mmu.h>
#include <kernel/heap.h>
#include <kernel/sched.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/assert.h>
#include <kernel/spinlock.h>

struct task *processes = NULL;
struct task *current_proc = NULL;

atomic_flag sched_lock = ATOMIC_FLAG_INIT;

long max_pid = 0;

void sched_stack_exit(void) {
    sched_kill(current_proc);
}

__attribute__((no_sanitize("undefined")))
struct task *sched_new_task(void *entry, const char *name) {
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
    proc->ctx.rflags = 0x202;
    proc->ring = 0;
    proc->name = name;
    proc->stack = stack;
    proc->state = RUNNING;
    proc->pid = max_pid++;
    proc->heap = heap_create();

    acquire(&sched_lock);
    if (!processes) {
        proc->prev = proc;
        proc->next = proc;
        processes = proc;
    } else {
        proc->prev = processes->prev;
        processes->prev->next = proc;
        proc->next = processes;
        processes->prev = proc;
    }
    release(&sched_lock);

    dprintf("%s:%d: created task \"%s\"\n", __FILE__, __LINE__, name);
    return proc;
}

void sched_schedule(struct registers *r) {
    acquire(&sched_lock);
    lapic_stop_timer();

    if (current_proc) {
        memcpy(&(current_proc->ctx), r, sizeof(struct registers));
    } else {
        current_proc = processes;
    }

    extern size_t pit_ticks;
    if (current_proc == RUNNING)
        current_proc->time.last = pit_ticks - current_proc->time.start;

    if (!current_proc->next) {
        current_proc = processes;
    } else {
        current_proc = current_proc->next;
    }

    while (current_proc->state != RUNNING) {
        if (current_proc->state == PAUSED
         && pit_ticks >= current_proc->time.end) {
            current_proc->state = RUNNING;
            current_proc->time.last = current_proc->time.end - current_proc->time.start;
            break;
        } else if (current_proc->state == KILLED) {
            struct task *proc = current_proc;
            current_proc = current_proc->next;

            max_pid = proc->pid;
            proc->prev->next = proc->next;
            proc->next->prev = proc->prev;
            heap_delete(proc->heap);
            mmu_free(PHYSICAL(proc->stack), 4);
            mmu_unmap_pages(4, (uintptr_t)proc->stack);
            kfree(proc);
            break;
        }
        current_proc = current_proc->next;
    }

    current_proc->time.start = pit_ticks;

    memcpy(r, &(current_proc->ctx), sizeof(struct registers));

    release(&sched_lock);
    lapic_eoi();
    lapic_oneshot(0x79, 5);
}

void sched_yield(void) {
    asm ("int $0x79");
}

void sched_block(enum task_state reason) {
    current_proc->state = reason;
    sched_yield();
}

void sched_unblock(struct task *proc) {
    proc->state = RUNNING;
}

void sched_sleep(int ms) {
    extern size_t pit_ticks;
    current_proc->time.end = pit_ticks + ms;
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
    printf("\n");

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
        sched_sleep(1000);
    }
}

void sched_start(void) {
    dprintf("%s:%d: jumpstarting scheduler...\n", __FILE__, __LINE__);
    irq_register(0x79 - 32, sched_schedule);
    lapic_ipi(0, 0x79);
}

void sched_install(void) {
    sched_new_task(sched_idle, "System Idle Process");
    sched_new_task(uptime_task, "Uptime Task");
}