#ifndef _HANDLE_REQUEST_H_
#define _HANDLE_REQUEST_H_

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <arpa/inet.h>

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "../../socket/utils/LinkedList.h"
#include "../../socket/utils/LinkedListUtils.h"

extern struct LinkedList *file_list;
extern struct LinkedList *user_list;
extern pthread_mutex_t lock_host;
extern pthread_mutex_t lock_file_list;
extern pthread_cond_t cond_file_list;

/* info for each client connected to the index server */
struct net_info{
	int sockfd;
	char ip_add[16];
	uint16_t port;			//client connect to index server
	uint16_t data_port;		//client listen for download_file_request
	pthread_mutex_t *lock_sockfd;
};

struct thread_data{
	struct net_info cli_info;
	char filename[200];
	uint32_t filesize;
	uint8_t seq_no;
};

void removeHost(struct DataHost host);
void handleSocketError(struct net_info cli_info, char *mess);

void update_file_list(struct net_info cli_info, char *info);
void process_list_files_request(struct net_info cli_info);
void *process_list_hosts_request(void *arg);

#endif
