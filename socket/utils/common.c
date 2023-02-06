#include "common.h"

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
    fclose(file);
	return sz;
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

char *addHeader(char *info, int header){
    char *message = calloc(MAX_BUFF_SIZE, sizeof(char));
    strcpy(message, itoa(header));
    if (info != NULL) {
        strcat(message, HEADER_DIVIDER);
        strcat(message, info);
        info = NULL;
    }
    return message;
}

char *appendInfo(char *current_info, char *new_info){
    if (current_info == NULL || strlen(current_info) == 0) {
        char *info = calloc(1+strlen(new_info), sizeof(char));
        strcpy(info, new_info);
        if (new_info != NULL) new_info = NULL;
        return info;
    } else {
        char *message = calloc(MAX_BUFF_SIZE, sizeof(char));
        strcpy(message, current_info);
        strcat(message, MESSAGE_DIVIDER);
        strcat(message, new_info);
        if (current_info != NULL) current_info = NULL;
        if (new_info != NULL) new_info = NULL;
        return message;
    }
}

char *getInfo(char *message){
    char *token;
    char *tmp = calloc(MAX_BUFF_SIZE, sizeof(char));
    char *info = calloc(MAX_BUFF_SIZE, sizeof(char));
    strcpy(tmp, message);
    token = strtok(tmp, HEADER_DIVIDER);
    strcpy(info, token + strlen(token) + strlen(HEADER_DIVIDER));
    free(tmp);
    return info;
}

char *nextInfo(char *info, int is_first){
    char *token;
    char *value = calloc(30, sizeof(char));
    if (is_first) token = strtok(info, MESSAGE_DIVIDER);
    else token = strtok(NULL, MESSAGE_DIVIDER);
    strcpy(value, token);
    return value;
}