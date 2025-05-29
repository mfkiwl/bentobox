#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("%d argument(s) received\n", argc);

    if (argc < 2) {
        return 1;
    }

    FILE *fptr = fopen(argv[1], "r");
    if (!fptr)
        return 1;

    char buffer[100];
    fgets(buffer, sizeof(buffer), fptr);
    printf("%s", buffer);

    fclose(fptr);
    return 0;
}