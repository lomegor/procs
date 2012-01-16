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

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include "utils/list.h"
#include "sender.h"
#include "saver.h"
#include "loader.h"

extern char *optarg;
extern int optind, opterr, optopt;

int command_index;
char* program_name;

void
usage(int i) {
	if (i==0) {
		fprintf(stderr,"Usage: %s save [PID|-n NAME] [OPTION]... \n",program_name);
		fprintf(stderr,"Checkpoints the current state of the program pointed by PID or NAME.\n");
		fprintf(stderr,"If no output is specified, tries local server to create new PROCS_ID to return\n");
		fprintf(stderr,"and if the local server is not running, dumps output to stdout\n");
		fprintf(stderr,"\nList of options:\n");
		fprintf(stderr,"    -h    --help\t\tshows this help message and exit\n");
		fprintf(stderr,"    -n    --name\t\tname of process to be checkpointed\n");
		fprintf(stderr,"    -o    --output\t\tfile to which checkpoint the process\n");
		fprintf(stderr,"    -K    --nokill\t\tkeep the process running after the checkpoint\n");
		fprintf(stderr,"    -f    --file-save\t\tdo not assume shared file and copy contents of open files\n");
		fprintf(stderr,"    -v    --verbose\t\tverbose output\n");
	} else if (i==1) {
		fprintf(stderr,"Usage: %s restore [FILE|PROCS_ID] [OPTION]... \n",program_name);
		fprintf(stderr,"Restores program pointed by FILE, and if it doesn't exist, checks PROCS_ID.\n");
		fprintf(stderr,"\nList of options:\n");
		fprintf(stderr,"    -h    --help\t\tshows this help message and exit\n");
		fprintf(stderr,"    -f    --file-save\t\tdo not assume shared file and copy contents of open files\n");
		fprintf(stderr,"    -v    --verbose\t\tverbose output\n");
	} else if (i==2) {
		fprintf(stderr,"Usage: %s send [PID|FILE|PROCS_ID] [-s SERVER] [OPTION]... \n",program_name);
		fprintf(stderr,"Sends program pointed by PID, FILE or PROCS_ID (checks in that order) to SERVER.\n");
		fprintf(stderr,"\nList of options:\n");
		fprintf(stderr,"    -h    --help\t\tshows this help message and exit\n");
		fprintf(stderr,"    -s    --server\t\tIP:PORT or HOSTNAME:PORT of procs server to send process\n");
		fprintf(stderr,"    -n    --name\t\tname of process to be checkpointed\n");
		fprintf(stderr,"    -K    --nokill\t\tkeep the process running after the checkpoint\n");
		fprintf(stderr,"    -f    --file-save\t\tdo not assume shared file and copy contents of open files\n");
		fprintf(stderr,"    -v    --verbose\t\tverbose output\n");
	} else if (i==3) {
		fprintf(stderr,"Usage: %s receive [PROCS_ID] [OPTION]... \n",program_name);
		fprintf(stderr,"Restores program pointed by PROCS_ID or waits until next process arrives.\n");
		fprintf(stderr,"\nList of options:\n");
		fprintf(stderr,"    -h    --help\t\tshows this help message and exit\n");
		fprintf(stderr,"    -f    --file-save\t\tdo not assume shared file and copy contents of open files\n");
		fprintf(stderr,"    -v    --verbose\t\tverbose output\n");
	} else if (i==4) {
		fprintf(stderr,"Usage: %s list [OPTION]... \n",program_name);
		fprintf(stderr,"Lists PROCS_ID in current server.\n");
		fprintf(stderr,"\nList of options:\n");
		fprintf(stderr,"    -h    --help\t\tshows this help message and exit\n");
		fprintf(stderr,"    -v    --verbose\t\tverbose output\n");
	} else if (i==5) {
		fprintf(stderr,"Usage: %s delete PROCS_ID [OPTION]... \n",program_name);
		fprintf(stderr,"Deletes program referenced by PROCS_ID in local server.\n");
		fprintf(stderr,"\nList of options:\n");
		fprintf(stderr,"    -h    --help\t\tshows this help message and exit\n");
		fprintf(stderr,"    -v    --verbose\t\tverbose output\n");
	} else {
		fprintf(stderr,"Usage: %s command [PID|FILE|PROCS_ID] [OPTION]... \n",program_name);
		fprintf(stderr,"Checkpoints a program and resumes it on another machine.\n");
		fprintf(stderr,"Can create checkpoints files and restore sent processes.\n");
		fprintf(stderr,"\nList of commands:\n");
		fprintf(stderr,"    save\t\tsaves program pointed by PID or by name in -n.\n");
		fprintf(stderr,"    restore\t\trestores program pointed by FILE or PROCS_ID\n");
		fprintf(stderr,"    send\t\tsends program pointed by FILE. If file does not exist, checks PROCS_ID\n");
		fprintf(stderr,"    receive\t\trestores program in current terminal pointed by PROCS_ID\n");
		fprintf(stderr,"    list\t\tlists PROCS_ID in local server\n");
		fprintf(stderr,"    delete\t\tdeletes PROCS_ID from local server\n");
		fprintf(stderr,"\nList of options:\n");
		fprintf(stderr,"    -h    --help\t\tshows this help message and exit\n");
		fprintf(stderr,"    -s    --server\t\tIP:PORT or HOSTNAME:PORT of procs server to send process\n");
		fprintf(stderr,"    -n    --name\t\tname of process to be checkpointed\n");
		fprintf(stderr,"    -o    --output\t\tfile to which checkpoint the process\n");
		fprintf(stderr,"    -K    --nokill\t\tkeep the process running after the checkpoint\n");
		fprintf(stderr,"    -v    --verbose\t\tverbose output\n");
	}
}

