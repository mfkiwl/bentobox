#include <stddef.h>
#include <kernel/arch/x86_64/lapic.h>
#include <kernel/mmu.h>
#include <kernel/sched.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/assert.h>

struct task *processes = NULL;
struct task *current_proc = NULL;

long max_pid = 0;

__attribute__((no_sanitize("undefined")))
struct task *sched_new_task(void *entry, const char *name) {
    struct task *proc = (struct task *)kmalloc(sizeof(struct task));
    proc->page_dir = kernel_pd;

    void *stack = mmu_alloc(4);
    mmu_map_pages(4, (uintptr_t)stack, (uintptr_t)VIRTUAL(stack), PTE_PRESENT | PTE_WRITABLE);
    memset(stack, 0, 4 * PAGE_SIZE);

    proc->ctx.rdi = 0;
    proc->ctx.rsi = 0;
    proc->ctx.rbp = 0;
    proc->ctx.rsp = (uint64_t)stack + (4 * PAGE_SIZE) - 4;
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
    proc->killed = false;
    proc->pid = max_pid++;
    proc->heap = heap_create();

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

    dprintf("%s:%d: created task \"%s\"\n", __FILE__, __LINE__, name);
    return proc;
}

void sched_schedule(struct registers *r) {
    lapic_stop_timer();

    if (current_proc) {
        memcpy(&(current_proc->ctx), r, sizeof(struct registers));
    } else {
        current_proc = processes;
    }

    if (!current_proc->next) {
        current_proc = processes;
    } else {
        current_proc = current_proc->next;
    }

    memcpy(r, &(current_proc->ctx), sizeof(struct registers));

    lapic_eoi();
    lapic_oneshot(0x79, 5);
}

void sched_yield(void) {
    unimplemented;
}

void sched_idle(void) {
    for (;;) {
        sched_yield();
    }
}

void sched_start(void) {
    dprintf("%s:%d: jumpstarting scheduler...\n", __FILE__, __LINE__);
    irq_register(0x79 - 32, sched_schedule);
    lapic_ipi(0, 0x79);
}

void sched_install(void) {
    sched_new_task(sched_idle, "System Idle Process");
}