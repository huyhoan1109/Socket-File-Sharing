#ifndef _SOCK_IO_H
#define _SOCK_IO_H

#define MAX_BUFF_SIZE 8192

typedef struct {
    uint8_t header;
    char payload[MAX_BUFF_SIZE];
} message;

int readBytes(int sock, void* buf, unsigned int nbytes);
int writeBytes(int sock, void* buf, unsigned int nbytes);

// HEADER:-:MESSAGE 

#endif