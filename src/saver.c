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

#include "saver.h"

#include <asm/ldt.h>
#include <linux/kdev_t.h>
#include <sys/ptrace.h>
#include <asm/ptrace.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>


static unsigned long last_vma_end;
extern int flags;

struct proc_process 
save_process(pid_t pid,char* name, int nokill)
{
	struct proc_process saved_process;
	saved_process.name=name;
	struct list *chunks=malloc(sizeof(struct list));
	list_init(chunks);
	long offset;

	if (ptrace(PTRACE_ATTACH, pid, NULL, NULL)== -1 && errno) {
		perror("error attach"); //TODO ERROR
		return saved_process;
	}
	waitpid(pid,0,0);

#ifdef X32
	fetch_chunks_tls(pid, chunks);
#endif
	fetch_chunks_vma(pid, chunks, &offset);
  fetch_chunks_fd(pid, chunks);
  //fetch_chunks_sighand(pid, &chunks);
  fetch_chunks_regs(pid,chunks);

	if (nokill) {
		if (ptrace(PTRACE_CONT, pid, NULL, NULL)== -1 && errno) {
				perror("error cont"); //TODO ERROR
				return saved_process;
		}
	} else if (ptrace(PTRACE_KILL, pid, NULL, NULL)== -1 && errno) {
		perror("error kill"); //TODO ERROR
		return saved_process;
	}
	saved_process.chunks = chunks;
	return saved_process;
}

void 
fetch_chunks_tls(pid_t pid, struct list *l)
{
	int i;
	struct proc_chunk *chunk;
	struct user_desc *u;

	for (i = 0; i < 256; i++) { /* FIXME: verify this magic number */
		if (!u) {
			u = malloc(sizeof(struct user_desc));
			memset(u, 0, sizeof(struct user_desc));
		}
		u->entry_number = i;
		if (ptrace(PTRACE_GET_THREAD_AREA, pid, i, u) == -1)
			continue;

		chunk = malloc(sizeof(struct proc_chunk));
		chunk->type = PROC_CHUNK_TLS;
		chunk->tls.u = u;
		list_append(l, chunk);
		u = NULL;
	}
}

void
fetch_chunks_regs(pid_t pid, struct list *l)
{
	struct proc_chunk *chunk = NULL;
	struct user *user_data;
	long* user_data_ptr;
	long pos;

	user_data = malloc(sizeof(struct user));\
	user_data_ptr = (long*) user_data;

	/* Get the user struct of the process */
	for(pos = 0; pos < sizeof(struct user)/sizeof(long); pos++) {
		user_data_ptr[pos] = ptrace(PTRACE_PEEKUSER, pid, (void*)(pos*sizeof(long)), NULL);
		if (errno != 0) {
			perror("ptrace(PTRACE_PEEKDATA): ");
		}
	}
	chunk = malloc(sizeof(struct proc_chunk));
	chunk->type = PROC_CHUNK_REGS;
	chunk->regs.user_data = user_data;
	chunk->regs.stopped = 1;
	list_append(l, chunk);
}

