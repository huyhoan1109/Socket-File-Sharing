#include "handle_request.h"

struct LinkedList *file_list = NULL;
struct LinkedList *user_list = NULL;

pthread_mutex_t lock_host = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_file_list = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_file_list = PTHREAD_COND_INITIALIZER;

static void displayFileListFromHost(uint32_t ip_addr, uint16_t port){
	char delim[74];
	memset(delim, '-', 73);
	delim[73] = 0;
	pthread_mutex_lock(&lock_file_list);
	pthread_cleanup_push(mutex_unlock, &lock_file_list);
	printf("%s\n", delim);
	printf("%-4s | %-21s | %-25s | %-10s\n", "No", "Host", "Filename", "Filesize (bytes)");
	struct Node *it = file_list->head;
	int k = 0;
	for (; it != NULL; it = it->next){
		struct FileOwner *fo = (struct FileOwner*)it->data;
		struct Node *it2 = fo->host_list->head;
		for (; it2 != NULL; it2 = it2->next){
			struct DataHost *dh = (struct DataHost*)it2->data;
			if (ip_addr == dh->ip_addr && port == dh->port){
				char host[22];
				struct in_addr addr;
				addr.s_addr = htonl(dh->ip_addr);
				sprintf(host, "%s:%u", inet_ntoa(addr), dh->port);
				k++;
				printf("%-4d | %-21s | %-25s | %-10u\n", k, host, fo->filename, dh->filesize);
				break;
			}
		}
	}
	pthread_cleanup_pop(0);
	pthread_mutex_unlock(&lock_file_list);
	printf("%s\n", delim);
}

static void displayFileList(){
	char delim[74];
	memset(delim, '-', 73);
	delim[73] = 0;
	pthread_mutex_lock(&lock_file_list);
	pthread_cleanup_push(mutex_unlock, &lock_file_list);
	if (file_list == NULL) {
		printf("No file current in the list\n");
	} else{
		printf("%s\n", delim);
		printf("%-4s | %-21s | %-25s | %-10s\n", "No", "Host", "Filename", "Filesize (bytes)");
		struct Node *it = file_list->head;
		int k = 0;
		for (; it != NULL; it = it->next){
			struct FileOwner *fo = (struct FileOwner*)it->data;
			if (fo->host_list == NULL){continue;}
			struct Node *it2 = fo->host_list->head;
			for (; it2 != NULL; it2 = it2->next){
				k++;
				struct DataHost *dh = (struct DataHost*)it2->data;
				char host[22];
				struct in_addr addr;
				addr.s_addr = htonl(dh->ip_addr);
				sprintf(host, "%s:%u", inet_ntoa(addr), dh->port);
				printf("%-4d | %-21s | %-25s | %-10u\n", k, host, fo->filename, dh->filesize);
			}
		}
		printf("%s\n", delim);
	}
	pthread_cleanup_pop(0);
	pthread_mutex_unlock(&lock_file_list);
}

void handleSocketError(struct net_info cli_info, char *mess){
	//print error message
	char err_mess[256];
	int errnum = errno;
	sprintf(err_mess, "(%s:%u) > %s", cli_info.ip_add, cli_info.port, mess);	
	strerror_r(errnum, err_mess, sizeof(err_mess));
	fprintf(stderr, "%s [ERROR]: %s\n", mess, err_mess);

	/* remove the host from the file_list */
	struct DataHost host;
	host.ip_addr = ntohl(inet_addr(cli_info.ip_add));
	host.port = cli_info.data_port;
	removeHost(host);
	fprintf(stderr, "Closing connection from (%s:%u)\n", cli_info.ip_add, cli_info.port);
	close(cli_info.sockfd);
	fprintf(stderr, "Connection from (%s:%u) closed\n", cli_info.ip_add, cli_info.port);

	/* display file list */
	fprintf(stdout, "Full file list after removing the host: \n");
	displayFileList();

	int ret = 100;
	pthread_exit(&ret);
}

void removeHost(struct DataHost host){
	pthread_mutex_lock(&lock_file_list);
	pthread_cleanup_push(mutex_unlock, &lock_file_list);
	if (file_list != NULL){
		struct Node *it = file_list->head;
		int need_to_remove_the_head = 0;
		for (; it != NULL; it = it->next){
			struct FileOwner *tmp = (struct FileOwner*)it->data;
			struct Node *host_node = getNodeByHost(tmp->host_list, host);
			if (host_node){
				removeNode(tmp->host_list, host_node);
				/* if the host_list is empty, also remove the file from file_list */
				if (tmp->host_list->n_nodes <= 0){
					if (it == file_list->head){
						need_to_remove_the_head = 1;
						continue;
					}
					it = it->prev;		//if (it == head) => it->prev == NULL
					removeNode(file_list, it->next);
				}
			}
		}
		if (need_to_remove_the_head){
			pop(file_list);
		}
	}
	pthread_cond_broadcast(&cond_file_list);
	pthread_cleanup_pop(0);
	pthread_mutex_unlock(&lock_file_list);
}

