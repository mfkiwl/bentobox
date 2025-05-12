#include <stdio.h>

int main() {
    puts("Hello, mlibc!"); // printf needs SSE, which we don't have (yet)
    return 0;
}