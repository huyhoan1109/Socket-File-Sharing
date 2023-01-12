#ifndef _HANDLE_DOWNLOAD_FILE_REQUEST_H_
#define _HANDLE_DOWNLOAD_FILE_REQUEST_H_
#include <stdint.h>

struct NetInfo{
	int sockfd;			//socket FD corresponds to the client
	char ip_add[16];	
	uint16_t port;
};
void* waitForDownloadRequest(void* arg);
#endif
