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
	char *message = calloc(MAX_BUFF_SIZE, sizeof(char)); 
	uint8_t packet_type;
	
	while (readBytes(servsock, message, MAX_BUFF_SIZE) > 0){	
		packet_type = protocolType(message);
		if (packet_type == LIST_FILES_RESPONSE){
			process_list_files_response(message);		
		} else if (packet_type == LIST_HOSTS_RESPONSE){
			process_list_hosts_response(message);
		}
	}
	
	free(message);
	wclear(win);
	echo(); 
	endwin();
	printf("Server has shut down!\n");
	exit(1);
}

void drawProgress(WINDOW *win, int y, int x, uint64_t current, uint64_t max){
	float f_percent = (float)(current) * 100.0 / (float)(max);
	int percentage = (int)f_percent;
	char show_percent[4] = {0};
	strcpy(show_percent, itoa(percentage));
	strcat(show_percent, "%");
	mvwaddstr(win, y, x, show_percent);
	mvwaddstr(win, y, x + 5, "[");
	mvwaddstr(win, y, x + win->_maxx - 8, "]");
	mvwhline(win, y, x+6, ACS_CKBOARD, (win->_maxx - 14) * percentage / 100);
	wrefresh(win);
}