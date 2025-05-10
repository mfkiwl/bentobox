/*
 * bentobox x86_64 userspace shell
 */

/*
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int clear(char input[]);
int echo(char input[]);
int help(char input[]);

const char *commands[] = {
    "clear",
    "echo",
    "help"
};

const void *handlers[] = {
    clear,
    echo,
    help
};

int clear(char input[]) {
    puts("\033[2J\033[H");
    return 0;
}

int echo(char input[]) {
    printf("%s\n", input + 5);
    return 0;
}

int help(char input[]) {
    puts("Commands: ");
    size_t count = sizeof(handlers) / sizeof(uintptr_t);
    for (size_t i = 0; i < count; i++) {
        printf("%s%s", commands[i], i < count - 1 ? ", " : "");
    }
    puts("\n");
    return 0;
}

void parse(char input[]) {
    if (!input[0]) {
        return;
    }

    for (size_t i = 0; i < sizeof(handlers) / sizeof(uintptr_t); i++) {
        if (!strncmp(input, commands[i], strlen(commands[i]))) {
            void(*handler)() = handlers[i];

            if (handlers[i] != NULL)
                handler();
            return;
        }
    }
    printf("%s: not found\n", input);
}

void __dso_handle() __attribute__((weak));

int main(int argc, char *argv[]) {
    char input[100];
    for (;;) {
        printf("# ");
        fgets(input, sizeof(input), stdin);
        parse(input);
    }
    return 0;
}
*/

#include <stdio.h>

int main() {
    printf("Hello, mlibc!");
    return 0;
}