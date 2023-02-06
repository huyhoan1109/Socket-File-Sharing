#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#define FILE_LOCK 0 // = INIT_STATE
#define FILE_NEW 1
#define FILE_DELETED 2

#define STORAGE "DB/"

#define HEADER_DIVIDER "[:]"
#define MESSAGE_DIVIDER "[&]"

#define IS_FIRST 1
#define IS_AFTER 0

#define PRESS_ENTER "Enter to select"
#define PRESS_ESC_BACK "Press esc to go back menu"

/* request types */

#define DATA_PORT_ANNOUNCEMENT 100
#define FILE_LIST_UPDATE 101
#define LIST_FILES_REQUEST 102
#define LIST_FILES_RESPONSE 103
#define LIST_HOSTS_REQUEST 104
#define LIST_HOSTS_RESPONSE 105
#define GET_FILE_REQUEST 106

#define READY_TO_SEND_DATA 200
#define FILE_NOT_FOUND 201
#define OPENING_FILE_ERROR 202
#define FILE_IS_BLOCK 203

#define MAX_BUFF_SIZE 8192
#define _FILE_OFFSET_BITS 64

char *itoa(uint32_t n);

void free_mem(void *arg);

void mutex_unlock(void *arg);

void cancel_thread(void *arg);

uint32_t getFileSize(char* dir, char *filename);

uint8_t protocolType(char *message);

char *addHeader(char *info, int header);

char *appendInfo(char *current_info, char *new_info);

char *getInfo(char *message);

char *nextInfo(char *info, int is_first);
#endif