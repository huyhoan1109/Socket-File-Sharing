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

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "handle_request.h"


void *serveClient(void *arg);
void close_socket(void *arg);

int main(int argc, char **argv){
	int port = 8000;
	if (argc > 1){
		if (argc == 3 && !strcmp(argv[1], "-r") && atoi(argv[2])) {
			port = atoi(argv[2]);
		} else{
			printf("unkown option: %s\n", argv[1]);
			exit(1);
		}
	}
	stream = fopen("history.log", "w");
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
			perror("accept connection");
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
			fprintf(stream, "create thread to handle %s:%u: %s\n", 
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

	printf("connection from client: %s\n", cli_addr);
	uint8_t packet_type;

	struct thread_data thrdt;	//use for process_list_hosts_request
	thrdt.cli_info = cli_info;

	/* receive request from clients,
	 * then response accordingly */
	//read packet type first
	while(readBytes(cli_info.sockfd, &packet_type, sizeof(packet_type)) > 0){
		if (packet_type == DATA_PORT_ANNOUNCEMENT){
			if (cli_info.data_port != 0){
				/* only accept DATA_PORT_ANNOUNCEMENT packet once */
				fprintf(stderr, "%s > only accept DATA_PORT_ANNOUNCEMENT packet once\n", 
						cli_addr);
				fprintf(stderr, "removing %s from the file list due to data port violation\n",
						cli_addr);
				struct DataHost host;
				host.ip_addr = inet_addr(cli_info.ip_add);
				host.port = cli_info.data_port;
				removeHost(host);
				close(cli_info.sockfd);
				int ret = 100;
				pthread_exit(&ret);
			}
			uint16_t data_port;
			int n_bytes = readBytes(cli_info.sockfd, &data_port, sizeof(data_port));
			if (n_bytes <= 0)
				handleSocketError(cli_info, "read from socket");
			fprintf(stream, "%s > dataPort: %u\n", cli_addr, ntohs(data_port));
			cli_info.data_port = ntohs(data_port);
		} else if (packet_type == FILE_LIST_UPDATE){
			if (cli_info.data_port == 0){
				//DATA_PORT_ANNOUNCEMENT must be sent first
				fprintf(stderr, "%s:%u > DATA_PORT_ANNOUNCEMENT must be sent first\n",
						cli_info.ip_add, cli_info.port);
				fprintf(stderr, "closing connection from %s:%u\n", cli_info.ip_add, cli_info.port);
				close(cli_info.sockfd);
				fprintf(stderr, "connection from %s:%u closed\n", cli_info.ip_add, cli_info.port);
				int ret = 100;
				pthread_exit(&ret);
			}
			update_file_list(cli_info);

		} else if (packet_type == LIST_FILES_REQUEST){
			/* process list_files_request, then reponse */
			process_list_files_request(cli_info);
		} else if (packet_type == LIST_HOSTS_REQUEST){
			fprintf(stream, "list_hosts_request\n");
			uint8_t sequence;
			long n_bytes;
			n_bytes = readBytes(thrdt.cli_info.sockfd,
								&sequence,
								sizeof(sequence));
			if (n_bytes <= 0){
				handleSocketError(thrdt.cli_info, "[LIST_HOSTS_REQUEST]read sequence number");
			}
			thrdt.seq_no = sequence;
			uint16_t filename_length;
			n_bytes = readBytes(thrdt.cli_info.sockfd, 
									&filename_length, 
									sizeof(filename_length));
			if (n_bytes <= 0){
				handleSocketError(thrdt.cli_info, "[LIST_HOSTS_REQUEST]read filename_length");
			}
			filename_length = ntohs(filename_length);
			fprintf(stream, "filename_length: %u\n", filename_length);
			if (filename_length <= 0){
				fprintf(stream, "[LIST_HOSTS_REQUEST]filename_length = 0\n");
				thrdt.filename[0] = 0;
				continue;
			}

			n_bytes = readBytes(thrdt.cli_info.sockfd, thrdt.filename, filename_length);
			if (n_bytes <= 0){
				handleSocketError(thrdt.cli_info, "read filename");
			}
			fprintf(stream, "filename: %s\n", thrdt.filename);
			pthread_t tid;
			fprintf(stream, "create new thread to process list_hosts_request\n");
			int thr = pthread_create(&tid, NULL, &process_list_hosts_request, &thrdt);
			if (thr != 0){
				handleSocketError(cli_info, "create new thread to process list_hosts_request");
			}
		}
	}
	handleSocketError(cli_info, "read from socket");
	close(cli_info.sockfd);
	pthread_cleanup_pop(0);		//free lock_sockfd
	free(cli_info.lock_sockfd);
	return NULL;
}
