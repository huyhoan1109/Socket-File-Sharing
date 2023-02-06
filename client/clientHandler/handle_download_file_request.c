#define _FILE_OFFSET_BITS 64

#include "connect_index_server.h"
#include "handle_download_file_request.h"

static void* handleDownloadFileReq(void *arg){
	pthread_detach(pthread_self());
	
	struct NetInfo cli_info = *((struct NetInfo*) arg);
	free(arg);
	
	long n_bytes;
	uint32_t offset = 0;
	
	char *filename = calloc(100, sizeof(char));
	char *info = calloc(MAX_BUFF_SIZE, sizeof(char));
	char *message = calloc(MAX_BUFF_SIZE, sizeof(char));
	
	n_bytes = readBytes(cli_info.sockfd, message, MAX_BUFF_SIZE);
	if (n_bytes <= 0){
		close(cli_info.sockfd);
		return NULL;
	} 

	uint8_t packet_type = protocolType(message);
	info = getInfo(message);
	
	if (packet_type == GET_FILE_REQUEST){
		filename = nextInfo(info, IS_FIRST);
		offset = ntohl((uint32_t) atoll(nextInfo(info, IS_AFTER)));

		uint8_t state;
		char *path = calloc(100, sizeof(char));
		struct Node *file_info = getFilesNode(monitorFiles, filename);
		FILE *file;
		if (!file_info) state = FILE_NOT_FOUND;
		else {
			if (file_info->status == FILE_LOCK) state = FILE_IS_BLOCK;
			else {
				strcpy(path, dirName);
				strcat(path, filename);
				file = fopen(path, "rb");
				free(path);
				if (file == NULL) {
					fclose(file);
					state = OPENING_FILE_ERROR;
				} else state = READY_TO_SEND_DATA;
			}
		}
		char char_state[5];
		strcpy(char_state, itoa(state));
		n_bytes = write(cli_info.sockfd, char_state, sizeof(char_state));
		if (n_bytes <= 0){
			close(cli_info.sockfd);
			return NULL;
		}
		switch (state){
			case FILE_NOT_FOUND:
			case FILE_IS_BLOCK:
			case OPENING_FILE_ERROR:
				break;
			case READY_TO_SEND_DATA:
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
				break;
			default:
				break;
		}
	}
	free(filename);
	free(info);
	free(message);
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