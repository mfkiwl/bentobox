#pragma once
#include <stddef.h>

long syscall0(long);
long syscall1(long, long);
long syscall2(long, long, long);
long syscall3(long, long, long, long);
long syscall4(long, long, long, long, long);
long syscall5(long, long, long, long, long, long);
long syscall6(long, long, long, long, long, long, long);
long syscall7(long, long, long, long, long, long, long, long);

void _exit(int status);
long read(int fd, const void *buf, size_t count);
long write(int fd, const void *buf, size_t count);

#define syscall(...) __syscall(__VA_ARGS__, syscall7, syscall6, syscall5, syscall4, syscall3, syscall2, syscall1, syscall0)(__VA_ARGS__)
#define __syscall(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME