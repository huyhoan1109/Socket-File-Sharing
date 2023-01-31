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
	int n_bytes = writeBytes(servsock, (void *)&LIST_FILES_REQUEST, sizeof(LIST_FILES_REQUEST));
	if (n_bytes <= 0)
	{
		print_error("Send LIST_FILES_REQUEST to index server");
		exit(1);
	}
	pthread_mutex_unlock(&lock_servsock);
}

void process_list_files_response()
{
	fprintf(stream, "Index server > list_files_response\n");
	long n_bytes = 0;
	uint8_t n_files = 0;
	// receive info from server
	n_bytes = readBytes(servsock, &n_files, sizeof(n_files));
	if (n_bytes <= 0)
	{
		print_error("Index server > read n_files");
		exit(1);
	}
	fprintf(stream, "Index server > n_files: %u\n", n_files);

	char delim[30];
	memset(delim, '-', 30);
	delim[29] = 0;
	uint8_t i = 0;
	if (n_files > 0){
		mvwaddstr(win, 2, 4, "No");
		mvwaddstr(win, 2, 10, "Filename");
	} else {
		mvwaddstr(win, 2, 4, "No files available");
	}
	for (; i < n_files; i++)
	{
		// filename length
		uint16_t filename_length;
		n_bytes = readBytes(servsock, &filename_length, sizeof(filename_length));
		if (n_bytes <= 0)
		{
			print_error("Index server > Read filename_length");
			exit(1);
		}
		filename_length = ntohs(filename_length);
		fprintf(stream, "Index server > filename_length: %u\n", filename_length);
		char filename[200];
		n_bytes = readBytes(servsock, filename, filename_length);
		if (n_bytes <= 0)
		{
			print_error("Index server > Read filename");
			exit(1);
		}
		fprintf(stream, "Index server > filename: %s\n", filename);
		mvwaddstr(win, 4 + (i * 2), 4, itoa(i+1));
		mvwaddstr(win, 4 + (i * 2), 10, filename);
		wrefresh(win);
	}
}