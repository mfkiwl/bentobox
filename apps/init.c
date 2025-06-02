#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    for (;;) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            char *arg[] = { "/bin/bash", NULL };
            char *envp[] = { "HOME=/root", NULL };
            execve(arg[0], arg, envp);
            perror("execvp");
            exit(1);
        } else {
            int status;
            waitpid(pid, &status, 0);
            fprintf(stderr, "%s:%d: restarting /bin/bash\n", __FILE__, __LINE__);
        }
    }

    return 0;
}