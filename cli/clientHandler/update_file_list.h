#ifndef _UPDATE_FILE_LIST_H_
#define _UPDATE_FILE_LIST_H_

#include <stdint.h>

struct FileStatus{
	char filename[200];
	uint8_t status;
	uint32_t filesize;
};

extern uint16_t dataPort;

void announceDataPort(int sockfd);
void *init_file_list(void *arg);
void monitor_directory(char *dir, int socketfd);
void share_file(void *arg);
void delete_from_server(void *arg);

#endif