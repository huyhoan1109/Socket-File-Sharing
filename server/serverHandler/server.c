#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "handle_request.h"


void *serveClient(void *arg);
void close_socket(void *arg);

int main(int argc, char **argv){
	int port = atoi(argv[1]);
	if (argc != 2){
		printf("Usage: ./server <port>");
		exit(1);
	}
	struct stat filestat;
	char history[20] = "history.log";
	stat(history, &filestat);
	time_t curr = time(NULL);
	if(curr - filestat.st_mtim.tv_sec < 4000) stream = fopen(history, "a+");
	else stream = fopen(history, "w");

	int servsock = socket(AF_INET, SOCK_STREAM, 0);
	if (servsock < 0){
		perror("socket");
		return 1;
	}

	int on = 1;
	setsockopt(servsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	struct sockaddr_in servsin;
	bzero(&servsin, sizeof(servsin));
	servsin.sin_addr.s_addr = htonl(INADDR_ANY);
	servsin.sin_family = AF_INET;
	servsin.sin_port = htons(port);

	if ((bind(servsock, (struct sockaddr*) &servsin, sizeof(servsin))) < 0){
		perror("bind");
		return 1;
	}

	if ((listen(servsock, 10)) < 0){
		perror("listen");
		return 1;
	} else {
		printf("Listening to port %d\n", port);
	}

	struct net_info *cli_info = malloc(sizeof(struct net_info));
	cli_info->data_port = 0;
	struct sockaddr_in clisin;
	unsigned int sin_len = sizeof(clisin);
	bzero(&clisin, sizeof(clisin));

	/* ignore EPIPE to avoid crashing when writing to a closed socket */
	signal (SIGPIPE, SIG_IGN);

	/* serve multiple clients simultaneously */
	while (1){
		cli_info->sockfd = accept(servsock, (struct sockaddr*) &clisin, &sin_len);
		if (cli_info->sockfd < 0){
			perror("Accept connection");
			continue;
		}

		inet_ntop(AF_INET, &(clisin.sin_addr), cli_info->ip_add, sizeof(cli_info->ip_add));
		cli_info->port = ntohs(clisin.sin_port);

		/* create a new thread for each client */
		pthread_t tid;
		int thr = pthread_create(&tid, NULL, &serveClient, (void*)cli_info);
		if (thr != 0){
			char err_mess[255];
			strerror_r(errno, err_mess, sizeof(err_mess));
			fprintf(stream, "Create thread to handle %s:%u: %s\n", 
					cli_info->ip_add, cli_info->port, err_mess);
			close(cli_info->sockfd);
			continue;
		}
		cli_info = malloc(sizeof(struct net_info));
		cli_info->data_port = 0;
	}
	close(servsock);
}

void close_socket(void *arg){
	int *fd = (int*)arg;
	close(*fd);
}

void *serveClient(void *arg){
	pthread_detach(pthread_self());
	struct net_info cli_info = *((struct net_info*) arg);
	free(arg);

	cli_info.lock_sockfd = malloc(sizeof(pthread_mutex_t));
	pthread_cleanup_push(free_mem, cli_info.lock_sockfd);

	*(cli_info.lock_sockfd) = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	//pthread_cleanup_push(close_socket, (void*)&(cli_info.sockfd));
	char cli_addr[256];
	sprintf(cli_addr, "%s:%u", cli_info.ip_add, cli_info.port);

	printf("Connection from client: %s\n", cli_addr);
	
	uint8_t packet_type;
	struct thread_data thrdt;	//use for process_list_hosts_request
	thrdt.cli_info = cli_info;

	/* receive request from clients,
	 * then response accordingly */
	char *message = malloc(MAX_BUFF_SIZE);
	char *subtext = malloc(MAX_BUFF_SIZE);
	while(readBytes(cli_info.sockfd, message, MAX_BUFF_SIZE)>0){
		printf("%s\n", message);
		packet_type = protocolType(message);
		char *token;
		switch (packet_type){
			case DATA_PORT_ANNOUNCEMENT:
				strcpy(subtext, message);
				token = strtok(subtext, MESSAGE_DIVIDER);
				token = strtok(NULL, MESSAGE_DIVIDER);
				if (cli_info.data_port != 0){
					/* only accept DATA_PORT_ANNOUNCEMENT packet once */
					struct DataHost host;
					host.ip_addr = inet_addr(cli_info.ip_add);
					host.port = cli_info.data_port;
					removeHost(host);
					close(cli_info.sockfd);
					int ret = 100;
					pthread_exit(&ret);
				}
				uint16_t data_port = (uint16_t) atoll(token);
				cli_info.data_port = ntohs(data_port);
				break;
			case FILE_LIST_UPDATE:
				if (cli_info.data_port == 0){
					//DATA_PORT_ANNOUNCEMENT must be sent first
					close(cli_info.sockfd);
					int ret = 100;
					pthread_exit(&ret);
				}
				update_file_list(cli_info, message);
				break;
			case LIST_FILES_REQUEST:
				process_list_files_request(cli_info);
				break;
			case LIST_HOSTS_REQUEST:
				strcpy(subtext, message);
				token = strtok(subtext, MESSAGE_DIVIDER);
				token = strtok(NULL, MESSAGE_DIVIDER);
			
				thrdt.seq_no = (uint8_t)atoi(token);
				
				token = strtok(NULL, MESSAGE_DIVIDER);
				strcpy(thrdt.filename, token);

				pthread_t tid;
				fprintf(stream, "Create new thread to process list_hosts_request\n");
				int thr = pthread_create(&tid, NULL, &process_list_hosts_request, &thrdt);
				if (thr != 0){
					handleSocketError(cli_info, "Create new thread to process list_hosts_request");
				}
				break;
			default:
				break;
		}
	}
	handleSocketError(cli_info, "Read from socket");
	close(cli_info.sockfd);
	pthread_cleanup_pop(0);		//free lock_sockfd
	free(cli_info.lock_sockfd);
	return NULL;
}