#include "list_files_request.h"
#include "connect_index_server.h"

void send_list_files_request()
{
	pthread_mutex_lock(&lock_servsock);
	int n_bytes = writeBytes(servsock, itoa(LIST_FILES_REQUEST), MAX_BUFF_SIZE);
	if (n_bytes <= 0){
		exit(1);
	}
	pthread_mutex_unlock(&lock_servsock);
}

void process_list_files_response(char *message){
	int n_files = 0;
	char *info = getInfo(message);
	n_files = atoi(nextInfo(info, IS_FIRST));
	if (n_files == 0){
		mvwaddstr(win, 2, 4, "No files available");
		wrefresh(win);
		return;
	}
	mvwaddstr(win, 2, 4, "No");
	mvwaddstr(win, 2, 10, "Filename");
	mvwaddstr(win, 2, 25, "Size (Byte)");
	mvwaddstr(win, 2, 45, "Address");
	wrefresh(win);
	int n_host;
	char *filename;
	int counter = 0;
	for (int i=0; i < n_files; i++)
	{
		filename = nextInfo(info, IS_AFTER);
		n_host = atoll(nextInfo(info, IS_AFTER));
		for (int j = 0; n_host > j; j++){
			mvwaddstr(win, 4 + (counter * 2), 4, itoa(counter+1));
			mvwaddstr(win, 4 + (counter * 2), 10, filename);
			mvwaddstr(win, 4 + (counter * 2), 25, nextInfo(info, IS_AFTER));
			mvwaddstr(win, 4 + (counter * 2), 45, nextInfo(info, IS_AFTER));
			counter ++;
		}
		wrefresh(win);
	}
}