#include <stdio.h>
#include "sharedmalloc.h"

int main(int argc, const char *argv[])
{
	void *data;
	int i = 0;
	char c;
	int ic;
	data = shared_malloc(1, "mappedfile.dat", HARD_LOCK);
	while (i < 10)
	{
		scanf(" %c", &c, 1);
		ic = (int)c;
		printf("%d: Your choose %c, which is %d in ascii\n", i,c,ic);
		memset(data, ic, 1);
		i++;
	}
	shared_free(data, 1);
	return 0;
}