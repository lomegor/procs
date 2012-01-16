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

#include <asm/ldt.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <elf.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "utils/common.h"
#include "utils/list.h"

unsigned long code_addr = TRAMPOLINE_ADDR;
unsigned long data_addr = 0;
int restore_file = 0;

void
restore_chunk_regs(struct proc_regs chunk)
{
	struct user user;
	user = *chunk.user_data;
	char *cp, *code = (char*)code_addr;
	struct user_regs_struct *r = &(user.regs);
	cp = code;

#ifdef X64
	/* set fs_base */
	*cp++=0x48; *cp++=0xc7; *cp++=0xc0;
	*(int*)(cp) = __NR_arch_prctl; cp+=4; /* mov foo, %rax  */
	*cp++=0x48; *cp++=0xbf;
	*(long*)(cp) = ARCH_SET_FS; cp+=8; /* mov foo, %rdi  */
	*cp++=0x48; *cp++=0xbe;
	*(long*)(cp) = r->fs_base; cp+=8; /* mov foo, %rsi  */
	*cp++=0x0f;*cp++=0x05; /* syscall */
	/* set gs_base */
	*cp++=0x48; *cp++=0xc7; *cp++=0xc0;
	*(int*)(cp) = __NR_arch_prctl; cp+=4; /* mov foo, %rax  */
	*cp++=0x48; *cp++=0xbf;
	*(long*)(cp) = ARCH_SET_GS; cp+=8; /* mov foo, %rdi  */
	*cp++=0x48; *cp++=0xbe;
	*(long*)(cp) = r->gs_base; cp+=8; /* mov foo, %rsi  */
	*cp++=0x0f;*cp++=0x05; /* syscall */

	/* munmap our data */
	*cp++=0x48; *cp++=0xc7; *cp++=0xc0;             /* mov foo, %rax */
	*(int*)(cp) = __NR_munmap;
	cp+=4;
	*cp++=0x48; *cp++=0xbf;                        /* mov foo, %rdi */
	*(long*)(cp) = data_addr;
	cp+=8;
	*cp++=0x48; *cp++=0xbe;                        /* mov foo, %rsi */
	*(long*)(cp) = DATA_LEN;
	cp+=8;
	*cp++=0x0f;*cp++=0x05;                         /* syscall */

	/* restore registers */
	*cp++=0x49; *cp++=0xb8; *(long*)(cp) = r->r8;  cp+=8;
	*cp++=0x49; *cp++=0xb9; *(long*)(cp) = r->r9;  cp+=8;
	*cp++=0x49; *cp++=0xba; *(long*)(cp) = r->r10; cp+=8;
	*cp++=0x49; *cp++=0xbb; *(long*)(cp) = r->r11; cp+=8;
	*cp++=0x49; *cp++=0xbc; *(long*)(cp) = r->r12; cp+=8;
	*cp++=0x49; *cp++=0xbd; *(long*)(cp) = r->r13; cp+=8;
	*cp++=0x49; *cp++=0xbe; *(long*)(cp) = r->r14; cp+=8;
	*cp++=0x49; *cp++=0xbf; *(long*)(cp) = r->r15; cp+=8;
	*cp++=0x48; *cp++=0xb9; *(long*)(cp) = r->rcx; cp+=8;
	*cp++=0x48; *cp++=0xba; *(long*)(cp) = r->rdx; cp+=8;
	*cp++=0x48; *cp++=0xbb; *(long*)(cp) = r->rbx; cp+=8;
	*cp++=0x48; *cp++=0xbc; *(long*)(cp) = r->rsp; cp+=8;
	*cp++=0x48; *cp++=0xbd; *(long*)(cp) = r->rbp; cp+=8;
	*cp++=0x48; *cp++=0xbe; *(long*)(cp) = r->rsi; cp+=8;
	*cp++=0x48; *cp++=0xbf; *(long*)(cp) = r->rdi; cp+=8;

	/* put return dest onto stack too */
	*cp++=0x48; *cp++=0xb8; *(long*)(cp) = r->rip; cp+=8;
	*cp++=0x50;
	*cp++=0x48; *cp++=0xb8; *(long*)(cp) = r->rax; cp+=8;

	/* put eflags onto the process' stack so we can pop it off */
	*cp++=0x68;
	*(int*)(cp) = r->eflags; cp+=4;

	/* pop eflags */
	*cp++=0x9d; 
	/* jump back to where we were. */
	*cp++=0xc3;
#elif X32
	*cp++=0xbd;*(long*)(cp) = r->ebp; cp+=4; /* mov foo, %ebp  */
	*cp++=0xbc;*(long*)(cp) = r->esp; cp+=4; /* mov foo, %esp  */
	/* set up gs */
	if (r->xgs != 0) {
		*cp++=0x66;*cp++=0xb8; *(short*)(cp) = r->xgs; cp+=2; /* mov foo, %eax  */
		*cp++=0x8e;*cp++=0xe8; /* mov %eax, %gs */
	}
	*cp++=0xb8;*(long*)(cp) = __NR_munmap; cp+=4; /* mov foo, %eax  */
	*cp++=0xbb;*(long*)(cp) = data_addr; cp+=4; /* mov foo, %ebx  */
	*cp++=0xb9;*(long*)(cp) = DATA_LEN; cp+=4; /* mov foo, %ecx  */
	*cp++=0xcd;*cp++=0x80; /* int $0x80 */

	/* restore registers */
	*cp++=0xba;*(long*)(cp) = r->edx; cp+=4; /* mov foo, %edx  */
	*cp++=0xbe;*(long*)(cp) = r->esi; cp+=4; /* mov foo, %esi  */
	*cp++=0xbf;*(long*)(cp) = r->edi; cp+=4; /* mov foo, %edi  */
	*cp++=0xbd;*(long*)(cp) = r->ebp; cp+=4; /* mov foo, %ebp  */
	*cp++=0xbc;*(long*)(cp) = r->esp; cp+=4; /* mov foo, %esp  */
	*cp++=0xb8;*(long*)(cp) = r->eax; cp+=4; /* mov foo, %eax  */
	*cp++=0xbb;*(long*)(cp) = r->ebx; cp+=4; /* mov foo, %ebx  */
	*cp++=0xb9;*(long*)(cp) = r->ecx; cp+=4; /* mov foo, %ecx  */

	*cp++=0x68;
	*(int*)(cp) = r->eflags; cp+=4;
	*cp++=0x9d; /* pop eflags */

	/* jump back to where we were. */
	*cp++=0xea;
	*(unsigned long*)(cp) = r->eip; cp+= 4;
	asm("mov %%cs,%w0": "=q"(r->xcs)); /* ensure we use the right CS for the current kernel */
	*(unsigned short*)(cp) = r->xcs; cp+= 2; /* jmp cs:foo */
#endif
	code_addr=(long)cp;
}

