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

static void* handleDownloadFileReq(void *arg){
	pthread_detach(pthread_self());
	struct NetInfo cli_info = *((struct NetInfo*) arg);
	free(arg);
	long n_bytes;
	char err_mess[256];	
	char filename[200];
	uint32_t offset = 0;
	char *token;
	char *subtext = calloc(MAX_BUFF_SIZE, sizeof(char));
	char *recv_msg = calloc(MAX_BUFF_SIZE, sizeof(char));
	n_bytes = readBytes(cli_info.sockfd, recv_msg, MAX_BUFF_SIZE);
	if (n_bytes <= 0){
		close(cli_info.sockfd);
		return NULL;
	} 
	strcpy(subtext, recv_msg);
	token = strtok(subtext, MESSAGE_DIVIDER);
	uint8_t packet_type = (uint8_t) atoi(token);
	
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
		char char_state[5];
		uint8_t state;

		if (file == NULL){
			sprintf(err_mess, "%s > Open \'%s\'", cli_addr, filename);
			int errnum = print_error(err_mess);
			if (errnum == ENOENT){
				state = FILE_NOT_FOUND;
			} else {
				state = OPENING_FILE_ERROR;
			}
			strcpy(char_state, itoa(state));
			n_bytes = write(cli_info.sockfd, char_state, sizeof(char_state));
			if (n_bytes <= 0){
				close(cli_info.sockfd);
				return NULL;
			}
		} else {
			strcpy(char_state, itoa(READY_TO_SEND_DATA));
			n_bytes = write(cli_info.sockfd, char_state, sizeof(char_state));
			if (n_bytes <= 0){
				close(cli_info.sockfd);
				return NULL;
			}
			fseeko(file, offset, SEEK_SET);
			char buff[MAX_BUFF_SIZE];
			int buf_len = 0;
			while (1){
				buf_len = fread(buff, 1, MAX_BUFF_SIZE, file);
				/* error when reading file or EOF */
				n_bytes = write(cli_info.sockfd, buff, buf_len);
				if (buf_len < MAX_BUFF_SIZE || n_bytes <= 0){
					break;
				}
			}
			fclose(file);
		}
	}
	close(cli_info.sockfd);
	return NULL;
}

void* waitForDownloadRequest(void *arg){
	pthread_detach(pthread_self());

	int sockfd = *(int*)arg;
	struct NetInfo *cli_info = calloc(1, sizeof(struct NetInfo));
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
		cli_info = calloc(1, sizeof(struct NetInfo));
	}
	close(sockfd);
}