void update_file_list(struct net_info cli_info, char *info){
	// FILE_LIST_UPDATE :=: file_num :=: (trang thai :=: ten :=: do lon ...)
	pthread_mutex_lock(&lock_file_list);
	if (!file_list){
		file_list = newLinkedList();
	}
	pthread_mutex_unlock(&lock_file_list);
		
	int n_files = atoi(nextInfo(info, IS_FIRST));
	int changed = 0;
	uint8_t status;
	uint32_t filesize;
	char *filename = calloc(30, sizeof(char));
	for (int i=0; n_files > i; i++){
		status = (uint8_t) atoi(nextInfo(info, IS_AFTER));		
		filename = nextInfo(info, IS_AFTER);
		filesize = (uint32_t) atoll(nextInfo(info, IS_AFTER));

		pthread_mutex_lock(&lock_file_list);
		struct DataHost host;
		host.ip_addr = ntohl(inet_addr(cli_info.ip_add));
		host.port = cli_info.data_port;
		host.filesize = filesize;
		pthread_cleanup_push(mutex_unlock, &lock_file_list);
		struct Node *host_node;
		struct Node *file_node;
		switch (status) {
			case FILE_AVAIL:
				host_node = newNode(&host, DATA_HOST_TYPE);
				file_node = getNodeByFilename(file_list, filename);
				if (file_node){
					struct FileOwner *file = (struct FileOwner*)file_node->data;
					struct Node *curr_host = llContainHost(file->host_list, host);
					// auto update value in curr_host
					if (curr_host) removeNode(file->host_list, curr_host);
					push(file->host_list, host_node);
				} else {
					struct FileOwner new_file;
					strcpy(new_file.filename, filename);
					new_file.host_list = newLinkedList();
					push(new_file.host_list, host_node);
					file_node = newNode(&new_file, FILE_OWNER_TYPE);
					push(file_list, file_node);
				}
				changed = 1;
				break;
			case FILE_DELETED:
				changed = 1;
				struct Node *it = file_list->head;
				for (; it != NULL; it = it->next){
					struct FileOwner *curr_file = (struct FileOwner*)(it->data);
					if(strcmp(curr_file->filename, filename) == 0){
						host_node = getNodeByHost(curr_file->host_list, host);
						if (host_node){
							removeNode(curr_file->host_list, host_node);
						}
					}
					if (curr_file->host_list->n_nodes <= 0){
						removeNode(file_list, it);
					}
				}
				changed = 1;
				break;
			default:
				break;
		}
		pthread_cond_broadcast(&cond_file_list);
		pthread_cleanup_pop(0);
		pthread_mutex_unlock(&lock_file_list);
	}
	if (changed){
		fprintf(stdout, "(%s:%u) > file list from this host:\n", cli_info.ip_add, cli_info.port);
		displayFileListFromHost(ntohl(inet_addr(cli_info.ip_add)), cli_info.data_port);
		fprintf(stdout, "Full file list:\n");
		displayFileList();
	}
	free(filename);
}

void process_list_files_request(struct net_info cli_info){
	pthread_mutex_lock(cli_info.lock_sockfd);
	pthread_mutex_lock(&lock_file_list);
	
	uint8_t n_files = 0;
	long n_bytes;
	if (file_list == NULL){
		n_files = 0;
	} else {
		n_files = file_list->n_nodes;
	} 
	char *info = appendInfo(NULL, itoa(n_files));
	if (n_files > 0){
		struct Node *it = file_list->head;
		for (; it != NULL; it = it->next){
			struct FileOwner *file = (struct FileOwner*)it->data;
			int n_host = file->host_list->n_nodes;
			info = appendInfo(info, file->filename);		
			info = appendInfo(info, itoa(n_host));
			struct Node *h_list = file->host_list->head;
			uint32_t minSize= 0xFFFFFFFF, maxSize=0;
			for (; h_list != NULL; h_list = h_list->next){
				struct DataHost *h_info = (struct DataHost*)h_list->data;
				if(h_info->filesize > maxSize) maxSize = h_info->filesize;
				if(h_info->filesize < minSize) minSize = h_info->filesize;
			}
			info = appendInfo(info, itoa(minSize));
			info = appendInfo(info, itoa(maxSize));
		}
	}
	char *message = addHeader(info, LIST_FILES_RESPONSE);
	n_bytes = writeBytes(cli_info.sockfd, message, MAX_BUFF_SIZE);
	if (n_bytes <= 0) {
		handleSocketError(cli_info, "Send list response");
	}
	free(message);
	free(info);
	pthread_mutex_unlock(&lock_file_list);
	pthread_mutex_unlock(cli_info.lock_sockfd);
}

