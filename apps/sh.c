#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int exec_(int argc, char *argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0] + 1, argv);
    }
    return 0;
}

int clear_(int argc, char *argv[]) {
    printf("\033[H\033[J");
    return 0;
}

int exit_(int argc, char *argv[]) {
    int code = argc > 1 ? atoi(argv[1]) : 0;
    exit(code);
    __builtin_unreachable();
}

struct command {
    char *name;
    void *function;
    int length;
} commands[] = {
    {
        .name = ":",
        .function = NULL
    },
    {
        .name = ".",
        .function = exec_,
        .length = 1
    },
    {
        .name = "clear",
        .function = clear_
    },
    {
        .name = "exit",
        .function = exit_
    },
};
typedef struct command command_t;

int count_args(char *str) {
    int count = 0, in_arg = 0;
    while (*str) {
        if (isspace(*str)) {
            in_arg = 0;
        } else if (!in_arg) {
            count++;
            in_arg = 1;
        }
        str++;
    }
    return count;
}

void parse_line(char *input) {
    if (!input[0]) return;

    int argc = count_args(input);
    char *argv[argc + 1];

    char *p = input;
    for (int i = 0; i < argc; i++) {
        while (*p == ' ') p++;
        if (*p == '\0') break;

        argv[i] = p;
        p = strchr(p, ' ');
        if (!p) break;
        
        *p++ = 0;
    }
    argv[argc] = NULL;

    for (int i = 0; i < sizeof commands / sizeof(command_t); i++) {
        if (!strncmp(input, commands[i].name, commands[i].length ? commands[i].length : strlen(commands[i].name))) {
            int (*function)(int argc, char *argv[]) = commands[i].function;
            if (function) function(argc, argv);
            return;
        }
    }
    printf("%s: not found\n", argv[0]);
}

void parse(char *input) {
    parse_line(input);
    //char *current = input, *next = NULL;
    //if ((next = strchr(current, ';')) || (next = strchr(current, '\\')) || (next = strchr(current, '\n'))) {
    //    printf("line break!\n");
    //}
}

int main(int argc, char *argv[]) {
    for (;;) {
        printf("# ");
        
        char input[100] = {0};
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';
        parse(input);
    }
    return 0;
}