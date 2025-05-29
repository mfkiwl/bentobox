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

    if (pid == 0) {
        char *argv[] = { "/bin/hi", NULL };
        execvp(argv[0], argv);
    }

    printf("Forked %d\n", pid);

    //char input[100] = {0};
    //fgets(input, sizeof(input), stdin);

    return 0;
}
