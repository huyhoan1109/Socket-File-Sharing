#ifndef _LIST_FILES_REQUEST_H_
#define _LIST_FILES_REQUEST_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "../../socket/utils/common.h"
#include "../../socket/utils/sockio.h"
#include "../../socket/utils/LinkedList.h"

void send_list_files_request();
void process_list_files_response(char *message);

#endif
