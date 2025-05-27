#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    //printf("Hello world!\n");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }

    //if (pid == 0) {
    //    char *argv[] = { "/bin/hello",  NULL };
    //    execvp(argv[0], argv);
    //    for (;;);
    //} else {
    //    printf("Spawned PID %d!\n", pid);
    //}
    printf("Spawned PID %d!\n", pid);
    return 0;

#if 0
    if (pid == 0) {
        // Child process
        char *argv[] = { "/bin/ls", "-l", NULL };
        execvp(argv[0], argv);

        // If exec fails:
        perror("exec failed");
        exit(1);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        printf("Child process exited with status %d\n", WEXITSTATUS(status));
    }
#endif

    return 0;
}
