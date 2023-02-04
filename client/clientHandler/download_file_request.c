#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <ncurses.h>
#include <unistd.h>

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "list_hosts_request.h"
#include "download_file_request.h"
#include "connect_index_server.h"

struct LinkedList *segment_list = NULL;
pthread_mutex_t lock_segment_list = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_segment_list = PTHREAD_COND_INITIALIZER;
int n_threads = 0;
pthread_mutex_t lock_n_threads = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_n_threads = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock_user = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_user = PTHREAD_COND_INITIALIZER;

const char tmp_dir[] = "./.temp/";

static void prepare_segment(struct Segment *seg, uint32_t offset, uint32_t size, uint32_t n_bytes, uint32_t downloading)
{
	seg->offset = offset;
	seg->seg_size = size;
	seg->n_bytes = n_bytes;
	seg->downloading = downloading;
	pthread_mutex_init(&seg->lock_seg, NULL);
}

static struct Segment *create_segment(uint8_t sequence) {
	pthread_mutex_lock(&lock_the_file);
	if (sequence != seq_no) {
		// Wrong sequence info
		pthread_mutex_unlock(&lock_the_file);
		return NULL;
	}
	uint32_t filesize = the_file->filesize;
	pthread_mutex_unlock(&lock_the_file);
	
	struct Segment *segment = NULL;
	pthread_mutex_lock(&lock_segment_list);
	
	pthread_cleanup_push(mutex_unlock, &lock_segment_list);
	if (segment_list->n_nodes == 0)
	{
		struct Segment *seg = calloc(1, sizeof(struct Segment));
		struct Node *seg_node = newNode(seg, SEGMENT_TYPE);
		free(seg);
		seg = (struct Segment *)(seg_node->data);
		segment = seg;
		prepare_segment(seg, 0, filesize, 0, 1);
		push(segment_list, seg_node);

		seg = calloc(1, sizeof(struct Segment));
		seg_node = newNode(seg, SEGMENT_TYPE);
		free(seg);
		seg = (struct Segment *)(seg_node->data);
		prepare_segment(seg, filesize, 0, 0, 0);
		push(segment_list, seg_node);
	}
	else
	{
		struct Node *it = segment_list->head;
		for (; it != segment_list->tail; it = it->next)
		{
			struct Segment *seg1 = (struct Segment *)(it->data);
			pthread_mutex_lock(&seg1->lock_seg);
			if (seg1->downloading == 0 && seg1->n_bytes < seg1->seg_size){
				if (seg1->n_bytes == 0) {
					segment = seg1;
					segment->downloading = 1;
				} else {
					struct Node *seg_node = newNode(seg1, SEGMENT_TYPE);
					segment = (struct Segment *)(seg_node->data);
					prepare_segment(segment, seg1->offset + seg1->n_bytes, seg1->seg_size - seg1->n_bytes, 0, 1);
					pthread_mutex_init(&segment->lock_seg, NULL);
					seg1->seg_size = seg1->n_bytes;
					insertNode(segment_list, seg_node, it);
				}
				pthread_mutex_unlock(&seg1->lock_seg);
				break;
			}
			pthread_mutex_unlock(&seg1->lock_seg);

			struct Segment *seg2 = (struct Segment *)(it->next->data);
			pthread_mutex_lock(&seg1->lock_seg);
			pthread_mutex_lock(&seg2->lock_seg);
			uint32_t interval = seg2->offset - (seg1->offset + seg1->n_bytes);
			
			if (interval >= 2 * MINIMUM_SEGMENT_SIZE){
				struct Node *seg_node = newNode(seg1, SEGMENT_TYPE);
				segment = (struct Segment *)(seg_node->data);		
				uint64_t new_offset = ((uint64_t)seg2->offset + (uint64_t)seg1->offset + (uint64_t)seg1->n_bytes) / 2;
				prepare_segment(segment, new_offset, interval / 2, 0, 1);
				insertNode(segment_list, seg_node, it);
				seg1->seg_size = segment->offset - seg1->offset;
			}
			
			pthread_mutex_unlock(&seg2->lock_seg);
			pthread_mutex_unlock(&seg1->lock_seg);
			if (interval >= 2 * MINIMUM_SEGMENT_SIZE){
				break;
			}
		}
	}
	pthread_cleanup_pop(0);
	pthread_mutex_unlock(&lock_segment_list);
	return segment;
}

static void terminate_thread(struct Segment *seg)
{
	if (seg){
		pthread_mutex_lock(&seg->lock_seg);
		seg->downloading = 0;
		pthread_mutex_unlock(&seg->lock_seg);
	}
	pthread_mutex_lock(&lock_n_threads);
	n_threads--;
	pthread_cond_signal(&cond_n_threads);
	pthread_mutex_unlock(&lock_n_threads);
	int ret = 100;
	pthread_exit(&ret);
}

static int connect_peer(struct DataHost dthost, char *addr_str)
{
	struct sockaddr_in sin;
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(dthost.ip_addr);
	sin.sin_port = htons(dthost.port);

	sprintf(addr_str, "%s:%u", inet_ntoa(sin.sin_addr), dthost.port);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0){
		return sockfd;
	}
	int conn = connect(sockfd, (struct sockaddr *)&sin, sizeof(sin));
	if (conn < 0) return conn;
	return sockfd;
}

