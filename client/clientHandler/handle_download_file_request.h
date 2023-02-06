#ifndef _HANDLE_DOWNLOAD_FILE_REQUEST_H_
#define _HANDLE_DOWNLOAD_FILE_REQUEST_H_

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "../../socket/utils/LinkedList.h"

struct NetInfo{
	int sockfd;			//socket FD corresponds to the client
	char ip_add[16];	
	uint16_t port;
};

void* waitForDownloadRequest(void* arg);

#endif
