#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv) {
	char map_line[100];
	unsigned int i;
	int f,r;
	f = open("tests/test5.c",O_RDONLY);
	while ((r=read(f,map_line,sizeof(map_line)))>0) {
		map_line[r]='\0';
		printf("%s",map_line);
		for (i=0; i<2000000000; i++);
	}
	return 0;
}
