/*
 * send-multiple-test.c
 *
 *  Created on: Sep 3, 2015
 *      Author: ed
 * (c) 2015, WigWag Inc.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

//#define GREASE_DEBUG_MODE 1
#include "grease_client.h"

#define ERROR_OUT(s,...) fprintf(stderr, "**ERROR** " s, ##__VA_ARGS__ )//#define ERROR_PERROR(s,...) fprintf(stderr, "*****ERROR***** " s, ##__VA_ARGS__ );
#define ERROR_PERROR(s,...) { perror("**ERROR** [ %s ] " s, ##__VA_ARGS__ ); }
#define DBG_OUT(s,...) fprintf(stderr, "**DEBUG** " s, ##__VA_ARGS__ )
#define IF_DBG( x ) { x }


void bye(int e) {
	SHUTDOWN_GLOG;
	exit(e);
}

int main(int argc, char *argv[]) {
	if(argc < 2) {
		exit(1);
	}

	int n = 1;

	if(argv[n][0] == '-' && argv[n][1] == '-') {
		if(!strcmp(argv[1]+2,"help")) {
			printf("Usage: grease_echo [--check] [--[LEVEL]] \"string here\"\n"
				   "            --check     will check to see if logger is live.\n"
				   "LEVELs:     --error\n"
				   "            --warn\n"
				   "            --success\n"
				   "            --info\n"
				   "            --debug\n"
				   "            --debug2\n"
				   "            --debug3\n"
				   "            --user1\n"
				   "            --user2\n");
			exit(1);
		}
		if(!strcmp(argv[1]+2,"check")) {
			if(grease_initLogger() != GREASE_OK) {
				printf("Grease not running.\n");
				bye(1);
			} else {
				printf("Grease running & OK.\n");
				bye(0);
			}
		}
		if(grease_fastInitLogger() != GREASE_OK) {
			fprintf(stderr,"    Error: Grease not running.\n");
	//		exit(1);
		}
		if(argc > 2 && argv[2][0] != '\0') {
			if(!strcmp(argv[1]+2,"info")) {
				GLOG_INFO(argv[2]);
				bye(0);
			} else
			if(!strcmp(argv[1]+2,"error")) {
				GLOG_ERROR(argv[2]);
				bye(0);
			} else
			if(!strcmp(argv[1]+2,"warn")) {
				GLOG_WARN(argv[2]);
				bye(0);
			} else
			if(!strcmp(argv[1]+2,"success")) {
				GLOG_SUCCESS(argv[2]);
				bye(0);
			} else
			if(!strcmp(argv[1]+2,"debug")) {
				GLOG_DEBUG(argv[2]);
				bye(0);
			} else
			if(!strcmp(argv[1]+2,"debug2")) {
				GLOG_DEBUG2(argv[2]);
				bye(0);
			} else
			if(!strcmp(argv[1]+2,"debug3")) {
				GLOG_DEBUG3(argv[2]);
				bye(0);
			} else
			if(!strcmp(argv[1]+2,"user1")) {
				GLOG_USER1(argv[2]);
				bye(0);
			} else
			if(!strcmp(argv[1]+2,"user2")) {
				GLOG_USER2(argv[2]);
				bye(0);
			} else {
				fprintf(stderr,"grease_echo: Unknown LEVEL.\n");
				GLOG(argv[2]);
				bye(1);
			}
		}
	} else {
		if(argv[1][0] != '\0') {
			GLOG(argv[1]);
		}
		bye(0);
	}

	bye(1);
	return 0;
}


/**
 * BUILD:  gcc grease_echo.c  grease_client.c -std=c99 -o grease_echo -ldl
 */
