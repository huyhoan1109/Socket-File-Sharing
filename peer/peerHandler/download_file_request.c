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

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "list_hosts_request.h"
#include "download_file_request.h"


struct LinkedList *segment_list = NULL;
pthread_mutex_t lock_segment_list = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_segment_list = PTHREAD_COND_INITIALIZER;
int n_threads = 0;
pthread_mutex_t lock_n_threads = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_n_threads = PTHREAD_COND_INITIALIZER;
const char tmp_dir[] = "./.temp/";

static void display_segment_list(){
	char long_delim[79];
	memset(long_delim, '#', 78);
	long_delim[78] = 0;

	char short_delim[33];
	memset(short_delim, '#', 32);
	short_delim[32] = 0;

	fprintf(stderr, "%s" BLUE " segment list " COLOR_RESET "%s\n", short_delim, short_delim);
	int max_width = 10;
	struct Node *it = segment_list->head;
	int i = 0;
	for (; it != NULL; it = it->next){
		struct Segment *seg = (struct Segment*)(it->data);
		i++;
		fprintf(stderr, "seg %-3d: offset=%*u, n_bytes=%*u, seg_size=%*u, downloading=%d\n",
				i, max_width, seg->offset, max_width, seg->n_bytes, 
				max_width, seg->seg_size, seg->downloading);
	}
	fprintf(stderr, "%s\n", long_delim);
}

static void prepare_segment(struct Segment *seg, 
							uint32_t offset, 
							uint32_t size, 
							uint32_t n_bytes, 
							uint32_t downloading)
{
	seg->offset = offset;
	seg->seg_size = size;
	seg->n_bytes = n_bytes;
	seg->downloading = downloading;
	pthread_mutex_init(&seg->lock_seg, NULL);
}

static struct Segment* create_segment(uint8_t sequence){
	pthread_mutex_lock(&lock_the_file);
	if (sequence != seq_no){
		pthread_mutex_unlock(&lock_the_file);
		return NULL;
	}
	uint32_t filesize = the_file->filesize;
	pthread_mutex_unlock(&lock_the_file);
	struct Segment *segment = NULL;
	
	pthread_mutex_lock(&lock_segment_list);
	fprintf(stream, "[create_segment] wait to access segment_list\n");
	pthread_cleanup_push(mutex_unlock, &lock_segment_list);
	if (segment_list->n_nodes == 0){
		struct Segment *seg = malloc(sizeof(struct Segment));
		struct Node *seg_node = newNode(seg, SEGMENT_TYPE);
		free(seg);
		seg = (struct Segment*)(seg_node->data);
		segment = seg;
		prepare_segment(seg, 0, filesize, 0, 1);
		push(segment_list, seg_node);

		seg = malloc(sizeof(struct Segment));
		seg_node = newNode(seg, SEGMENT_TYPE);
		free(seg);
		seg = (struct Segment*)(seg_node->data);
		prepare_segment(seg, filesize, 0, 0, 0);
		push(segment_list, seg_node);
	} else {
		struct Node *it = segment_list->head;
		for (; it != segment_list->tail; it = it->next){
			struct Segment *seg1 = (struct Segment*)(it->data);

			pthread_mutex_lock(&seg1->lock_seg);
			if (seg1->downloading == 0 && seg1->n_bytes < seg1->seg_size){
				if (seg1->n_bytes == 0){
					segment = seg1;
					segment->downloading = 1;
				} else {
					struct Node *seg_node = newNode(seg1, SEGMENT_TYPE);
					segment = (struct Segment*)(seg_node->data);
					prepare_segment(segment, seg1->offset + seg1->n_bytes, 
									seg1->seg_size - seg1->n_bytes,
									0, 1);
					pthread_mutex_init(&segment->lock_seg, NULL);
					seg1->seg_size = seg1->n_bytes;
					insertNode(segment_list, seg_node, it);
				}
				pthread_mutex_unlock(&seg1->lock_seg);
				break;
			}
			pthread_mutex_unlock(&seg1->lock_seg);

			struct Segment *seg2 = (struct Segment*)(it->next->data);
			pthread_mutex_lock(&seg1->lock_seg);
			pthread_mutex_lock(&seg2->lock_seg);
			uint32_t interval = seg2->offset - (seg1->offset + seg1->n_bytes);
			if (interval >= 2*MINIMUM_SEGMENT_SIZE){
				struct Node *seg_node = newNode(seg1, SEGMENT_TYPE);
				segment = (struct Segment*)(seg_node->data);
				//avoid being over range value of uint32_t
				uint64_t new_offset = ((uint64_t)seg2->offset
									+ (uint64_t)seg1->offset 
									+ (uint64_t)seg1->n_bytes)/2;
				prepare_segment(segment, new_offset, interval/2, 0, 1);
				insertNode(segment_list, seg_node, it);
				seg1->seg_size = segment->offset - seg1->offset;
			}
			pthread_mutex_unlock(&seg2->lock_seg);
			pthread_mutex_unlock(&seg1->lock_seg);
			if (interval >= 2*MINIMUM_SEGMENT_SIZE){
				break;
			}
		}
	}
	display_segment_list();
	pthread_cleanup_pop(0);
	pthread_mutex_unlock(&lock_segment_list);
	return segment;
}

