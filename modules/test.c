#include <kernel/printf.h>
#include <kernel/module.h>

int init() {
    printf("Hello from test module!\n");
    return 0;
}

struct Module metadata = {
    .name = "test module",
    .init = init
};