#pragma once
#include <stdarg.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/sys/sched.h>

#define stdin (this_core()->current_proc->fd_table[0])
#define stdout (this_core()->current_proc->fd_table[1])

int vsprintf(char *s, const char *fmt, va_list args);
int vprintf(const char *fmt, va_list args);
int sprintf(char *str, const char *fmt, ...);
int dprintf(const char *fmt, ...);
int printf(const char *fmt, ...);