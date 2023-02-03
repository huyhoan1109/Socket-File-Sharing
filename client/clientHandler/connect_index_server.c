#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "connect_index_server.h"
#include "list_files_request.h"
#include "list_hosts_request.h"
#include "update_file_list.h"

pthread_mutex_t lock_servsock = PTHREAD_MUTEX_INITIALIZER;

int servsock = 0;
WINDOW *win;
char old_server_ip[30];
uint16_t old_port;

void connect_to_index_server(char *servip, uint16_t index_port){
	/* connect to index server */
	struct sockaddr_in servsin;
	bzero(&servsin, sizeof(servsin));

	servsin.sin_family = AF_INET;

	strcpy(old_server_ip, servip);
	old_port = index_port;

	/* parse the buf to get ip address and port */
	servsin.sin_addr.s_addr = inet_addr(servip);
	servsin.sin_port = htons(index_port);

	int servsocket = socket(AF_INET, SOCK_STREAM, 0);
	if (servsocket < 0) exit(1);
	if (connect(servsocket, (struct sockaddr*) &servsin, sizeof(servsin)) < 0) exit(1);
	servsock = servsocket;
}

void* process_response(void *arg){
	pthread_detach(pthread_self());
	int servsock = *(int*)arg;
	/* process responses from the index server */
	char *message = malloc(MAX_BUFF_SIZE); 
	uint8_t packet_type;
	// long n_bytes;
	while (readBytes(servsock, message, MAX_BUFF_SIZE) > 0){
		packet_type = protocolType(message);
		if (packet_type == LIST_FILES_RESPONSE){
			process_list_files_response(message);
			// fflush(stdout);
		} else if (packet_type == LIST_HOSTS_RESPONSE){
			fprintf(stream, "list_hosts_response\n");
			process_list_hosts_response(message);
		}
	}
	wclear(win);
	echo(); // re-enable echo
	endwin();
	printf("Server has shut down!\n");
	exit(1);
}