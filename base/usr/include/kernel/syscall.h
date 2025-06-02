#pragma once
#include <kernel/arch/x86_64/idt.h>

#define SYS_read        0
#define SYS_write       1
#define SYS_open        2
#define SYS_close       3
#define SYS_stat        4
#define SYS_fstat       5
#define SYS_lseek       8
#define SYS_mmap        9
#define SYS_ioctl       16
#define SYS_access      21
#define SYS_getpid      39
#define SYS_clone       56
#define SYS_execve      59
#define SYS_exit        60
#define SYS_wait4       61
#define SYS_arch_prctl  158
#define SYS_gettid      186
#define SYS_futex       202
#define SYS_getdents64  217
#define SYS_clock_gettime 228
#define SYS_newfstatat  262

long sys_read(struct registers *r);
long sys_write(struct registers *r);
long sys_open(struct registers *r);
long sys_close(struct registers *r);
long sys_stat(struct registers *r);
long sys_fstat(struct registers *r);
long sys_lseek(struct registers *r);
long sys_mmap(struct registers *r);
long sys_rt_sigaction(struct registers *r);
long sys_ioctl(struct registers *r);
long sys_access(struct registers *r);
long sys_dup(struct registers *r);
long sys_getpid(struct registers *r);
long sys_clone(struct registers *r);
long sys_execve(struct registers *r);
long sys_exit(struct registers *r);
long sys_wait4(struct registers *r);
long sys_getuid(struct registers *r);
long sys_getgid(struct registers *r);
long sys_geteuid(struct registers *r);
long sys_getegid(struct registers *r);
long sys_getppid(struct registers *r);
long sys_getgpid(struct registers *r);
long sys_arch_prctl(struct registers *r);
long sys_getdents64(struct registers *r);
long sys_clock_gettime(struct registers *r);