#pragma once
#include <stdarg.h>

int vsprintf(char *s, const char *fmt, va_list args);
int sprintf(char *str, const char *fmt, ...);
int dprintf(const char *fmt, ...);
int printf(const char *fmt, ...);