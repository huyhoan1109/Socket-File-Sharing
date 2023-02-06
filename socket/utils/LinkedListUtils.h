#ifndef _LINKED_LIST_UTILS_H_
#define _LINKED_LIST_UTILS_H_

#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include "LinkedList.h"

struct Node* llContainFile(struct LinkedList *ll, char *filename);

struct Node* llContainHost(struct LinkedList *ll, struct DataHost host);

struct Node* getNodeByFilename(struct LinkedList *ll, char *filename);

struct Node* getNodeByHost(struct LinkedList *ll, struct DataHost host);

#endif

