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
char dir_name[20];
struct LinkedList *monitor_files = NULL;

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

void popNode(struct LinkedList *ll, char *filename){
	struct Node *rm_node = getNode(monitor_files, filename);
	if (rm_node) removeNode(monitor_files, rm_node); 
}

void lockNode(struct LinkedList *ll, char *filename){
	struct Node *locked_node = getNode(monitor_files, filename);
	locked_node->status = FILE_LOCK;
}

void announceDataPort(int sockfd){
	/* mutex lock the socket to avoid intersection of multiple messages 
	 * from different types */
	pthread_mutex_lock(&lock_servsock);

	char *message = malloc(MAX_BUFF_SIZE);
	
	strcpy(message, itoa(DATA_PORT_ANNOUNCEMENT));
	strcat(message, MESSAGE_DIVIDER);
	strcat(message, itoa(dataPort));

	int n_bytes = writeBytes(sockfd, message, MAX_BUFF_SIZE);
	if (n_bytes <= 0){exit(1);}
	
	fprintf(stream, "update_file_list.c > socket = %u\n", ntohs(dataPort));

	pthread_mutex_unlock(&lock_servsock);
}

static void send_file_list(int sockfd, struct FileStatus *fs, uint8_t n_fs){
	/* mutex lock the socket to avoid intersection of multiple messages 
	 * from different types */
	pthread_mutex_lock(&lock_servsock);

	//send header
	char *message = malloc(MAX_BUFF_SIZE);;
	strcpy(message, itoa(FILE_LIST_UPDATE));
	strcat(message, MESSAGE_DIVIDER);
	strcat(message, itoa(n_fs));
	for (int i = 0;i < n_fs; i++){
		//file_status file_name file size
		fprintf(stream, "Status: %u\n", fs[i].status);
		fprintf(stream, "filename: \'%s\'\n", fs[i].filename);
		fprintf(stream, "filesize: %u\n", fs[i].filesize);
		strcat(message, MESSAGE_DIVIDER);
		strcat(message, itoa(fs[i].status));
		strcat(message, MESSAGE_DIVIDER);
		strcat(message, fs[i].filename);
		strcat(message, MESSAGE_DIVIDER);
		strcat(message, itoa(fs[i].filesize));
	}
	writeBytes(servsock, message, MAX_BUFF_SIZE);
	pthread_mutex_unlock(&lock_servsock);
	return;
}

void delete_from_server(void *arg){
	char *filename = (char*) arg;
	struct FileStatus fs[20];
	int n_fs = 0;
	struct Node *it = monitor_files->head;
	for (;it != NULL; it = it->next){
		char *name = (char*)it->data;
		if(strcmp(name, filename) == 0){
			char notify[100] = {0};
			if(it->status == FILE_NEW){
				it->status = FILE_DELETED;
				strcpy(notify, "File '");
				strcat(notify, filename);
				strcat(notify, "' has been deleted from server!");
				mvwaddstr(win, 6, 4, notify);
				wrefresh(win);
			} else {
				strcpy(notify, "You haven't shared this file!");
				mvwaddstr(win, 6, 4, notify);
			}
		}
		uint32_t sz = getFileSize(dir_name, name);
		fs[n_fs].filesize = sz;
		fs[n_fs].status = it->status;
		strcpy(fs[n_fs].filename, name);
		n_fs ++;	
	}
	lockNode(monitor_files, filename);
	send_file_list(servsock, fs, n_fs);
	return;
}

