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

#ifndef _COMMON_H
#define _COMMON_H

#include <asm/ldt.h>
#include <linux/termios.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/user.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "list.h"

#define FALSE 0
#define TRUE !0	

#ifdef X64
#define TRAMPOLINE_ADDR		0x003000000
#define TRAMPOLINE_LEN 10000
#define DATA_ADDR		0x004000000
#define DATA_LEN 10000000
#elif X32
#define TRAMPOLINE_ADDR		0x0030000
#define TRAMPOLINE_LEN 10000
#define DATA_ADDR		0x00400000
#define DATA_LEN 10000000
#endif

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 12092

/* Constants for proc_chunk.type */
#define PROC_CHUNK_HEADER		0x01
#define PROC_CHUNK_MISC		0x02
#define PROC_CHUNK_REGS		0x03
#define PROC_CHUNK_I387_DATA	0x04
#define PROC_CHUNK_TLS		0x05
#define PROC_CHUNK_FD		0x06
#define PROC_CHUNK_VMA		0x07
#define PROC_CHUNK_SIGHAND	0x08
#define PROC_CHUNK_FINAL		0xFF

/* Constants for cp_fd.type */
#define PROC_CHUNK_FD_FILE	0x01
#define PROC_CHUNK_FD_CONSOLE	0x02
#define PROC_CHUNK_FD_SOCKET	0x03
#define PROC_CHUNK_FD_FIFO	0x04
#define PROC_CHUNK_FD_LINK	0x05
#define PROC_CHUNK_FD_DIR         0x06
#define PROC_CHUNK_FD_MAXFD	0x07

#define PROC_VMA_TEXT 0x1

#define GB		(1024*1024*1024)


#define LOCAL_SERVER_SOCK "/etc/procs/procs.sock"
#define SIZE_OF_ID 30

/* FLAGS */
#define PROC_FLAG_SAVE_FILE 1


struct proc_vma {
	unsigned long start, length;
	int prot, flags, dev, inode,have_data,is_lib,is_heap;
	long pg_off;
	char *filename;
	long *data;
};

struct proc_misc {
	char *cmdline;
	char *cwd;
	char *env;
};

struct proc_regs {
	struct user *user_data;
	void *opaque; /* For arch-specific data */
	int stopped;
};

struct proc_sighand {
	int sig_num;
	struct k_sigaction *ksa;
};

struct proc_console {
	struct termios termios;
};

struct proc_file {
	char *filename;
	char *contents;
	int deleted;
	long size;
};

struct proc_socket_tcp {
	struct sockaddr_in sin;
	void *ici; /* If the system supports tcpcp. */
};

struct proc_socket_udp {
	struct sockaddr_in sin;
};

struct proc_socket_unix {
	int type, listening;
	struct sockaddr_un sockname;
	struct sockaddr_un peername;
};

struct proc_socket {
	char *contents;  /* MAO -- In case socket has data in it still */
	int  size;       /* MAO -- amount of data in socket            */
	int proto;
	union {
		struct proc_socket_tcp s_tcp;
		struct proc_socket_udp s_udp;
		struct proc_socket_unix s_unix;
	};
};

struct proc_fifo {
	char *fifoname;  /* MAO -- for name pipes                    */
	char *contents;  /* MAO -- In case pipe has data in it still */
	int  size;       /* MAO -- amount of data in FIFO            */
	pid_t target_pid;
	int self_other_fd;
};

struct proc_fd {
	int fd;
	int mode;
	int close_on_exec;
	int fcntl_status;
	long offset;
	int type;
	union {
		struct proc_console console;
		struct proc_file file;
		struct proc_fifo fifo;
		struct proc_socket socket;
	};
};

struct proc_tls {
    struct user_desc* u;
};

struct proc_chunk {
	unsigned char type;
	union {
		struct proc_misc misc;
		struct proc_regs regs;
		struct proc_fd fd;
		struct proc_vma vma;
		struct proc_sighand sighand;
		struct proc_tls tls;
	};
};

struct proc_process {
	char* name;
	struct list *chunks;
};

int verbose;
int flags;

void
free_chunk(void* item);

void
free_process(struct proc_process* p);

unsigned long 
checksum(char *ptr, long len);

unsigned long
get_long(char** start, char search, unsigned int base);

void
write_to_file_pointer(void* fp, void* mem, size_t s);

long
read_from_file_pointer(void* fp, void* mem, size_t s);

void
write_to_socket(void* fp, void* mem, size_t s);

long
read_from_socket(void* socket,void* mem, size_t size);

void
write_chunk(void* item,void* other);

int
write_process(struct proc_process process, void* fp,void (w_fun)(void*,void*,size_t));

struct proc_process* 
read_process(void* fp,long (r_fun)(void*,void*,size_t));
#endif
