#pragma once
#include <stddef.h>

void *memcpy(void *restrict dest, const void *restrict src, long n);
void *memset(void *dest, int c, long n);
int strlen(const char* s);
int strncmp(const char *x, const char *y, register size_t n);
char *strcpy(char* dest, const char* src);