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

#define MENUMAX 7

int menuitem = -1;

char MenuList[MENUMAX][40] = {
	"1. List file",
	"2. Available file",
	"3. Remove file from server",
	"4. Download file",
	"5. Share file",
	"6. Something",
	"7. Exit Program"
};

void drawmenu(WINDOW *win, int item, int start, int end)
{
	wclear(win);
	box(win, 0, 0);
	mvwaddstr(win, 0, 2, " Main Menu ");
	int c;
	clear();
	for (c = start; c <= end; c++)
	{
		if (c == item)
			wattron(win, A_REVERSE); // highlight selection
		mvwaddstr(win, 2 + ((c-start) * 2), 12, MenuList[c]);
		wattroff(win, A_REVERSE); // remove highlight
	}
	mvwaddstr(win, 15, 12, PRESS_ENTER);
	wrefresh(win);
}

void initMenu(WINDOW *win)
{	
	int key;
	menuitem = 0;
	int itemnum = 5;
	int start = 0;
	int end = start + itemnum;
	drawmenu(win, menuitem, start, end);
	keypad(stdscr, TRUE);
	noecho(); // disable echo
	do
	{
		key = wgetch(win);
		switch (key)
		{
		case 65:
		case 'w':
			menuitem--;
			if (menuitem < 0) {
				menuitem = MENUMAX - 1;
				end = MENUMAX - 1;
				start = end - itemnum;
			}
			if (menuitem < start) {
				start--;
				end--;
			}
			break;
		case 66:
		case 's':
			menuitem++;
			if (menuitem > MENUMAX - 1) {
				menuitem = 0;
				start = 0;
				end = start + itemnum;
			}
			if (menuitem > end) {
				start++;
				end++;
			}
			break;
		case '\n':
			wclear(win);
			box(win, 0, 0);
			return;
			break;
		default:
			break;
		}
		drawmenu(win, menuitem, start, end);
	} while (key != '1');
}
void initScreen(WINDOW *win, char *title)
{
	mvwaddstr(win, 0, 2, title);
	int key;
	menuitem = -1;
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

void initShareScreen(WINDOW *win){
	wclear(win);
	box(win, 0, 0);
	mvwaddstr(win, 0, 2, " Share File Screen ");
	int key;
	menuitem = -1;	
	char *fileName = calloc(30, sizeof(char));
	char *path = calloc(100, sizeof(char));
	mvwaddstr(win, 14, 4, PRESS_ESC_BACK);
	mvwaddstr(win, 2, 4, "File name to share:");
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
				mvwaddstr(win, 0, 2, " Share File Screen ");
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
			if (access(path, F_OK) != -1)
			{
				if (strlen(fileName) == 0) {
					mvwaddstr(win, 4, 4, "No input available");
				} else {
					share_file(fileName);
				}
			} else {
				mvwaddstr(win, 4, 4, "File not available");
			}
		}
	} while (1);
	free(fileName);
}

void initRemoveScreen(WINDOW *win)
{
	wclear(win);
	box(win, 0, 0);
	mvwaddstr(win, 0, 2, " Remove Screen ");
	int key;
	char *fileName = calloc(30, sizeof(char));
	menuitem = -1;
	char *path = calloc(100, sizeof(char));
	mvwaddstr(win, 14, 4, PRESS_ESC_BACK);
	mvwaddstr(win, 2, 4, "File name to remove:");
	keypad(stdscr, TRUE);
	int temp = 0;
	do
	{
		key = wgetch(win);
		if (key == 27)
		{
			wclear(win);
			box(win, 0, 0);
			free(fileName);
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
				mvwaddstr(win, 0, 2, " Remove Screen ");
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
			char *del_mod = calloc(100, sizeof(char));
			int mod_i=0;
			mvwaddstr(win, 4, 4, "Delete from your device too (Y/N): ");
			do {
				mod_key = wgetch(win);
				if (mod_key == 27)
				{
					wclear(win);
					box(win, 0, 0);
					free(fileName);
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
						mvwaddstr(win, 0, 2, " Remove Screen ");
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
			if (access(path, F_OK) == -1){
				mvwaddstr(win, 6, 4, "File not available!");
			} else {
				if (strcasecmp(del_mod, "n")==0){
					delete_from_server(fileName);
				} else if (strcasecmp(del_mod, "y")==0){
					remove(path);
					mvwprintw(win, 6, 4, "Delete %s successfully", fileName);
				} else {
					mvwaddstr(win, 6, 4, "Please press Y/N to process!");
				}
			}
		}
	} while (1);
	free(fileName);
}
void initDownloadScreen(WINDOW *win)
{
	wclear(win);
	box(win, 0, 0);
	mvwaddstr(win, 0, 2, " Download Screen ");
	mvwaddstr(win, 14, 4, PRESS_ESC_BACK);
	mvwaddstr(win, 2, 4, "File name to download:");
	keypad(stdscr, TRUE);
	int temp = 0;
	int key;
	char *fileName = calloc(30, sizeof(char));
	menuitem = -1;
	char *path = calloc(100, sizeof(char));
	do
	{
		key = wgetch(win);
		if (key == 27)
		{
			wclear(win);
			box(win, 0, 0);
			free(fileName);
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
				mvwaddstr(win, 0, 2, " Download Screen ");
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
				if (strlen(fileName) == 0) mvwprintw(win, 4, 4, "No input available");
				else mvwprintw(win, 4, 4, "%s existed", fileName); 
			} else {
				pthread_mutex_lock(&lock_the_file);
				the_file = calloc(1, sizeof(struct FileOwner));
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
					mvwprintw(win, 8, 4, "Elapsed time: %s miliseconds", itoa(duration));
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
	thr = pthread_create(&tid, NULL, &init_file_list, STORAGE);
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
	while (menuitem != MENUMAX - 1)
	{
		
		if (menuitem == -1){
			initMenu(win);
		}
		if (menuitem == 0)
		{
			wrefresh(win);
			send_list_files_request();
			initScreen(win, " List File Screen ");
		}
		else if (menuitem == 1)
		{
			wclear(win);
			box(win, 0, 0);
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
			initRemoveScreen(win);
		}
		else if (menuitem == 3)
		{
			initDownloadScreen(win);
			refreshListFile(monitorFiles, STORAGE);
		}
		else if (menuitem == 4)
		{
			initShareScreen(win);
		}
		else if (menuitem == 5)
		{
			initScreen(win, " Something ");
		}
	}
	echo(); // re-enable echo
	endwin();
	return 0;
}