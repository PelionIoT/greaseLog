/**
 * @file pty_test.cc
 *
 * @date Apr 12, 2015
 * @author ed
 * @description just a test
 */


#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#define _XOPEN_SOURCE
#include <stdlib.h>

int main() {
	int fdm, fds;
	char *slavename;
//	extern char *ptsname();
	fdm = open("/dev/ptmx", O_RDWR);  /* open master */
	grantpt(fdm);                     /* change permission of slave */
	unlockpt(fdm);                    /* unlock slave */
	slavename = ptsname(fdm);      	   /* get name of slave */
	printf("slave: %s\n",slavename);
	dprintf(fdm,"Hello 1\n");
	sleep(10);
	dprintf(fdm,"Hello 2\n");
	sleep(5);
	close(fdm);
	//	fds = open(slavename, O_RDWR);    /* open slave */
//	if (fork() == 0) {
//	    close(fdm);                   /* close master */
//	    ioctl(fds, I_PUSH, "ptem");   /* push ptem */
//	    ioctl(fds, I_PUSH, "ldterm"); /* push ldterm */
//	    exec( /* some program */ );
//	}
//	close(fds);                       /* close slave */
//	ioctl(fdm, I_PUSH, "pckt");       /* push pckt */0

	return 0;
}
