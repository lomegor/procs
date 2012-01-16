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
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <getopt.h>
#include <semaphore.h>

#include "utils/list.h"
#include "utils/common.h"
#include "loader.h"

char* ip;
int port;


sem_t mutex;
sem_t next;
struct proc_process *next_process;
struct procs {
	char id[SIZE_OF_ID];
	struct proc_process* proc;
};
struct list procs_id;
int current_id=0;


void*
receive_process(int sock) {
	struct proc_process *pp = read_process(&sock,read_from_socket);
	syslog(LOG_INFO,"Process received... adding to list");
	sem_wait(&mutex);
	char tmp[30];
	sprintf(tmp,"PROCS%d",current_id++);
	struct procs* p = malloc(sizeof(struct procs));
	strcpy(p->id,tmp);
	p->proc=pp;
	list_append(&procs_id,p);
	next_process=pp;
	sem_post(&next);
	sem_post(&mutex);
	return p;
}

void*
receive_client(void* other) {
	receive_process(*(int*)other);
  close(*(int*)other);
	pthread_exit(NULL);
	return NULL;
}


void*
wait_for_receive(void* nothing) {
	int receiver;
	struct sockaddr_in mine;

	receiver = socket(AF_INET, SOCK_STREAM, 0);
	if (receiver < 0) {
	 	syslog(LOG_ERR,"Error creating receiving socket"); 
	 	pthread_exit(NULL);
		return NULL;
	}
	mine.sin_family = AF_INET; 
	mine.sin_addr.s_addr = inet_addr(ip);
	mine.sin_port = htons(port);
	if (bind(receiver,(struct sockaddr *)&mine,sizeof(mine)) < 0) {
	 	syslog(LOG_ERR,"Error binding receiving socket"); 
	 	pthread_exit(NULL);
		return NULL;
	}
	listen(receiver,5); 
	while (1) {	
		struct sockaddr_in them;
		pthread_t threadn;
		socklen_t cliente_len = sizeof(them);
		int client = accept(receiver,(struct sockaddr *)&them, &cliente_len);
		if (pthread_create(&threadn,NULL,receive_client,(void *) &client))
			syslog(LOG_WARNING,"Error binding receiving socket"); 
		else
			syslog(LOG_INFO,"Receiving process...");
	}
  close(receiver);
	pthread_exit(NULL);
	return NULL;
}

void
write_id(void* item,void* other) {
	int l = strlen(((struct procs *)item)->id);
	write(*(int*)other,((struct procs *)item)->id,l);
	unsigned char type = '\n';
	write(*(int*)other,&type,sizeof(type));
}

int
is_id(void* item,void* id) {
	return (id!=NULL && !strcmp(((struct procs *)item)->id,(char*)id));
}

void
send_process_to_admin(void*other, void* id, void* c) {
	if (!is_id(other,id))
			return;
	id = NULL;
	write_process(*(((struct procs *)other)->proc),c,write_to_socket);
	close(*(int*)c);
	return;
}

void*
admin(void* other) {
	int client = *(int*)other;
	unsigned char type;
	read_from_socket(&client,&type,sizeof(type));
	if (type=='l') {
		sem_wait(&mutex);
		list_iterate2(procs_id,&client,write_id);
		sem_post(&mutex);
		type=PROC_CHUNK_FINAL;
		write(client,&type,sizeof(type));
	} else if (type=='d') {
		char id[SIZE_OF_ID];
		read_from_socket(&client,&id,SIZE_OF_ID*sizeof(char));
		sem_wait(&mutex);
		struct item *p = (struct item*) list_delete(&procs_id,&id,is_id);
		if (p!=NULL) {
			free_process(((struct procs*)(p->p))->proc);
			free(p->p);
			free(p);
		}
		sem_post(&mutex);
	} else if (type=='r') {
		char id[SIZE_OF_ID];
		read_from_socket(&client,&id,SIZE_OF_ID*sizeof(char));
		sem_wait(&mutex);
		list_iterate3(procs_id,&id,&client,send_process_to_admin);
		if (id!=NULL) {
			type=PROC_CHUNK_FINAL;
			write(client,&type,sizeof(type));
		}
		sem_post(&mutex);
	} else if (type=='w') {
		sem_wait(&mutex);
		sem_init(&next,1,0);
		sem_post(&mutex);
		sem_wait(&next);
		sem_wait(&mutex);
		write_process(*next_process,&client,write_to_socket);
		sem_post(&mutex);
	} else if (type=='s') {
		struct procs *p = (struct procs*) receive_process(client);
		write_to_socket(&client,p->id,SIZE_OF_ID*sizeof(char));
	}
	close(client);
	pthread_exit(NULL);
	return NULL;
}

