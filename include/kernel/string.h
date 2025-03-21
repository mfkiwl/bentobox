#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int   memcmp(const void *s1, const void *s2, size_t n);

int   strlen(const char* s);
bool  strcmp(const char* a, const char* b);
int   strncmp(const char *x, const char *y, register size_t n);
char *strcpy(char* dest, const char* src);
char *strchr(register const char *s, int c);
char *strstr(const char *a, const char *b);
int   atoi(char *s);