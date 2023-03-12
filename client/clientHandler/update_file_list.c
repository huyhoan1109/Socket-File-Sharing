#include "update_file_list.h"
#include "connect_index_server.h"

uint16_t dataPort = 0;
pthread_mutex_t lock_notify = PTHREAD_MUTEX_INITIALIZER;

void announceDataPort(int sockfd){
	/* mutex lock the socket to avoid intersection of multiple messages 
	 * from different types */
	pthread_mutex_lock(&lock_servsock);

	char *info = calloc(MAX_BUFF_SIZE, sizeof(char));

	info = appendInfo(info, itoa(dataPort));

	char *message = addHeader(info, DATA_PORT_ANNOUNCEMENT);

	writeBytes(sockfd, message, MAX_BUFF_SIZE);
	free(message);
	free(info);	
	pthread_mutex_unlock(&lock_servsock);
}

static void send_file_list(int sockfd, struct FileStatus *fs, uint8_t n_fs){
	/* mutex lock the socket to avoid intersection of multiple messages from different types */
	pthread_mutex_lock(&lock_servsock);

	char *info = calloc(MAX_BUFF_SIZE, sizeof(char));
	info = appendInfo(NULL, itoa(n_fs));
	for (int i = 0;i < n_fs; i++){
		//file_status file_name file size
		info = appendInfo(info, itoa(fs[i].status));
		info = appendInfo(info, fs[i].filename);
		info = appendInfo(info, itoa(fs[i].filesize));
	}
	char *message = addHeader(info, FILE_LIST_UPDATE);
	writeBytes(servsock, message, MAX_BUFF_SIZE);
	free(message);
	free(info);
	
	pthread_mutex_unlock(&lock_servsock);
	return;
}

void delete_from_server(void *arg){
	char *filename = (char*) arg;
	struct FileStatus fs[20];
	int n_fs = 0;
	monitorFiles = refreshListFile(monitorFiles, dirName);	
	if(monitorFiles->n_nodes == 0) {
		mvwaddstr(win, 6, 4,"You don't have any file");
		wrefresh(win);
		return;
	}
	struct Node *it = monitorFiles->head;
	for (;it != NULL; it = it->next){
		char *name = (char*)it->data;
		if(strcmp(name, filename) == 0){
			if(it->status == FILE_AVAIL){
				it->status = FILE_DELETED;
				mvwprintw(win, 6, 4, "File '%s' has been deleted from server!", filename);
			} 
			if (it->status == FILE_LOCK){
				mvwprintw(win, 6, 4, "You haven't shared this file!", filename);
				continue;
			}
			wrefresh(win);
		}
		if (it->status != FILE_LOCK){
			uint32_t sz = getFileSize(dirName, name);
			fs[n_fs].filesize = sz;
			fs[n_fs].status = it->status;
			strcpy(fs[n_fs].filename, name);
			n_fs ++;	
		}
	}
	lockFilesNode(monitorFiles, filename);
	send_file_list(servsock, fs, n_fs);
	return;
}

void share_file(void *arg){
	char *filename = (char*) arg;
	monitorFiles = refreshListFile(monitorFiles, dirName);	
	struct Node *temp = getFilesNode(monitorFiles, filename);
	if (temp == NULL) {
		mvwaddstr(win, 4, 4,"You don't have any file");
		wrefresh(win);
		return;
	}
	if (temp->status == FILE_AVAIL){
		mvwaddstr(win, 4, 4,"You're already shared this file");
		wrefresh(win);
		return;
	} else {
		temp->status = FILE_AVAIL;
	}
	struct FileStatus fs[20];
	int n_fs = 0, counter = 0;
	struct Node *it = monitorFiles->head;
	for (;it != NULL; it = it->next){
		drawProgress(win, 4, 4, counter+1, monitorFiles->n_nodes);
		if (it->status != FILE_LOCK){
			char *name = (char*)it->data;
			uint32_t sz = getFileSize(dirName, (char*)name);
			fs[n_fs].filesize = sz;
			fs[n_fs].status = it->status;
			strcpy(fs[n_fs].filename, name);
			n_fs ++;	
		}
		counter ++;
	}
	send_file_list(servsock, fs, n_fs);
	mvwprintw(win, 6, 4, "File '%s' has been shared!", filename);
}

void* init_file_list(void *arg){
	//data_port_announcement must be sent first
	announceDataPort(servsock);

	/* list files in a directory */
	monitorFiles = refreshListFile(NULL, dirName);
	int counter = 0;
	if (monitorFiles->n_nodes > 0){
		struct FileStatus fs[20];
		uint8_t n_fs = 0;
		struct Node *it;
		for ( ;it != NULL; it = it->next) {
			if(it->status != FILE_LOCK){
				strcpy(fs[n_fs].filename, it->data);
				fs[n_fs].filesize = getFileSize(dirName, it->data);
				fs[n_fs].status = it->status;
				counter++;
			}
		}
		if (counter > 0) send_file_list(servsock, fs, n_fs);
	}
	monitor_directory(dirName, servsock);
	return NULL;
}

void monitor_directory(char *dir, int socketfd){
	/* monitoring continously */
	if (monitorFiles == NULL){
		monitorFiles = newLinkedList();
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
					if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO){
						struct Node *file_node = newNode(event->name, STRING_TYPE);
						push(monitorFiles, file_node);
					} else if (event->mask & IN_CLOSE_WRITE){
						struct Node *file_node = getFilesNode(monitorFiles, event->name);
						if (file_node){
							if (file_node->status == FILE_AVAIL) {
								fs[n_fs].status = FILE_AVAIL;
								strcpy(fs[n_fs].filename, event->name);
								fs[n_fs].filesize = getFileSize(dir, event->name);
								n_fs ++;
							}
						}
					} else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM){
						fs[n_fs].status = FILE_DELETED;
						strcpy(fs[n_fs].filename, event->name);
						fs[n_fs].filesize = 0;
						n_fs ++;
						popFilesNode(monitorFiles, event->name);
					}
				}
			}
			i += EVENT_SIZE + event->len;
		}
		send_file_list(socketfd, fs, n_fs);
	}
}