#pragma once
#include <stdarg.h>
#include <kernel/sys/sched.h>

#define stdin (current_proc->fd_table[0])
#define stdout (current_proc->fd_table[1])

int vsprintf(char *s, const char *fmt, va_list args);
int sprintf(char *str, const char *fmt, ...);
int dprintf(const char *fmt, ...);
int printf(const char *fmt, ...);