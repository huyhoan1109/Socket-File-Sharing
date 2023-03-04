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
	// int counter = 0;
	mvwaddstr(win, 2, 4, "No");
	mvwaddstr(win, 2, 10, "Filename");
	mvwaddstr(win, 2, 26, "Hosts");
	mvwaddstr(win, 2, 35, "Min (bytes)");
	mvwaddstr(win, 2, 48, "Max (bytes)");
	wrefresh(win);
	for (int i=0; i < n_files; i++)
	{
		mvwaddstr(win, 4 + (i * 2), 4, itoa(i+1));
		mvwaddstr(win, 4 + (i * 2), 10, nextInfo(info, IS_AFTER));
		mvwaddstr(win, 4 + (i * 2), 26, nextInfo(info, IS_AFTER));
		mvwaddstr(win, 4 + (i * 2), 35, nextInfo(info, IS_AFTER));
		mvwaddstr(win, 4 + (i * 2), 48, nextInfo(info, IS_AFTER));
		wrefresh(win);
	}
}