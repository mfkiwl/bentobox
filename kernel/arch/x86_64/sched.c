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
    lapic_stop_timer();
}

void sched_unlock(void) {
    lapic_eoi();
    lapic_oneshot(0x79, 5);
}

void sched_add_task(struct task *proc, struct cpu *core) {
    sched_lock();
    if (!core) {
        core = get_core(next_cpu);
    }
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
}

struct task *sched_new_task(void *entry, const char *name) {
    struct task *proc = (struct task *)kmalloc(sizeof(struct task));
    memset(proc, 0, sizeof(struct task));
    proc->pml4 = this_core()->pml4;

    uint64_t *stack = VIRTUAL(mmu_alloc(4));
    mmu_map_pages(4, stack, PHYSICAL(stack), PTE_PRESENT | PTE_WRITABLE);
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
    proc->name = (char *)name;
    proc->stack = (uint64_t)stack + (4 * PAGE_SIZE);
    proc->stack_bottom = (uint64_t)stack;
    proc->gs = 0;
    proc->fs = 0;
    proc->state = RUNNING;
    proc->pid = max_pid++;
    proc->user = false;
    proc->heap = heap_create();
    proc->fd_table[0] = fd_new(vfs_open(vfs_root, "/dev/keyboard"), 0);
    proc->fd_table[1] = fd_new(vfs_open(vfs_root, "/dev/console"), 0);
    proc->fd_table[2] = fd_new(vfs_open(vfs_root, "/dev/serial0"), 0);
    proc->vma = NULL;

    return proc;
}

struct task *sched_new_user_task(void *entry, const char *name, int argc, char *argv[], char *env[]) {
    struct task *proc = (struct task *)kmalloc(sizeof(struct task));
    memset(proc, 0, sizeof(struct task));
    proc->pml4 = mmu_create_user_pm(proc);

