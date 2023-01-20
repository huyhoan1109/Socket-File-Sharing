#include <stdio.h>
#include <ctype.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdlib.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "sockio.h"

/** readBytes - read a number of bytes from the socket buffer.
 * Try to read exactly the number of bytes which you expect to read at one.
 * @sock: File descriptor of the socket.
 * @buf: Buffer to write output data in.
 * @nbytes: Number of bytes which you expect to read.
 *
 * Return: The number of actual read bytes if there is no error,
 *			otherwise return non-positive value (returned value of read() func)
 */
int readBytes(int sock, void* buf, unsigned int nbytes){
	unsigned int read_bytes = 0;
	unsigned int total = 0;
	while (1){
		read_bytes = read(sock, buf, nbytes);
		if (read_bytes <= 0)
			//return error code
			return read_bytes;
		total += read_bytes;
		if (read_bytes >= nbytes)
			break;
		nbytes -= read_bytes;
		buf += read_bytes;
	}
	return total;
}

/** writeBytes - write a number of bytes to the socket buffer.
 * Try to write exactly the number of bytes which you expect to write at one.
 * @sock: File descriptor of the socket.
 * @buf: Buffer to read input data from.
 * @nbytes: Number of bytes which you expect to write.
 *
 * Return: The number of actual wrote bytes if there is no error,
 *			otherwise return non-positive value (returned value of write() func)
 */
int writeBytes(int sock, void* buf, unsigned int nbytes){
	unsigned int wrote_bytes = 0;
	unsigned int total = 0;
	while (1) {
		wrote_bytes = write(sock, buf, nbytes);
		if (wrote_bytes <= 0){
			//return error code
			return wrote_bytes;
		}
		total += wrote_bytes;
		if (wrote_bytes >= nbytes) break;
		nbytes -= wrote_bytes;
		buf += wrote_bytes;
	}	
	return total;
}