static void
as_memcpy(long src,long dst, long length) {
	char *p = (char*)code_addr;
	//memcpy(src,dst,length)
#ifdef X64
	*p++=0x48; *p++=0xbe;                        /* mov foo, %rsi */
	*(long*)(p) = src; p+=8;
	*p++=0x48; *p++=0xbf;                        /* mov foo, %rdi */
	*(long*)(p) = dst; p+=8;
	*p++=0x48; *p++=0xb9;                        /* mov foo, %rcx */
	*(long*)(p) = length>>3; p+=8;
	*p++=0xf3;*p++=0x48;*p++=0xa5;               /* rep movsl */
#elif X32
	*p++=0xbe;*(long*)(p)=src; p+=4;             /* mov foo, %esi */
	*p++=0xbf;*(long*)(p)=dst; p+=4;             /* mov foo, %edi */
	*p++=0xb9;*(long*)(p)=length>>2; p+=4;       /* mov foo, %ecx */
	*p++=0xf3;*p++=0xa5;                         /* rep movsl */
#endif
	code_addr = (long)p;
}
	static void 
as_mmap_memcpy(long mmap_addr, long mmap_len,
		int mmap_prot, int mmap_opt, int fd, int offset, long src)
{
	char *p = (char*)code_addr;

	// mmap(mmap_addr, mmap_len, mmap_prot, mmap_opt, fd, offset); 
#ifdef X64
	*p++=0x48; *p++=0xc7; *p++=0xc0;             /* mov foo, %rax */
	*(int*)(p) = __NR_mmap;
	p+=4;
	*p++=0x48; *p++=0xbf;                        /* mov foo, %rdi */
	*(long*)(p) = mmap_addr;
	p+=8;
	*p++=0x48; *p++=0xbe;                        /* mov foo, %rsi */
	*(long*)(p) = mmap_len;
	p+=8;
	*p++=0x48; *p++=0xba;                        /* mov foo, %rdx */
	*(long*)(p) = mmap_prot;
	p+=8;
	*p++=0x49; *p++=0xba;                        /* mov foo, %r10 */
	*(long*)(p) = mmap_opt;
	p+=8;
	*p++=0x49; *p++=0xb9;                        /* mov 0, %r0 */
	*(long*)(p) = fd;
	p+=8;
	*p++=0x49; *p++=0xb8;                        /* mov 0, %r8 */
	*(long*)(p) = offset;
	p+=8;
	*p++=0x0f;*p++=0x05;                         /* syscall */
#elif X32
	*p++=0xb8;*(long*)(p)=__NR_mmap2; p+=4;      /* mov foo, %eax */
	*p++=0xbb;*(long*)(p)=mmap_addr; p+=4;       /* mov foo, %ebx */
	*p++=0xb9;*(long*)(p)=mmap_len; p+=4;        /* mov foo, %ecx */
	*p++=0xba;*(long*)(p)=mmap_prot; p+=4;			/* mov foo, %edx */
	*p++=0xbe;*(long*)(p)=mmap_opt; p+=4;				/* mov foo, %esi */
	*p++=0xcd;*p++=0x80;			 /* int $0x80 */
#endif
	code_addr = (long)p;
	if (fd<=0) 
		as_memcpy(src,mmap_addr,mmap_len);
}

