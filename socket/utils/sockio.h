#ifndef _SOCK_IO_H
#define _SOCK_IO_H

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int readBytes(int sock, void* buf, unsigned int nbytes);
int writeBytes(int sock, void* buf, unsigned int nbytes);

#endif