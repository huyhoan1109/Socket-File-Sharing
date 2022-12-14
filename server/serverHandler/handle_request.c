#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "handle_request.h"

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "../../socket/utils/LinkedList.h"
#include "../../socket/utils/LinkedListUtils.h"

struct LinkedList *file_list = NULL;
pthread_mutex_t lock_file_list = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_file_list = PTHREAD_COND_INITIALIZER;

static void displayFileListFromHost(uint32_t ip_addr, uint16_t port){
	char delim[74];
	memset(delim, '-', 73);
	delim[73] = 0;
	pthread_mutex_lock(&lock_file_list);
	pthread_cleanup_push(mutex_unlock, &lock_file_list);
	printf("%s\n", delim);
	printf("%-4s | %-21s | %-25s | %-10s\n", "No", "Host", "Filename", "Filesize (byte)");
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
				printf("%-4d | %-21s | %-25s | %-10u\n", k, host, fo->filename, fo->filesize);
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
	printf("%s\n", delim);
	printf("%-4s | %-21s | %-25s | %-10s\n", "No", "Host", "Filename", "Filesize (byte)");
	struct Node *it = file_list->head;
	int k = 0;
	for (; it != NULL; it = it->next){
		struct FileOwner *fo = (struct FileOwner*)it->data;
		if (fo->host_list == NULL){
			fprintf(stream, "\'%s\' has fo->host_list == NULL\n", fo->filename);
			continue;
		}
		struct Node *it2 = fo->host_list->head;
		for (; it2 != NULL; it2 = it2->next){
			k++;
			struct DataHost *dh = (struct DataHost*)it2->data;
			char host[22];
			struct in_addr addr;
			addr.s_addr = htonl(dh->ip_addr);
			sprintf(host, "%s:%u", inet_ntoa(addr), dh->port);
			printf("%-4d | %-21s | %-25s | %-10u\n", k, host, fo->filename, fo->filesize);
		}
	}
	pthread_cleanup_pop(0);
	pthread_mutex_unlock(&lock_file_list);
	printf("%s\n", delim);
}

void handleSocketError(struct net_info cli_info, char *mess){
	//print error message
	char err_mess[256];
	sprintf(err_mess, "%s:%u > %s", cli_info.ip_add, cli_info.port, mess);
	print_error(err_mess);

	/* remove the host from the file_list */
	struct DataHost host;
	host.ip_addr = ntohl(inet_addr(cli_info.ip_add));
	host.port = cli_info.data_port;
	removeHost(host);

	fprintf(stderr, "closing connection from %s:%u\n", cli_info.ip_add, cli_info.port);
	close(cli_info.sockfd);
	fprintf(stderr, "connection from %s:%u closed\n", cli_info.ip_add, cli_info.port);

	/* display file list */
	fprintf(stdout, "Full file list after removing the host: \n");
	displayFileList();

	int ret = 100;
	pthread_exit(&ret);
}

void removeHost(struct DataHost host){
	fprintf(stream, "function: removeHost\n");
	pthread_mutex_lock(&lock_file_list);
	pthread_cleanup_push(mutex_unlock, &lock_file_list);
	struct Node *it = file_list->head;
	int need_to_remove_the_head = 0;
	for (; it != NULL; it = it->next){
		struct FileOwner *tmp = (struct FileOwner*)it->data;
		struct Node *host_node = getNodeByHost(tmp->host_list, host);
		//printf("host_node: %p\n", host_node);
		if (host_node){
			fprintf(stream, "function: removeNode/host\n");
			removeNode(tmp->host_list, host_node);
			//fprintf(stream, "removeNode/host done\n");
			/* if the host_list is empty, also remove the file from file_list */
			if (tmp->host_list->n_nodes <= 0){
				if (it == file_list->head){
					need_to_remove_the_head = 1;
					continue;
				}
				it = it->prev;		//if (it == head) => it->prev == NULL
				fprintf(stream, "function: removeNode/file\n");
				removeNode(file_list, it->next);
				//fprintf(stream, "removeNode/file done\n");
			}
		}
	}
	if (need_to_remove_the_head){
		pop(file_list);
	}
	pthread_cond_broadcast(&cond_file_list);
	pthread_cleanup_pop(0);
	pthread_mutex_unlock(&lock_file_list);
}

