#ifndef SERVER_THREAD_H
#define SERVER_THREAD_H

#include <3ds.h>

extern Thread server_thread;
extern void server_thread_main(void *arg);

extern bool server_run;

#endif  // SERVER_THREAD_H
