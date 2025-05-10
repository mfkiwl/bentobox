#include <kernel/arch/x86_64/smp.h>
#include <kernel/sched.h>
#include <kernel/printf.h>

long sys_exit(struct registers *r) {
    dprintf("%s:%d: %s: exiting with status %lu\n", __FILE__, __LINE__, __func__, r->rdi);
    sched_kill(this_core()->current_proc, r->rdi);
    __builtin_unreachable();
}