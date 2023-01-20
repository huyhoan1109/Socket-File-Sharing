#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h> 

#include "../../socket/utils/common.h"
#include "connect_index_server.h"
#include "update_file_list.h"
#include "list_files_request.h"
#include "list_hosts_request.h"
#include "download_file_request.h"
#include "handle_download_file_request.h"

const char EXIT_CMD[] = "/exit";

void print_help();

int main(int argc, char **argv){
	stream = fopen("history.log", "a+");
	if (argc != 3){
		printf("Usage: ./peer <host> <port>\n");
		exit(1);
	}
	/* ignore EPIPE to avoid crashing when writing to a closed socket */
	signal (SIGPIPE, SIG_IGN);

	int dataSock = socket(AF_INET, SOCK_STREAM, 0);
	if (dataSock < 0){
		print_error("create socket");
		exit(1);
	}
	struct sockaddr_in dataSockIn;
	bzero(&dataSockIn, sizeof(dataSockIn));
	dataSockIn.sin_addr.s_addr = htonl(INADDR_ANY);
	dataSockIn.sin_family = AF_INET;
	dataSockIn.sin_port = 0;	//let the OS chooses the port
	
	if ((bind(dataSock, (struct sockaddr*) &dataSockIn, sizeof(dataSockIn))) < 0){
		print_error("bind socket");
		exit(1);
	}

	if ((listen(dataSock, 20)) < 0){
		print_error("listen on socket");
		exit(1);
	}
	
	struct sockaddr_in real_sock_in;	//use to store the listening port
	socklen_t socklen = sizeof(real_sock_in);

	/* get the current socket info (include listening port) */
	if ((getsockname(dataSock, (struct sockaddr*)&real_sock_in, &socklen) < 0)){
		print_error("getsockname");
		exit(1);
	}

	/* port used to listen for download_file_request */
	dataPort = real_sock_in.sin_port;	//network byte order
	
	/* listen for download file request */
	pthread_t download_tid;
	int thr = pthread_create(&download_tid, NULL, waitForDownloadRequest, &dataSock);
	if (thr != 0){
		print_error("New thread to handle download file request");
		exit(1);
	}
	
	connect_to_index_server(argv[1], atoi(argv[2]));

	printf("Listening for download_file_request at: 0.0.0.0:%u\n", dataPort);

	pthread_t tid;
	thr = pthread_create(&tid, NULL, &update_file_list, STORAGE);
	if (thr != 0){
		print_error("New thread to update file list to the index server");
		exit(1);
	}

	/* listen for and process responses from the index server */
	pthread_t process_response_tid;
	thr = pthread_create(&process_response_tid, NULL, &process_response, (void*)&servsock);
	if (thr != 0){
		print_error("New thread to process response from the index server");
		close(servsock);
		exit(1);
	}
	char path[100];
	mkdir(tmp_dir, 0700);
	while(1){
		char input[400];
		fgets(input, sizeof(input), stdin);

		char *command = strtok(input, " \n\t");

		if (command == NULL){
			continue;
		}
		if (strcmp(command, "/ls") == 0){
			send_list_files_request();
		} else if (strcmp(command, "/avail") == 0){
			printf("You have: ");
			int h_f = 0;
			DIR *d;
			struct dirent *dir;
			d = opendir(STORAGE);
			if (d){
				while ((dir = readdir(d)) != NULL){
					//Condition to check regular file.
					if(dir->d_type==DT_REG && dir->d_name[0] != '.'){
						printf("\'%s\' ", dir->d_name);
						h_f = 1;
					}
				}
				closedir(d);
			}
			if (!h_f) printf("no files in database.\n");
			else printf("\n");
		} else if (strcmp(command, "/rm") == 0) {
			char *filename = strtok(NULL, " \n\t");
			if (filename){
				strcpy(path, STORAGE);
				strcat(path, "/");
				strcat(path, filename);
				int ret = remove(path);
				if (ret != 0){
					print_error("remove file");
				} else {
					printf("Delete %s successfully\n", filename);
				}
			} else {
				printf("help: /rm <filename>\n");
			}
			fflush(stdout);
		} else if (strcmp(command, "/get") == 0){
			char *filename = strtok(NULL, " \n\t");
			if (filename){
				strcpy(path, STORAGE);
				strcat(path, "/");
				strcat(path, filename);
				if (access(path, F_OK) != -1){
					fprintf(stdout, "\'%s\' existed\n", filename);
				} else {
					pthread_mutex_lock(&lock_the_file);
					the_file = malloc(sizeof(struct FileOwner));
					the_file->host_list = newLinkedList();
					strcpy(the_file->filename, filename);
					the_file->filesize = 0;
					pthread_mutex_unlock(&lock_the_file);

					pthread_mutex_lock(&lock_segment_list);
					segment_list = newLinkedList();
					pthread_mutex_unlock(&lock_segment_list);

					pthread_mutex_lock(&lock_n_threads);
					n_threads = 0;
					pthread_mutex_unlock(&lock_n_threads);

					struct timespec begin, end;
					clock_gettime(CLOCK_MONOTONIC_RAW, &begin);

					send_list_hosts_request(filename);
					
					download_done();

					clock_gettime(CLOCK_MONOTONIC_RAW, &end);

					long duration = (end.tv_sec - begin.tv_sec)*1e3 + (end.tv_nsec - begin.tv_nsec)/1e6;
					printf("Elapsed time: %ld miliseconds\n", duration);
				}
			} else {
				printf("help: /get <filename>\n");
			}
			fflush(stdout);
		} else if (strcmp(command, "/help") == 0) {
			print_help();
			fflush(stdout);
		} else if (strcmp(command, EXIT_CMD) == 0){
			return 0;
		} else {
			print_help();
			fflush(stdout);
		}
	}
	return 0;
}

void print_help(){
	prettyprint("Command", 0, NULL);
	prettyprint("Feature", 0, NULL);
	printf("\n");
	prettyprint("/ls", 0, "Command");
	prettyprint("list files", 0, "Feature");
	printf("\n");
	prettyprint("/avail", 0, "Command");
	prettyprint("has files", 0, "Feature");
	printf("\n");
	prettyprint("/rm <file>", 0, "Command");
	prettyprint("remove file", 0, "Feature");
	printf("\n");
	prettyprint("/get <file>", 0, "Command");
	prettyprint("download file", 0, "Feature");
	printf("\n");
	prettyprint("/exit", 0, "Command");
	prettyprint("exit program", 0, "Feature");
	printf("\n");
}