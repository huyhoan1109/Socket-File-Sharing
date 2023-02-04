#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "list_hosts_request.h"
#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "../../socket/utils/LinkedListUtils.h"
#include "connect_index_server.h"
#include "download_file_request.h"

struct FileOwner *the_file = NULL;
uint8_t seq_no = 0;
pthread_mutex_t lock_the_file = PTHREAD_MUTEX_INITIALIZER;

void send_list_hosts_request(char *filename){
	pthread_mutex_lock(&lock_servsock);

	// LIST_HOST_REQUEST :=: seq_no :=: filename

	char *message = calloc(MAX_BUFF_SIZE, sizeof(char));
	strcpy(message, itoa(LIST_HOSTS_REQUEST));
	strcat(message, MESSAGE_DIVIDER);
	pthread_mutex_lock(&lock_the_file);
	strcat(message, itoa(seq_no)); 
	strcat(message, MESSAGE_DIVIDER);
	strcat(message, filename);
	pthread_mutex_unlock(&lock_the_file);
	writeBytes(servsock, message, MAX_BUFF_SIZE);
	pthread_mutex_unlock(&lock_servsock);
}

void process_list_hosts_response(char* message){
	char *subtext = calloc(MAX_BUFF_SIZE, sizeof(char));
	strcpy(subtext, message);
	
	char *token;
	token = strtok(subtext, MESSAGE_DIVIDER);
	token = strtok(NULL, MESSAGE_DIVIDER);	
	uint8_t sequence = atol(token);
	
	token = strtok(NULL, MESSAGE_DIVIDER);
	uint32_t filesize = atoll(token);

	pthread_mutex_lock(&lock_the_file);

	if (sequence == seq_no) the_file->filesize = filesize;
	
	token = strtok(NULL, MESSAGE_DIVIDER);
	uint32_t n_hosts = atol(token);

	if (n_hosts == 0 && sequence == seq_no){
		/* file not found */
		pthread_mutex_lock(&lock_segment_list);
		pthread_cond_signal(&cond_segment_list);
		pthread_mutex_unlock(&lock_segment_list);
		pthread_mutex_unlock(&lock_the_file);
		return;
	}

	uint8_t i = 0;
	for (; i < n_hosts; i++){
		token = strtok(NULL, MESSAGE_DIVIDER);
		uint8_t status = atol(token);
		
		token = strtok(NULL, MESSAGE_DIVIDER);
		uint32_t ip_addr = atoll(token);
		
		token = strtok(NULL, MESSAGE_DIVIDER);
		uint16_t data_port = atoll(token);	
		
		if (sequence == seq_no){
			struct DownloadInfo *dinfo = calloc(1, sizeof(struct DownloadInfo));
			dinfo->dthost.ip_addr = ip_addr;
			dinfo->dthost.port = data_port;
			dinfo->seq_no = sequence;
			if (status == FILE_NEW){
				struct Node *host_node = getNodeByHost(the_file->host_list, dinfo->dthost);
				if (!host_node){
					host_node = newNode(&dinfo->dthost, DATA_HOST_TYPE);
					push(the_file->host_list, host_node);
					/* create new thread to download file */
					pthread_mutex_lock(&lock_n_threads);
					if (n_threads >= 0){
						pthread_t tid;
						int thr = pthread_create(&tid, NULL, &download_file, dinfo);
						if (thr != 0){
							free(dinfo);
							pthread_mutex_unlock(&lock_n_threads);
							continue;
						}
						n_threads ++;
					}
					pthread_mutex_unlock(&lock_n_threads);
				}
			} else if (status == FILE_DELETED) {
				struct Node *host_node = getNodeByHost(the_file->host_list, dinfo->dthost);
				free(dinfo);
				if (host_node){
					removeNode(the_file->host_list, host_node);
				} 
			}
		}
	}
	pthread_mutex_unlock(&lock_the_file);
}