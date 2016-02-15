/*FailoverManager*/
#include <stdio.h>
#include "sharedmalloc.h"

int main()
{
	void *memcached1_comm;
	void *memcached1_slabs;
	void *memcached1_slabs_lists;
	void *memcached1_assoc;
	void *memcached2_slabs;
	void *memcached2_slabs_lists;
	void *memcached2_assoc;
	int i = 0;
	char update_exist[] = "t";
	printf("FailoverManager had been started\n");
	memcached1_comm = shared_malloc(1, "failover_comm1", HARD_LOCK);
	memcached1_slabs = shared_malloc(4294967296, "slabsKey1", HARD_LOCK);
	memcached1_slabs_lists = shared_malloc(4325, "slabsListsKey1", HARD_LOCK);
	memcached1_assoc = shared_malloc(16777216,"assocKey1",HARD_LOCK);
	memcached2_slabs = shared_malloc(4294967296, "slabsKey2", HARD_LOCK);
	memcached2_slabs_lists = shared_malloc(4325, "slabsListsKey2", HARD_LOCK);
	memcached2_assoc = shared_malloc(16777216,"assocKey2",HARD_LOCK);
	
	/* memset(data, 0x0, 1); */
	while(1)
	{
		usleep(500*1000);
		if (strcmp ((char *) memcached1_comm,update_exist) == 0)
		{
			printf("%d: update exist\n", i);
			printf("%d: copy slabs\n", i);
			memcpy(memcached2_slabs, memcached1_slabs,4294967296);
			printf("%d: copy slabs lists\n", i);
			memcpy(memcached2_slabs_lists, memcached1_slabs_lists,4325);
			printf("%d: copy assoc\n", i);
			memcpy(memcached2_assoc, memcached1_assoc,16777216);
			printf("%d: copy completed\n", i);
			printf("%d: cleaning update\n", i);
			memset(memcached1_comm,102 /* 102 is 'f' in ascii - for false */ , 1);
			printf("%d: update cleaned\n", i);
		}
		else
		{
			printf("%d: no update\n", i);
		}
		
		i++;
	}
	printf("cleaning all allocated memory");
	shared_free(memcached1_comm, 1);
	shared_free(memcached1_slabs, 4294967296);
	shared_free(memcached1_slabs_lists, 4325);
	shared_free(memcached1_assoc, 16777216);
	shared_free(memcached2_slabs, 4294967296);
	shared_free(memcached2_slabs_lists, 4325);
	shared_free(memcached2_assoc, 16777216);
	printf("memory clean - stopping failover manager\n");
	return 0;
}