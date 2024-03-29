#ifndef _DATA_STRUCTURE_
#define _DATA_STRUCTURE_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>

// message body {message type, message content}
#define INSERT_NODE_SUCCESS 1
#define INSERT_NODE_NOT_IN_LL -2 
#define INSERT_NODE_ARGS_NULL -1

#define REMOVE_NODE_ARGS_NULL -1
#define REMOVE_NODE_EMPTY_LL -2
#define REMOVE_NODE_SUCCESS 1

#define INIT_STATE 0

#define SEGMENT_TYPE	0
#define INT_TYPE		1 
#define FILE_OWNER_TYPE 2
#define DATA_HOST_TYPE 3
#define PTHREAD_T_TYPE 4
#define STRING_TYPE 5

struct FileOwner{
	char filename[200];
	struct LinkedList *host_list;	// LinkedList with data type DataHost
};


struct Segment{
	uint32_t offset;			//position in the file
	uint32_t n_bytes;			//number of bytes received
	uint32_t seg_size;			//this segment size (number of bytes need to received)
	pthread_mutex_t lock_seg;
	int downloading;			//is this segment being downloaded by a thread?
};


/* info for each peer that own the data */
struct DataHost{
	uint8_t status;
	uint16_t port;
	uint32_t ip_addr;
	uint32_t filesize;
};

struct Node {
	void *data;
	int status;
	uint8_t type;			//data type
	struct Node *next;
	struct Node *prev;
};

struct LinkedList{
	struct Node *head;
	struct Node *tail;
	uint8_t n_nodes;	//number of nodes in the LL
};

struct Node* newNode(void *data, int data_type);

struct LinkedList* newLinkedList();

struct LinkedList* copyLinkedList(struct LinkedList *srcll);

void freeNode(struct Node *node);

void destructLinkedList(void *arg);

int llistContain(struct LinkedList ll, struct Node *node);

int insertNode(struct LinkedList *ll, struct Node *node, struct Node *offset);

int removeNode(struct LinkedList *ll, struct Node *node);

struct Node* pop(struct LinkedList *ll);

void push(struct LinkedList *ll, struct Node *node);

#endif