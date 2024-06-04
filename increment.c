#include <stdio.h>
#include <stdint.h>

int main(){
	uint32_t a, b;
	scanf("%u %u", &a, &b);
	printf("%u", ((a - 1) / b + 1)*b-a);
	return 0;
}