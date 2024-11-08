#include <stdio.h>

int main(void)
{
	unsigned u = -1;
	u >>= 1;
	printf("%u", u);
	
	int i = -1;
	i >>= 1;
	printf("%i", i);
}
