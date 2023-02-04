#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "common.h"

#define MAX_BUFF_SIZE 8192
#define _FILE_OFFSET_BITS 64

uint8_t protocolType(char *message){
    char *subtext = calloc(MAX_BUFF_SIZE, sizeof(char));
    int code;
    strcpy(subtext, message);
    code = (uint8_t) atoi(strtok(subtext, MESSAGE_DIVIDER));
    free(subtext);
    return code;
}

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
		fprintf(stderr, "Open file %s: %s\n", filename, err_mess);
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
	if (arg) free(arg);
}

void mutex_unlock(void *arg){
	pthread_mutex_t *lock = (pthread_mutex_t*)arg;
	pthread_mutex_unlock(lock);
}

void cancel_thread(void *arg){
	pthread_t tid = *(pthread_t*)arg;
	pthread_cancel(tid);
}

void *reverse(char *s)
{
    int i, j;
    char c;
    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
    return s;
} 

char *itoa(uint32_t n){
    uint32_t i, sign;
    char *s = NULL;
    if ((sign = n) < 0)  /* record sign */
        n = -n;          /* make n positive */
    i = 0;
    do {       /* generate digits in reverse order */
        s = realloc(s, sizeof(char));
        s[i++] = n % 10 + '0';   /* get next digit */
    } while ((n /= 10) > 0);     /* delete it */
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    s = reverse(s);
    return s;
}