void 
restore_chunk_tls(struct proc_tls tls)
{
	if (!tls.u->base_addr)
		return;
	
	memcpy((void*)data_addr,tls.u,sizeof(struct user_desc));
	char* p = (char*)code_addr;
	*p++=0xb8;*(long*)(p)=__NR_set_thread_area; p+=4;      /* mov foo, %eax */
	*p++=0xbb;*(long*)(p)=data_addr; p+=4;       /* mov foo, %ebx */
	*p++=0xcd;*p++=0x80;			 /* int $0x80 */
	code_addr = (long) p;
	data_addr+=sizeof(struct user_desc);
}

void
restore_chunk_vma(struct proc_vma vma)
{
	/*printf("VMA %08lx-%08lx (size:%8ld) %c%c%c%c %08lx %02x:%02x %d\t%s\n",
		vma.start, vma.start+vma.length, vma.length,
		(vma.prot&PROT_READ)?'r':'-',
		(vma.prot&PROT_WRITE)?'w':'-',
		(vma.prot&PROT_EXEC)?'x':'-',
		(vma.prot&MAP_PRIVATE)?'p':'s',
		vma.pg_off,
		vma.dev >> 8,
		vma.dev & 0xff,
		vma.inode,
		vma.filename);*/
	if (vma.is_heap) {
		memcpy((void*)data_addr,vma.data,vma.length);
		as_memcpy(data_addr,(long)sbrk(vma.length),vma.length);
		data_addr+=vma.length;
	} else if (vma.have_data) {
		memcpy((void*)data_addr,vma.data,vma.length);
		as_mmap_memcpy(vma.start,vma.length,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_ANONYMOUS|vma.flags,0,0,data_addr);
		data_addr+=vma.length;
	} else if (vma.filename[0]) {
		int fd = open(vma.filename, O_RDONLY);
		as_mmap_memcpy(vma.start,vma.length,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_ANONYMOUS|vma.flags,fd,vma.pg_off,0);
	}
}

