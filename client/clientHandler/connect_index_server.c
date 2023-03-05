#include "update_file_list.h"
#include "list_files_request.h"
#include "list_hosts_request.h"
#include "connect_index_server.h"

WINDOW *win;
int servsock = 0;
char dirName[30];
int recv_host = 0;
struct LinkedList *monitorFiles;
pthread_mutex_t lock_key = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_servsock = PTHREAD_MUTEX_INITIALIZER;

void connect_to_index_server(char *servip, uint16_t index_port, char *storage_dir){
	/* connect to index server */
	struct sockaddr_in servsin;
	bzero(&servsin, sizeof(servsin));

	servsin.sin_family = AF_INET;

	/* parse the buf to get ip address and port */
	servsin.sin_addr.s_addr = inet_addr(servip);
	servsin.sin_port = htons(index_port);

	int servsocket = socket(AF_INET, SOCK_STREAM, 0);
	if (servsocket < 0) {
		printf("Server error\n");
		exit(1);
	}
	if (connect(servsocket, (struct sockaddr*) &servsin, sizeof(servsin)) < 0) {
		printf("Can't connect to server\n");
		exit(1);
	}
	servsock = servsocket;
	strcpy(dirName, storage_dir);
}

void* process_response(void *arg){
	pthread_detach(pthread_self());
	
	int servsock = *(int*)arg;
	char *message = calloc(MAX_BUFF_SIZE, sizeof(char)); 
	uint8_t packet_type;
	
	while (readBytes(servsock, message, MAX_BUFF_SIZE) > 0){	
		packet_type = protocolType(message);
		switch (packet_type)
		{
		case LIST_FILES_RESPONSE:
			process_list_files_response(message);
			break;
		case LIST_HOSTS_RESPONSE:
			process_list_hosts_response(message);
			break;
		}
		memset(message, 0, BUFSIZ);
	}
	free(message);
	wclear(win);
	echo(); 
	endwin();
	printf("Server has shut down!\n");
	exit(1);
}

void drawProgress(WINDOW *win, int y, int x, uint64_t current, uint64_t max){
	float f_percent = (float)(current) * 100.0 / (float)(max);
	int percentage = (int)f_percent;
	char show_percent[4] = {0};
	strcpy(show_percent, itoa(percentage));
	strcat(show_percent, "%");
	mvwaddstr(win, y, x, show_percent);
	mvwaddstr(win, y, x + 5, "[");
	mvwaddstr(win, y, x + win->_maxx - 8, "]");
	mvwhline(win, y, x+6, ACS_CKBOARD, (win->_maxx - 14) * percentage / 100);
	wrefresh(win);
}

struct Node* getFilesNode(struct LinkedList *ll, char *filename){
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

void popFilesNode(struct LinkedList *ll, char *filename){
	struct Node *rm_node = getFilesNode(ll, filename);
	if (rm_node) removeNode(ll, rm_node); 
}

void lockFilesNode(struct LinkedList *ll, char *filename){
	struct Node *locked_node = getFilesNode(ll, filename);
	locked_node->status = FILE_LOCK;
}

struct LinkedList *refreshListFile(struct LinkedList *ll, char *dir_name){
	DIR *dir;
	struct dirent *ent;
	dir = opendir(dir_name);
	if (ll == NULL){
		ll = newLinkedList();
	}
	if (dir){
		while ((ent = readdir(dir)) != NULL){
			if (ent->d_name[0] == '.') continue;			
			uint32_t sz = getFileSize(dir_name, ent->d_name);
			if (sz < 0) continue;
			struct Node *file_node = getFilesNode(ll, ent->d_name);
			if (!file_node) {
				file_node = newNode(ent->d_name, STRING_TYPE);
				push(ll, file_node);
			}
		}
		closedir(dir);
	} else exit(1);
	return ll;
}