#include <stdio.h>

int main(int argc, char** argv) {
	unsigned int i;
	for (i=0; i<2000000000; i++) if (!(i%1000000)) printf("%d\n",i);
	return 0;
}
