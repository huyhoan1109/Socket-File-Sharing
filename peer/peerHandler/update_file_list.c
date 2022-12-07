#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <sys/stat.h>

#include "update_file_list.h"
#include "connect_index_server.h"

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "../../socket/utils/LinkedList.h"

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

uint16_t dataPort = 0;
struct LinkedList *monitor_files = NULL;

void announceDataPort(int sockfd){
	/* mutex lock the socket to avoid intersection of multiple messages 
	 * from different types */
	pthread_mutex_lock(&lock_servsock);

	int n_bytes = writeBytes(sockfd, 
							(void*)&DATA_PORT_ANNOUNCEMENT, 
							sizeof(DATA_PORT_ANNOUNCEMENT));
	if (n_bytes <= 0){
		print_error("send DATA_PORT_ANNOUNCEMENT to index server");
		exit(1);
	}
	fprintf(stream, "update_file_list.c > dataPort = %u\n", ntohs(dataPort));
	n_bytes = writeBytes(sockfd,
						(void*)&dataPort,
						sizeof(dataPort));
	if (n_bytes <= 0){
		print_error("send data port to index server");
		exit(1);
	}

	pthread_mutex_unlock(&lock_servsock);
}

static void send_file_list(int sockfd, struct FileStatus *fs, uint8_t n_fs){
	/* mutex lock the socket to avoid intersection of multiple messages 
	 * from different types */
	pthread_mutex_lock(&lock_servsock);

	//send header
	int n_bytes = writeBytes(sockfd, (void*)&FILE_LIST_UPDATE, sizeof(FILE_LIST_UPDATE));

	if (n_bytes <= 0){
		print_error("send FILE_LIST_UPDATE to index server");
		exit(1);
	}
	
	//send the number of files
	n_bytes = writeBytes(sockfd, (void*)&n_fs, sizeof(n_fs));

	//send file list
	uint8_t i = 0;
	for (; i < n_fs; ++i){
		fprintf(stream, "sending info of \'%s\'\n", fs[i].filename);

		//send file status
		fprintf(stream, "status: %u\n", fs[i].status);
		n_bytes = writeBytes(sockfd, (void*)&fs[i].status, sizeof(fs[i].status));
		if (n_bytes <= 0){
			print_error("send file status to index server");
			exit(1);
		}
		//send filename length
		//+1 to include '\0'
		uint16_t filename_length = strlen(fs[i].filename) + 1;
		fprintf(stream, "filename_length: %u\n", filename_length);
		filename_length = htons(filename_length);
		fprintf(stream, "filename_length: %u (big endian)\n", filename_length);
		n_bytes = writeBytes(sockfd, (void*)&filename_length, sizeof(filename_length));
		if (n_bytes <= 0){
			print_error("send filename length to index server");
			exit(1);
		}
		//send filename
		fprintf(stream, "filename: \'%s\'\n", fs[i].filename);
		n_bytes = writeBytes(sockfd, (void*)(fs[i].filename), ntohs(filename_length));
		if (n_bytes <= 0){
			print_error("send filename to index server");
			exit(1);
		}
		//send file size
		fprintf(stream, "filesize: %u\n", fs[i].filesize);
		fs[i].filesize = htonl(fs[i].filesize);
		fprintf(stream, "filesize: %u (big endian)\n", fs[i].filesize);
		n_bytes = writeBytes(sockfd, (void*)&fs[i].filesize, sizeof(fs[i].filesize));
		if (n_bytes <= 0){
			print_error("send filesize to index server");
			exit(1);
		}
	}
	pthread_mutex_unlock(&lock_servsock);
	return;
}

