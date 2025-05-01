#include <libc/syscall.h>

extern int main(int argc, char *argv[]);

void pre_main(int argc, char *argv[]) {
    _exit(main(argc, argv));
    __builtin_unreachable();
}