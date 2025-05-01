long syscall0(long rax) {
    long ret;
    asm volatile (
        "syscall"
        : "=a" (ret)
        : "a" (rax)
        : "rcx", "r11"
    );
    return ret;
}

long syscall1(long rax, long rdi) {
    long ret;
    asm volatile (
        "syscall"
        : "=a" (ret)
        : "a" (rax), "D" (rdi)
        : "rcx", "r11"
    );
    return ret;
}

long syscall2(long rax, long rdi, long rsi) {
    long ret;
    asm volatile (
        "syscall"
        : "=a" (ret)
        : "a" (rax), "D" (rdi), "S" (rsi)
        : "rcx", "r11"
    );
    return ret;
}

long syscall3(long rax, long rdi, long rsi, long rdx) {
    long ret;
    asm volatile (
        "syscall"
        : "=a" (ret)
        : "a" (rax), "D" (rdi), "S" (rsi), "d" (rdx)
        : "rcx", "r11"
    );
    return ret;
}

long syscall4(long rax, long rdi, long rsi, long rdx, long r10) {
    long ret;
    asm volatile (
        "mov %5, %%r10\n"
        "syscall"
        : "=a"(ret)
        : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long syscall5(long rax, long rdi, long rsi, long rdx, long r10, long r8) {
    long ret;
    asm volatile (
        "mov %5, %%r10\n"
        "mov %6, %%r8\n"
        "syscall"
        : "=a"(ret)
        : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long syscall6(long rax, long rdi, long rsi, long rdx, long r10, long r8, long r9) {
    long ret;
    asm volatile (
        "mov %5, %%r10\n"
        "mov %6, %%r8\n"
        "mov %7, %%r9\n"
        "syscall"
        : "=a"(ret)
        : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx),
          "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long syscall7(long rax, long rdi, long rsi, long rdx, long r10, long r8, long r9, long r11) {
    long ret;
    asm volatile (
        "mov %5, %%r10\n"
        "mov %6, %%r8\n"
        "mov %7, %%r9\n"
        "syscall"
        : "=a"(ret)
        : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx),
          "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}