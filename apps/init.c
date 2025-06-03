#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    FILE *fptr;
    char hostname[256];
    if (!(fptr = fopen("/etc/hostname", "r")) ||
        !fgets(hostname, sizeof hostname, fptr) ||
        sethostname(hostname, strlen(hostname)) != 0) {
        perror("init: failed to set hostname");
    } else {
        fclose(fptr);
    }

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
            fprintf(stderr, "init: restarting /bin/bash\n");
        }
    }
    return -1;
}