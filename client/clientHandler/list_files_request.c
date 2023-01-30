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
	// printf("%s\n", delim);
	// printf("%-4s | %-25s\n", "No", "Filename");
	uint8_t i = 0;
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
		mvwaddstr(win, 2 + (i * 2), 12, filename);
		mvwaddstr(win, 2 + (i * 2), 6, itoa(i+1));
		wrefresh(win);
		// printf("%-4u | %-25s\n", i + 1, filename);
	}
	// printf("%s\n", delim);
}