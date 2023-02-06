#ifndef _LIST_HOSTS_REQUEST_H_
#define _LIST_HOSTS_REQUEST_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#include <arpa/inet.h>

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "../../socket/utils/LinkedList.h"
#include "../../socket/utils/LinkedListUtils.h"

extern uint8_t seq_no;
extern struct FileOwner *the_file;
extern pthread_mutex_t lock_the_file;

void send_list_hosts_request(char *filename);
void process_list_hosts_response(char *message);

#endif
