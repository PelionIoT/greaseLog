/*
 * send-multiple-test.c
 *
 *  Created on: Sep 3, 2015
 *      Author: ed
 * (c) 2015, WigWag Inc.
 */

#include <stdio.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#define GREASE_DEBUG_MODE 1
#include "../../grease_client.h"

#define ERROR_OUT(s,...) fprintf(stderr, "**ERROR** " s, ##__VA_ARGS__ )//#define ERROR_PERROR(s,...) fprintf(stderr, "*****ERROR***** " s, ##__VA_ARGS__ );
#define ERROR_PERROR(s,...) { perror("**ERROR** [ %s ] " s, ##__VA_ARGS__ ); }
#define DBG_OUT(s,...) fprintf(stderr, "**DEBUG** " s, ##__VA_ARGS__ )
#define IF_DBG( x ) { x }


int main(int argc, char *argv[]) {


	if(argc < 2) {
		ERROR_OUT("Need args. %s [socket-path]\n", argv[0]);
		exit(1);
	}

	char *data = "blah blah blah";


	fd_set readfds;

	char dump[5];

	const int nbuffers = 10;
	char *raw_buffer[nbuffers];
	struct iovec iov[nbuffers];
	struct msghdr message;

	socklen_t optsize;

	// using MSG_DONTWAIT instead
//			int flags = fcntl(socket_fd, F_GETFL, 0);
//			if (flags < 0) {
//				ERROR_PERROR("UnixDgramSink: Error getting socket flags\n",errno);
//			}
//			flags = flags|O_NONBLOCK;
//			if(fcntl(socket_fd, F_SETFL, flags) < 0) {
//				ERROR_PERROR("UnixDgramSink: Error setting socket non-blocking flags\n",errno);
//			}
	int recv_cnt = 0;
	int err_cnt = 0;
	char *path = argv[1];
	struct sockaddr_un sink_dgram_addr;


	int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if(socket_fd < 0) {
		ERROR_PERROR("UnixDgramSink: Failed to create SOCK_DGRAM socket.\n");
	} else {
		memset(&sink_dgram_addr,0,sizeof(sink_dgram_addr));
			sink_dgram_addr.sun_family = AF_UNIX;
//			unlink(path); // get rid of it if it already exists
			strcpy(sink_dgram_addr.sun_path,path);
//			if(bind(socket_fd, (const struct sockaddr *) &sink_dgram_addr, sizeof(sink_dgram_addr)) < 0) {
//				ERROR_PERROR("UnixDgramSink: Failed to bind() SOCK_DGRAM socket.\n");
//				close(socket_fd);
//				exit(1);
//			}
	}

	// discover socket max recieve size (this will be the max for a non-fragmented log message
	int rcv_buf_size = 65536;
	setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &rcv_buf_size, sizeof(int));

	getsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &rcv_buf_size, &optsize);
	// http://stackoverflow.com/questions/10063497/why-changing-value-of-so-rcvbuf-doesnt-work
	if(rcv_buf_size < 100) {
		ERROR_OUT("UnixDgramSink: Failed to start reader thread - SO_RCVBUF too small\n");
		exit(1);
	} else {
		DBG_OUT("UnixDgramSink: SO_RCVBUF is %d\n", rcv_buf_size);
	}




	message.msg_name=&sink_dgram_addr;
	message.msg_namelen=sizeof(struct sockaddr_un);
	message.msg_iov=iov;
	message.msg_iovlen=nbuffers;
	message.msg_control=NULL;
	message.msg_controllen=0;
	message.msg_flags = 0;



		// ////////////////////////////////
		// Business work of sink.........
		// ////////////////////////////////
//		message.msg_name=&sink_dgram_addr;
//		message.msg_namelen=sizeof(struct sockaddr_un);
//		message.msg_iov=iov;
//		message.msg_iovlen=nbuffers;
//		message.msg_control=NULL;
//		message.msg_controllen=0;
//		message.msg_flags = 0;

//		for(int n=0;n<nbuffers;n++) {
//			raw_buffer[n] = (char *) malloc(rcv_buf_size);
//			memset(raw_buffer[n],0,rcv_buf_size);
//			strcpy(raw_buffer[n],data);
//			iov[n].iov_base = raw_buffer[n];
//			iov[n].iov_len = strlen(data);
//		}
//
//		if((recv_cnt = sendmsg(socket_fd, &message, 0)) < 0) {
////			if(errno != EAGAIN || errno != EWOULDBLOCK) {
//				ERROR_PERROR("UnixDgramSink: Error on sendmsg() \n");
//				err_cnt++;
////			}
//		} else {
//			DBG_OUT("Sent %d byte\n", recv_cnt);
//		}

//		DBG_OUT("recv_cnt = %d\n",recv_cnt);
//		DBG_OUT("msg_iovlen = %d\n", message.msg_iovlen);
//		for(int n=0;n<nbuffers;n++) {
//			DBG_OUT("msg_iov[%d].iov_len = %d\n", n, iov[n].iov_len);
//			DBG_OUT("iov.base[%d] = %s\n",n, iov[n].iov_base);
//
//		}

		INIT_GLOG;
		for(int n=0;n<100;n++) {
			GLOG("hello there. hello there. hello there. hello there. 仐传 1");
			GLOG_DEBUG("hello there. hello there. hello there. hello there. 仐传 2");
			GLOG_ERROR("hello there. hello there. hello there. hello there. 仐传 3");
		}

}


/**
 * BUILD:  gcc send-multiple-test.c ../../grease_client.c -std=c99 -o send-multiple-test
 */
