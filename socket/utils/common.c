#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "common.h"

#define _FILE_OFFSET_BITS 64

const uint8_t DATA_PORT_ANNOUNCEMENT = 0;
const uint8_t FILE_LIST_UPDATE = 1;
const uint8_t LIST_FILES_REQUEST = 2;
const uint8_t LIST_FILES_RESPONSE = 3;
const uint8_t LIST_HOSTS_REQUEST = 4;
const uint8_t LIST_HOSTS_RESPONSE = 5;

const uint8_t READY_TO_SEND_DATA = 0;
const uint8_t FILE_NOT_FOUND = 1;
const uint8_t OPENING_FILE_ERROR = 2;

FILE *stream = NULL;

uint32_t getFileSize(char* dir, char *filename){
	char *path = calloc(100, sizeof(char));
	strcpy(path, dir);
	strcat(path, "/");
	strcat(path, filename);
	FILE *file = fopen(path, "rb");
	free(path);
	char err_mess[256];
	if (!file){
		strerror_r(errno, err_mess, sizeof(err_mess));
		fprintf(stderr, "open file %s: %s\n", filename, err_mess);
		return -1;
	}
	fseeko(file, 0, SEEK_END);
	uint32_t sz = ftello(file);
	rewind(file);
	return sz;
}

int print_error(char *mess){
	char err_mess[256];
	int errnum = errno;
	strerror_r(errnum, err_mess, sizeof(err_mess));
	fprintf(stderr, "%s [ERROR]: %s\n", mess, err_mess);
	return errnum;
}

void free_mem(void *arg){
	if (arg)
		free(arg);
}

void mutex_unlock(void *arg){
	pthread_mutex_t *lock = (pthread_mutex_t*)arg;
	pthread_mutex_unlock(lock);
}

void cancel_thread(void *arg){
	pthread_t tid = *(pthread_t*)arg;
	pthread_cancel(tid);
}