void* update_file_list(void *arg){
	char *dir_name = (char*)arg;
	//data_port_announcement must be sent first
	announceDataPort(servsock);
	/* list files in a directory */
	struct FileStatus fs[20];
	uint8_t n_fs = 0;
	DIR *dir;
	struct dirent *ent;
	dir = opendir(dir_name);
	if (dir){
		while ((ent = readdir(dir)) != NULL){
			if (ent->d_name[0] == '.')
				continue;			
			fprintf(stream, "new file: %s\n", ent->d_name);
			strcpy(fs[n_fs].filename, ent->d_name);
			fprintf(stream, "fs[n_fs].filename: %s\n", fs[n_fs].filename);
			uint32_t sz = getFileSize(dir_name, ent->d_name);
			fprintf(stream, "filesize: %u\n", sz);
			if (sz < 0)
				continue;
			fs[n_fs].filesize = sz;
			fs[n_fs ++].status = FILE_NEW;
		}
		closedir(dir);

		if (n_fs > 0)
			send_file_list(servsock, fs, n_fs);
		else
			fprintf(stream, "Directory %s is empty\n", dir_name);
		
	} else {
		print_error("opendir");
		exit(1);
	}
	monitor_directory(dir_name, servsock);
	return NULL;
}

struct Node* getNode(struct LinkedList *ll, char *filename){
	if (!ll || !filename)
		return NULL;
	
	struct Node *it = ll->head;
	for (; it != NULL; it = it->next){
		char *name = (char*)it->data;
		if (strcmp(name, filename) == 0)
			return it;
	}

	return NULL;
}

void monitor_directory(char *dir, int socketfd){
	fprintf(stream, "exec monitor_directory\n");
	/* monitoring continously */
	if (monitor_files == NULL){
		monitor_files = newLinkedList();
	}
	int length, i = 0;
	int inotifyfd;
	int watch;
	char buffer[BUF_LEN];
	inotifyfd = inotify_init();
	if (inotifyfd < 0){
		print_error("inotify_init");
		exit(1);
	}

	watch = inotify_add_watch(inotifyfd, dir, IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_CLOSE_WRITE | IN_MOVED_FROM);
	if (watch < 0){
		print_error("inotify_add_watch");
		exit(1);
	}

	while (1){
		struct FileStatus fs[20];
		uint8_t n_fs = 0;

		length = read(inotifyfd, buffer, BUF_LEN);

		if (length < 0){
			print_error("inotify_read");
			continue;
		}
		
		i = 0;
		while (i < length){
			struct inotify_event *event = (struct inotify_event*) &buffer[i];
			if (event->len){
				if (!(event->mask & IN_ISDIR)){
					if (event->mask & IN_CREATE){
						struct Node *file_node = newNode(event->name, STRING_TYPE);
						push(monitor_files, file_node);
					} else if (event->mask & IN_MOVED_TO){
						fprintf(stream, "The file %s was created.\n", event->name);
						fs[n_fs].status = FILE_NEW;
						strcpy(fs[n_fs].filename, event->name);
						uint32_t sz = getFileSize(dir, event->name);
						if (sz < 0)
							continue;
						fs[n_fs].filesize = sz;
						n_fs ++;
					} else if (event->mask & IN_CLOSE_WRITE){
						struct Node *file_node = getNode(monitor_files, event->name);
						if (file_node){
							fprintf(stream, "The file %s was created.\n", event->name);
							fs[n_fs].status = FILE_NEW;
							strcpy(fs[n_fs].filename, event->name);
							uint32_t sz = getFileSize(dir, event->name);
							printf("filesize: %u\n", sz);
							if (sz < 0)
								continue;
							fs[n_fs].filesize = sz;
							n_fs ++;
							printf("n_fs: %d\n", n_fs);
							removeNode(monitor_files, file_node);
						}
					} else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM){
						fprintf(stream, "The file %s was deleted.\n", event->name);
						fs[n_fs].status = FILE_DELETED;
						strcpy(fs[n_fs].filename, event->name);
						fs[n_fs].filesize = 0;
						n_fs ++;
					}
				}
			}
			i += EVENT_SIZE + event->len;
		}
		send_file_list(socketfd, fs, n_fs);
	}
}