void
munmap_all() {
	FILE* f;
	char tmp_fn[30],
			 map_line[1024],
			 *filename=NULL,
			 *ptr1,*ptr2;
	unsigned long start,length;
	char* p = (char*)code_addr;
	snprintf(tmp_fn, 30, "/proc/%d/maps", getpid());
	f = fopen(tmp_fn, "r");
	while (fgets(map_line, sizeof(map_line), f)) {
		ptr1 = map_line;
		start = get_long(&ptr1,'-',16);
		ptr1++;
		length = get_long(&ptr1,' ',16) - start;
		ptr1++;

		ptr1 +=9; /* to filename */
		while (*ptr1 == ' ') ptr1++;
		if (*ptr1 != '\n') { /* we have a filename too to grab */
			ptr2 = strchr(ptr1, '\n');
			*ptr2 = '\0';
			filename = malloc((long)ptr2-(long)ptr1+1);
			filename = strdup(ptr1);
		}
		if (start!=code_addr //not our data
				&& start!=data_addr
				&& (filename==NULL 
					|| strstr(filename,"[heap]") == NULL 
					|| strstr(filename,"[stack]") == NULL)) { //not the stack or the heap
			//munmap(start,length)
#ifdef X64
			*p++=0x48; *p++=0xc7; *p++=0xc0;             /* mov foo, %rax */
			*(int*)(p) = __NR_munmap;
			p+=4;
			*p++=0x48; *p++=0xbf;                        /* mov foo, %rdi */
			*(long*)(p) = start;
			p+=8;
			*p++=0x48; *p++=0xbe;                        /* mov foo, %rsi */
			*(long*)(p) = length;
			p+=8;
			*p++=0x0f;*p++=0x05;                         /* syscall */
#elif X32
			*p++=0xb8;*(long*)(p) = __NR_munmap; p+=4; /* mov foo, %eax  */
			*p++=0xbb;*(long*)(p) = start; p+=4; /* mov foo, %ebx  */
			*p++=0xb9;*(long*)(p) = length; p+=4; /* mov foo, %ecx  */
			*p++=0xcd;*p++=0x80; /* int $0x80 */
#endif
		}
	}
	fclose(f);
	code_addr=(long)p;
}

void
restore_chunk_fd(struct proc_fd pf) {
	char tmpname[] = "/tmp/procs_XXXXXX";
	int ftmp;
	if (pf.type==PROC_CHUNK_FD_FILE) {
		if (pf.file.contents==NULL || !restore_file) {
			ftmp = open(pf.file.filename,pf.mode);
			if (ftmp>0) {
				dup2(ftmp,pf.fd);
				if (ftmp!=pf.fd) {
					close(ftmp);
				}
				lseek(pf.fd,pf.offset,SEEK_SET);
				return;
			}
		}
		ftmp = mkstemp(tmpname);
		fprintf(stderr,"Restoring file %s as %s\n",pf.file.filename,tmpname);
		write(ftmp,pf.file.contents,pf.file.size);
		lseek(ftmp,pf.offset,SEEK_SET);
		dup2(ftmp,pf.fd);
		if (ftmp!=pf.fd) {
			close(ftmp);
		}
		lseek(pf.fd,pf.offset,SEEK_SET);
	} else {
		ftmp = open(pf.file.filename,pf.mode);
		if (ftmp>0) {
			dup2(ftmp,pf.fd);
			if (ftmp!=pf.fd) {
				close(ftmp);
			}
			lseek(pf.fd,pf.offset,SEEK_SET);
			return;
		}
		printf("Could not open %s\n",pf.file.filename);
	}
}

void
restore_process(void * item) {
	struct proc_chunk* c = (struct proc_chunk*)item;
	switch (c->type) {
		case PROC_CHUNK_REGS:
			restore_chunk_regs(c->regs);
			break;
		case PROC_CHUNK_VMA:
			restore_chunk_vma(c->vma);
			break;
		case PROC_CHUNK_TLS:
			restore_chunk_tls(c->tls);
			break;
		case PROC_CHUNK_FD:
			restore_chunk_fd(c->fd);
			break;
		default:
			printf("Unknown chunk type read (0x%x)\n", c->type);
	}
}

int
main(int argc, char *argv[], char *envp[]) {
	if (argc<3)
		exit(-1);
	char* filename = argv[1];
	restore_file = atoi(argv[2]);
	FILE* datafile = fopen(filename,"r");
	code_addr = (long)mmap((void*)TRAMPOLINE_ADDR,TRAMPOLINE_LEN,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,0,0);
	void* code = (void*)code_addr;
	data_addr = (long)mmap((void*)DATA_ADDR,DATA_LEN,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,0,0);
	munmap_all();
	struct proc_process *pp = read_process(datafile,read_from_file_pointer);
	fclose(datafile);
	list_iterate(*pp->chunks,restore_process);
	((void(*)(void))code)();
	//asm("jmp 0x003000000");
	return -1;
}
