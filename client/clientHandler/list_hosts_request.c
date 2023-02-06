#include "list_hosts_request.h"
#include "connect_index_server.h"
#include "download_file_request.h"

uint8_t seq_no = 0;
struct FileOwner *the_file = NULL;
pthread_mutex_t lock_the_file = PTHREAD_MUTEX_INITIALIZER;

void send_list_hosts_request(char *filename){
	pthread_mutex_lock(&lock_servsock);
	// LIST_HOST_REQUEST :=: seq_no :=: filename

	char *info = calloc(MAX_BUFF_SIZE, sizeof(char));
	
	pthread_mutex_lock(&lock_the_file);
	
	info = appendInfo(NULL, itoa(seq_no));
	info = appendInfo(info, filename);
	
	pthread_mutex_unlock(&lock_the_file);

	char *message = addHeader(info, LIST_HOSTS_REQUEST);

	writeBytes(servsock, message, MAX_BUFF_SIZE);
	free(message);
	free(info);
	pthread_mutex_unlock(&lock_servsock);
}

void process_list_hosts_response(char* message){
	char *info = getInfo(message);
	uint8_t sequence = atol(nextInfo(info, IS_FIRST));
	uint32_t filesize = atoll(nextInfo(info, IS_AFTER));

	pthread_mutex_lock(&lock_the_file);
	if (sequence == seq_no) totalsize = filesize;
	
	uint32_t n_hosts = atol(nextInfo(info, IS_AFTER));

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
		uint8_t status = atol(nextInfo(info, IS_AFTER));
		uint32_t ip_addr = atoll(nextInfo(info, IS_AFTER));
		uint16_t data_port = atoll(nextInfo(info, IS_AFTER));	
		
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
	free(info);
}