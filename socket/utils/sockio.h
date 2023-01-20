#ifndef _SOCK_IO_H
#define _SOCK_IO_H

#define MAX_BUFF_SIZE 8192
#define ENCODE_KEY 


int readBytes(int sock, void* buf, unsigned int nbytes);
int writeBytes(int sock, void* buf, unsigned int nbytes);

#endif