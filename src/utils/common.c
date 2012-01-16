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

#include "common.h"

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

int verbose = FALSE;

unsigned long 
checksum(char *ptr, long len) {
	long sum=0,i;
	for (i = 0; i < len; i++)
		sum = ((sum << 5) + sum) ^ ptr[i];
	return sum;
}

void
free_chunk(void* item) {
	struct proc_chunk *pc = (struct proc_chunk*) item;
	switch (pc->type) {
		case PROC_CHUNK_REGS:
			free(pc->regs.user_data);
			break;
		case PROC_CHUNK_VMA:
			free(pc->vma.filename);
			free(pc->vma.data);
			break;
		case PROC_CHUNK_TLS:
			free(pc->tls.u);
			break;
		default:
			break;
	}
	free(pc);
}
void
free_process(struct proc_process* p) {
	list_iterate(*p->chunks,free_chunk);
	list_free(*p->chunks);
	free(p);
}

//search for char, replace and get long number from string
//returns place in first pointer
unsigned long
get_long(char** start, char search, unsigned int base) {
	char *ptr1,*ptr2;
	ptr1 = *start;
	if ((ptr2 = strchr(ptr1, search)) == NULL)
		return 0; 
	*ptr2 = '\0';
	*start = ptr2;
	return strtoul(ptr1, NULL, base);
}

struct writer {
	void (*w)(void*,void*,size_t);
	void *fp;
};

void
write_to_file_pointer(void* fp, void* mem, size_t s) {
	fwrite(mem,s,1,(FILE*)fp);
}

long
read_from_file_pointer(void* fp, void* mem, size_t s) {
	long i = fread(mem,s,1,(FILE*)fp);
	if (i==0) return -1;
	return i;
}

void
write_to_socket(void* fp, void* mem, size_t s) {
	write(*(int*)fp,mem,s);
}

long
read_from_socket(void* socket,void* mem, size_t size) {
	long bytesread=0;
	while (bytesread<size) {
		long a = read(*(int*)socket,(void*)(mem+bytesread),size-bytesread);
		if (a<0)
			return a;
		bytesread+=a;
	}
	return bytesread;
}

void
write_chunk(void* item,void* other) {
	struct proc_chunk *chunk = (struct proc_chunk *) item;
	struct writer* w = (struct writer*)other;
	switch (chunk->type) {
		case PROC_CHUNK_REGS:
			w->w(w->fp,&chunk->type,sizeof(chunk->type));
			w->w(w->fp,chunk->regs.user_data,sizeof(struct user));
			w->w(w->fp,&chunk->regs.stopped,sizeof(chunk->regs.stopped));
			break;
		case PROC_CHUNK_VMA:
			w->w(w->fp,&chunk->type,sizeof(chunk->type));
			w->w(w->fp,&chunk->vma.start,sizeof(chunk->vma.start));
			w->w(w->fp,&chunk->vma.length,sizeof(chunk->vma.length));
			w->w(w->fp,&chunk->vma.prot,sizeof(chunk->vma.prot));
			w->w(w->fp,&chunk->vma.flags,sizeof(chunk->vma.flags));
			w->w(w->fp,&chunk->vma.dev,sizeof(chunk->vma.dev));
			w->w(w->fp,&chunk->vma.inode,sizeof(chunk->vma.inode));
			w->w(w->fp,&chunk->vma.have_data,sizeof(chunk->vma.have_data));
			w->w(w->fp,&chunk->vma.is_lib,sizeof(chunk->vma.is_lib));
			w->w(w->fp,&chunk->vma.is_heap,sizeof(chunk->vma.is_heap));
			w->w(w->fp,&chunk->vma.pg_off,sizeof(chunk->vma.pg_off));
			int length = chunk->vma.filename!=NULL?strlen(chunk->vma.filename):0;
			w->w(w->fp,&length,sizeof(length));
			w->w(w->fp,chunk->vma.filename,length);
			if (chunk->vma.have_data)
				w->w(w->fp,chunk->vma.data,chunk->vma.length);
			break;
		case PROC_CHUNK_TLS:
			w->w(w->fp,&chunk->type,sizeof(chunk->type));
			w->w(w->fp,chunk->tls.u,sizeof(struct user_desc));
			break;
		case PROC_CHUNK_FD:
			w->w(w->fp,&chunk->type,sizeof(chunk->type));
			w->w(w->fp,&chunk->fd.fd,sizeof(int));
			w->w(w->fp,&chunk->fd.type,sizeof(int));
			w->w(w->fp,&chunk->fd.mode,sizeof(int));
			w->w(w->fp,&chunk->fd.offset,sizeof(long));
			if (chunk->fd.type==PROC_CHUNK_FD_FILE) {
				int length = strlen(chunk->fd.file.filename);
				w->w(w->fp,&length,sizeof(int));
				if (length>0)
					w->w(w->fp,chunk->fd.file.filename,length+1);
				if (chunk->fd.file.contents!=NULL) {
					w->w(w->fp,&chunk->fd.file.size,sizeof(long));
					w->w(w->fp,chunk->fd.file.contents,chunk->fd.file.size);
				} else {
					long length_l = 0;
					w->w(w->fp,&length_l,sizeof(long));
				}
			} else if (chunk->fd.type==PROC_CHUNK_FD_DIR) {
				w->w(w->fp,&length,sizeof(int));
				if (length>0)
					w->w(w->fp,chunk->fd.file.filename,length);
				w->w(w->fp,&length,sizeof(int));
				w->w(w->fp,&chunk->fd.file.size,sizeof(int));
			} else {
				printf("Invalid FD chunk type %d!", chunk->fd.type);
				break;
			}
			break;
		default:
			break;
	}
}

