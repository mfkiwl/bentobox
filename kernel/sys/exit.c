#include <kernel/arch/x86_64/smp.h>
#include <kernel/sched.h>

int sys_exit(struct registers *r) {
    sched_kill(this_core()->current_proc, r->rdi);
    return 0;
}