void update_file_list(struct net_info cli_info){
	pthread_mutex_lock(&lock_file_list);
	if (!file_list){
		file_list = newLinkedList();
	}
	pthread_mutex_unlock(&lock_file_list);

	//pthread_cleanup_push(mutex_unlock, &lock_file_list);
	//pthread_cleanup_push(mutex_unlock, cli_info.lock_sockfd);

	char cli_addr[22];

	sprintf(cli_addr, "%s:%u", cli_info.ip_add, cli_info.port);

	fprintf(stream, "%s > file_list_update\n", cli_addr);

	long n_bytes;
	uint8_t n_files;
	n_bytes = readBytes(cli_info.sockfd, &n_files, sizeof(n_files));
	if (n_bytes <= 0){
		handleSocketError(cli_info, "read n_files from socket");
	}
	fprintf(stream, "%s > n_files: %u\n", cli_addr, n_files);
	uint8_t i = 0;
	int changed = 0;
	for (; i < n_files; i++){
		//file status
		uint8_t status;
		n_bytes = readBytes(cli_info.sockfd, &status, sizeof(status));
		if (n_bytes <= 0){
			handleSocketError(cli_info, "read status from socket");
		}
		fprintf(stream, "%s > status: %u\n", cli_addr, status);

		//filename length
		uint16_t filename_length;
		n_bytes = readBytes(cli_info.sockfd, &filename_length, sizeof(filename_length));
		if (n_bytes <= 0){
			handleSocketError(cli_info, "read filename_length from socket");
		}
		filename_length = ntohs(filename_length);
		fprintf(stream, "%s > filename_length: %u\n", cli_addr, filename_length);

		//filename
		char filename[256];
		n_bytes = readBytes(cli_info.sockfd, filename, filename_length);
		if (n_bytes <= 0){
			handleSocketError(cli_info, "read filename from socket"); 
		} 
		fprintf(stream, "%s > filename: %s\n", cli_addr, filename);

		//filesize
		uint32_t filesize;
		n_bytes = readBytes(cli_info.sockfd, &filesize, sizeof(filesize));
		if (n_bytes <= 0){
			handleSocketError(cli_info, "read filesize from socket");
		}
		filesize = ntohl(filesize);
		fprintf(stream, "%s > filesize: %u\n", cli_addr, filesize);
		
		//struct DataHost *host = malloc(sizeof(struct DataHost));
		struct DataHost host;
		host.ip_addr = ntohl(inet_addr(cli_info.ip_add));
		host.port = cli_info.data_port;

		pthread_mutex_lock(&lock_file_list);
		pthread_cleanup_push(mutex_unlock, &lock_file_list);
		if (status == FILE_NEW){
			fprintf(stream, "[update_file_list] %s > new file \'%s\'", cli_addr, filename);
			changed = 1;
			/* add the host to the list */
			struct Node *host_node = newNode(&host, DATA_HOST_TYPE);

			fprintf(stream, "[update_file_list] adding host\n");

			//check if the file has already been in the list
			struct Node *file_node = getNodeByFilename(file_list, filename);
			if (file_node){
				fprintf(stream, "\'%s\' existed, add host to the list\n", filename);
				/* insert the host into the host_list of the file
				 */

				//get the node which contains the file in the file_list
				struct FileOwner *file = (struct FileOwner*)file_node->data;

				/* need to check if the host already existed */
				if (!llContainHost(file->host_list, host))
					push(file->host_list, host_node);
			} else {
				fprintf(stream, "add \'%s\' to file_list, add host to host_list\n", filename);
				//create a new node to store file's info
				struct FileOwner new_file;
				strcpy(new_file.filename, filename);
				fprintf(stream, "%s > new_file->filename: %s\n", cli_addr, new_file.filename);
				new_file.filesize = filesize;
				fprintf(stream, "%s > new_file->filesize: %u\n", cli_addr, new_file.filesize);
				new_file.host_list = newLinkedList();
				fprintf(stream, "%s > new_file->host_list: %p\n", cli_addr, new_file.host_list);
				push(new_file.host_list, host_node);
				fprintf(stream, "%s > new_file->host_list->head: %p\n", 
						cli_addr, new_file.host_list->head);
				file_node = newNode(&new_file, FILE_OWNER_TYPE);
				push(file_list, file_node);
			}
			//pthread_cond_broadcast(&cond_file_list);

			fprintf(stream, "%s > added a new file: %s\n", 
					cli_addr, filename);
		} else if (status == FILE_DELETED){
			fprintf(stream, "[update_file_list] %s > \'%s\' deleted\n", cli_addr, filename);
			changed = 1;
			/* remove the host from the list */
			//remove the host from the host_list of the file
			/* need to check if the file really exist in the file_list
			 * need to check if the host really exist in the host_list of the file */
			struct Node *file_node = getNodeByFilename(file_list, filename);
			if (file_node){
				struct FileOwner *file = (struct FileOwner*)(file_node->data);
				struct Node *host_node = getNodeByHost(file->host_list, host);
				if (host_node)
					removeNode(file->host_list, host_node);
				/* if the host_list is empty, also remove the file from file_list */
				if (file->host_list->n_nodes <= 0){
					removeNode(file_list, file_node);
				}
				fprintf(stream, "%s > deleted a file: %s\n", cli_addr, filename);
			}
		}
		pthread_cond_broadcast(&cond_file_list);
		pthread_cleanup_pop(0);
		pthread_mutex_unlock(&lock_file_list);
	}
	if (changed){
		fprintf(stdout, "%s > file list from this host:\n", cli_addr);
		displayFileListFromHost(ntohl(inet_addr(cli_info.ip_add)), cli_info.data_port);
		fprintf(stdout, "Full file list:\n");
		displayFileList();
	}

	//pthread_cleanup_pop(1);
	//pthread_cleanup_pop(1);
}

