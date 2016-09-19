/*
 * Added as part of the memcached-1.4.24_RDMA project.
 * Allocates shared memory of a given size.
 * The memory is shared across all processes that use the same key.
 * sharedmalloc is implemented using mmap.
 * Created by Benjamin (Ben) Chaney.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define NO_LOCK 0
#define SOFT_LOCK 1
#define HARD_LOCK 2
char *gen_full_path(const char*key, const char *dir);
#define KEYPATH "/tmp/memkey/"
#define LOCKPATH "/tmp/memlock/"

/*shared_malloc allocates shared memory of a given size
//it is shared accross all processes that use the same key
//(filename) to initialize it.
//return: a ptr pointing to a memory segment of length size
//or NULL on failure
//size: the amount of memory to be allocated in bytes
//key: the key to map with the key should be long and
//unique for seperate memory segments
//lock: how to lock the given section of memory into
//      physical memory possible values are:
//      NO_LOCK: do not attempt to lock into physical memory
//      SOFT_LOCK: attempt to lock, but allow locking to fail
//      HARD_LOCK: the allocation will fail if it can not be locked
*/
void *shared_malloc(void *addr, size_t size, const char *key, int lock);

/*shared_free frees memory that was allocated by shared_malloc
//it must be called in every process that calles shared_malloc
//and calling it in one processs does not invalidate another
//processes access to the memory. Memory containing sensative
//information that is allocated in this way should be cleared
//by some process before being freed
*/
void shared_free(void *ptr, size_t size);

/*resizes a buffer that was allocated with shared_malloc
//when the key  or the old size does not correspond to
//the value that was originally passed to malloc (or a
//previous realloc) behavor is undefined. Lock status can
//be changed by realloc
//ptr: a ptr to the memoy section to be resized
//size: same as shared_malloc
//old_size: the size that was previously passed when *ptr
//          was allocated
//key: the key that was previously passed when *ptr was allocated
//lock: same as malloc
*/
void *shared_realloc(void *addr, void *prt, size_t size, size_t old_size, char *key, int lock);
