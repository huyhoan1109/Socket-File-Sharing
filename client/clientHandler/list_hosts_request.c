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

	char *message = malloc(MAX_BUFF_SIZE);;
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
	printf("%s\n", message);
	char *subtext = malloc(MAX_BUFF_SIZE);
	strcpy(subtext, message);
	char *token;
	token = strtok(subtext, MESSAGE_DIVIDER);
	
	fprintf(stream, "Exec process_list_hosts_response\n");
	token = strtok(NULL, MESSAGE_DIVIDER);
	uint8_t sequence = atol(token);
	fprintf(stream, "[process_list_hosts_response] Received seq_no: %u\n", sequence);
	token = strtok(NULL, MESSAGE_DIVIDER);
	
	uint32_t filesize = ntohl(atoll(token));
	fprintf(stream, "[process_list_hosts_response] filesize: %u\n", filesize);

	pthread_mutex_lock(&lock_the_file);
	if (sequence == seq_no) the_file->filesize = filesize;
	
	token = strtok(NULL, MESSAGE_DIVIDER);
	uint32_t n_hosts = atol(token);
	fprintf(stream, "[process_list_hosts_response] n_hosts: %u\n", n_hosts);

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
		fprintf(stream, "[process_list_hosts_response] Status: %u\n", status);
		
		token = strtok(NULL, MESSAGE_DIVIDER);
		uint32_t ip_addr = ntohl(atoll(token));
		fprintf(stream, "[process_list_hosts_response] IP: %u\n", ip_addr);
		printf("%d", ip_addr);
		token = strtok(NULL, MESSAGE_DIVIDER);
		uint16_t data_port = ntohs(atoll(token));	
		fprintf(stream, "[process_list_hosts_response] Port: %u\n", data_port);
		
		if (sequence == seq_no){
			struct DownloadInfo *dinfo = malloc(sizeof(struct DownloadInfo));
			dinfo->dthost.ip_addr = ip_addr;
			dinfo->dthost.port = data_port;
			dinfo->seq_no = sequence;
			if (status == FILE_NEW){
				struct Node *host_node = getNodeByHost(the_file->host_list, dinfo->dthost);
				if (!host_node){
					fprintf(stream, "[process_list_hosts_response] New host, add to the list\n");
					host_node = newNode(&dinfo->dthost, DATA_HOST_TYPE);
					push(the_file->host_list, host_node);
					/* create new thread to download file */
					pthread_mutex_lock(&lock_n_threads);
					if (n_threads >= 0){
						pthread_t tid;
						int thr = pthread_create(&tid, NULL, &download_file, dinfo);
						if (thr != 0){
							fprintf(stream, "Cannot create new thread to download file\n");
							free(dinfo);
							pthread_mutex_unlock(&lock_n_threads);
							continue;
						}
						n_threads ++;
						fprintf(stream, "[process_list_hosts_response] Created new thread to download file\n");
					}
					pthread_mutex_unlock(&lock_n_threads);
				}
			} else if (status == FILE_DELETED) {
				struct Node *host_node = getNodeByHost(the_file->host_list, dinfo->dthost);
				free(dinfo);
				if (host_node){
					removeNode(the_file->host_list, host_node);
					fprintf(stream, "[process_list_hosts_response] Host removed\n");
				} else {
					fprintf(stream, "[process_list_hosts_response] Host is not existed\n");
				}
			}
		}
	}
	pthread_mutex_unlock(&lock_the_file);
}