void process_list_files_request(struct net_info cli_info){
	char cli_addr[22];
	sprintf(cli_addr, "%s:%u", cli_info.ip_add, cli_info.port);
	fprintf(stream, "%s > list_files_request\n", cli_addr);

	long n_bytes;
	uint8_t n_files = 0;
	pthread_mutex_lock(cli_info.lock_sockfd);
	//pthread_cleanup_push(mutex_unlock, cli_info.lock_sockfd);
	//pthread_cleanup_push(mutex_unlock, cli_info.lock_sockfd);
	//pthread_cleanup_push(mutex_unlock, &lock_file_list);

	n_bytes = writeBytes(cli_info.sockfd, 
						(void*)&LIST_FILES_RESPONSE, 
						sizeof(LIST_FILES_RESPONSE));
	if (n_bytes <= 0){
		pthread_mutex_unlock(cli_info.lock_sockfd);
		handleSocketError(cli_info, "send LIST_FILES_RESPONSE message");
	}
	

	pthread_mutex_lock(&lock_file_list);

	if (file_list == NULL)
		n_files = 0;
	else {
		n_files = file_list->n_nodes;
	} 

	n_bytes = writeBytes(cli_info.sockfd, 
						&n_files, 
						sizeof(n_files));
	if (n_bytes <= 0){
		pthread_mutex_unlock(&lock_file_list);
		pthread_mutex_unlock(cli_info.lock_sockfd);
		handleSocketError(cli_info, "send n_files");
	}

	//pthread_cleanup_pop(0);		//lock_sockfd

	if (n_files == 0){
		pthread_mutex_unlock(&lock_file_list);
		pthread_mutex_unlock(cli_info.lock_sockfd);
		return;
	}

	//pthread_cleanup_push(mutex_unlock, cli_info.lock_sockfd);

	struct Node *it = file_list->head;
	for (; it != NULL; it = it->next){
		struct FileOwner *file = (struct FileOwner*)it->data;
		uint16_t filename_length = strlen(file->filename) + 1;
		filename_length = htons(filename_length);
		n_bytes = writeBytes(cli_info.sockfd, &filename_length, sizeof(filename_length));
		if (n_bytes <= 0){
			pthread_mutex_unlock(&lock_file_list);
			pthread_mutex_unlock(cli_info.lock_sockfd);
			handleSocketError(cli_info, "send filename_length");
		}
		
		n_bytes = writeBytes(cli_info.sockfd, file->filename, ntohs(filename_length));
		if (n_bytes <= 0){
			pthread_mutex_unlock(&lock_file_list);
			pthread_mutex_unlock(cli_info.lock_sockfd);
			handleSocketError(cli_info, "send filename");
		}
	}
	pthread_mutex_unlock(&lock_file_list);
	//pthread_cleanup_pop(0);		//lock_sockfd
	pthread_mutex_unlock(cli_info.lock_sockfd);
}

