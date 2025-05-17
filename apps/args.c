#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("/bin/args\n");
    printf("argc: %d\n", argc);
    printf("argv: { ");
    for (int i = 0; i < argc; i++) {
        printf("%s%s", argv[i], (i < argc - 1) ? ", " : "");
    }
    printf(" }\n");
    return 0;
}