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

char *multicopy(int time, char* str){
    char *after = malloc(sizeof(str) * time);
    int i = 0;
    while(i < time){
        char *now = after + i * strlen(str);
        strcpy(now, str);
        i += 1;
        now = now + 1;
    }
    return after;
}

int prettyprint(char *x, int dist, void *col){
    col = (char*) col;
    char *space1, *space2;
    int padding, sp1, sp2;
    if(col == NULL){
        space1 = multicopy((int)( ( dist - strlen(x) ) / 2 + strlen(x) ), " ");
        space2 = multicopy((int)( ( dist - strlen(x) ) / 2 + strlen(x) ), " ");
        sp1 = strlen(space1);
        sp2 = strlen(space2);
        padding = strlen(x) + sp1 + sp2 + 4;;
    } else{
        int diff = strlen(col) - strlen(x);
        int col_sp = (int)(( dist - strlen(col) ) / 2) + strlen(col);
        int k = diff % 2;
        sp1 = col_sp + diff/2 + k;
        sp2 = col_sp + diff/2;
        space1 = multicopy(sp1, " ");
        space2 = multicopy(sp2, " ");
        padding = strlen(x) + sp1 + sp2;
    }
    print_center(x, space1, space2);
    return padding;
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

char *itoa(int n){
    int i, sign;
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