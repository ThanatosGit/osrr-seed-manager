#include <3ds.h>
#include <dirent.h>
#include <ftw.h>
#include <string.h>
#include <unzip.h>

#include "helpers.h"

#define READ_BUFFER_SIZE 256000

char luma_path_partial[] = "/luma/titles/";
char dir_delimiter = '/';

int unzip(const char *seed_zip_name, const char *title_id) {
    char luma_path_full[MAX_FILENAME];
    char luma_path_for_delete[MAX_FILENAME];
    char zip_file_name[MAX_FILENAME];
    snprintf(zip_file_name, MAX_FILENAME, "%s%s", homebrew_path, seed_zip_name);
    snprintf(luma_path_full, MAX_FILENAME, "%s%s%c", luma_path_partial, title_id, dir_delimiter);
    snprintf(luma_path_for_delete, MAX_FILENAME, "%s%s", luma_path_partial, title_id);

    printf("Deleting existing mod folder. This may take a short moment...");
    FS_Archive sd_archive;
    FSUSER_OpenArchive(&sd_archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    FSUSER_DeleteDirectoryRecursively(sd_archive, fsMakePath(PATH_ASCII, luma_path_for_delete));
    FSUSER_CloseArchive(sd_archive);
    create_file_path_dirs(luma_path_full);
    printf("done\n");

    // Open the zip file
    unzFile zipfile = unzOpen(zip_file_name);
    if (zipfile == NULL) {
        printf("Etiher the file does not exists or is not a zip file: %s\n", zip_file_name);
        return -1;
    }

    // Get info about the zip file
    unz_global_info global_info;
    if (unzGetGlobalInfo(zipfile, &global_info) != UNZ_OK) {
        printf("Could not read file global info\n");
        unzClose(zipfile);
        return -1;
    }

    // Buffer to hold data read from the zip file.
    char *read_buffer = (char *)calloc(READ_BUFFER_SIZE, sizeof(char));

    // Loop to extract all files
    uLong i;
    for (i = 0; i < global_info.number_entry; ++i) {
        // Get info about current file.
        unz_file_info file_info;
        char filename[MAX_FILENAME];
        if (unzGetCurrentFileInfo(
                zipfile,
                &file_info,
                filename,
                MAX_FILENAME,
                NULL, 0, NULL, 0) != UNZ_OK) {
            printf("Could not read file:\n%s\n", filename);
            unzClose(zipfile);
            free(read_buffer);
            return -1;
        }

        // Check if this entry is a directory or file.
        char full_file_path[MAX_FILENAME * 2];
        snprintf(full_file_path, MAX_FILENAME * 2, "%s%s", luma_path_full, filename);
        const size_t filename_length = strlen(filename);
        if (filename[filename_length - 1] == dir_delimiter) {
            // Entry is a directory, so create it.
            printf("Create dir: %s\n", filename);
            mkdir(full_file_path, S_IRWXU | S_IRWXG | S_IRWXU);
        } else {
            // Entry is a file, so extract it.
            printf("Extract file: %s\n", filename);
            if (unzOpenCurrentFile(zipfile) != UNZ_OK) {
                printf("Could not open file: \n%s\n", filename);
                unzClose(zipfile);
                free(read_buffer);
                return -1;
            }

            // Open a file to write out the data.
            FILE *out = fopen(full_file_path, "wb");
            if (out == NULL) {
                printf("Could not open destination file: \n%s\n", full_file_path);
                unzCloseCurrentFile(zipfile);
                unzClose(zipfile);
                free(read_buffer);
                return -1;
            }

            int error = UNZ_OK;
            do {
                error = unzReadCurrentFile(zipfile, read_buffer, READ_BUFFER_SIZE);
                if (error < 0) {
                    printf("Error %d\n", error);
                    unzCloseCurrentFile(zipfile);
                    unzClose(zipfile);
                    free(read_buffer);
                    return -1;
                }

                // Write data to file.
                if (error > 0) {
                    fwrite(read_buffer, error, 1, out);  // You should check return of fwrite...
                }
            } while (error > 0);

            fclose(out);
        }

        unzCloseCurrentFile(zipfile);

        // Go the the next entry listed in the zip file.
        if ((i + 1) < global_info.number_entry) {
            if (unzGoToNextFile(zipfile) != UNZ_OK) {
                printf("Could not read next file\n");
                unzClose(zipfile);
                free(read_buffer);
                return -1;
            }
        }
    }

    unzClose(zipfile);
    free(read_buffer);

    printf("Unzip done. Seed applied successfully\n");
    return 0;
}
