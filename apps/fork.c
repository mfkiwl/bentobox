#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }

    printf("Forked %d\n", pid);

    if (pid == 0)
        _exit(0);
    return 0;
}