int
get_one_vma(pid_t pid, char* line, struct proc_vma *vma, int vma_no, long *bin_offset) {
	char *ptr1,*ptr2 = NULL; //pointer to char in line
	int to_read = 0; //do we need to store the data?
	memset(vma, 0, sizeof(struct proc_vma));

	//Find start and end of vma
	ptr1 = line;
	vma->start = get_long(&ptr1,'-',16);
	ptr1++;
	vma->length = get_long(&ptr1,' ',16) - vma->start;
	ptr1++;

	//prot and flags
	vma->prot = 0;
	vma->flags = MAP_FIXED;
	ptr1[0] == 'r' && (vma->prot |= PROT_READ);
	ptr1[1] == 'w' && (vma->prot |= PROT_WRITE);
	ptr1[2] == 'x' && (vma->prot |= PROT_EXEC);
	if (ptr1[3] == 's')
		vma->flags |= MAP_SHARED;
	else if (ptr1[3] == 'p')
		vma->flags |= MAP_PRIVATE;

	//offset of page
	ptr1 = ptr1+5; /* to pgoff */
	vma->pg_off = get_long(&ptr1,' ',16);
	if ((signed long)vma->pg_off < 0)
		vma->flags |= MAP_GROWSDOWN;

	//device major:minor
	ptr1++;
	unsigned long dmajor = get_long(&ptr1,':',16);
	ptr1++;
	unsigned long dminor = get_long(&ptr1,' ',16);
	vma->dev = MKDEV(dmajor,dminor);

	ptr1++;
	vma->inode = get_long(&ptr1,' ',10);
	ptr1++;
	while (*ptr1 == ' ') ptr1++;
	if (*ptr1 != '\n') { /* we have a filename too to grab */
		ptr2 = strchr(ptr1, '\n');
		*ptr2 = '\0';
		vma->filename = strdup(ptr1);
		//mark if it should be read (first two lines and not library)
		to_read = 1;
		if (strstr(ptr1, ".so") != NULL) {
			vma->is_lib=1;
			char* ptr3 = ptr1;
			while ((ptr2 = strchr(ptr3,'/')) != NULL)
				ptr3 = ptr2+1;
			ptr2 = strchr(ptr1,'-');
			*ptr2 = '.';
			*(ptr2+1)='s';
			*(ptr2+2)='o';
			*(ptr2+3)='\0';
			vma->filename = strdup(ptr3);
		} else if (strstr(ptr1,"[heap]") != NULL) {
			(*bin_offset) = vma->start;
			vma->flags |= MAP_ANONYMOUS;
			vma->is_heap=1;
		}
	} else {
		vma->flags |= MAP_ANONYMOUS;
		to_read = 1;
	}

	if (to_read) {
		vma->data = malloc(vma->length);
		long i, n = vma->length/sizeof(long);
		for (i = 0; i < n; i++) {
			vma->data[i] = ptrace(PTRACE_PEEKTEXT, pid, vma->start+(i*sizeof(long)), 0);
			if (errno) {
				//perror("Error peeking memory");
			}
		}
		vma->have_data=1;
	} else {
		if (vma->data!=NULL) {
			free(vma->data);
			vma->data=NULL;
		}
		vma->have_data=0;
	}
	last_vma_end = vma->start + vma->length;
	return 1;
}

void fetch_chunks_vma(pid_t pid, struct list *l, long *bin_offset) {
	struct proc_chunk *chunk   = NULL;
	char tmp_fn[30]          = "", //map filename
			 map_line[1024]      = ""; //each line of map
	FILE        *f = NULL; //map file
	int vma_no     = 0; //count of vma
	//open maps file
	//iterate through map lines
	snprintf(tmp_fn, 30, "/proc/%d/maps", pid);
	f = fopen(tmp_fn, "r");
	while (fgets(map_line, sizeof(map_line), f)) {
		//alloc chunk and set type
		chunk = malloc(sizeof(struct proc_chunk));
		chunk->type = PROC_CHUNK_VMA;
		get_one_vma(pid, map_line, &chunk->vma,	vma_no, bin_offset);
		/*printf("VMA %08lx-%08lx (size:%8ld) %c%c%c%c %08lx %02x:%02x %d\t%s\n",
				chunk->vma.start, chunk->vma.start+chunk->vma.length, chunk->vma.length,
				(chunk->vma.prot&PROT_READ)?'r':'-',
				(chunk->vma.prot&PROT_WRITE)?'w':'-',
				(chunk->vma.prot&PROT_EXEC)?'x':'-',
				(chunk->vma.prot&MAP_PRIVATE)?'p':'s',
				chunk->vma.pg_off,
				chunk->vma.dev >> 8,
				chunk->vma.dev & 0xff,
				chunk->vma.inode,
				chunk->vma.filename);*/
		vma_no++;
		if (chunk->vma.filename==NULL	|| strstr(chunk->vma.filename,"[vsyscall]")==NULL) {
			list_append(l, chunk);
		} else {
			free(chunk);
		}
	}
	fclose(f);
}