void share_file(void *arg){
	char *filename = (char*) arg;	
	char notify[100] = {0};
	struct Node *temp = getNode(monitor_files, filename);
	if (temp->status == FILE_NEW){
		strcpy(notify, "You're already shared this file");
		mvwaddstr(win, 4, 4, notify);
		wrefresh(win);
		return;
	} else {
		temp->status = FILE_NEW;
	}
	struct FileStatus fs[20];
	int n_fs = 0;
	struct Node *it = monitor_files->head;
	for (;it != NULL; it = it->next){
		float f_percent = (float)(n_fs+1) * 100.0 / (float)(monitor_files->n_nodes);
		int percentage = (int)f_percent;
		char show_percent[4] = {0};
		strcpy(show_percent, itoa(percentage));
		strcat(show_percent, "%");
		mvwaddstr(win, 4, 4, show_percent);
		mvwaddstr(win, 4, 9, "[");
		mvwaddstr(win, 4, 9 + win->_maxx - 13, "]");
		mvwhline(win, 4, 9 + 1, ACS_CKBOARD, (win->_maxx - 14) * percentage / 100);
		wrefresh(win);
		char *name = (char*)it->data;
		uint32_t sz = getFileSize(dir_name, (char*)name);
		fs[n_fs].filesize = sz;
		fs[n_fs].status = it->status;
		strcpy(fs[n_fs].filename, name);
		n_fs ++;	
	}
	send_file_list(servsock, fs, n_fs);
	strcpy(notify, "File '");
	strcat(notify, filename);
	strcat(notify, "' has been shared!");
	mvwaddstr(win, 6, 4, notify);
}

void* update_file_list(void *arg){
	strcpy(dir_name, (char*)arg);
	//data_port_announcement must be sent first
	announceDataPort(servsock);
	/* list files in a directory */
	struct FileStatus fs[20];
	uint8_t n_fs = 0;
	DIR *dir;
	struct dirent *ent;
	dir = opendir(dir_name);
	if (monitor_files == NULL){
		monitor_files = newLinkedList();
	}
	if (dir){
		while ((ent = readdir(dir)) != NULL){
			if (ent->d_name[0] == '.') continue;			
			uint32_t sz = getFileSize(dir_name, ent->d_name);
			if (sz < 0) continue;
			struct Node *file_node = getNode(monitor_files, ent->d_name);
			if (!file_node) {
				file_node = newNode(ent->d_name, STRING_TYPE);
				push(monitor_files, file_node);
			}
			strcpy(fs[n_fs].filename, ent->d_name);
			fs[n_fs].filesize = sz;
			fs[n_fs].status = file_node->status;
			n_fs ++;
		}
		closedir(dir);
		if (n_fs > 0) send_file_list(servsock, fs, n_fs);
		else fprintf(stream, "Directory %s is empty\n", dir_name);
	} else exit(1);
	monitor_directory(dir_name, servsock);
	return NULL;
}

void monitor_directory(char *dir, int socketfd){
	fprintf(stream, "Exec monitor_directory\n");
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
		exit(1);
	}
	watch = inotify_add_watch(inotifyfd, dir, IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_CLOSE_WRITE | IN_MOVED_FROM);
	if (watch < 0){
		exit(1);
	}
	while (1){
		struct FileStatus fs[20];
		uint8_t n_fs = 0;
		length = read(inotifyfd, buffer, BUF_LEN);
		if (length < 0){
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
						fs[n_fs].status = FILE_LOCK;
						strcpy(fs[n_fs].filename, event->name);
						uint32_t sz = getFileSize(dir, event->name);
						if (sz < 0) continue;
						fs[n_fs].filesize = sz;
						n_fs ++;
					} else if (event->mask & IN_CLOSE_WRITE){
						struct Node *file_node = getNode(monitor_files, event->name);
						if (file_node){
							fprintf(stream, "The file %s was created.\n", event->name);
							if (file_node->status == FILE_LOCK) fs[n_fs].status = FILE_LOCK;
							else if (file_node->status == FILE_NEW) fs[n_fs].status = FILE_NEW;
							strcpy(fs[n_fs].filename, event->name);
							uint32_t sz = getFileSize(dir, event->name);
							if (sz < 0) continue;
							fs[n_fs].filesize = sz;
							n_fs ++;
						} 
					} else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM){
						fprintf(stream, "The file %s was deleted.\n", event->name);
						fs[n_fs].status = FILE_DELETED;
						strcpy(fs[n_fs].filename, event->name);
						fs[n_fs].filesize = 0;
						n_fs ++;
						popNode(monitor_files, event->name);
					}
				}
			}
			i += EVENT_SIZE + event->len;
		}
		send_file_list(socketfd, fs, n_fs);
	}
}