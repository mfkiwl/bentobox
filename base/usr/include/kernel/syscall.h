#pragma once
#include <kernel/arch/x86_64/idt.h>

long sys_read(struct registers *);
long sys_write(struct registers *);
long sys_exit(struct registers *);
long sys_gettid(struct registers *r);
long sys_arch_prctl(struct registers *r);
long sys_mmap(struct registers *r);