static void send_host_list(struct thread_data *thrdt, struct LinkedList *chg_hosts){
	pthread_mutex_lock(thrdt->cli_info.lock_sockfd);
	long n_bytes;
	
	//add seq_no and filesize;
	char *info = calloc(MAX_BUFF_SIZE, sizeof(char));
	
	info = appendInfo(NULL, itoa(thrdt->seq_no));

	//add n_hosts
	if (chg_hosts == NULL){
		info = appendInfo(info, itoa(0));
	} else {
		info = appendInfo(info, itoa(chg_hosts->n_nodes));
		struct Node *it = chg_hosts->head;
		for (; it != NULL; it = it->next){
			struct DataHost *host = (struct DataHost*)(it->data);
			//send status
			info = appendInfo(info, itoa(host->status));
			//send ip
			info = appendInfo(info, itoa(host->ip_addr));
			//send data port
			info = appendInfo(info, itoa(host->port));
			//send file size 
			info = appendInfo(info, itoa(host->filesize));
		}
	}
	char *message = addHeader(info, LIST_HOSTS_RESPONSE);
	n_bytes = write(thrdt->cli_info.sockfd, message, MAX_BUFF_SIZE);
	if (n_bytes <= 0){
		handleSocketError(thrdt->cli_info, "Send host response");
	}
	free(info);
	free(message);
	pthread_mutex_unlock(thrdt->cli_info.lock_sockfd);
	return;
}

void* process_list_hosts_request(void *arg){
	pthread_detach(pthread_self());
	struct thread_data *thrdt = (struct thread_data*)arg;
	char filename[256];
	
	strcpy(filename, thrdt->filename);
	uint8_t sequence = thrdt->seq_no;

	struct Node *file_node = NULL;
	
	pthread_mutex_lock(&lock_file_list);
	file_node = getNodeByFilename(file_list, thrdt->filename);
	pthread_mutex_unlock(&lock_file_list);
	if (!file_node){
		send_host_list(thrdt, NULL);
		return NULL;
	}

	//the old host list, use to compare with the new host list
	//to detect the changed hosts
	struct LinkedList *old_ll = newLinkedList();

	//the changed hosts
	struct LinkedList *chg_hosts = NULL;
	pthread_mutex_lock(&lock_file_list);

	while(1){
		/* client request a new file */
		if (sequence != thrdt->seq_no){
			pthread_mutex_unlock(&lock_file_list);
			destructLinkedList(old_ll);
			/* terminate this thread */
			int ret = 100;
			pthread_exit(&ret);
		}
		file_node = getNodeByFilename(file_list, thrdt->filename);
		if (file_node){
			/* there is at least 1 host that contains the file */
			chg_hosts = newLinkedList();
			struct FileOwner *file = (struct FileOwner*)(file_node->data);

			/* check if the i-th host of the file->host_list is the new host:
			 * check[i]: the number of comparison performed with the i-th host,
			 * if check[i] is equal to the number of hosts in the old_ll,
			 * => the i-th host is the new host */
			
			uint8_t *check = calloc(file->host_list->n_nodes, sizeof(uint8_t));
			bzero(check, file->host_list->n_nodes);

			struct Node *it1 = old_ll->head;
			for (; it1 != NULL; it1 = it1->next){
				struct DataHost *host1 = (struct DataHost*)(it1->data);
				struct Node *it2 = file->host_list->head;
				int same = 0;
				uint8_t i = 0;
				for (; it2 != NULL; it2 = it2->next){
					struct DataHost *host2 = (struct DataHost*)(it2->data);
					if(host1->ip_addr == host2->ip_addr && host1->port == host2->port){
						same = 1;
						break;
					}
					check[i++] ++;
				}
				i++;
				for (; i < file->host_list->n_nodes; i++){
					check[i]++;
				}
				/* cannot find any host that is the same as the host1,
				 * host1 has been deleted */
				if (!same){
					host1->status = FILE_DELETED;
					struct Node *new_node = newNode(host1, DATA_HOST_TYPE);
					push(chg_hosts, new_node);
				}
			}
			uint8_t i = 0;
			struct Node *it = file->host_list->head;
			for (; it != NULL; it = it->next){
				/* new host */
				if (check[i] == old_ll->n_nodes){
					struct DataHost *host2 = (struct DataHost*)(it->data);
					host2->status = FILE_AVAIL;
					struct Node *new_node = newNode(host2, DATA_HOST_TYPE);
					push(chg_hosts, new_node);
				}
				i++;
			}
			free(check);
			destructLinkedList(old_ll);
			old_ll = copyLinkedList(file->host_list);
		} else {
			/* there is no host that own the file */
			chg_hosts = copyLinkedList(old_ll);
			struct Node *it = chg_hosts->head;
			for (; it != NULL; it = it->next){
				struct DataHost *host = (struct DataHost*)(it->data);
				host->status = FILE_DELETED;
			}
			destructLinkedList(old_ll);
			old_ll = newLinkedList();
		}

		pthread_mutex_unlock(&lock_file_list);

		if (chg_hosts->n_nodes != 0) send_host_list(thrdt, chg_hosts);

		destructLinkedList(chg_hosts);
		chg_hosts = NULL;
		pthread_cond_wait(&cond_file_list, &lock_file_list);
	}

	return NULL;
}