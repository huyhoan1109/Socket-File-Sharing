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
#include <ncurses.h>
#include <time.h>

#include "../../socket/utils/common.h"
#include "connect_index_server.h"
#include "update_file_list.h"
#include "list_files_request.h"
#include "list_hosts_request.h"
#include "download_file_request.h"
#include "handle_download_file_request.h"

#define MENUMAX 6

const char EXIT_CMD[] = "/exit";
int menuitem = 0;

void drawmenu(WINDOW *win, int item)
{
	int c;
	char menu[MENUMAX][40] = {
		"1. List file",
		"2. Available file",
		"3. Remove file from server",
		"4. Download file",
		"5. Share file",
		"6. Exit Program"
	};
	clear();
	for (c = 0; c < MENUMAX; c++)
	{
		if (c == item)
			wattron(win, A_REVERSE); // highlight selection
		mvwaddstr(win, 2 + (c * 2), 12, menu[c]);
		wattroff(win, A_REVERSE); // remove highlight
	}
	mvwaddstr(win, 15, 12, PRESS_ENTER);
	wrefresh(win);
}
void initMenu(WINDOW *win)
{
	mvwaddstr(win, 0, 2, " Main Menu ");
	int key;
	menuitem = 0;
	drawmenu(win, menuitem);
	keypad(stdscr, TRUE);
	noecho(); // disable echo
	// wgetch(win);
	do
	{
		key = wgetch(win);
		switch (key)
		{
		case 65:
		case 'w':
			menuitem--;
			if (menuitem < 0)
				menuitem = MENUMAX - 1;
			break;
		case 66:
		case 's':
			menuitem++;
			if (menuitem > MENUMAX - 1)
				menuitem = 0;
			break;
		case '\n':
			wclear(win);
			box(win, 0, 0);
			return;
			break;
		default:
			break;
		}
		drawmenu(win, menuitem);
	} while (key != '1');
}
void initScreen(WINDOW *win, char title[])
{
	mvwaddstr(win, 0, 2, title);
	int key;
	menuitem = 0;
	mvwaddstr(win, 14, 4, PRESS_ESC_BACK);
	keypad(stdscr, TRUE);
	noecho(); // disable echo
	do
	{
		key = wgetch(win);
		if (key == 27)
		{
			wclear(win);
			box(win, 0, 0);
			return;
		}
	} while (key != '1');
}

void initShareScreen(WINDOW *win, char *title, char noti[]){
	wclear(win);
	box(win, 0, 0);
	mvwaddstr(win, 0, 2, title);
	int key;
	char fileName[30] = {0};
	menuitem = 0;
	char path[100];
	mvwaddstr(win, 14, 4, PRESS_ESC_BACK);
	mvwaddstr(win, 2, 4, "File name to share:");
	mvwaddstr(win, 6, 4, noti);
	keypad(stdscr, TRUE);
	int temp = 0;
	do
	{
		key = wgetch(win);
		if (key == 27)
		{
			wclear(win);
			box(win, 0, 0);
			return;
		}
		if (key == 127 || key == 8)
		{
			if (temp > -1)
			{
				fileName[temp] = 0;
				if (temp > 0) temp --; 
				wclear(win);
				box(win, 0, 0);
				mvwaddstr(win, 0, 2, title);
				mvwaddstr(win, 14, 4, PRESS_ESC_BACK);
				mvwaddstr(win, 2, 4, "File name to share:");
				mvwaddstr(win, 2, 25, fileName);
				wrefresh(win);
			}
		}
		else if (key != '\n')
		{
			fileName[temp] = key;
			temp++;
			mvwaddstr(win, 2, 25, fileName);
		}
		else if (key == '\n')
		{
			strcpy(path, STORAGE);
			strcat(path, "/");
			strcat(path, fileName);
			char notify[30] = {0};
			if (access(path, F_OK) != -1)
			{
				if (strlen(fileName) == 0) {
					strcpy(notify, "No input available");
					mvwaddstr(win, 4, 4, notify);
				} else {
					share_file(fileName);
				}
			} else {
				strcpy(notify, "File not available");
				mvwaddstr(win, 4, 4, notify);
			}
		}
	} while (1);
}

