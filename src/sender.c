/*
 *  Copyright 2011 Sebastian Ventura
 *  This file is part of procs.
 *
 *  procs is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
*
 *  procs is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with procs.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/common.h"
#include "utils/list.h"
#include "loader.h"
#include "saver.h"
#include "sender.h"


int
send_process(struct proc_process process, char* ip, int port) {
	int sender;
	struct sockaddr_in them;

	sender = socket(AF_INET, SOCK_STREAM, 0);
	if (sender < 0) {
		perror("Creating socket");
		return -1;
	}
	bzero((char *) &them, sizeof(them));
	struct hostent *server = gethostbyname(ip);
	them.sin_port = htons(port);
	them.sin_family = AF_INET;
	memcpy(&them.sin_addr.s_addr,server->h_addr,server->h_length);

	if (connect(sender,(struct sockaddr *)&them,sizeof(them)) < 0) {
		perror("Connecting socket");
		return -1;
	}

	write_process(process,&sender,write_to_socket);
	close(sender);

	return 0;
}