char*
send_proc_to_local_server(struct proc_process p) {
	struct sockaddr_un them;
	int sender = socket(AF_UNIX, SOCK_STREAM, 0);
 	if (sender < 0) {
		printf("Error creating connection to server\n");
	 	return NULL;
 	}
	them.sun_family = AF_UNIX; 
	strcpy(them.sun_path,LOCAL_SERVER_SOCK);
 	if (connect(sender,(struct sockaddr *)&them,sizeof(them)) < 0) {
		printf("Error connecting to server\n");
	 	return NULL;
 	}
 	unsigned char type = 's';
 	write_to_socket(&sender,&type,sizeof(char));
 	write_process(p,&sender,write_to_socket);
	char* id = malloc(SIZE_OF_ID*sizeof(char));;
 	read_from_socket(&sender,id,SIZE_OF_ID*sizeof(char));
 	return id;
}

struct proc_process *
receive_proc_from_local_server(char* id) {
	struct sockaddr_un them;
	int sender = socket(AF_UNIX, SOCK_STREAM, 0);
 	if (sender < 0) {
		printf("Error creating connection to server\n");
	 	return NULL;
 	}
	them.sun_family = AF_UNIX; 
	strcpy(them.sun_path,LOCAL_SERVER_SOCK);
 	if (connect(sender,(struct sockaddr *)&them,sizeof(them)) < 0) {
		printf("Error connecting to server\n");
	 	return NULL;
 	}
 	if (id!=NULL) {
	 	unsigned char type = 'r';
	 	write_to_socket(&sender,&type,sizeof(char));
	 	write_to_socket(&sender,id,SIZE_OF_ID*sizeof(char));
		return read_process(&sender,read_from_socket);
	} else {
	 	unsigned char type = 'w';
	 	write_to_socket(&sender,&type,sizeof(char));
		return read_process(&sender,read_from_socket);
	}
}

void
list_from_local_server() {
	struct sockaddr_un them;
	int sender = socket(AF_UNIX, SOCK_STREAM, 0);
 	if (sender < 0) {
		printf("Error creating connection to server\n");
		return;
 	}
	them.sun_family = AF_UNIX; 
	strcpy(them.sun_path,LOCAL_SERVER_SOCK);
 	if (connect(sender,(struct sockaddr *)&them,sizeof(them)) < 0) {
		printf("Error connecting to server\n");
	 	return;
 	}
 	unsigned char type = 'l';
 	write_to_socket(&sender,&type,sizeof(char));
	while (read(sender,&type,sizeof(type))>=0) {
		if (type==PROC_CHUNK_FINAL)
			break;
		printf("%c",type);
	}
}

void
delete_from_local_server(char* id) {
	struct sockaddr_un them;
	int sender = socket(AF_UNIX, SOCK_STREAM, 0);
 	if (sender < 0) {
		printf("Error creating connection to server\n");
		return;
 	}
	them.sun_family = AF_UNIX; 
	strcpy(them.sun_path,LOCAL_SERVER_SOCK);
 	if (connect(sender,(struct sockaddr *)&them,sizeof(them)) < 0) {
		printf("Error connecting to server\n");
	 	return;
 	}
 	unsigned char type = 'd';
 	write_to_socket(&sender,&type,sizeof(char));
 	write_to_socket(&sender,id,SIZE_OF_ID*sizeof(char));
}

extern int verbose;
extern int flags;

