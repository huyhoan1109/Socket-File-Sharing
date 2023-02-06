#include "LinkedList.h"

struct Node* newNode(void *data, int data_type){
	struct Node* newN = calloc(1, sizeof(struct Node));
	newN->status = INIT_STATE;
	switch (data_type){
		case SEGMENT_TYPE:
		{
			struct Segment *tmp = calloc(1, sizeof(struct Segment));
			*tmp = *((struct Segment*)data);
			newN->data = tmp;
			newN->type = SEGMENT_TYPE;
			break;
		}
		case INT_TYPE:
		{
			int *tmp = calloc(1, sizeof(int));
			*tmp = *((int*) data);
			newN->data = tmp;
			newN->type = INT_TYPE;
			break;
		}
		case FILE_OWNER_TYPE:
		{
			struct FileOwner *tmp = calloc(1, sizeof(struct FileOwner));
			*tmp = *((struct FileOwner*)data);
			newN->data = tmp;
			newN->type = FILE_OWNER_TYPE;
			break;
		}
		case DATA_HOST_TYPE:
		{
			struct DataHost *tmp = calloc(1, sizeof(struct DataHost));
			*tmp = *((struct DataHost*)data);
			newN->data = tmp;
			newN->type = DATA_HOST_TYPE;
			break;
		}
		case PTHREAD_T_TYPE:
		{
			pthread_t *tmp = calloc(1, sizeof(pthread_t));
			*tmp = *((pthread_t*)data);
			newN->data = tmp;
			newN->type = PTHREAD_T_TYPE;
			break;
		}
		case STRING_TYPE:
		{
			char *input = (char*)data;
			char *tmp = calloc(strlen(input) + 1, sizeof(char));
			strcpy(tmp, input);
			newN->data = tmp;
			newN->type = STRING_TYPE;
			break;
		}
		default:
			free(newN);
			newN = NULL;
			break;
	}
	return newN;
}

struct LinkedList* newLinkedList(){
	struct LinkedList *ll = calloc(1, sizeof(struct LinkedList));
	ll->head = NULL;
	ll->tail = NULL;
	ll->n_nodes = 0;
	return ll;
}

struct LinkedList* copyLinkedList(struct LinkedList *srcll){
	if (srcll == NULL)
		return NULL;

	struct LinkedList *dstll = newLinkedList();
	struct Node *it = srcll->head;
	for (; it != NULL; it = it->next){
		/* TODO: different route for FILE_OWNER_TYPE due to its LinkedList */
		struct Node *new_node = newNode(it->data, it->type);
		push(dstll, new_node);
	}
	return dstll;
}

void freeNode(struct Node *node){
	if (!node)
		return;
	switch(node->type){
		case FILE_OWNER_TYPE:
		{
			struct FileOwner *file = (struct FileOwner*)(node->data);
			destructLinkedList(file->host_list);
			break;
		}
	}
	free(node->data);
	free(node);
}

void destructLinkedList(void *arg){
	struct LinkedList *ll = (struct LinkedList*)arg;
	if (ll->n_nodes == 0){
		free(ll);
		return;
	}

	struct Node *it = ll->head;

	while (it != NULL){
		struct Node *tmp = it->next;
		freeNode(it);
		it = tmp;
	}

	free(ll);
}

int llistContain(struct LinkedList ll, struct Node *node){
	if (ll.n_nodes == 0){
		return 0;
	}
	struct Node *it = ll.head;
	for ( ; it != NULL; it = it->next){
		if (it == node)
			return 1;
	}
	return 0;
}

int insertNode(struct LinkedList *ll, struct Node *node, struct Node *offset){
	if (!ll || !node) return INSERT_NODE_ARGS_NULL;
	
	if (ll->n_nodes == 0){
		ll->n_nodes ++;
		ll->head = node;
		ll->head->prev = NULL;
		ll->head->next = NULL;
		ll->tail = node;
		return INSERT_NODE_SUCCESS;
	}

	if (!llistContain(*ll, offset) && offset != NULL) return INSERT_NODE_NOT_IN_LL;

	if (offset == NULL){
		//insert into the head
		struct Node *tmp = ll->head;
		ll->head = node;
		ll->head->next = tmp;
		ll->head->next->prev = ll->head;
	} else {
		struct Node *tmp = offset->next;
		offset->next = node;
		node->next = tmp;
		node->prev = offset;

		if (offset == ll->tail){
			ll->tail = node;
		} else {
			node->next->prev = node;
		}
	}
	ll->n_nodes += 1;
	return INSERT_NODE_SUCCESS;
}

int removeNode(struct LinkedList *ll, struct Node *node){
	if (!ll || !node) return REMOVE_NODE_ARGS_NULL;

	if (ll->n_nodes <= 0) return REMOVE_NODE_EMPTY_LL;

	struct Node *prev = node->prev;
	struct Node *next = node->next;

	if (ll->n_nodes == 1){
		ll->head = NULL;
		ll->tail = NULL;
	} else if (ll->head == node){
		//remove the head
		ll->head = node->next;
		ll->head->prev = NULL;
	} else if (ll->tail == node){
		//remove the tail
		ll->tail = node->prev;
		ll->tail->next = NULL;
	} else {
		prev->next = next;
		next->prev = prev;
	}

	//printf("free node->data\n");
	switch (node->type){
		case FILE_OWNER_TYPE:
		{
			struct FileOwner *file = (struct FileOwner*)(node->data);
			destructLinkedList(file->host_list);
			break;
		}
		default:
		{
			free(node->data);
			break;
		}
	}

	free(node);
	ll->n_nodes --;

	return REMOVE_NODE_SUCCESS;
}

void push(struct LinkedList *ll, struct Node *node){
	insertNode(ll, node, ll->tail);
}

struct Node* pop(struct LinkedList *ll){
	if (ll->n_nodes == 0)
		return NULL;
	struct Node *res = ll->head;
	removeNode(ll, ll->head);
	return res;
}
