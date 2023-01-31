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

#include "../../socket/utils/common.h"
#include "connect_index_server.h"
#include "update_file_list.h"
#include "list_files_request.h"
#include "list_hosts_request.h"
#include "download_file_request.h"
#include "handle_download_file_request.h"

#define MENUMAX 5

const char EXIT_CMD[] = "/exit";
int menuitem = 0;

void print_help();
void drawmenu(WINDOW *win, int item)
{
	int c;
	char menu[MENUMAX][21] = {
			"1. List file",
			"2. Availabel file",
			"3. Remove file",
			"4. Download file",
			"5. Exit Program"};

	clear();
	for (c = 0; c < MENUMAX; c++)
	{
		if (c == item)
			wattron(win, A_REVERSE); // highlight selection
		mvwaddstr(win, 2 + (c * 2), 12, menu[c]);
		wattroff(win, A_REVERSE); // remove highlight
	}
	mvwaddstr(win, 13, 2, "Enter to select");
	wrefresh(win);
}
void initMenu(WINDOW *win)
{
	mvwprintw(win, 0, 2, "Main Menu");
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
		case 's':
			menuitem++;
			if (menuitem > MENUMAX - 1)
				menuitem = 0;
			break;
		case 'w':
			menuitem--;
			if (menuitem < 0)
				menuitem = MENUMAX - 1;
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
	mvwprintw(win, 0, 2, title);
	int key;
	menuitem = 0;
	mvwaddstr(win, 13, 2, "Press esc to go back menu");
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
void initRemoveScreen(WINDOW *win, char title[], char noti[])
{
	wclear(win);
	box(win, 0, 0);
	mvwprintw(win, 0, 2, title);
	int key;
	char fileName[30] = {0};
	menuitem = 0;
	char path[100];
	mvwaddstr(win, 13, 2, "Press esc to go back menu");
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
			if (temp != 0)
			{
				fileName[temp] = 0;
				temp--;
				wclear(win);
				box(win, 0, 0);
				mvwprintw(win, 0, 2, title);
				mvwaddstr(win, 13, 2, "Press esc to go back menu");
				mvwaddstr(win, 2, 4, "File name to remove:");
				mvwaddstr(win, 4, 4, fileName);
				wrefresh(win);
			}
		}
		else if (key != '\n')
		{
			fileName[temp] = key;
			temp++;
			mvwaddstr(win, 4, 4, fileName);
		}
		else if (key == '\n')
		{
			// fileName[temp - 1] = 0;
			strcpy(path, STORAGE);
			strcat(path, "/");
			strcat(path, fileName);
			int ret = remove(path);
			if (ret != 0)
			{
				char notiFail[100] = {0};
				strcat(notiFail, "ERROR:");
				strcat(notiFail, fileName);
				strcat(notiFail, " ");
				strcat(notiFail, "is not a file");
				initRemoveScreen(win, "Remove file screen", notiFail);
			}
			else
			{
				char notiSuccess[100] = {0};
				strcat(notiSuccess, "Delete");
				strcat(notiSuccess, " ");
				strcat(notiSuccess, fileName);
				strcat(notiSuccess, " ");
				strcat(notiSuccess, "successfully");
				mvwaddstr(win, 6, 4, notiSuccess);
			}
		}
	} while (1);
}
void initDownloadScreen(WINDOW *win, char title[], char noti[])
{
	wclear(win);
	box(win, 0, 0);
	mvwprintw(win, 0, 2, title);
	int key;
	char fileName[30] = {0};
	menuitem = 0;
	char path[100];
	mvwaddstr(win, 13, 2, "Press esc to go back menu");
	mvwaddstr(win, 2, 4, "File name to download:");
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
			if (temp != 0)
			{
				fileName[temp] = 0;
				temp--;
				wclear(win);
				box(win, 0, 0);
				mvwprintw(win, 0, 2, title);
				mvwaddstr(win, 13, 2, "Press esc to go back menu");
				mvwaddstr(win, 2, 4, "File name to download:");
				mvwaddstr(win, 4, 4, fileName);
				wrefresh(win);
			}
		}
		else if (key != '\n')
		{
			fileName[temp] = key;
			temp++;
			mvwaddstr(win, 4, 4, fileName);
		}
		else if (key == '\n')
		{
			strcpy(path, STORAGE);
			strcat(path, "/");
			strcat(path, fileName);
			if (access(path, F_OK) != -1)
			{
				char notiExist[30];
				strcat(notiExist, fileName);
				strcat(notiExist, " ");
				strcat(notiExist, "existed");
				mvwaddstr(win, 6, 4, notiExist);
				initDownloadScreen(win, "Download file screen", notiExist);
			}
			else
			{
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
				download_done();
				clock_gettime(CLOCK_MONOTONIC_RAW, &end);
				long duration = (end.tv_sec - begin.tv_sec) * 1e3 + (end.tv_nsec - begin.tv_nsec) / 1e6;
				// printf("Elapsed time: %ld miliseconds\n", duration);
				char notiTime[100] = {0};
				strcat(notiTime, "Elapsed time: ");
				strcat(notiTime, itoa(duration));
				strcat(notiTime, " miliseconds");
				mvwaddstr(win, 8, 4, notiTime);
			}
		}
	} while (1);
}
int main(int argc, char **argv)
{

	stream = fopen("history.log", "a+");
	if (argc != 3)
	{
		printf("Usage: ./peer <host> <port>\n");
		exit(1);
	}
	/* ignore EPIPE to avoid crashing when writing to a closed socket */
	signal(SIGPIPE, SIG_IGN);

	int dataSock = socket(AF_INET, SOCK_STREAM, 0);
	if (dataSock < 0)
	{
		print_error("create socket");
		exit(1);
	}
	struct sockaddr_in dataSockIn;
	bzero(&dataSockIn, sizeof(dataSockIn));
	dataSockIn.sin_addr.s_addr = htonl(INADDR_ANY);
	dataSockIn.sin_family = AF_INET;
	dataSockIn.sin_port = 0; // let the OS chooses the port

	if ((bind(dataSock, (struct sockaddr *)&dataSockIn, sizeof(dataSockIn))) < 0)
	{
		print_error("bind socket");
		exit(1);
	}

	if ((listen(dataSock, 20)) < 0)
	{
		print_error("listen on socket");
		exit(1);
	}

	struct sockaddr_in real_sock_in; // use to store the listening port
	socklen_t socklen = sizeof(real_sock_in);

	/* get the current socket info (include listening port) */
	if ((getsockname(dataSock, (struct sockaddr *)&real_sock_in, &socklen) < 0))
	{
		print_error("getsockname");
		exit(1);
	}

	/* port used to listen for download_file_request */
	dataPort = real_sock_in.sin_port; // network byte order

	/* listen for download file request */
	pthread_t download_tid;
	int thr = pthread_create(&download_tid, NULL, waitForDownloadRequest, &dataSock);
	if (thr != 0)
	{
		print_error("New thread to handle download file request");
		exit(1);
	}

	connect_to_index_server(argv[1], atoi(argv[2]));

	printf("Listening for download_file_request at: 0.0.0.0:%u\n", dataPort);

	pthread_t tid;
	thr = pthread_create(&tid, NULL, &update_file_list, STORAGE);
	if (thr != 0)
	{
		print_error("New thread to update file list to the index server");
		exit(1);
	}
	initscr();
	noecho();
	curs_set(0);
	int yMax, xMax;
	getmaxyx(stdscr, yMax, xMax);
	win = newwin(16, 50, yMax / 5, xMax / 5);
	box(win, 0, 0);
	/* listen for and process responses from the index server */
	pthread_t process_response_tid;
	thr = pthread_create(&process_response_tid, NULL, &process_response, (void *)&servsock);
	if (thr != 0)
	{
		print_error("New thread to process response from the index server");
		close(servsock);
		exit(1);
	}
	while (menuitem != 4)
	{
		initMenu(win);
		if (menuitem == 0)
		{
			wrefresh(win);
			send_list_files_request();
			initScreen(win, "List File Screen");
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
						mvwaddstr(win, 2 + (item * 2), 12, dir->d_name);
						mvwaddstr(win, 2 + (item * 2), 6, itoa(item + 1));
						wrefresh(win);
						item++;
						h_f = 1;
					}
				}
				closedir(d);
			}
			if (!h_f)
				mvwaddstr(win, 2, 4, "No files in database");
			initScreen(win, "Availabel file screen");
		}
		else if (menuitem == 2)
		{
			initRemoveScreen(win, "Remove file screen", "");
		}
		else if (menuitem == 3)
		{
			initDownloadScreen(win, "Download file screen", "");
		}
	}
	echo(); // re-enable echo
	endwin();
	// return 0;
	char path[100];
	mkdir(tmp_dir, 0700);
	while (1)
	{
		char input[400];
		fgets(input, sizeof(input), stdin);

		char *command = strtok(input, " \n\t");

		if (command == NULL)
		{
			continue;
		}
		if (strcmp(command, "/ls") == 0)
		{
			send_list_files_request();
		}
		else if (strcmp(command, "/avail") == 0)
		{
			// printf("You have: ");
			int h_f = 0;
			DIR *d;
			struct dirent *dir;
			d = opendir(STORAGE);
			if (d)
			{
				while ((dir = readdir(d)) != NULL)
				{
					// Condition to check regular file.
					if (dir->d_type == DT_REG && dir->d_name[0] != '.')
					{
						printf("\'%s\' ", dir->d_name);
						h_f = 1;
					}
				}
				closedir(d);
			}
			if (!h_f)
				printf("no files in database.\n");
			else
				printf("\n");
		}
		else if (strcmp(command, "/rm") == 0)
		{
			char *filename = strtok(NULL, " \n\t");
			if (filename)
			{
				strcpy(path, STORAGE);
				strcat(path, "/");
				strcat(path, filename);
				int ret = remove(path);
				if (ret != 0)
				{
					print_error("remove file");
				}
				else
				{
					printf("Delete %s successfully\n", filename);
				}
			}
			else
			{
				printf("help: /rm <filename>\n");
			}
			fflush(stdout);
		}
		else if (strcmp(command, "/get") == 0)
		{
			char *filename = strtok(NULL, " \n\t");
			if (filename)
			{
				strcpy(path, STORAGE);
				strcat(path, "/");
				strcat(path, filename);
				if (access(path, F_OK) != -1)
				{
					fprintf(stdout, "\'%s\' existed\n", filename);
				}
				else
				{
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

					long duration = (end.tv_sec - begin.tv_sec) * 1e3 + (end.tv_nsec - begin.tv_nsec) / 1e6;
					printf("Elapsed time: %ld miliseconds\n", duration);
				}
			}
			else
			{
				printf("help: /get <filename>\n");
			}
			fflush(stdout);
		}
		else if (strcmp(command, "/help") == 0)
		{
			print_help();
			fflush(stdout);
		}
		else if (strcmp(command, EXIT_CMD) == 0)
		{
			return 0;
		}
		else
		{
			print_help();
			fflush(stdout);
		}
	}
	return 0;
}

void print_help()
{
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