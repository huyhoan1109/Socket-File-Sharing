#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "list_files_request.h"
#include "connect_index_server.h"

void send_list_files_request()
{
	pthread_mutex_lock(&lock_servsock);
	// send header to server
	int n_bytes = writeBytes(servsock, itoa(LIST_FILES_REQUEST), MAX_BUFF_SIZE);
	if (n_bytes <= 0){
		exit(1);
	}
	pthread_mutex_unlock(&lock_servsock);
}

void process_list_files_response(char *message)
{
	uint8_t n_files = 0;
	char *token;
	char *subtext = malloc(MAX_BUFF_SIZE);
	strcpy(subtext, message);
	token = strtok(subtext, MESSAGE_DIVIDER);
	token = strtok(NULL, MESSAGE_DIVIDER);
	n_files = atol(token);
	fprintf(stream, "Index server > n_files: %u\n", n_files);
	uint8_t i = 0;
	if (n_files == 0){
		mvwaddstr(win, 2, 4, "No files available");
		wrefresh(win);
		return;
	} 
	mvwaddstr(win, 2, 4, "No");
	mvwaddstr(win, 2, 10, "Filename");
	mvwaddstr(win, 2, 20, "Size");
	wrefresh(win);
	for (; i < n_files; i++)
	{
		token = strtok(NULL, MESSAGE_DIVIDER);	
		fprintf(stream, "Index server > filename: %s\n", token);
		mvwaddstr(win, 4 + (i * 2), 4, itoa(i+1));
		mvwaddstr(win, 4 + (i * 2), 10, token);
		token = strtok(NULL, MESSAGE_DIVIDER);
		mvwaddstr(win, 4 + (i * 2), 20, token);
		wrefresh(win);
	}
}