#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#include "handle_download_file_request.h"
#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"

static void close_connection(struct NetInfo cli_info){
	char cli_addr[22];
	sprintf(cli_addr, "%s:%u", cli_info.ip_add, cli_info.port);
//	printf("Closing connection from %s\n", cli_addr);
	close(cli_info.sockfd);
//	printf("Connection from %s closed\n", cli_addr);
}

static void* handleDownloadFileReq(void *arg){
	pthread_detach(pthread_self());
	struct NetInfo cli_info = *((struct NetInfo*) arg);
	free(arg);
	
	char err_mess[256];
	char cli_addr[22];

	sprintf(cli_addr, "%s:%u", cli_info.ip_add, cli_info.port);
	
	uint16_t filename_length;
	char filename[200];
	uint32_t offset = 0;

	int n_bytes = readBytes(cli_info.sockfd, &filename_length, sizeof(filename_length));
	if (n_bytes <= 0){
		sprintf(err_mess, "%s > Read filename_length", cli_addr);
		print_error(err_mess);
		close_connection(cli_info);
		return NULL;
	}
	filename_length = ntohs(filename_length);
	fprintf(stream, "%s:%u > filename_length: %u\n", cli_info.ip_add, cli_info.port, filename_length);

	n_bytes = readBytes(cli_info.sockfd, &filename, filename_length);
	if (n_bytes <= 0){
		sprintf(err_mess, "%s > Read filename", cli_addr);
		print_error(err_mess);
		close_connection(cli_info);
		return NULL;
	}
	
	n_bytes = readBytes(cli_info.sockfd, &offset, sizeof(offset));
	if (n_bytes <=0){
		sprintf(err_mess, "%s > Read offset", cli_addr);
		print_error(err_mess);
		close_connection(cli_info);
		return NULL;
	}
	offset = ntohl(offset);

//	fprintf(stdout, "%s > Required file \'%s\', offset=%u\n", cli_addr, filename, offset);
	char *path = calloc(100, sizeof(char));
	strcpy(path, STORAGE);
	strcat(path, "/");
	strcat(path, filename);
	FILE *file = fopen(path, "rb");
	free(path);
	if (file == NULL){
		sprintf(err_mess, "%s > Open \'%s\'", cli_addr, filename);
		int errnum = print_error(err_mess);
		uint8_t state;
		if (errnum == ENOENT){
			state = FILE_NOT_FOUND;
		} else {
			state = OPENING_FILE_ERROR;
		}
		n_bytes = writeBytes(cli_info.sockfd, (void*)&state, sizeof(state));
		if (n_bytes <= 0){
			sprintf(err_mess, "%s > Send file_error message", cli_addr);
			print_error(err_mess);
			close_connection(cli_info);
			return NULL;
		}
	}
	
	n_bytes = writeBytes(cli_info.sockfd, (void*)&READY_TO_SEND_DATA, sizeof(READY_TO_SEND_DATA));
	if (n_bytes <= 0){
		sprintf(err_mess, "%s > Send READY_TO_SEND_DATA message", cli_addr);
		print_error(err_mess);
		close_connection(cli_info);
		return NULL;
	}
	
	fseeko(file, offset, SEEK_SET);
	
	char buff[MAX_BUFF_SIZE];
	int buf_len = 0;
	int done = 0;

	while (1){
		buf_len = fread(buff, 1, MAX_BUFF_SIZE, file);
		/* error when reading file or EOF */
		if (buf_len < MAX_BUFF_SIZE){
			if (feof(file)){
				done = 1;
			} else {
				sprintf(err_mess, "%s > Read \'%s\'", cli_addr, filename);
				print_error(err_mess);
			}
			n_bytes = writeBytes(cli_info.sockfd, buff, buf_len);
			break;
		}
		n_bytes = writeBytes(cli_info.sockfd, buff, buf_len);
		if (n_bytes <= 0)
			break;
	}
	fclose(file);
	if (n_bytes <= 0){
		sprintf(err_mess, "%s > Send content of \'%s\'", cli_addr, filename);
		print_error(err_mess);
		close_connection(cli_info);
		return NULL;
	} else {
		if (done){
			fprintf(stream, "%s > Sending \'%s\' done\n", cli_addr, filename);
		}
	}
	close_connection(cli_info);
	return NULL;
}

void* waitForDownloadRequest(void *arg){
	pthread_detach(pthread_self());

	int sockfd = *(int*)arg;
	struct NetInfo *cli_info = malloc(sizeof(struct NetInfo));
	struct sockaddr_in clisin;
	unsigned int sin_len = sizeof(clisin);
	bzero(&clisin, sizeof(clisin));
	while (1){
		cli_info->sockfd = accept(sockfd, (struct sockaddr*) &clisin, &sin_len);
		if (cli_info->sockfd < 0){
			print_error("Accept connection");
			continue;
		}

		inet_ntop(AF_INET, &(clisin.sin_addr), cli_info->ip_add, sizeof(cli_info->ip_add));
		cli_info->port = ntohs(clisin.sin_port);

		//fprintf(stderr, "Connection from %s:%u\n", cli_info->ip_add, cli_info->port);

		/* create a new thread for each client */
		pthread_t tid;
		int thr = pthread_create(&tid, NULL, &handleDownloadFileReq, (void*)cli_info);
		if (thr != 0){
			print_error("Create new thread to handle download_file_request");
			close(cli_info->sockfd);
			continue;
		}
		cli_info = malloc(sizeof(struct NetInfo));
	}
	close(sockfd);
}