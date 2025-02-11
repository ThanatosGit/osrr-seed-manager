#include "server_thread.h"

#include <3ds.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "helpers.h"

#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000
#define RECV_BUFFERSIZE 1024 * 1024

extern char homebrew_path[];

Thread server_thread;
static u32 *SOC_buffer = NULL;
s32 server_sock = -1;
s32 client_sock = -1;
bool server_run = 1;
struct sockaddr_in server;

uint8_t new_data = 0;
char recv_buffer[RECV_BUFFERSIZE];

void failExit(const char *fmt, ...) {
    if (server_sock > 0) close(server_sock);
    if (client_sock > 0) close(client_sock);

    va_list ap;

    printf(CONSOLE_RED);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("Cannot start server. Use this application without it or close it, connect to your wifi and restart\n");
    printf(CONSOLE_RESET);
}

void soc_shutdown() {
    printf("Waiting for socExit...\n");
    socExit();
}

int init_server() {
    int ret;
    int opt = 1;

    SOC_buffer = (u32 *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (SOC_buffer == NULL) {
        failExit("memalign: failed to allocate\n");
        return 1;
    }

    if (R_FAILED(ret = socInit(SOC_buffer, SOC_BUFFERSIZE))) {
        failExit("socInit: 0x%08X\n", (unsigned int)ret);
        return 1;
    }

    aptSetSleepAllowed(false);

    atexit(soc_shutdown);

    if (R_FAILED(server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP))) {
        failExit("socket: %d %s\n", errno, strerror(errno));
        return 1;
    }

    setsockopt((s32)&server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(1234);
    server.sin_addr.s_addr = INADDR_ANY;

    if (R_FAILED(ret = bind(server_sock, (struct sockaddr *)&server, sizeof(server)))) {
        close(server_sock);
        failExit("bind: %d %s\n", errno, strerror(errno));
        return 1;
    }

    fcntl(server_sock, F_SETFL, fcntl(server_sock, F_GETFL, 0) | O_NONBLOCK);

    if (R_FAILED(ret = listen(server_sock, 5))) {
        failExit("listen: %d %s\n", errno, strerror(errno));
        return 1;
    }
    return 0;
}

int receive_until_bytes(int bytes_to_receive) {
    int remaining_bytes = bytes_to_receive;
    int received;

    while (remaining_bytes) {
        received = recv(client_sock, recv_buffer + bytes_to_receive - remaining_bytes, bytes_to_receive, 0);
        if (received <= 0) {
            printf(CONSOLE_RED);
            printf("Error while receiving from network\n");
            printf(CONSOLE_RESET);
            return 1;
        }
        remaining_bytes = remaining_bytes - received;
    }
    return 0;
}

void flush_to_file(Handle &file_handle, ssize_t to_write, u64 &offset_in_file) {
    int ret;
    // how many bytes are written so far in this function call
    ssize_t written_this_flush = 0;
    ssize_t left_to_write = to_write;
    // how many bytes are written per FSFILE_Write call
    u32 bytes_written;

    while (written_this_flush < to_write) {
        if (R_FAILED(ret = FSFILE_Write(file_handle, &bytes_written, offset_in_file, recv_buffer + written_this_flush, left_to_write, 0))) {
            printf(CONSOLE_RED);
            printf("Error could not create / open file\n");
            printf(CONSOLE_RESET);
            close(client_sock);
            continue;
        }
        written_this_flush = written_this_flush + bytes_written;
        left_to_write = left_to_write - bytes_written;
        offset_in_file = offset_in_file + bytes_written;
    }
}

void server_thread_main(void *arg) {
    int ret;
    struct sockaddr_in client;
    u32 clientlen = sizeof(client);

    if (init_server()) server_run = false;

    while (server_run) {
        memset(&client, 0, sizeof(client));
        client_sock = accept(server_sock, (struct sockaddr *)&client, &clientlen);

        if (client_sock < 0) {
            if (errno != EAGAIN) {
                failExit("accept: %d %s\n", errno, strerror(errno));
                server_run = false;
            }
        } else {
            fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) & ~O_NONBLOCK);
            printf("Connecting port %d from %s\n", client.sin_port, inet_ntoa(client.sin_addr));

            // first length of file name
            memset(recv_buffer, 0, RECV_BUFFERSIZE);
            if (receive_until_bytes(4)) {
                close(client_sock);
                continue;
            }
            int filename_length;
            memcpy(&filename_length, recv_buffer, 4);

            // second receive file name
            memset(recv_buffer, 0, 4);
            if (receive_until_bytes(filename_length)) {
                close(client_sock);
                continue;
            }
            char file_name_buffer[512];
            snprintf(file_name_buffer, 512, "%s%s", homebrew_path, recv_buffer);
            printf("Receiving %s\n", file_name_buffer);

            // third length of zip
            memset(recv_buffer, 0, filename_length);
            if (receive_until_bytes(4)) {
                close(client_sock);
                continue;
            }

            // fourth receive data for file
            int _zip_total_length;
            memcpy(&_zip_total_length, recv_buffer, 4);
            // total length to be received
            u64 zip_total_length = _zip_total_length;
            // received bytes so far
            u64 zip_received_bytes = 0;
            // keep track of offset in the file
            u64 offset_in_file = 0;
            // we make multipe recv calls to fill the buffer and keep track of pos
            ssize_t recv_buffer_pos = 0;

            // create file and open it for writing
            Handle file_handle;
            FSUSER_CreateFile(sd_archive, fsMakePath(PATH_ASCII, file_name_buffer), 0, zip_total_length);
            if (R_FAILED(FSUSER_OpenFile(&file_handle, sd_archive, fsMakePath(PATH_ASCII, file_name_buffer), FS_OPEN_WRITE, 0))) {
                printf(CONSOLE_RED);
                printf("Error could not create / open file\n");
                printf(CONSOLE_RESET);
                close(client_sock);
                continue;
            }

            while ((ret = recv(client_sock, recv_buffer + recv_buffer_pos, RECV_BUFFERSIZE - recv_buffer_pos, 0)) != 0) {
                zip_received_bytes += ret;
                recv_buffer_pos += ret;

                // flush only every 512 Kilobytes because sd access is slow
                if (recv_buffer_pos > 1024 * 512) {
                    flush_to_file(file_handle, recv_buffer_pos, offset_in_file);
                    recv_buffer_pos = 0;
                }
            }

            // flush the rest
            flush_to_file(file_handle, recv_buffer_pos, offset_in_file);

            if (zip_total_length == zip_received_bytes)
                printf("\nFile received\n");
            else {
                printf(CONSOLE_RED);
                printf("\nDid not received complete file!\n");
                printf(CONSOLE_RESET);
                remove(file_name_buffer);
            }

            close(client_sock);
            client_sock = -1;
            new_data = 1;
            continue;
        }
    }
    if (server_sock != -1) close(server_sock);
    socExit();
    free(SOC_buffer);
}
