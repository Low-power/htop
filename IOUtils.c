/*
htop - IOUtils.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include <sys/types.h>
}*/

#include <unistd.h>
#include <errno.h>

// Read some bytes. Retry on EINTR and when we don't get as many bytes as we requested.
ssize_t xread(int fd, void *buffer, size_t count) {
	char *p = buffer;
	do {
		ssize_t res = read(fd, p, count);
		if (res == -1) {
			if(errno == EINTR) continue;
			return -1;
		}
		if (count == 0 || res == 0) return p - (char *)buffer;
		p += res;
		count -= res;
	} while(count > 0);
	return p - (char *)buffer;
}
