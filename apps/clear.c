#include <stdio.h>

int main() {
    printf("\033[H\033[J");
    fflush(stdout);
    return 0;
}