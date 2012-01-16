#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv) {
	char map_line[100];
	unsigned int i;
	int f,fw,r;
	f = open("tests/test5.c",O_RDONLY);
	fw = open("tmpsrc5.c",O_WRONLY);
	while ((r=read(f,map_line,sizeof(map_line)))>0) {
		write(fw,map_line,r);
		for (i=0; i<2000000000; i++);
	}
	return 0;
}
