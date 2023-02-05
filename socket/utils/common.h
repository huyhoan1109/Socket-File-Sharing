#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdint.h>
#include <stdio.h>

#define FILE_LOCK 0 // = INIT_STATE
#define FILE_NEW 1
#define FILE_DELETED 2
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
#define STORAGE "DB/"
#define print_center(x, space1, space2) printf("|%s %s %s|", space1, x, space2);

#define MESSAGE_DIVIDER "[:]"
#define PRESS_ESC_BACK "Press esc to go back menu"
#define PRESS_ENTER "Enter to select"

// header
/* request types */
#define DATA_PORT_ANNOUNCEMENT 100
#define FILE_LIST_UPDATE 101
#define LIST_FILES_REQUEST 102
#define LIST_FILES_RESPONSE 103
#define LIST_HOSTS_REQUEST 104
#define LIST_HOSTS_RESPONSE 105
#define GET_FILE_REQUEST 106

#define READY_TO_SEND_DATA 0
#define FILE_NOT_FOUND 1
#define OPENING_FILE_ERROR 2
#define FILE_IS_BLOCK 3

uint8_t protocolType(char *message);

uint32_t getFileSize(char* dir, char *filename);

int print_error(char *mess);

void free_mem(void *arg);

void mutex_unlock(void *arg);

void cancel_thread(void *arg);

char *multicopy(int time, char* str);

int prettyprint(char *x, int dist, void *col);

char *itoa(uint32_t n);

#endif