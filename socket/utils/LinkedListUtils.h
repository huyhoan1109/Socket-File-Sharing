#ifndef _LINKED_LIST_UTILS_H_
#define _LINKED_LIST_UTILS_H_

#include <netinet/in.h>
#include "LinkedList.h"

struct Node* llContainFile(const struct LinkedList *ll, char *filename);

struct Node* llContainHost(const struct LinkedList *ll, struct DataHost host);

struct Node* getNodeByFilename(const struct LinkedList *ll, char *filename);

struct Node* getNodeByHost(const struct LinkedList *ll, struct DataHost host);
#endif