void
fetch_chunks_fd(pid_t pid, struct list *l)
{
	struct proc_chunk *chunk;
	struct dirent *fd_dirent;
	struct stat stat_buf;
	DIR *proc_fd;
	FILE *fdinfo,*fp;
	char tmp_fn[1024], real_fn[1024], info[1024],info_line[1024];
	char *infoptr;
	int cnt = 0;

	snprintf(tmp_fn, 30, "/proc/%d/fd", pid);
	proc_fd = opendir(tmp_fn);

	while ((fd_dirent = readdir(proc_fd)) != NULL) {
		if (fd_dirent->d_type != DT_LNK || cnt++<3)
			continue;
		chunk = malloc(sizeof(struct proc_chunk));
		chunk->fd.fd = atoi(fd_dirent->d_name);

		/* Find out if it's open for r/w/rw */
		snprintf(tmp_fn, 1024, "/proc/%d/fd/%d", pid, chunk->fd.fd);
		lstat(tmp_fn, &stat_buf);
		if ((stat_buf.st_mode & S_IRUSR) && (stat_buf.st_mode & S_IWUSR))
			chunk->fd.mode = O_RDWR;
		else if (stat_buf.st_mode & S_IWUSR)
			chunk->fd.mode = O_WRONLY;
		else
			chunk->fd.mode = O_RDONLY;

		sprintf(info, "/proc/%d/fdinfo/%d", pid, chunk->fd.fd);
		fdinfo = fopen(info,"r");
		fgets(info_line, sizeof(info_line), fdinfo);
		infoptr = info_line;
		infoptr+=4;
		chunk->fd.offset = get_long(&infoptr,'\n',10);	

		//chunk->fd.offset        = r_lseek(pid, chunk->fd.fd, 0, SEEK_CUR);
		//TODO: GET CLOSE_ON_EXEC
		//chunk->fd.close_on_exec = r_fcntl(pid, chunk->fd.fd, F_GETFD);
		//chunk->fd.fcntl_status  = r_fcntl(pid, chunk->fd.fd, F_GETFL);


		/* Read the link to get the real file name */
		memset(real_fn,'\0',1024);
		readlink(tmp_fn, real_fn, 1024);
		stat(real_fn, &stat_buf);

		switch (stat_buf.st_mode & S_IFMT) {
			case S_IFREG:
				chunk->fd.file.filename = NULL;
				fp = fopen(real_fn,"r");
				fseek(fp, 0L, SEEK_END);
				chunk->fd.file.size = ftell(fp);
				fseek(fp, 0L, SEEK_SET);
				chunk->fd.file.contents = NULL;
				chunk->fd.file.filename = malloc(1024);
				strcpy(chunk->fd.file.filename,real_fn);

				if ((flags & PROC_FLAG_SAVE_FILE) && (chunk->fd.file.size > 0)) {
					chunk->fd.file.contents = malloc(chunk->fd.file.size);
					fread(chunk->fd.file.contents,1,chunk->fd.file.size,fp);
				}
				fclose(fp);

				chunk->fd.type = PROC_CHUNK_FD_FILE;
				break;

			case S_IFDIR:
				chunk->fd.file.filename = NULL;
				chunk->fd.file.size     = stat_buf.st_size;
				chunk->fd.file.contents = NULL;
				chunk->fd.file.filename = malloc(256);
				strcpy(chunk->fd.file.filename,real_fn);
				chunk->fd.type = PROC_CHUNK_FD_DIR;
				break;

			default:
				free(chunk);
				continue;
		}

		chunk->type = PROC_CHUNK_FD;
		list_append(l, chunk);
		chunk = NULL;
	}
}
