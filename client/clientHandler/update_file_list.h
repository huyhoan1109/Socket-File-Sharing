#ifndef _UPDATE_FILE_LIST_H_
#define _UPDATE_FILE_LIST_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/inotify.h>

#include <arpa/inet.h>

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "../../socket/utils/LinkedList.h"

#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
#define EVENT_SIZE  ( sizeof (struct inotify_event) )

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