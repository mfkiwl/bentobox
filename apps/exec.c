#include <unistd.h>
#include <stdio.h>

int main() {
    printf("running exec\n");

    execl("/bin/open", "/bin/open", NULL);

    perror("execl failed");
    return 1;
}