#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdint.h>
#include <stdio.h>

#define FILE_NEW 0
#define FILE_DELETED 1
#define PACKET_TYPE_SIZE 1

#define GREEN			"\x1b[32m"
#define YELLOW			"\x1b[33m"
#define BLUE			"\x1b[94m"
#define RED				"\x1b[91"
#define COLOR_RESET		"\x1b[0m"
#define GREEN			"\x1b[32m"
#define YELLOW			"\x1b[33m"
#define BLUE			"\x1b[94m"
#define RED				"\x1b[91"
#define STORAGE "DB"
#define print_center(x, space1, space2) printf("|%s %s %s|", space1, x, space2);

/* request types */
extern const uint8_t DATA_PORT_ANNOUNCEMENT;
extern const uint8_t FILE_LIST_UPDATE;
extern const uint8_t LIST_FILES_REQUEST;
extern const uint8_t LIST_FILES_RESPONSE;
extern const uint8_t LIST_HOSTS_REQUEST;
extern const uint8_t LIST_HOSTS_RESPONSE;

/* request status types */
extern const uint8_t READY_TO_SEND_DATA;
extern const uint8_t FILE_NOT_FOUND;
extern const uint8_t OPENING_FILE_ERROR;

extern FILE *stream;

uint32_t getFileSize(char* dir, char *filename);

int print_error(char *mess);

void free_mem(void *arg);

void mutex_unlock(void *arg);

void cancel_thread(void *arg);

char *multicopy(int time, char* str);

int prettyprint(char *x, int dist, void *col);

#endif