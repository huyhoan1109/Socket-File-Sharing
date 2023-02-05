#ifndef _CONNECT_INDEX_SERVER_
#define _CONNECT_INDEX_SERVER_

#include <pthread.h>
#include <ncurses.h>

extern int servsock;
extern WINDOW *win;
extern pthread_mutex_t lock_servsock;		//mutex lock for the socket that faces the index server
extern struct LinkedList *monitorFiles;
extern char dirName[30];

void drawProgress(WINDOW *win, int y, int x, uint64_t current, uint64_t max);
void connect_to_index_server(char *servip, uint16_t index_port);
void* process_response(void *arg);

struct LinkedList *refreshListFile(struct LinkedList *ll, char *dir_name);
struct Node* getFilesNode(struct LinkedList *ll, char *filename);
void popFilesNode(struct LinkedList *ll, char *filename);
void lockFilesNode(struct LinkedList *ll, char *filename);

#endif