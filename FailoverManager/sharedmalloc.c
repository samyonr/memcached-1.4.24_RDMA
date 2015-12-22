#include "sharedmalloc.h"

/*
int file_lock(int fd){
  struct flock lock;
  lock.l_type = F_WRLOCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;
  lock.l_pid = getpid();
  return fcntl(fd, F_SETLKW, &lock);
}

int file_unlock(int fd){
}
*/
/*
int file_mutex_lock(char * key){
  int dir_stat = mkdir(KEYPATH, 0766);
  if(dir_stat == -1 && errno != EEXIST){
    perror("Locking Failure: ");
    return 0;
  }
  char *path = gen_full_path(key, LOCKPATH);
  if(!path){
    fprintf(stderr, "Locking Failure: Malloc Failed");
    return 0;
  }
  int fd;
  while((fd = open(path, O_RDWR | O_CREAT | OEXCL, 0666)) == -1){
    if(errno != EEXIST){
      free(path);
      return 0
    }
  }
  free(path);
  close(fd);
  return 1;
}

void file_mutex_unlock(char *key){
  char *path = gen_full_path(key, LOCKPATH);
  if(!path){
    return;
  }
  unlink(path);
  free(path);
}
*/
char *gen_full_path(const char* key, const char *dir);

char *gen_full_path(const char* key, const char *dir)
{
	size_t keylen = strlen(key);
	size_t pathlen = strlen(dir);
	char *ret = malloc(keylen+pathlen+1);
	if(!ret)
	{
		return NULL;
	}
	memcpy(ret, dir, pathlen);
	memcpy(ret+pathlen, key, keylen);
	ret[pathlen+keylen] = '\0';
	return ret;
}

void *shared_malloc(size_t size, const char *key, int lock)
{
	char *path;
	int dir_stat;
	int fd;
	int fallocate_ret;
	void *ret;
	dir_stat = mkdir(KEYPATH, 0766);
	if(dir_stat == -1 && errno != EEXIST)
	{
		perror("Error allocating shared memory: ");
		return NULL;
	}
	path = gen_full_path(key, KEYPATH);
	if(!path)
	{
		fprintf(stderr, "Error allocating shared memory: Malloc failed");
		return NULL;
	}

	/*open a file to map memory against*/
	fd = open(path, O_RDWR | O_CREAT, 0666);
	free(path);
	if(fd == -1)
	{
		perror("Error allocating shared memory: ");
		return NULL;
	}

	/*ensure the file is large enough*/
	fallocate_ret = posix_fallocate(fd, 0, size);
	if(fallocate_ret != 0)
	{
		errno = fallocate_ret;
		perror("Error allocating shared memory: ");
		close(fd);
		return NULL;
	}

	/*map the file into memory*/
	ret = mmap((caddr_t)0, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	if(ret == MAP_FAILED)
	{
 		perror("Error allocating shared memory: ");
		close(fd);
		return NULL;
	}
  
	/*close the file*/
	close(fd);
  
	/*lock the file in memory if a lock is requested*/
	if(lock == SOFT_LOCK || lock == HARD_LOCK)
	{
		if(mlock(ret, size) == -1)
		{
			perror("Error locking shared memory: ");
			if(lock == HARD_LOCK)
			{
				munmap(ret, size);
				return NULL;
      		}
    	}
  	}

	return ret;  
}

void *shared_realloc(void *ptr, size_t size, size_t old_size, char *key, int lock)
{
	void *ret = shared_malloc(size, key, lock);
	if(ret)
	{
		shared_free(ptr, old_size);
	}
	return ret;
}

void shared_free(void *ptr, size_t size)
{
	munmap(ptr, size);  
}
