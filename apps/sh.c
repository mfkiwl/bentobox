#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int builtin_exec(int argc, char *argv[]) {
    if (access(argv[0], F_OK)) return 1;
    
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0][0] == '.' ? argv[0] + 1 : argv[0], argv);
        printf("%s: Permission denied\n", argv[0]);
        exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    return 0;
}

int builtin_clear(int argc, char *argv[]) {
    printf("\033[H\033[J");
    return 0;
}

int builtin_exit(int argc, char *argv[]) {
    int code = argc > 1 ? atoi(argv[1]) : 0;
    exit(code);
    __builtin_unreachable();
}

int builtin_help(int argc, char *argv[]);

struct command {
    char *name;
    char *syntax;
    void *function;
    int length;
} commands[] = {
    {
        .name = ":",
        .function = NULL
    },
    {
        .name = ".",
        .syntax = "[file]",
        .function = builtin_exec,
        .length = 1
    },
    {
        .name = "clear",
        .function = builtin_clear
    },
    {
        .name = "exit",
        .syntax = "[n]",
        .function = builtin_exit
    },
    {
        .name = "help",
        .function = builtin_help
    }
};
typedef struct command command_t;

int builtin_help(int argc, char *argv[]) {
    printf(
        "/bin/sh, version 0.1\n"
        "These shell commands are defined internally. Type 'help' to see this list.\n"
        "\n"
    );
    
    for (uint32_t i = 0; i < sizeof commands / sizeof(command_t); i++) {
        if (i % 2 == 0) printf(" %-35s", commands[i].name);
        else printf(" %s\n", commands[i].name);
        
        if ((i == sizeof commands / sizeof(command_t) - 1) && (i % 2 == 0)) {
            printf("\n");
        }
    }
    return 0;
}

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

char *PATH = "/etc:/bin";

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

    if (!builtin_exec(argc, argv)) return;

    char file[strlen(argv[0]) + 1];
    char path_copy[strlen(PATH) + 1];
    strcpy(file, argv[0]);
    strcpy(path_copy, PATH);

    char *ptr = path_copy;
    while (ptr) {
        char *next = strchr(ptr, ':');
        if (next) *next = 0;

        char path[strlen(file) + strlen(ptr) + 2];
        strcpy(path, ptr);
        strcat(path, "/");
        strcat(path, file);

        argv[0] = path;
        if (!builtin_exec(argc, argv)) return;

        if (!next) break;
        ptr = next + 1;
    }

    printf("%s: not found\n", file);
}

void parse(char *input) {
    parse_line(input);
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