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
	close(cli_info.sockfd);
}

static void* handleDownloadFileReq(void *arg){
	// printf("A lot of information");
	pthread_detach(pthread_self());
	struct NetInfo cli_info = *((struct NetInfo*) arg);
	free(arg);
	long n_bytes;
	char err_mess[256];	
	char filename[200];
	uint32_t offset = 0;
	char *token;
	char *subtext = malloc(MAX_BUFF_SIZE);
	char *recv_msg = malloc(MAX_BUFF_SIZE);
	n_bytes = read(cli_info.sockfd, recv_msg, MAX_BUFF_SIZE);
	if (n_bytes <= 0){
		close_connection(cli_info);
		return NULL;
	} 
	strcpy(subtext, recv_msg);
	token = strtok(subtext, MESSAGE_DIVIDER);
	uint8_t packet_type = (uint8_t) atoi(token);
	// printf("%u\n", GET_FILE_REQUEST);
	if (packet_type == GET_FILE_REQUEST){
		token = strtok(NULL, MESSAGE_DIVIDER);
		strcpy(filename, token);
		token = strtok(NULL, MESSAGE_DIVIDER);
		offset = ntohl((uint32_t) atoll(token));

		char *path = calloc(100, sizeof(char));
		strcpy(path, STORAGE);
		strcat(path, "/");
		strcat(path, filename);
		FILE *file = fopen(path, "rb");
		free(path);
		char cli_addr[22];
		sprintf(cli_addr,"%s:%u", cli_info.ip_add, cli_info.port);
		if (file == NULL){
			sprintf(err_mess, "%s > Open \'%s\'", cli_addr, filename);
			int errnum = print_error(err_mess);
			uint8_t state;
			if (errnum == ENOENT){
				state = FILE_NOT_FOUND;
			} else {
				state = OPENING_FILE_ERROR;
			}
			printf("state: %d\n", state);
			n_bytes = writeBytes(cli_info.sockfd, (void*)&state, MAX_BUFF_SIZE);
			if (n_bytes <= 0){
				close_connection(cli_info);
				return NULL;
			}
			return NULL;
		}
		uint8_t ready = READY_TO_SEND_DATA;
		n_bytes = writeBytes(cli_info.sockfd, (void*)&ready, MAX_BUFF_SIZE);
		if (n_bytes <= 0){
			close_connection(cli_info);
			return NULL;
		}
		// printf("Ready\n");
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
				}
				n_bytes = writeBytes(cli_info.sockfd, buff, buf_len);
				break;
			}
			n_bytes = writeBytes(cli_info.sockfd, buff, buf_len);
			if (n_bytes <= 0) break;
		}
		// printf("End\n");
		fclose(file);
		if (n_bytes <= 0){
			sprintf(err_mess, "%s > Send content of \'%s\'", cli_addr, filename);
			close_connection(cli_info);
			return NULL;
		} else {
			if (done){
				fprintf(stream, "%s > Sending \'%s\' done\n", cli_addr, filename);
			}
		}
		close_connection(cli_info);
	}
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
			continue;
		}

		inet_ntop(AF_INET, &(clisin.sin_addr), cli_info->ip_add, sizeof(cli_info->ip_add));
		cli_info->port = ntohs(clisin.sin_port);

		/* create a new thread for each client */
		pthread_t tid;
		int thr = pthread_create(&tid, NULL, &handleDownloadFileReq, (void*)cli_info);
		if (thr != 0){
			close(cli_info->sockfd);
			continue;
		}
		cli_info = malloc(sizeof(struct NetInfo));
	}
	close(sockfd);
}