void*
wait_for_admin(void* nothing) {
	char tmp[30];
	sprintf(tmp,"rm -f %s",LOCAL_SERVER_SOCK);
	system(tmp);
	int receiver;
	struct sockaddr_un mine;

	receiver = socket(AF_UNIX, SOCK_STREAM, 0);
	if (receiver < 0) {
		syslog(LOG_ERR,"Error creating admin socket"); 
		pthread_exit(NULL);
		return NULL;
	}
	mine.sun_family = AF_UNIX; 
	strcpy(mine.sun_path,LOCAL_SERVER_SOCK);
	if (bind(receiver,(struct sockaddr *)&mine,sizeof(mine)) < 0) {
	 	syslog(LOG_ERR,"Error binding admins socket"); 
	 	pthread_exit(NULL);
		return NULL;
	}
	listen(receiver,5); 
	while (1) {	
		struct sockaddr_in them;
		pthread_t threadn;
		socklen_t cliente_len = sizeof(them);
		int client = accept(receiver,(struct sockaddr *)&them, &cliente_len);
		if (pthread_create(&threadn,NULL,admin,(void *) &client))
			syslog(LOG_WARNING,"Error creating admin"); 
		else
			syslog(LOG_INFO,"Receiving admin");
	}
  close(receiver);
	pthread_exit(NULL);
	return NULL;
}

extern char *optarg;
extern int optind, opterr, optopt;
extern int verbose;
char* program_name;

void
usage() {
	fprintf(stderr,"Usage: %s [HOSTNAME[:PORT]]... \n",program_name);
	fprintf(stderr,"Creates a local server to receive process from the procs program.\n");
	fprintf(stderr,"HOSTNAME and PORT default to %s and %d respectively.\n",DEFAULT_HOST,DEFAULT_PORT);
	fprintf(stderr,"\nList of options:\n");
	fprintf(stderr,"    -h    --help\t\tshows this help message and exit\n");
	fprintf(stderr,"    -f    --nofork\t\tdoes not fork daemon\n");
	fprintf(stderr,"    -v    --verbose\t\tverbose output\n");
}

void signal_handler(int sig) {
	switch(sig) {
		case SIGHUP:
			syslog(LOG_WARNING, "Received SIGHUP signal.");
			break;
		case SIGTERM:
			syslog(LOG_WARNING, "Received SIGTERM signal.");
			break;
		default:
			syslog(LOG_WARNING, "Unhandled signal (%d) %s", sig, strsignal(sig));
			break;
	}
	sem_destroy(&mutex);
	sem_destroy(&next);
	exit(0);
}

int 
main(int argc, char** argv) {
	list_init(&procs_id);
	program_name = argv[0];
	static struct option long_options[] = {
		{"nofork", 1, 0, 'f'},
		{"verbose", 0, 0, 'v'},
		{"help", 0, 0, 'h'},
		{NULL, 0, NULL, 0}
	};
	int c,option_index = 0;
	int forkit = TRUE;
	while ((c = getopt_long(argc, argv, "vhf", long_options, &option_index)) != -1) {
		switch (c) {
			case 'f':
				forkit=FALSE;
				break;
			case 'v':
				verbose=TRUE;	
				break;
			case 'h':
				usage();
				exit(0);
				break;
			default:
				fprintf(stderr,"?? getopt returned character code 0%o ??\n", c);
		}
	}

	openlog("procs_log",LOG_CONS,LOG_USER);	
	pid_t pid, sid;
	if (forkit) {
		pid = fork();
		if (pid < 0)
			exit(EXIT_FAILURE);
		if (pid > 0)
			exit(EXIT_SUCCESS);
		signal(SIGHUP, signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGINT, signal_handler);
		signal(SIGQUIT, signal_handler);
		sid = fork();
		if (sid < 0)
			exit(EXIT_FAILURE);
		else if (sid > 0)
			exit(EXIT_SUCCESS);
		umask(0);

		sid = setsid();
		if (sid < 0) {
			syslog(LOG_ERR,"Couldn't get sid");
			exit(EXIT_FAILURE);
		}
		if ((chdir("/")) < 0) {
			syslog(LOG_ERR,"Couldn't change dir");
			exit(EXIT_FAILURE);
		}
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}

	ip=DEFAULT_HOST;
	port=DEFAULT_PORT;
	if (optind < argc) {
		char* pos = strchr(argv[optind],':');
		if (pos!=NULL) {
			*pos='\0';
			ip=argv[optind];
			port=*(++pos);
		} else {
			ip=argv[optind];
			port = DEFAULT_PORT;
		}
	}

	sem_init(&mutex, 0, 1);
	sem_init(&next, 0, 1);
	pthread_t admin,receiver;
	pthread_create(&admin,NULL,wait_for_admin,(void*)NULL);
	pthread_create(&receiver,NULL,wait_for_receive,(void*)NULL);
	while(1) {
		//gather system data
		sleep(10000);
	}
	closelog();
	exit(EXIT_SUCCESS);
}
