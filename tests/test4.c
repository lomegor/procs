#include <stdio.h>

void recursion(int i) {
	if (i==0) return;
	sleep(1);
	printf("%d\n",i);
	recursion(i-1);
}

int main(int argc, char** argv) {
	recursion(100);
}
