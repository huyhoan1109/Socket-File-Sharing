#ifndef _DOWNLOAD_FILE_REQUEST_H_
#define _DOWNLOAD_FILE_REQUEST_H_

#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <ncurses.h>
#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "../../socket/utils/LinkedList.h"

#define MINIMUM_SEGMENT_SIZE	81920	//80KB

extern int n_threads;
extern uint32_t totalsize;
extern const char tmp_dir[];
extern int download_file_done;

extern pthread_mutex_t lock_user;
extern pthread_cond_t cond_n_threads;
extern pthread_mutex_t lock_n_threads;
extern pthread_cond_t cond_segment_list;
extern pthread_mutex_t lock_segment_list;

extern struct LinkedList *segment_list;

struct DownloadInfo{
	struct DataHost dthost;
	uint8_t seq_no;
};

void* download_file(void *arg);

int download_done();

#endif