static void send_host_list(struct thread_data *thrdt, struct LinkedList *chg_hosts){
	fprintf(stream, "execute send_host_list\n");
	pthread_mutex_lock(thrdt->cli_info.lock_sockfd);
	//pthread_cleanup_push(mutex_unlock, thrdt->cli_info.lock_sockfd);
	
	//send header
	fprintf(stream, "[send_host_list] send LIST_HOSTS_RESPONSE\n");
	long n_bytes = writeBytes(thrdt->cli_info.sockfd, 
							(void*)&LIST_HOSTS_RESPONSE, 
							sizeof(LIST_HOSTS_RESPONSE));
	if (n_bytes <= 0){
		pthread_mutex_unlock(thrdt->cli_info.lock_sockfd);
		handleSocketError(thrdt->cli_info, "send LIST_HOSTS_RESPONSE message");
	}

	//send sequence number
	fprintf(stream, "[send_host_list] send sequence number\n");
	n_bytes = writeBytes(thrdt->cli_info.sockfd,
						&thrdt->seq_no,
						sizeof(thrdt->seq_no));
	if (n_bytes <= 0){
		pthread_mutex_unlock(thrdt->cli_info.lock_sockfd);
		handleSocketError(thrdt->cli_info, "[send_host_list] send sequence number");
	}
	
	//send filename length
	//uint16_t filename_length = strlen(thrdt->filename) + 1;
	//fprintf(stream, "[send_host_list]send filename_length: %u\n", filename_length);
	//filename_length = htons(filename_length);

	//n_bytes = writeBytes(thrdt->cli_info.sockfd, &filename_length, sizeof(filename_length));
	//if (n_bytes <= 0){
	//	handleSocketError(thrdt->cli_info, "send filename_length");
	//}
	//
	////send filename
	//fprintf(stream, "[send_host_list] send filename: %s\n", thrdt->filename);
	//n_bytes = writeBytes(thrdt->cli_info.sockfd, thrdt->filename, ntohs(filename_length));
	//if (n_bytes <= 0){
	//	handleSocketError(thrdt->cli_info, "send filename");
	//}

	//send filesize
	fprintf(stream, "[send_host_list] send filesize: %u\n", thrdt->filesize);
	uint32_t filesize = htonl(thrdt->filesize);
	n_bytes = writeBytes(thrdt->cli_info.sockfd, &filesize, sizeof(filesize));
	if (n_bytes <= 0){
		pthread_mutex_unlock(thrdt->cli_info.lock_sockfd);
		handleSocketError(thrdt->cli_info, "send filesize");
	}

	//send n_hosts
	if (chg_hosts == NULL){
		uint8_t n_nodes = 0;
		fprintf(stream, "[send_host_list]n_hosts: %u\n", n_nodes);

		n_bytes = writeBytes(thrdt->cli_info.sockfd, 
							&(n_nodes), 
							sizeof(n_nodes));
		if (n_bytes <= 0){
			pthread_mutex_unlock(thrdt->cli_info.lock_sockfd);
			handleSocketError(thrdt->cli_info, "send n_hosts");
		}
		pthread_mutex_unlock(thrdt->cli_info.lock_sockfd);
		return;
	}

	fprintf(stream, "[send_host_list]n_hosts: %u\n", chg_hosts->n_nodes);
	n_bytes = writeBytes(thrdt->cli_info.sockfd, 
						&(chg_hosts->n_nodes), 
						sizeof(chg_hosts->n_nodes));
	if (n_bytes <= 0){
		pthread_mutex_unlock(thrdt->cli_info.lock_sockfd);
		handleSocketError(thrdt->cli_info, "send n_hosts");
	}

	struct Node *it = chg_hosts->head;
	for (; it != NULL; it = it->next){
		struct DataHost *host = (struct DataHost*)(it->data);
		//send status
		fprintf(stream, "[send_host_list] send status: %u\n", host->status);
		n_bytes = writeBytes(thrdt->cli_info.sockfd, &(host->status), sizeof(host->status));
		if (n_bytes <= 0){
			pthread_mutex_unlock(thrdt->cli_info.lock_sockfd);
			handleSocketError(thrdt->cli_info, "send status");
		}

		//send ip
		fprintf(stream, "[send_host_list] send ip: %u\n", host->ip_addr);
		uint32_t ip_addr = htonl(host->ip_addr);
		n_bytes = writeBytes(thrdt->cli_info.sockfd, &ip_addr, sizeof(ip_addr));
		if (n_bytes <= 0){
			pthread_mutex_unlock(thrdt->cli_info.lock_sockfd);
			handleSocketError(thrdt->cli_info, "send ip addr");
		}

		//send data port
		fprintf(stream, "[send_host_list] send data port: %u\n", host->port);
		uint16_t data_port = htons(host->port);
		n_bytes = writeBytes(thrdt->cli_info.sockfd, &data_port, sizeof(data_port));
		if (n_bytes <= 0){
			pthread_mutex_unlock(thrdt->cli_info.lock_sockfd);
			handleSocketError(thrdt->cli_info, "send data port");
		}
	}
	pthread_mutex_unlock(thrdt->cli_info.lock_sockfd);
}

