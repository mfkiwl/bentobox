#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    FILE *fp;
    int c;
    
    if (argc == 1) {
        while ((c = getchar()) != EOF) {
            putchar(c);
        }
    } else {
        for (int i = 1; i < argc; i++) {
            if (argv[i][0] == '-' && argv[i][1] == '\0') {
                while ((c = getchar()) != EOF) {
                    putchar(c);
                }
            } else {
                fp = fopen(argv[i], "r");
                if (!fp) {
                    perror(argv[i]);
                    continue;
                }
                
                while ((c = fgetc(fp)) != EOF) {
                    putchar(c);
                }
                
                fclose(fp);
            }
        }
    }
    
    return 0;
}