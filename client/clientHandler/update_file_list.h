#ifndef _UPDATE_FILE_LIST_H_
#define _UPDATE_FILE_LIST_H_

#include <stdint.h>

struct FileStatus{
	char filename[200];
	uint8_t status;
	uint32_t filesize;
};

extern uint16_t dataPort;

extern struct LinkedList *monitor_files;

void announceDataPort(int sockfd);
void *update_file_list(void *arg);
void monitor_directory(char *dir, int socketfd);
void share_file(void *arg);
void delete_from_server(void *arg);
void re_run();

#endif