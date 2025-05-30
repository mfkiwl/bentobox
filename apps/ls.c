#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

const char *get_file_color(const char *path, const char *name) {
    char full_path[1024];
    
    if (path[strlen(path) - 1] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", path, name);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", path, name);
    }
    
    struct stat file_stat;
    if (stat(full_path, &file_stat) == -1)
        return "";

    if (S_ISDIR(file_stat.st_mode)) {
        return "\033[94m";
    } else if (S_ISCHR(file_stat.st_mode) || S_ISBLK(file_stat.st_mode)) {
        return "\033[33m";
    } else if (S_ISREG(file_stat.st_mode)) {
        return "\033[91m";
    }
    return "";
}

int main(int argc, char *argv[]) {
    const char *path = "/";

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
        const char *color = get_file_color(path, entry->d_name);
        printf("%s%s\033[0m  ", color, entry->d_name);
    }
    
    printf("\n");
    closedir(dir);
    return 0;
}