#include <stdio.h>

int main(int argc, char** argv) {
	unsigned int i;
	for (i=0; i<100; i++) {
		sleep(1);
		printf("%d\n",i);
	}
}
