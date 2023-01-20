#ifndef _LIST_HOSTS_REQUEST_H_
#define _LIST_HOSTS_REQUEST_H_

#include <pthread.h>
#include <stdint.h>

#include "../../socket/utils/LinkedList.h"


extern struct FileOwner *the_file;
extern uint8_t seq_no;		//sequence number, identifier for each list_hosts_request => do as a queue
extern pthread_mutex_t lock_the_file;

void send_list_hosts_request(char *filename);
void process_list_hosts_response();

#endif
