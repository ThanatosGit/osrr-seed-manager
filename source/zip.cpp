#include <3ds.h>
#include <archive.h>
#include <archive_entry.h>
#include <dirent.h>
#include <ftw.h>
#include <setjmp.h>
#include <string.h>

#include <cstring>
#include <filesystem>
#include <string>

#include "helpers.h"

#define READ_BUFFER_SIZE 262144

char luma_path_partial[] = "/luma/titles/";
char dir_delimiter = '/';

// based on https://github.com/libarchive/libarchive/wiki/Examples#user-content-A_Complete_Extractor
int unzip(const char *seed_zip_name, const char *title_id) {
    int ret;
    const u64 buffer_size = 1024 * 1024;
    u8 *buffer = (u8 *)calloc(buffer_size, 1);
    struct archive_entry *archive_entry;
    Handle file_handle;
    int flags;
    flags = ARCHIVE_EXTRACT_TIME;
    flags |= ARCHIVE_EXTRACT_PERM;
    flags |= ARCHIVE_EXTRACT_ACL;
    flags |= ARCHIVE_EXTRACT_FFLAGS;

    // delete old seed
    char luma_path_full[MAX_FILENAME];
    char luma_path_for_delete[MAX_FILENAME];
    char zip_file_name[MAX_FILENAME];
    char entry_name[MAX_FILENAME];
    snprintf(zip_file_name, MAX_FILENAME, "%s%s", homebrew_path, seed_zip_name);
    snprintf(luma_path_full, MAX_FILENAME, "%s%s%c", luma_path_partial, title_id, dir_delimiter);
    snprintf(luma_path_for_delete, MAX_FILENAME, "%s%s", luma_path_partial, title_id);

    printf("Deleting existing mod folder %s. This may take a short moment...\n", luma_path_for_delete);
    FSUSER_DeleteDirectoryRecursively(sd_archive, fsMakePath(PATH_ASCII, luma_path_for_delete));
    FSUSER_CreateDirectory(sd_archive, fsMakePath(PATH_ASCII, luma_path_full), 0);

    struct archive *archive_read = archive_read_new();
    archive_read_support_filter_all(archive_read);
    archive_read_support_format_all(archive_read);

    ret = archive_read_open_filename(archive_read, zip_file_name, 16384);
    if (ret != ARCHIVE_OK) {
        printf(CONSOLE_RED);
        printf("Failed to open file: %s\n", zip_file_name);
        printf(CONSOLE_RESET);
    }

    while ((ret = archive_read_next_header(archive_read, &archive_entry)) == ARCHIVE_OK) {
        if (ret != ARCHIVE_OK) {
            printf(CONSOLE_RED);
            printf("Failed to read next header\n");
            printf(CONSOLE_RESET);
        }

        snprintf(entry_name, MAX_FILENAME, "%s%s%c%s", luma_path_partial, title_id, dir_delimiter, archive_entry_pathname(archive_entry));
        archive_entry_update_pathname_utf8(archive_entry, luma_path_full);
        u64 entry_size = archive_entry_size(archive_entry);

        if (entry_size == 0) {
            FSUSER_CreateDirectory(sd_archive, fsMakePath(PATH_ASCII, entry_name), 0);
            continue;
        }
        printf("Extracting: %s\n", entry_name);
        FSUSER_CreateFile(sd_archive, fsMakePath(PATH_ASCII, entry_name), 0, entry_size);
        FSUSER_OpenFile(&file_handle, sd_archive, fsMakePath(PATH_ASCII, entry_name), FS_OPEN_WRITE, 0);

        u32 bytes_written = 0;
        ssize_t read_bytes;
        do {
            read_bytes = archive_read_data(archive_read, buffer, buffer_size);
            if (read_bytes < 0) {
                printf(CONSOLE_RED);
                printf("Failed to read data header\n");
                printf(CONSOLE_RESET);
            }
            if (read_bytes > 0) {
                if (R_FAILED(ret = FSFILE_Write(file_handle, &bytes_written, bytes_written, buffer, read_bytes, FS_WRITE_FLUSH))) {
                    printf(CONSOLE_RED);
                    printf("Error could not write to file\n");
                    printf(CONSOLE_RESET);
                    break;
                }
            }
        } while (read_bytes != 0);
        FSFILE_Close(file_handle);
    }
    printf("Unzip done. Seed applied successfully\n");
    archive_read_free(archive_read);
    if (buffer != NULL) free(buffer);
    return 0;
}