void* process_list_hosts_request(void *arg){
	fprintf(stream, "execute process_list_hosts_request\n");
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
			fprintf(stream, "[process_list_hosts_request] terminate thread due to the difference of sequence number\n");
			int ret = 100;
			pthread_exit(&ret);
		}
		file_node = getNodeByFilename(file_list, thrdt->filename);
		if (file_node){
			/* there is at least 1 host that contains the file */
			chg_hosts = newLinkedList();
			struct FileOwner *file = (struct FileOwner*)(file_node->data);
			thrdt->filesize = file->filesize;
			fprintf(stream, "[process_list_hosts_request]filesize: %u\n", file->filesize);

			/* check if the i-th host of the file->host_list is the new host:
			 * check[i]: the number of comparison performed with the i-th host,
			 * if check[i] is equal to the number of hosts in the old_ll,
			 * => the i-th host is the new host */
			uint8_t *check = malloc(file->host_list->n_nodes);
			fprintf(stream, "initialize check\n");
			bzero(check, file->host_list->n_nodes);

			fprintf(stream, "[process_list_hosts_request] detect new/deleted hosts\n");

			struct Node *it1 = old_ll->head;
			fprintf(stream, "[process_list_hosts_request] check for deleted hosts\n");
			for (; it1 != NULL; it1 = it1->next){
				struct DataHost *host1 = (struct DataHost*)(it1->data);
				fprintf(stream, "[process_list_hosts_request]\'%s\' compare host (%u:%u) with\n", 
						filename, host1->ip_addr, host1->port);
				struct Node *it2 = file->host_list->head;
				int same = 0;
				uint8_t i = 0;
				for (; it2 != NULL; it2 = it2->next){
					struct DataHost *host2 = (struct DataHost*)(it2->data);
					fprintf(stream, "[process_list_hosts_request]host (%u:%u)\n", 
							host2->ip_addr, host2->port);
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
			fprintf(stream, "[process_list_hosts_request] check for new hosts\n");
			for (; it != NULL; it = it->next){
				/* new host */
				if (check[i] == old_ll->n_nodes){
					struct DataHost *host2 = (struct DataHost*)(it->data);
					host2->status = FILE_NEW;
					struct Node *new_node = newNode(host2, DATA_HOST_TYPE);
					push(chg_hosts, new_node);
				}
				i++;
			}

			fprintf(stream, "[process_list_hosts_request] free(check)\n");
			free(check);
			fprintf(stream, "[process_list_hosts_request] destruct old_ll\n");
			destructLinkedList(old_ll);
			fprintf(stream, "[process_list_hosts_request] copy to old_ll\n");
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

		//int old_state;
		//prevent thread exiting, avoid sending incomplete message
		//pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);

		if (chg_hosts->n_nodes == 0){
			fprintf(stream, "[process_list_hosts_request]\'%s\' no update\n", filename);
		} else {
			send_host_list(thrdt, chg_hosts);
		}

		//allow this thread to exit
		//pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
		//pthread_testcancel();	//thread cancelation point

		destructLinkedList(chg_hosts);
		chg_hosts = NULL;
		pthread_cond_wait(&cond_file_list, &lock_file_list);
	}

	return NULL;
}