void *download_file(void *arg)
{
	//pthread_detach(pthread_self());
	struct DownloadInfo dinfo = *(struct DownloadInfo *)arg;
	free(arg);

	char addr_str[22];

	char buff[MAX_BUFF_SIZE];
	uint32_t n_write;
	while (1) {
		struct Segment *segment = create_segment(dinfo.seq_no);
		if (segment == NULL){
			pthread_mutex_lock(&lock_the_file);
			uint8_t current_seq = seq_no;
			pthread_mutex_unlock(&lock_the_file);

			if (dinfo.seq_no == current_seq) {
				pthread_mutex_lock(&lock_segment_list);
				pthread_cond_signal(&cond_segment_list);
				pthread_mutex_unlock(&lock_segment_list);
			}
			terminate_thread(segment);
		}

		pthread_mutex_lock(&lock_user);
		int sockfd = connect_peer(dinfo.dthost, addr_str);
		if (sockfd < 0) {
			terminate_thread(segment);
		}
		
		/* send download file request */
		char *message = calloc(MAX_BUFF_SIZE, sizeof(char));
		strcpy(message, itoa(GET_FILE_REQUEST));
		strcat(message, MESSAGE_DIVIDER);
		strcat(message, the_file->filename);
		strcat(message, MESSAGE_DIVIDER);
		strcat(message, itoa(segment->offset));
		writeBytes(sockfd, message, MAX_BUFF_SIZE);
		free(message);

		char full_name[100];
		strcpy(full_name, tmp_dir);
		strcat(full_name, the_file->filename);
		int filefd = open(full_name, O_WRONLY | O_CREAT | O_TRUNC, 0664);
		if (filefd < 0) {
			close(filefd);
			perror("file descriptor");
			terminate_thread(segment); // Error file descriptor
		}


		/* receive file status message */
		char status_char[5];
		int n_bytes = read(sockfd, status_char, sizeof(status_char));
		if (n_bytes <= 0) {
			terminate_thread(segment);
		}
		uint8_t file_status = (uint8_t) atol(status_char);
		mkdir(tmp_dir, 0666);
		switch (file_status) {
			case FILE_NOT_FOUND:
			case OPENING_FILE_ERROR:
				terminate_thread(segment);
				break;
			case READY_TO_SEND_DATA:
				lseek(filefd, segment->offset, SEEK_SET);
				int i = 0;
				while (1){
					n_bytes = read(sockfd, buff, sizeof(buff));
					if (n_bytes <= 0){
						close(filefd);
						pthread_mutex_lock(&segment->lock_seg);
						segment->downloading = 0;
						pthread_mutex_unlock(&segment->lock_seg);
						break;
					}
					pthread_mutex_lock(&segment->lock_seg);
					uint32_t remain = segment->seg_size - segment->n_bytes;
					n_bytes = (remain < n_bytes) ? remain : n_bytes;
					n_write = write(filefd, buff, n_bytes);
					segment->n_bytes += n_write;
					
					drawProgress(win, 4, 4, segment->n_bytes, segment->seg_size);
					
					if (n_write < 0){
						close(filefd);
						pthread_mutex_unlock(&segment->lock_seg);
						terminate_thread(segment);
					}
					if (segment->n_bytes >= segment->seg_size){
						/* got enough data */
						close(filefd);
						segment->downloading = 0;
						pthread_mutex_unlock(&segment->lock_seg);
						break;
					}
					i += 1;
					pthread_mutex_unlock(&segment->lock_seg);
				}
				break;
		}
		close(sockfd);
		pthread_mutex_unlock(&lock_user);
	}
	return NULL;
}

int download_done()
{
	/* check if the file has been downloaded successfully */
	int file_not_found = 0;
	while (1){
		pthread_cond_wait(&cond_segment_list, &lock_segment_list);
		struct Node *it = segment_list->head;
		if (!it){
			file_not_found = 1;
			pthread_mutex_unlock(&lock_segment_list);
			break;
		}
		int done = 1;
		for (; it != segment_list->tail; it = it->next){
			struct Segment *seg = (struct Segment *)(it->data);
			if (seg->downloading || seg->n_bytes < seg->seg_size){
				done = 0;
				break;
			}
		}
		pthread_mutex_unlock(&lock_segment_list);
		if (done) break;
	}
	struct timespec req;
	req.tv_nsec = 100000000;
	req.tv_sec = 0;
	while (1) {
		pthread_mutex_lock(&lock_n_threads);
		if (n_threads <= 0) {
			n_threads = -1;
			pthread_mutex_unlock(&lock_n_threads);
			break;
		}
		pthread_mutex_unlock(&lock_n_threads);
		nanosleep(&req, NULL);
	}

	pthread_mutex_lock(&lock_the_file);
	if (file_not_found){
		mvwprintw(win, 4, 4, "%s not found", the_file->filename);
	} else {
		mvwprintw(win, 6, 4, "Received %s successfully", the_file->filename);
	}
	pthread_mutex_unlock(&lock_the_file);


	pthread_mutex_lock(&lock_the_file);
	/* increase sequence number */
	seq_no++;

	/* move the file to the current directory */
	char full_name[400];
	strcpy(full_name, tmp_dir);
	strcat(full_name, the_file->filename);
	
	char *path = calloc(100, sizeof(char));
	strcpy(path, STORAGE);
	strcat(path, "/");
	strcat(path, the_file->filename);
	rename(full_name, path);

	destructLinkedList(the_file->host_list);
	free(path);
	free(the_file);
	the_file = NULL;
	pthread_mutex_unlock(&lock_the_file);

	pthread_mutex_lock(&lock_segment_list);
	destructLinkedList(segment_list);
	segment_list = NULL;
	pthread_mutex_unlock(&lock_segment_list);
	return 1;
}