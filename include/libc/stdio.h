#pragma once
#include <stdarg.h>

#define stdin  0
#define stdout 1
#define stderr 2

char *fgets(char *buf, unsigned int len, int stream);
int puts(const char *s);
int vsprintf(char *s, const char *fmt, va_list args);
int printf(const char *fmt, ...);