    asm volatile ("cli" : : : "memory");
    uintptr_t stack_top = 0x00007ffffffff000;
    uintptr_t stack_bottom = stack_top - (USER_STACK_SIZE * PAGE_SIZE);
    uintptr_t stack_bottom_phys = (uintptr_t)mmu_alloc(USER_STACK_SIZE);
    uintptr_t stack_top_phys = stack_bottom_phys + (USER_STACK_SIZE * PAGE_SIZE);
    uint64_t *kernel_stack = VIRTUAL(mmu_alloc(4));
    mmu_map_pages(USER_STACK_SIZE, (void *)stack_bottom, (void *)stack_bottom_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    mmu_map_pages(4, kernel_stack, PHYSICAL(kernel_stack), PTE_PRESENT | PTE_WRITABLE);

    memset(VIRTUAL_IDENT(stack_bottom_phys), 0, (USER_STACK_SIZE * PAGE_SIZE));
    long depth = 16;

    uint64_t argv_ptrs[argc + 1];
    argv_ptrs[argc] = 0;

    int i = 0;
    for (i = 0; i < argc; i++) {
        depth += ALIGN_UP(strlen(argv[i]), 16);
        argv_ptrs[i] = (uint64_t)(stack_top - depth);
        strcpy((char *)VIRTUAL_IDENT(stack_top_phys - depth), argv[i]);
    }

    depth += 8;
    *VIRTUAL_IDENT(stack_top_phys - depth) = 0;

    for (i = argc; i >= 0; i--) {
        depth += 8;
        *VIRTUAL_IDENT(stack_top_phys - depth) = argv_ptrs[i];
    }

    depth += 8;
    *VIRTUAL_IDENT(stack_top_phys - depth) = argc;

    asm volatile ("sti" : : : "memory");
    
    proc->ctx.rdi = 0;
    proc->ctx.rsi = 0;
    proc->ctx.rbp = 0;
    proc->ctx.rsp = stack_top - depth;
    proc->ctx.rbx = 0;
    proc->ctx.rdx = 0;
    proc->ctx.rcx = 0;
    proc->ctx.rax = 0;
    proc->ctx.rip = (uint64_t)entry;
    proc->ctx.cs = 0x23;
    proc->ctx.ss = 0x1b;
    proc->ctx.rflags = 0x202;
    proc->name = kmalloc(strlen(name) + 1);
    strcpy(proc->name, name);
    proc->stack = stack_top;
    proc->stack_bottom = (uint64_t)stack_bottom;
    proc->stack_bottom_phys = (uint64_t)stack_bottom_phys;
    proc->kernel_stack = (uint64_t)kernel_stack + (4 * PAGE_SIZE);
    proc->kernel_stack_bottom = (uint64_t)kernel_stack;
    proc->gs = 0;
    proc->fs = 0;
    proc->state = RUNNING;
    proc->pid = max_pid++;
    proc->user = true;
    proc->heap = heap_create();
    proc->fd_table[0] = fd_new(vfs_open(vfs_root, "/dev/keyboard"), 0);
    proc->fd_table[1] = fd_new(vfs_open(vfs_root, "/dev/console"), 0);
    proc->fd_table[2] = fd_new(vfs_open(vfs_root, "/dev/serial0"), 0);
    proc->vma = vma_create();

    return proc;
}

void sched_schedule(struct registers *r) {
    sched_lock();

    if (this) {
        if (this->state != FRESH)
            memcpy(&(this->ctx), r, sizeof(struct registers));
        else
            this->state = RUNNING;
        this->gs = read_kernel_gs();
        asm volatile ("fxsave %0 " : : "m"(this->fxsave));
    } else {
        this = process_list;
    }

    size_t hpet_ticks = hpet_get_ticks();
    if (this->state == RUNNING)
        this->time.last = hpet_ticks - this->time.start;

    if (!this->next) {
        this = process_list;
    } else {
        this = this->next;
    }

    while (this->state != RUNNING) {
        if (this->state == PAUSED
         && hpet_ticks >= this->time.end) {
            this->state = RUNNING;
            this->time.last = this->time.end - this->time.start;
            break;
        }

        this = this->next;
    }

    this->time.start = hpet_ticks;

    memcpy(r, &(this->ctx), sizeof(struct registers));
    if (this_core()->pml4 != this->pml4)
        vmm_switch_pm(this->pml4);
    write_kernel_gs((uint64_t)this);
    set_kernel_stack(this->kernel_stack);
    asm volatile ("fxrstor %0 " : : "m"(this->fxsave));
    wrmsr(IA32_FS_BASE, this->fs);

    sched_unlock();
}

void sched_yield(void) {
    //lapic_ipi(this_core()->lapic_id, 0x79);
    asm volatile ("int $0x79\n");
}

void sched_block(enum task_state reason) {
    this->state = reason;
    sched_yield();
}

void sched_unblock(struct task *proc) {
    proc->state = RUNNING;
}

void sched_sleep(int us) {
    this->time.end = hpet_get_ticks() + us * (hpet_period / 1000000);
    sched_block(PAUSED);
}

void sched_kill(struct task *proc, int status) {
    sched_lock();

    max_pid = proc->pid;
    proc->state = KILLED;
    proc->prev->next = proc->next;
    proc->next->prev = proc->prev;
    proc->next = this_core()->terminated_processes;
    this_core()->terminated_processes = proc;
    
    sched_unblock(this_core()->cleaner_proc);
    sched_unlock();

    if (proc == this) {
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
            sched_lock();
            this_core()->pml4 = proc->pml4;
            if (proc->sections[0].length > 0) {
                for (int i = 0; proc->sections[i].length; i++) {
                    mmu_free((void *)mmu_get_physical(proc->pml4, proc->sections[i].ptr), ALIGN_UP(proc->sections[i].length, PAGE_SIZE) / PAGE_SIZE);
                    mmu_unmap_pages(ALIGN_UP(proc->sections[i].length, PAGE_SIZE) / PAGE_SIZE, (void *)proc->sections[i].ptr);
                }
            }

            mmu_unmap_pages(USER_STACK_SIZE, (void *)proc->stack_bottom);
            mmu_unmap_pages(4, (void *)proc->kernel_stack_bottom);
            mmu_free((void *)proc->stack_bottom_phys, USER_STACK_SIZE);
            mmu_free(PHYSICAL(proc->kernel_stack_bottom), 4);
            kfree(proc->name);
            heap_delete(proc->heap);
            vma_destroy(proc->vma);
            mmu_destroy_user_pm(proc->pml4);
            sched_unlock();
        } else {
            mmu_unmap_pages(4, (void *)proc->stack_bottom);
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
        struct task *cleaner = sched_new_task(sched_cleaner, "System");
        sched_add_task(cleaner, get_core(i));
        cleaner->state = PAUSED;
        get_core(i)->cleaner_proc = cleaner;
    }

    printf("\033[92m * \033[97mInitialized scheduler\033[0m\n");
}