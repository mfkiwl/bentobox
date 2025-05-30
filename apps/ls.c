#include <stdio.h>
#include <dirent.h>

int main(int argc, char *argv[]) {
    const char *path = ".";

    if (argc > 1) {
        path = argv[1];
    }

    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        printf("%s  ", entry->d_name);
    }
    printf("\n");

    closedir(dir);
    return 0;
}
