/*
   Copyright (C) 2000-2015  Ulric Eriksson <ulric@siag.nu>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.
*/

#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
//#include <arpa/inet.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

#include "config.h"

static void error(char *fmt, ...)
{
	char b[4096];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(b, sizeof b, fmt, ap);
	fprintf(stderr, "%s\n", b);
	va_end(ap);
	exit(EXIT_FAILURE);
}

static void alarm_handler(int dummy)
{
	;
}

static void usage(void)
{
	printf("usage: penctl host:port command\n");
	exit(0);
}

static int open_unix_socket(char *path)
{
	int n, fd;
	struct sockaddr_un serv_addr;

	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) error("error opening socket");
	memset(&serv_addr, 0, sizeof serv_addr);
	serv_addr.sun_family = AF_UNIX;
	snprintf(serv_addr.sun_path, sizeof serv_addr.sun_path, "%s", path);
	n = connect(fd, (struct sockaddr *)&serv_addr, sizeof serv_addr);
	if (n == -1) {
		error("error connecting to server");
	}
	return fd;
}

static int open_socket(char *addr, char *port)
{
	int fd = -1;
#if 0
	struct sockaddr_in serv_addr;
	struct hostent *h;
	struct in_addr a;
	struct servent *s;
#else
	struct addrinfo *ai;
	struct addrinfo hints;
	struct addrinfo *runp;
#endif
	int n;
#if 0
	h = gethostbyname(addr);
	if (h == NULL) error("unknown or invalid address %s\n", addr);
	memcpy(&a, h->h_addr, h->h_length);
	s = getservbyname(port, "tcp");
	if (s == NULL) po = atoi(port);
	else po = ntohs(s->s_port);
	fd = socket(AF_INET, SOCK_STREAM, 0);
#else
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_STREAM;
	n = getaddrinfo(addr, port, &hints, &ai);
	if (n != 0) {
		error("getaddrinfo: %s", gai_strerror(n));
	}
	runp = ai;
	/* only using first result; should try all */
	fd = socket(runp->ai_family, runp->ai_socktype, runp->ai_protocol);
#endif

	if (fd < 0) error("error opening socket");
#if 0
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = a.s_addr;
	serv_addr.sin_port = htons(po);
#endif
	signal(SIGALRM, alarm_handler);
#if 0
	n = connect(fd, (struct sockaddr *)&serv_addr, sizeof serv_addr);
#else
	n = connect(fd, runp->ai_addr, runp->ai_addrlen);
#endif
	alarm(0);
	if (n == -1) {
		error("error connecting to server");
	}
	return fd;
}

int main(int argc, char **argv)
{
	int i, fd, n;
	char b[1024], *p;

	if (argc < 3) {
		usage();
	}

	if (strchr(argv[1], '/')) {
		fd = open_unix_socket(argv[1]);
	} else {
		n = 1+strlen(argv[1]);	/* one for \0 */
		if (n > sizeof b) error("Overlong arg '%s'", argv[1]);
		strcpy(b, argv[1]);
		/* We need the *last* : to allow such arguments as ::1:10080
		   if pen's control port is ipv6 localhost:10080 */
		p = strrchr(b, ':');
		if (p == NULL) error("no port given");

		*p++ = '\0';

		fd = open_socket(b, p);
	}

	n = 2+strlen(argv[2]);	/* one for \n, one for \0 */
	if (n > sizeof b) error("Overlong arg '%s'", argv[2]);
	strcpy(b, argv[2]);
	for (i = 3; argv[i]; i++) {
		n = n+1+strlen(argv[i]);
		if (n > sizeof b) error("Overlong arg '%s'", argv[i]);
		strcat(b, " ");
		strcat(b, argv[i]);
	}
	strcat(b, "\n");
	n = write(fd, b, strlen(b));
	if (n == -1) error("error writing to socket");
	for (;;) {
		n = read(fd, b, sizeof b);
		if (n == 0) break;
		if (n == -1) error("error reading from socket");
		n = write(1, b, n);
		if (n == -1) error("error writing to stdout");
	}
	close(fd);

	return 0;
}
