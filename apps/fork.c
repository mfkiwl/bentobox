#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    //printf("# ");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }

    //if (pid == 0) {
    //    char *argv[] = { "/bin/args", "hello", "world", NULL };
    //    execvp(argv[0], argv);
    //}

    printf("Forked %d\n", pid);
    return 0;
}
