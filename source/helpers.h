#ifndef HELPERS_H
#define HELPERS_H

#define MAX_FILENAME 512

#include <3ds.h>

extern void create_file_path_dirs(char *file_path);

extern char homebrew_path[20];
extern char config_file[11];
extern FS_Archive sd_archive;

#endif  // HELPERS_H