int
write_process(struct proc_process process, void* fp,void (w_fun)(void*,void*,size_t)) {
	struct writer w;
	w.fp = fp;
	w.w = w_fun;
	list_iterate2(*process.chunks,&w,write_chunk);
	unsigned char final = PROC_CHUNK_FINAL;
	w_fun(fp,&final,sizeof(final));
	return 0;
}


struct proc_process* 
read_process(void* fp,long (r_fun)(void*,void*,size_t)) {
	struct proc_process* pp = malloc(sizeof(struct proc_process));
	pp->name="test";
	pp->chunks = malloc(sizeof(struct list));
	list_init(pp->chunks);
	unsigned char type;
	long n=0;
	int has_chunks=FALSE;
	while ((n=r_fun(fp,&type,sizeof(type)))>=0) {
		int length;
		struct proc_chunk* pc;
		if (type==PROC_CHUNK_FINAL) 
			break;
		has_chunks=TRUE;
		switch (type) {
			case PROC_CHUNK_REGS:
				pc = malloc(sizeof(struct proc_chunk));
				pc->type=type;
				pc->regs.user_data = malloc(sizeof(struct user));
				r_fun(fp,pc->regs.user_data,sizeof(struct user));
				r_fun(fp,&pc->regs.stopped,sizeof(pc->regs.stopped));
				list_append(pp->chunks,pc);
				break;
			case PROC_CHUNK_VMA:
				pc = malloc(sizeof(struct proc_chunk));
				pc->type=type;
				r_fun(fp,&pc->vma.start,sizeof(pc->vma.start));
				r_fun(fp,&pc->vma.length,sizeof(pc->vma.length));
				r_fun(fp,&pc->vma.prot,sizeof(pc->vma.prot));
				r_fun(fp,&pc->vma.flags,sizeof(pc->vma.flags));
				r_fun(fp,&pc->vma.dev,sizeof(pc->vma.dev));
				r_fun(fp,&pc->vma.inode,sizeof(pc->vma.inode));
				r_fun(fp,&pc->vma.have_data,sizeof(pc->vma.have_data));
				r_fun(fp,&pc->vma.is_lib,sizeof(pc->vma.is_lib));
				r_fun(fp,&pc->vma.is_heap,sizeof(pc->vma.is_heap));
				r_fun(fp,&pc->vma.pg_off,sizeof(pc->vma.pg_off));
				r_fun(fp,&length,sizeof(length));
				if (length>0) {
					pc->vma.filename = malloc(length);
					r_fun(fp,pc->vma.filename,length);
				}
				if (pc->vma.have_data) {
					pc->vma.data = malloc(pc->vma.length);
					r_fun(fp,pc->vma.data,pc->vma.length);
				} 
				list_append(pp->chunks,pc);
				break;
			case PROC_CHUNK_TLS:
				pc = malloc(sizeof(struct proc_chunk));
				pc->type=type;
				pc->tls.u = malloc(sizeof(struct user_desc));
				r_fun(fp,pc->tls.u,sizeof(struct user_desc));
				list_append(pp->chunks,pc);
				break;
			case PROC_CHUNK_FD:
				pc = malloc(sizeof(struct proc_chunk));
				pc->type=type;
				r_fun(fp,&pc->fd.fd, sizeof(int));
				r_fun(fp,&pc->fd.type, sizeof(int));
				r_fun(fp,&pc->fd.mode, sizeof(int));
				r_fun(fp,&pc->fd.offset, sizeof(long));
				if (pc->fd.type==PROC_CHUNK_FD_FILE) {
					r_fun(fp,&length,sizeof(length));
					if (length>0) {
						pc->fd.file.filename = malloc(length+1);
						r_fun(fp,pc->fd.file.filename,length+1);
					}
					r_fun(fp,&pc->fd.file.size,sizeof(long));
					pc->fd.file.contents=NULL;
					if (pc->fd.file.size>0) {
						pc->fd.file.contents = malloc(pc->fd.file.size);
						r_fun(fp, pc->fd.file.contents, pc->fd.file.size);
					} 
				} else if (pc->fd.type==PROC_CHUNK_FD_DIR) {
					r_fun(fp,&length,sizeof(length));
					if (length>0) {
						pc->fd.file.filename = malloc(length);
						r_fun(fp,pc->fd.file.filename,length);
					}
					r_fun(fp,&pc->fd.file.size,sizeof(int));
				} else {
					printf("Invalid FD chunk type %d!",pc->fd.type);
					free(pc);
					break;
				}
				list_append(pp->chunks,pc);
				break;
			default:
				break;
		}
	}
	if (!has_chunks)
		return NULL;
	return pp;
}