void initRemoveScreen(WINDOW *win, char *title, char noti[])
{
	wclear(win);
	box(win, 0, 0);
	mvwaddstr(win, 0, 2, title);
	int key;
	char fileName[30] = {0};
	menuitem = 0;
	char path[100];
	mvwaddstr(win, 14, 4, PRESS_ESC_BACK);
	mvwaddstr(win, 2, 4, "File name to remove:");
	mvwaddstr(win, 6, 4, noti);
	keypad(stdscr, TRUE);
	int temp = 0;
	do
	{
		key = wgetch(win);
		if (key == 27)
		{
			wclear(win);
			box(win, 0, 0);
			return;
		}
		if (key == 127 || key == 8)
		{
			if (temp > -1)
			{
				fileName[temp] = 0;
				if (temp > 0) temp --; 
				wclear(win);
				box(win, 0, 0);
				mvwaddstr(win, 0, 2, title);
				mvwaddstr(win, 14, 4, PRESS_ESC_BACK);
				mvwaddstr(win, 2, 4, "File name to remove:");
				mvwaddstr(win, 2, 27, fileName);
				wrefresh(win);
			}
		}
		else if (key != '\n')
		{
			fileName[temp] = key;
			temp++;
			mvwaddstr(win, 2, 27, fileName);
		}
		else if (key == '\n')
		{
			int mod_key;
			char del_mod[30] = {0};
			int mod_i=0;
			mvwaddstr(win, 4, 4, "Delete from your device too (Y/N): ");
			do {
				mod_key = wgetch(win);
				if (mod_key == 27)
				{
					wclear(win);
					box(win, 0, 0);
					return;
				}
				if (mod_key == 127 || mod_key == 8)
				{
					if (mod_i > -1)
					{
						del_mod[mod_i] = 0;
						if (mod_i > 0) mod_i --; 
						wclear(win);
						box(win, 0, 0);
						mvwaddstr(win, 0, 2, title);
						mvwaddstr(win, 14, 4, PRESS_ESC_BACK);
						mvwaddstr(win, 2, 4, "File name to remove:");
						mvwaddstr(win, 2, 27, fileName);
						mvwaddstr(win, 4, 4, "Delete from your device too (Y/N): ");
						wrefresh(win);
					}
				}
				else if (mod_key != '\n')
				{
					del_mod[mod_i] = mod_key;
					mod_i++;
					mvwaddstr(win, 4, 39, del_mod);
				}
				else if (mod_key == '\n') break;
			} while(1);
			strcpy(path, STORAGE);
			strcat(path, "/");
			strcat(path, fileName);
			char removeNoti[100] = {0};
			if (access(path, F_OK) == -1){
				strcpy(removeNoti, "File not available!");
				mvwaddstr(win, 6, 4, removeNoti);
			} else {
				if (strcasecmp(del_mod, "n")==0){
					delete_from_server(fileName);
				} else if (strcasecmp(del_mod, "y")==0){
					remove(path);
					char removeNoti[100] = {0};
					strcat(removeNoti, "Delete");
					strcat(removeNoti, " ");
					strcat(removeNoti, fileName);
					strcat(removeNoti, " ");
					strcat(removeNoti, "successfully");
					mvwaddstr(win, 6, 4, removeNoti);
				} else {
					strcpy(removeNoti, "Please press Y/N to process!");
					mvwaddstr(win, 6, 4, removeNoti);
				}
			}
		}
	} while (1);
}
void initDownloadScreen(WINDOW *win, char *title, char noti[])
{
	wclear(win);
	box(win, 0, 0);
	mvwaddstr(win, 0, 2, title);
	mvwaddstr(win, 14, 4, PRESS_ESC_BACK);
	mvwaddstr(win, 2, 4, "File name to download:");
	mvwaddstr(win, 6, 4, noti);
	keypad(stdscr, TRUE);
	int temp = 0;
	int key;
	char fileName[30] = {0};
	menuitem = 0;
	char path[100];
	do
	{
		key = wgetch(win);
		if (key == 27)
		{
			wclear(win);
			box(win, 0, 0);
			return;
		}
		if (key == 127 || key == 8)
		{
			if (temp > -1)
			{
				fileName[temp] = 0;
				if (temp > 0) temp--;
				wclear(win);
				box(win, 0, 0);
				mvwaddstr(win, 0, 2, title);
				mvwaddstr(win, 14, 4, PRESS_ESC_BACK);
				mvwaddstr(win, 2, 4, "File name to download:");
				mvwaddstr(win, 2, 27, fileName);
				wrefresh(win);
			}
		}
		else if (key != '\n')
		{
			fileName[temp] = key;
			temp++;
			mvwaddstr(win, 2, 27, fileName);
		}
		else if (key == '\n')
		{

			strcpy(path, STORAGE);
			strcat(path, "/");
			strcat(path, fileName);
			if (access(path, F_OK) != -1)
			{
				char notify[30] = {0};
				strcat(notify, fileName);
				strcat(notify, " ");
				strcat(notify, "existed");
				if (strlen(fileName) == 0) strcpy(notify, "No input available");
				mvwaddstr(win, 4, 4, notify);
			} else {
				pthread_mutex_lock(&lock_the_file);
				the_file = malloc(sizeof(struct FileOwner));
				the_file->host_list = newLinkedList();
				strcpy(the_file->filename, fileName);
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
				send_list_hosts_request(fileName);
				int rev = download_done();
				clock_gettime(CLOCK_MONOTONIC_RAW, &end);
				long duration = (end.tv_sec - begin.tv_sec) * 1e3 + (end.tv_nsec - begin.tv_nsec) / 1e6;
				if (rev) {
					char notiTime[100] = {0};
					strcat(notiTime, "Elapsed time: ");
					strcat(notiTime, itoa(duration));
					strcat(notiTime, " miliseconds");
					mvwaddstr(win, 8, 4, notiTime);
				}
			}
		}
	} while (1);
}
int main(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("Usage: ./peer <host> <port>\n");
		exit(1);
	}

	struct stat filestat;
	char history[20] = "history.log";
	stat(history, &filestat);
	time_t curr = time(NULL);
	if(curr - filestat.st_mtim.tv_sec < 4000) stream = fopen(history, "a+");
	else stream = fopen(history, "w");

	/* ignore EPIPE to avoid crashing when writing to a closed socket */
	signal(SIGPIPE, SIG_IGN);

	int dataSock = socket(AF_INET, SOCK_STREAM, 0);
	if (dataSock < 0) exit(1);
	
	struct sockaddr_in dataSockIn;
	bzero(&dataSockIn, sizeof(dataSockIn));
	dataSockIn.sin_addr.s_addr = htonl(INADDR_ANY);
	dataSockIn.sin_family = AF_INET;
	dataSockIn.sin_port = 0; // let the OS chooses the port

	if ((bind(dataSock, (struct sockaddr *)&dataSockIn, sizeof(dataSockIn))) < 0) exit(1);

	if ((listen(dataSock, 20)) < 0) exit(1);

	struct sockaddr_in real_sock_in; // use to store the listening port
	socklen_t socklen = sizeof(real_sock_in);

	/* get the current socket info (include listening port) */
	if ((getsockname(dataSock, (struct sockaddr *)&real_sock_in, &socklen) < 0)) exit(1);

	/* port used to listen for download_file_request */
	dataPort = real_sock_in.sin_port; // network byte order
	
	/* create thread */
	int thr;
	
	/* listen for download file request */
	pthread_t download_tid;
	thr = pthread_create(&download_tid, NULL, waitForDownloadRequest, &dataSock);
	if (thr != 0) exit(1);

	/* connect to server */
	connect_to_index_server(argv[1], atoi(argv[2]));
	pthread_t tid;
	thr = pthread_create(&tid, NULL, &update_file_list, STORAGE);
	if (thr != 0) exit(1);

	initscr();
	noecho();
	curs_set(0);
	int yMax, xMax;
	getmaxyx(stdscr, yMax, xMax);
	win = newwin(17, 55, yMax / 5, xMax / 5);
	box(win, 0, 0);
	/* listen for and process responses from the index server */
	pthread_t process_response_tid;
	thr = pthread_create(&process_response_tid, NULL, &process_response, (void *)&servsock);
	if (thr != 0){
		close(servsock);
		exit(1);
	}
	while (menuitem != 5)
	{
		
		initMenu(win);
		if (menuitem == 0)
		{
			wrefresh(win);
			send_list_files_request();
			initScreen(win, " List File Screen ");
		}
		else if (menuitem == 1)
		{
			int h_f = 0;
			DIR *d;
			struct dirent *dir;
			d = opendir(STORAGE);
			if (d)
			{
				int item = 0;
				while ((dir = readdir(d)) != NULL)
				{
					// Condition to check regular file.
					if (dir->d_type == DT_REG && dir->d_name[0] != '.')
					{
						mvwaddstr(win, 4 + (item * 2), 4, itoa(item + 1));
						mvwaddstr(win, 4 + (item * 2), 10, dir->d_name);
						wrefresh(win);
						item++;
						h_f = 1;
					}
				}
				closedir(d);
			}
			if (!h_f) mvwaddstr(win, 2, 4, "No files in database");
			else {
				mvwaddstr(win, 2, 4, "No");
				mvwaddstr(win, 2, 10, "Filename");
			}
			initScreen(win, " Available file screen ");
		}
		else if (menuitem == 2)
		{
			initRemoveScreen(win, " Remove file screen ", "");
		}
		else if (menuitem == 3)
		{
			initDownloadScreen(win, " Download file screen ", "");
		}
		else if (menuitem == 4)
		{
			initShareScreen(win, " Share file screen ", "");
		}
	}
	echo(); // re-enable echo
	endwin();
	return 0;
}