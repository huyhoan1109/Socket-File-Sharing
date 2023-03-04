#include "list_hosts_request.h"
#include "connect_index_server.h"
#include "download_file_request.h"

uint8_t seq_no = 0;
struct FileOwner *the_file = NULL;
pthread_mutex_t lock_the_file = PTHREAD_MUTEX_INITIALIZER;

void initDownload(){
	wclear(win);
	box(win, 0, 0);
	mvwaddstr(win, 0, 2, " Download Screen ");
	mvwaddstr(win, 14, 4, PRESS_BACK);
	mvwaddstr(win, 2, 4, "File name to download:");
	mvwaddstr(win, 2, 27, the_file->filename);
}

void show_option(int choice, int start, int end, struct DownloadInfo dlist[]){
	initDownload();
	mvwaddstr(win, 4, 4, "No");
	mvwaddstr(win, 4, 10, "Address");
	mvwaddstr(win, 4, 25, "Filesize (bytes)");
	for (int c = start; c < end; c++)
	{
		mvwaddstr(win, 6 + ((c-start) * 2), 4, itoa(c+1));
		// highlight selection
		if (c == choice) wattron(win, A_REVERSE);
		struct in_addr addr; 
		addr.s_addr = htonl(dlist[c].dthost.ip_addr);
		mvwaddstr(win, 6 + ((c-start) * 2), 10, inet_ntoa(addr));
		// remove highlight
		wattroff(win, A_REVERSE);
		mvwaddstr(win, 6 + ((c-start) * 2), 25, itoa(dlist[c].dthost.filesize));
	}
	wrefresh(win);
}

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
	if (recv_host == 0) return;
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
		return;
	}
	struct DownloadInfo *dlist = calloc(n_hosts, sizeof(struct DownloadInfo));
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
		int maxItems = 3;
		int items = (n_hosts < maxItems) ? n_hosts:maxItems ;
		int choice = 0;
		int start = 0;
		int end = start + items;
		int key;
		show_option(0, start, end, dlist);
		do {
			key = wgetch(win);
			if (key == 'w'){
				choice -= 1;
				if(choice < 0) {
					choice = n_hosts - 1;
					end = n_hosts;
					start = (end < items) ? 0:(end - items);
				}
				if (choice < start) {
					start--;
					end--;
				}
			} else if(key == 's'){
				choice += 1;
				if(choice > n_hosts - 1) {
					choice = 0;
					start = 0;
					end = start + items;	
				}
				if (choice > end - 1){
					start ++;
					end ++;
				}
			}
			if (key == '\n') {
				break;
			}
			else {
				show_option(choice, start, end, dlist);
			}
		} while (1);
		if (sequence == seq_no){
			initDownload();
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
	free(dlist);
	free(info);
	pthread_mutex_unlock(&lock_the_file);
	recv_host = 0;
	return;
}