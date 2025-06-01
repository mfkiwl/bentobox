#pragma once
#include <kernel/arch/x86_64/idt.h>

long sys_read(struct registers *r);
long sys_write(struct registers *r);
long sys_open(struct registers *r);
long sys_close(struct registers *r);
long sys_stat(struct registers *r);
long sys_lseek(struct registers *r);
long sys_mmap(struct registers *r);
long sys_rt_sigaction(struct registers *r);
long sys_ioctl(struct registers *r);
long sys_access(struct registers *r);
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
long sys_gettid(struct registers *r);
long sys_getdents64(struct registers *r);