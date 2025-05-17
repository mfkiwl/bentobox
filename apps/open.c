#include <stdio.h>

int main(int argc, char *argv[]) {
    FILE *fptr = fopen("/foo.txt", "r");
    if (!fptr)
        return 1;

    char buffer[100];
    fgets(buffer, sizeof(buffer), fptr);

    printf("foo.txt contents are %s", buffer);

    fclose(fptr);
    return 0;
}