#ifndef _CONNECT_INDEX_SERVER_
#define _CONNECT_INDEX_SERVER_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <ncurses.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"

extern WINDOW *win;
extern int servsock;
extern int recv_host;
extern char dirName[30];
extern pthread_mutex_t lock_key;
extern pthread_mutex_t lock_servsock;		//mutex lock for the socket that faces the index server
extern pthread_cond_t recv_all_host;
extern struct LinkedList *monitorFiles;

void drawProgress(WINDOW *win, int y, int x, uint64_t current, uint64_t max);

void* process_response(void *arg);
void connect_to_index_server(char *servip, uint16_t index_port, char *storage_dir);

void popFilesNode(struct LinkedList *ll, char *filename);
void lockFilesNode(struct LinkedList *ll, char *filename);
struct Node* getFilesNode(struct LinkedList *ll, char *filename);
struct LinkedList *refreshListFile(struct LinkedList *ll, char *dir_name);

#endif