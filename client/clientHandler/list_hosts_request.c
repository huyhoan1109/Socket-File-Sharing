#include "list_hosts_request.h"
#include "connect_index_server.h"
#include "download_file_request.h"

uint8_t seq_no = 0;
struct FileOwner *the_file = NULL;
pthread_mutex_t lock_the_file = PTHREAD_MUTEX_INITIALIZER;

void send_list_hosts_request(char *filename){
	pthread_mutex_lock(&lock_servsock);
	// LIST_HOST_REQUEST :=: seq_no :=: filename

	char *info = calloc(MAX_BUFF_SIZE, sizeof(char));
	
	pthread_mutex_lock(&lock_the_file);
	
	info = appendInfo(NULL, itoa(seq_no));
	info = appendInfo(info, filename);
	
	pthread_mutex_unlock(&lock_the_file);

	char *message = addHeader(info, LIST_HOSTS_REQUEST);

	writeBytes(servsock, message, MAX_BUFF_SIZE);
	free(message);
	free(info);
	pthread_mutex_unlock(&lock_servsock);
}

void process_list_hosts_response(char* message){
	pthread_mutex_lock(&lock_key);
	char *info = getInfo(message);
	uint8_t sequence = atol(nextInfo(info, IS_FIRST));
	
	// uint32_t filesize = atoll(nextInfo(info, IS_AFTER));

	pthread_mutex_lock(&lock_the_file);
	
	// if (sequence == seq_no) totalsize = filesize;
	
	uint32_t n_hosts = atol(nextInfo(info, IS_AFTER));

	if (n_hosts == 0 && sequence == seq_no){
		/* file not found */
		pthread_mutex_lock(&lock_segment_list);
		pthread_cond_signal(&cond_segment_list);
		pthread_mutex_unlock(&lock_segment_list);
		pthread_mutex_unlock(&lock_the_file);
		pthread_mutex_unlock(&lock_key);
		return;
	}
	struct DownloadInfo dlist[n_hosts];
	uint8_t status[n_hosts]; 
	for (uint8_t i = 0; i < n_hosts; i++){
		status[i] = atol(nextInfo(info, IS_AFTER));
		dlist[i].seq_no = sequence;
		dlist[i].dthost.ip_addr = atoll(nextInfo(info, IS_AFTER));
		dlist[i].dthost.port = atoll(nextInfo(info, IS_AFTER));	
		dlist[i].dthost.filesize = atoll(nextInfo(info, IS_AFTER));
	}
	if(n_hosts == 0){
		mvwaddstr(win, 4, 4, "No file available\n");
	} else {
		int choice;
		if (n_hosts > 1) {
			int i = 0;
			mvwaddstr(win, 4, 4, "No");
			mvwaddstr(win, 4, 10, "Port");
			mvwaddstr(win, 4, 25, "Filesize (Byte)");
			for (; n_hosts > i; i++){
				mvwaddstr(win, 6 + (i * 2), 4, itoa(i+1));
				mvwaddstr(win, 6 + (i * 2), 10, itoa(dlist[i].dthost.port));
				mvwaddstr(win, 6 + (i * 2), 25, itoa(dlist[i].dthost.filesize));
				wrefresh(win);
			}
			mvwprintw(win, 6 + (i * 2), 4, "Your choice (1-%d): ", n_hosts);
			char *choice_char = calloc(30, sizeof(char));
			int key, temp=0;
			do {
				key = wgetch(win);
				if (key == 127 || key == 8)
				{
					if (temp > -1)
					{
						choice_char[temp] = 0;
						if (temp > 0) temp--;
						wclear(win);
						box(win, 0, 0);
						mvwaddstr(win, 0, 2, " Download Screen ");
						mvwaddstr(win, 14, 4, PRESS_BACK);
						mvwaddstr(win, 2, 4, "File name to download:");
						mvwaddstr(win, 2, 27, the_file->filename);
						mvwaddstr(win, 4, 4, "No");
						mvwaddstr(win, 4, 10, "Port");
						mvwaddstr(win, 4, 25, "Filesize (Byte)");
						for (int j=0; n_hosts > j; j++){
							mvwaddstr(win, 6 + (j * 2), 4, itoa(j+1));
							mvwaddstr(win, 6 + (j * 2), 10, itoa(dlist[j].dthost.port));
							mvwaddstr(win, 6 + (j * 2), 25, itoa(dlist[j].dthost.filesize));
						}
						mvwprintw(win, 6 + (i * 2), 4, "Your choice (1-%d): ", n_hosts);
						mvwaddstr(win, 6 + (i * 2), 25, choice_char);
						wrefresh(win);
					}
				}
				else if (key != '\n'){
					choice_char[temp++] = key;
					mvwprintw(win, 6 + (i * 2), 4, "Your choice (1-%d): ", n_hosts);
					mvwaddstr(win, 6 + (i * 2), 25, choice_char);
				}
				else if (key == '\n'){
					choice = atoi(choice_char);
					memset(choice_char, 0, temp * sizeof(char));
					temp = 0;
					if (choice != 0 && choice < n_hosts + 1) {
						choice -= 1;
						wclear(win);
						box(win, 0, 0);
						mvwaddstr(win, 0, 2, " Download Screen ");
						mvwaddstr(win, 14, 4, PRESS_BACK);
						mvwaddstr(win, 2, 4, "File name to download:");
						mvwaddstr(win, 2, 27, the_file->filename);
						mvwaddstr(win, 2, 4, "File name to download:");
						mvwaddstr(win, 2, 27, the_file->filename);
						break;
					} else {
						mvwprintw(win, 6 + (i * 2), 4, "Input not available");
					}
				}
			} while(1);
		} else {
			choice = 0;
		}
		if (sequence == seq_no){
			struct DownloadInfo *dinfo = calloc(1, sizeof(struct DownloadInfo));
			dinfo->seq_no = dlist[choice].seq_no;
			dinfo->dthost.ip_addr = dlist[choice].dthost.ip_addr;
			dinfo->dthost.port = dlist[choice].dthost.port;
			dinfo->dthost.filesize = dlist[choice].dthost.filesize;
			totalsize = dlist[choice].dthost.filesize;
			if (status[choice] == FILE_AVAIL){
				struct Node *host_node = getNodeByHost(the_file->host_list, dinfo->dthost);
				if (!host_node){
					host_node = newNode(&dinfo->dthost, DATA_HOST_TYPE);
					push(the_file->host_list, host_node);
					/* create new thread to download file */
					pthread_mutex_lock(&lock_n_threads);
					if (n_threads >= 0){
						pthread_t tid;
						pthread_create(&tid, NULL, &download_file, dinfo);
						n_threads ++;
					}
					pthread_mutex_unlock(&lock_n_threads);
				}
			} else if (status[choice] == FILE_DELETED) {
				struct Node *host_node = getNodeByHost(the_file->host_list, dinfo->dthost);
				if (host_node){
					removeNode(the_file->host_list, host_node);
				} 
				free(dinfo);
			}
		}
	}
	pthread_mutex_unlock(&lock_key);
	pthread_mutex_unlock(&lock_the_file);
	free(info);
}