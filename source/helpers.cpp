#include "helpers.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

char homebrew_path[] = "/osrr-seed-manager/";
char config_file[] = "config.txt";

// https://stackoverflow.com/questions/21236508/to-create-all-needed-folders-with-fopen
void create_file_path_dirs(char *file_path) {
    char *dir_path = (char *)malloc(strlen(file_path) + 1);
    char *next_step = strchr(file_path, '/');
    while (next_step != NULL) {
        int dir_path_len = next_step - file_path;
        memcpy(dir_path, file_path, dir_path_len);
        dir_path[dir_path_len] = '\0';
        mkdir(dir_path, S_IRWXU | S_IRWXG | S_IROTH);
        next_step = strchr(next_step + 1, '/');
    }
    free(dir_path);
}