static void terminate_thread(struct Segment *seg){
	if (seg){
		pthread_mutex_lock(&seg->lock_seg);
		fprintf(stream, "[terminate_thread] complete segment\n");
		seg->downloading = 0;
		pthread_mutex_unlock(&seg->lock_seg);
	}
	pthread_mutex_lock(&lock_n_threads);
	fprintf(stream, "[terminate_thread] decrease n_threads\n");
	n_threads --;
	pthread_cond_signal(&cond_n_threads);
	pthread_mutex_unlock(&lock_n_threads);
	fprintf(stream, "[terminate_thread] decreased n_threads\n");
	int ret = 100;
	pthread_exit(&ret);
}

static void handle_error(struct Segment *seg, char *addr_str, char *mess){
	char err_mess[256];
	sprintf(err_mess, "%s > %s", addr_str, mess);
	print_error(err_mess);
	terminate_thread(seg);
}

static int connect_peer(struct DataHost dthost, char *addr_str){
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

	int conn = connect(sockfd, (struct sockaddr*)&sin, sizeof(sin));
	if (conn < 0)
		return conn;

	//fprintf(stdout, "connected to %s\n", addr_str);
	return sockfd;
}

void* download_file(void *arg){
	fprintf(stream, "function download_file\n");
	//pthread_detach(pthread_self());
	struct DownloadInfo dinfo = *(struct DownloadInfo*)arg;
	free(arg);

	char addr_str[22];

	char buff[MAX_BUFF_SIZE];
	uint32_t n_write;
	
	while(1){
		struct Segment *segment = create_segment(dinfo.seq_no);
		if (segment == NULL){
			fprintf(stream, "no more segment need to be downloaded\n");
			pthread_mutex_lock(&lock_the_file);
			uint8_t current_seq = seq_no;
			pthread_mutex_unlock(&lock_the_file);

			/* Emitting this signal means that no more segments need to be downloaded.
			 * if dinfo.seq_no != current_seq:
			 *		Some threads terminated and emitted this signal, 
			 *		the system detected that the file has been downloaded successfully, 
			 *		so no need to emit signal any more */
			if (dinfo.seq_no == current_seq){
				fprintf(stream, "[download_file] emit signal\n");
				pthread_mutex_lock(&lock_segment_list);
				pthread_cond_signal(&cond_segment_list);
				pthread_mutex_unlock(&lock_segment_list);
			}
			fprintf(stream, "[download_file] terminate thread\n");
			terminate_thread(segment);
		}
		int sockfd = connect_peer(dinfo.dthost, addr_str);
		if (sockfd < 0){
			handle_error(segment, addr_str, "connect to download");
		}
		fprintf(stdout, YELLOW "%s > segment offset: %u" COLOR_RESET "\n", addr_str, segment->offset);

		/* send download file request */
		uint32_t n_bytes = 0;
		uint16_t filename_length = strlen(the_file->filename) + 1;
		filename_length = htons(filename_length);
		n_bytes = writeBytes(sockfd, &filename_length, sizeof(filename_length));
		if (n_bytes <= 0){
			handle_error(segment, addr_str, "send filename_length");
		}

		n_bytes = writeBytes(sockfd, the_file->filename, ntohs(filename_length));
		if (n_bytes <= 0){
			handle_error(segment, addr_str, "send filename");
		}

		uint32_t offset = htonl(segment->offset);
		n_bytes = writeBytes(sockfd, &offset, sizeof(offset));
		if (n_bytes <= 0){
			handle_error(segment, addr_str, "send offset");
		}

		/* receive file status message */
		uint8_t file_status;
		n_bytes = readBytes(sockfd, &file_status, sizeof(file_status));
		if (n_bytes <= 0){
			handle_error(segment, addr_str, "read file status");
		}
		
		if (file_status == FILE_NOT_FOUND){
			fprintf(stdout, "%s > file not found\n", addr_str);
			close(sockfd);
			terminate_thread(segment);
		} else if (file_status == OPENING_FILE_ERROR){
			fprintf(stdout, "%s > opening file error\n", addr_str);
			close(sockfd);
			terminate_thread(segment);
		} else if (file_status == READY_TO_SEND_DATA){
			/* open file to save data */
			char full_name[400];
			strcpy(full_name, tmp_dir);
			strcat(full_name, the_file->filename);
			int filefd = open(full_name, O_WRONLY | O_CREAT, 0664);
			if (filefd < 0){
				print_error("[download_file] open file to save data");
				terminate_thread(segment);
			}


			//fprintf(stream, "[download_file] lseek to %u\n", segment->offset);
			lseek(filefd, segment->offset, SEEK_SET);
			//fprintf(stream, "[download_file] pos after lseeking: %u\n", pos);
			//FILE *file = fopen(full_name, "wb");
			//if (file == NULL){
			//	fprintf(stderr, "%s > [download_file] cannot open file", the_file->filename);
			//	terminate_thread(segment);
			//}
			//fprintf(stream, "[download_file] fseeko to %u\n", segment->offset);
			//fseeko(file, segment->offset, SEEK_SET);
			//fprintf(stream, "[download_file] position after fseeking: %u\n", (uint32_t)ftello(file));

			while (1){
				n_bytes = read(sockfd, buff, sizeof(buff));
				if (n_bytes <= 0){
					close(filefd);
					//fclose(file);
					//fclose(bak);
					print_error("[download_file] read data");
					pthread_mutex_lock(&segment->lock_seg);
					segment->downloading = 0;
					pthread_mutex_unlock(&segment->lock_seg);
					break;
				}
				pthread_mutex_lock(&segment->lock_seg);
				uint32_t remain = segment->seg_size - segment->n_bytes;
				n_bytes = (remain < n_bytes) ? remain : n_bytes;
				n_write = write(filefd, buff, n_bytes);
				//fwrite(buff, 1, n_bytes, bak);
				segment->n_bytes += n_write;
				if (n_write < 0){
					/* error */
					//fflush(file);
					//fclose(file);
					//fclose(bak);
					close(filefd);
					close(sockfd);
					pthread_mutex_unlock(&segment->lock_seg);
					handle_error(segment, addr_str, "write to file");
				}
				if (segment->n_bytes >= segment->seg_size){
					/* got enough data */
					//fclose(file);
					//fclose(bak);
					close(filefd);
					segment->downloading = 0;
					fprintf(stdout, GREEN "%s > segment offset %u done, n_bytes=%u" COLOR_RESET "\n" , 
							addr_str, segment->offset, segment->n_bytes);
					pthread_mutex_unlock(&segment->lock_seg);
					break;
				}
				pthread_mutex_unlock(&segment->lock_seg);
			}
			close(sockfd);
		}
	}
	return NULL;
}

