#include "LinkedListUtils.h"

struct Node* llContainFile(struct LinkedList *ll, char *filename){
    if (ll == NULL) return NULL;
	struct Node *it = ll->head;
	for (; it != NULL; it = it->next){
		struct FileOwner *file = (struct FileOwner*)it->data;
		if (! strcmp(file->filename, filename)){
			return it;
		}
	}
	return NULL;
}

struct Node* llContainHost(struct LinkedList *ll, struct DataHost host){
    if (ll == NULL) return NULL;
	struct Node *it = ll->head;
	for (; it != NULL; it = it->next){
		struct DataHost *data_host = (struct DataHost*)it->data;
		if (host.ip_addr == data_host->ip_addr && host.port == data_host->port){
			return it;
		}
	}
	return NULL;
}

struct Node* getNodeByFilename(struct LinkedList *ll, char *filename){
	return llContainFile(ll, filename);
}

struct Node* getNodeByHost(struct LinkedList *ll, struct DataHost host){
	return llContainHost(ll, host);
}