int 
main(int argc, char** argv)
{
	program_name = argv[0];
	static char *commands[] = {
		"save",
		"restore",
		"send",
		"receive",
		"list",
		"delete"
	};
	int commands_len = sizeof(commands)/sizeof(char*);
	static struct option long_options[] = {
		{"server", 1, 0, 's'},
		{"name", 1, 0, 'n'},
		{"output", 1, 0, 'o'},
		{"nokill", 0, 0, 'K'},
		{"verbose", 0, 0, 'v'},
		{"help", 0, 0, 'h'},
		{"file-save", 0, 0, 'f'},
		{NULL, 0, NULL, 0}
	};
	int c,i,option_index = 0;
	if (argc<2) {
		usage(-1);
		exit(1);
	}
	for (i=0;i<commands_len;i++) {
		if (!strcmp(commands[i],argv[1]))
			break;
	}
	if (!strcmp(argv[1],"--help") || !strcmp(argv[1],"-h")) {
		usage(-1);
		exit(0);
	}
	if (i==commands_len) {
		usage(-1);
		exit(1);
	}
	command_index = i;
	char** real_argv = malloc(sizeof(char*)*argc-1);
	real_argv[0] = argv[0];
	for (i=1;i<argc-1;i++) {
		real_argv[i] = argv[i+1];
	}
	char *output=NULL,*host=NULL,*name=NULL;
	int nokill = FALSE;
	verbose = 0;
	flags = 0;
	while ((c = getopt_long(argc-1, real_argv, "s:vn:hKo:f", long_options, &option_index)) != -1) {
		switch (c) {
			case 's':
				host = malloc(strlen(optarg));
				strcpy(host,optarg);
				break;
			case 'v':
				verbose=TRUE;
				break;
			case 'n':
				name = malloc(strlen(optarg));
				strcpy(name,optarg);
				break;
			case 'h':
				usage(command_index);
				exit(0);
				break;
			case 'o':
				output = malloc(strlen(optarg));
				strcpy(output,optarg);
				break;
			case 'K':
				nokill = TRUE;
				break;
			case 'f':
				flags |= PROC_FLAG_SAVE_FILE;
				break;
			default:
				fprintf(stderr,"?? getopt returned character code 0%o ??\n", c);
		}
	}
	if (command_index==0) {
		//save
		if (optind >= argc-1 && name==NULL) {
			usage(command_index);
			exit(1);
		}
		pid_t pid = 0;
		if (name!=NULL) {
			char tmpproc[100];
			sprintf(tmpproc,"ps ux | awk '/%s/ && !/awk/ {print $2}'",name);
			FILE* proc = popen(tmpproc,"r");
			fscanf(proc,"%d",&pid);
			pclose(proc);
		} else {
			pid = atoi(real_argv[optind]);
		}
		struct proc_process p = save_process(pid,"test",nokill);
		if (output!=NULL) {
			FILE* proc = fopen(output,"w+");
			write_process(p,proc,write_to_file_pointer);
			fclose(proc);
		} else {
			char* id;
			if ((id = send_proc_to_local_server(p))==NULL)
				write_process(p,stdout,write_to_file_pointer);
			printf("%s\n",id);
		}
	} else if (command_index==1) {
		//restore
		if (optind >= argc-1) {
			usage(command_index);
			exit(1);
		}
		char* f_id = real_argv[optind];
		FILE* proc = fopen(f_id,"r");	
		struct proc_process *p=NULL;
		if (!proc) {
			p = receive_proc_from_local_server(f_id);
		} else {
			p = read_process(proc,read_from_file_pointer);
		}
		if (p!=NULL)
			restore_process(*p);
	} else if (command_index==2) {
		//send
		if (optind >= argc-1 && name==NULL) {
			usage(command_index);
			exit(1);
		}
		pid_t pid = 0;
		struct proc_process *p=NULL;
		if (name!=NULL) {
			char tmpproc[100];
			sprintf(tmpproc,"ps ux | awk '/%s/ && !/awk/ {print $2}'",name);
			FILE* proc = popen(tmpproc,"r");
			fscanf(proc,"%d",&pid);
			pclose(proc);
			struct proc_process tmpp = save_process(pid,"test",nokill);
			p = &tmpp;
		} else {
			char* f_id = real_argv[optind];
			FILE* proc = fopen(f_id,"r");	
			if (!proc) {
				p = receive_proc_from_local_server(f_id);
				if (!p) {
					pid = atoi(real_argv[optind]);
					struct proc_process tmpp = save_process(pid,"test",nokill);
					p = &tmpp;
				}
			} else {
				p = read_process(proc,read_from_file_pointer);
			}
		}

		char* ip=DEFAULT_HOST;
		int port=DEFAULT_PORT;
		if (host!=NULL) {
			char* pos = strchr(host,':');
			if (pos!=NULL) {
				*pos='\0';
				ip=host;
				port=*(++pos);
			} else {
				ip=host;
				port = DEFAULT_PORT;
			}
		}
		send_process(*p,ip,port);
	} else if (command_index==3) {
		//receive
		char* id = NULL;
		if (optind >= argc-1) {
			id = real_argv[optind];
		}
		struct proc_process *p= receive_proc_from_local_server(id);
		if (p!=NULL)
			restore_process(*p);
	} else if (command_index==4) {
		//list
		list_from_local_server();
	} else if (command_index==5) {
		//delete
		if (optind >= argc-1) {
			usage(command_index);
			exit(1);
		}
		char id[SIZE_OF_ID];
		strcpy(id,real_argv[optind]);
		delete_from_local_server(id);
	} 
	return 0;
}