int download_done(){
	/* check if the file has been downloaded successfully */
	int file_not_found = 0;
	while(1){
		pthread_cond_wait(&cond_segment_list, &lock_segment_list);
		struct Node *it = segment_list->head;
		if (!it){
			file_not_found = 1;
			pthread_mutex_unlock(&lock_segment_list);
			break;
		}
		int done = 1;
		for (; it != segment_list->tail; it = it->next){
			struct Segment *seg = (struct Segment*)(it->data);
			if (seg->downloading || seg->n_bytes < seg->seg_size){
				done = 0;
				break;
			}
		}
		pthread_mutex_unlock(&lock_segment_list);
		if (done)
			break;
	}

	//while(1){
	//	pthread_cond_wait(&cond_n_threads, &lock_n_threads);
	//	fprintf(stream, "n_threads: %d\n", n_threads);
	//	if (n_threads <= 0){
	//		n_threads = -1;
	//		pthread_mutex_unlock(&lock_n_threads);
	//		break;
	//	}
	//	pthread_mutex_unlock(&lock_n_threads);
	//}

	struct timespec req;
	req.tv_nsec = 100000000;
	req.tv_sec = 0;
	while(1){
		pthread_mutex_lock(&lock_n_threads);
		if (n_threads <= 0){
			n_threads = -1;
			pthread_mutex_unlock(&lock_n_threads);
			break;
		}
		pthread_mutex_unlock(&lock_n_threads);
		nanosleep(&req, NULL);
	}

	pthread_mutex_lock(&lock_the_file);
	if (file_not_found){
		fprintf(stdout, "index server > \'%s\' not found\n", the_file->filename);
	} else {
		fprintf(stdout, "received \'%s\' successfully\n", the_file->filename);
	}
	pthread_mutex_unlock(&lock_the_file);

	/* send "done" message to index server */
	send_list_hosts_request("");

	pthread_mutex_lock(&lock_the_file);
	/* increase sequence number */
	fprintf(stream, "[download_done] increase sequence number\n");
	seq_no ++;

	/* move the file to the current directory */
	fprintf(stream, "[download_done] move the file from .temp/ to %s\n", STORAGE);
	char full_name[400];
	strcpy(full_name, tmp_dir);
	strcat(full_name, the_file->filename);
	char *path = calloc(100, sizeof(char));
	strcpy(path, STORAGE);
	strcat(path, "/");
	strcat(path, the_file->filename);
	rename(full_name, path);

	/* destruct the_file and segment_list */
	fprintf(stream, "[download_done] destruct the_file\n");
	if (!the_file){
		fprintf(stream, "thefile == NULL\n");
	}
	destructLinkedList(the_file->host_list);
	free(path);
	free(the_file);
	fprintf(stream, "assign the_file=NULL\n");
	the_file = NULL;
	pthread_mutex_unlock(&lock_the_file);

	fprintf(stream, "[download_done] destruct segment_list\n");
	pthread_mutex_lock(&lock_segment_list);
	destructLinkedList(segment_list);
	segment_list = NULL;
	pthread_mutex_unlock(&lock_segment_list);	
	return 1;
}