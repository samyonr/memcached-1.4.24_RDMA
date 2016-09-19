/********************************************************
 * Added as part of the memcached-1.4.24_RDMA project.
 * Implementing backup system via BSD sockets.
 * BackupClient method receives the address to connect too,
 * connects to the Memcached Backup Server in connectToServer method, and runs a RunBackupClient thread.
 * The client thread samples the queue every 2 seconds, and when there is an item in the queue,
 * starts the backup process.
 * BackupServer method receives an address to listen too,
 * creates a RunBackupServer thread, and on each incoming connection starts connection_handler thread.
 * After the connection with the client is establisged, the backup receives the memory backup, and closes the connection.
 ********************************************************/

#include "backup.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <assert.h>
#include "queue.h"
#include "sharedmalloc.h"
#include "memcached.h"

#define MAXDATASIZE 10000 // max number of bytes we can get at once
/*
 * get sockaddr, IPv4 or IPv6:
 */ 
void *get_in_addr(struct sockaddr *sa);
/*
 * Closes the socket
 */
int closeSocket(int sockfd);
/*
 * Handels SIGCHLD Signal
 */
void sigchld_handler(int s);
/*
 * Server connection handler thread.
 * Receives the memory backup within 3 steps - assoc, slabs and slabs_lists.
 */
void *connection_handler(void *socket_desc);
/*
 * Server backup thread. Waits for incoming communication.
 */
void *RunBackupServer(void *arg);
/*
 * Samples the queue every 2 seconds. When the queue is not empty,
 * starts the backup process - sebds the assoc, slabs and slab_lists memory sections.
 */
void *RunBackupClient(void *arg);
/*
 * Connects via BSD Socket to the given server.
 */
int connectToServer(char *clientHostname, char *clientPort, int *sockfd);
/*
 * Load the given file into memory (RAM) and sends it in chunks via BSD Socket
 */
int sendBackupToClients(char *fileToSend, char *msg, int msgSize);

static pthread_t g_serverThread;
static int g_backups_count = 0;
static int g_client_socketfd[MAX_BACKUPS];

/*
 * Loads the given file into the memory (RAM)
 */
long ae_load_file_to_memory(const char *filename, char **result)
{
	long size = 0;
	FILE *f = fopen(filename, "rb");
	if (f == NULL)
	{
		*result = NULL;
		return -1; // -1 means file opening fail
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	*result = (char *)malloc(size+1);
	if (size != fread(*result, sizeof(char), size, f))
	{
		free(*result);
		return -2; // -2 means file reading fail
	}
	fclose(f);
	(*result)[size] = 0;
	return size;
}

/*
 * Loads the given data into a file.
 */
long ae_load_memory_to_file(const char *filename, const char *data, const int size)
{
	FILE *f = fopen(filename, "wb");
	if (f == NULL)
	{
		return -1;
	}
	if (fwrite(data, sizeof(char), size, f) == 0)
	{
		fclose(f);
		return -1;
	}
	fclose(f);
	return 1;

}

/*
 * splits string accroding to the given delimiter
 */
char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

/*
 * get sockaddr, IPv4 or IPv6:
 */ 
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*
 * Connects via BSD Socket to the given server.
 */
int connectToServer(char *clientHostname, char *clientPort, int *sockfd)
{
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rv = getaddrinfo(clientHostname, clientPort, &hints, &servinfo);
    if (rv != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
    	*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (*sockfd == -1)
        {
            perror("client: socket\n");
            continue;
        }
        rv = connect(*sockfd, p->ai_addr, p->ai_addrlen);
        if (rv == -1)
        {
            close(*sockfd);
            perror("client: connect\n");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    return 0;
}

/*
 * receivs data from sockfd, and returns it in buf.
 * numbytes is the number of received bytes.
 * The received data is null terminated.
 */
int receive(int sockfd, char *buf, int *numbytes)
{
    *numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0);
    if (*numbytes == -1)
    {
        perror("recv\n");
        exit(1);
    }

    buf[*numbytes] = '\0';

    printf("client: received '%s'\n",buf);

    return 0;
}

/*
 * Closes the socket
 */
int closeSocket(int sockfd)
{
	close(sockfd);
	return 0;
}

#define BACKLOG 10     // how many pending connections queue will hold

/*
 * Handels SIGCHLD Signal
 */
void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

/*
 * Load the given file into memory (RAM) and sends it in chunks via BSD Socket
 */
int sendBackupToClients(char *fileToSend, char *msg, int msgSize)
{
	int i;
	char *content;
	char *content_temp;
	long size, size_temp;
	ssize_t n;
	size = ae_load_file_to_memory(fileToSend, &content);
	if (size < 0)
	{
		puts("Error loading file");
		return 1;
	}
	for (i = 0; i < g_backups_count; i++)
	{
		size_temp = size;
		content_temp = content;
		send((g_client_socketfd[i]), msg, msgSize, 0);
		send((g_client_socketfd[i]), &size_temp, sizeof(long), 0);
		do
		{
			n = send((g_client_socketfd[i]), content_temp, size_temp, 0);
			content_temp += n;
			size_temp -= n;
		} while(size_temp > 0);
	}

	free(content);
	return 0;
}


int BackupClient(char *clientHostnamePortwithPort)
{
	int rv;
	char** hostAndPort = str_split(clientHostnamePortwithPort, ':');
	struct addr	*addr = (struct addr*)malloc(sizeof(struct addr));
	addr->ip = hostAndPort[0];
	addr->port = hostAndPort[1];

	if (g_backups_count >= MAX_BACKUPS)
	{
		printf("Maximal number of backups reached\n");
		return -1;
	}

    if (connectToServer(hostAndPort[0], hostAndPort[1] , &g_client_socketfd[g_backups_count]) != 0)
    {
    	printf("Error creating client connection\n");
    	return -1;

    }
    g_backups_count++;

    //Create backup server thread
    rv = pthread_create(&g_serverThread, NULL, RunBackupClient, (void*) addr);
    if(rv < 0)
    {
    	printf("Error creating backup client thread\n");
    	return -1;
    }

    return 0;

}

int BackupServer(char *clientHostnamePortwithPort)
{
	int rv;
	char** hostAndPort = str_split(clientHostnamePortwithPort, ':');
	struct addr	*addr = (struct addr*)malloc(sizeof(struct addr));
	addr->ip = hostAndPort[0];
	addr->port = hostAndPort[1];

    //Create backup server thread
    rv = pthread_create(&g_serverThread, NULL, RunBackupServer, (void*) addr);
    if(rv < 0)
    {
    	printf("Error creating backup server thread\n");
    }
    return 0;
}

/*
 * Samples the queue every 2 seconds. When the queue is not empty,
 * starts the backup process - sebds the assoc, slabs and slab_lists memory sections.
 */
void *RunBackupClient(void *arg)
{
	int queue_val;
	char *path;

	while (1)
	{
		//check if there a message waiting in the queue
		if (!queue_empty())
		{
				queue_val = queue_frontelement();
				printf("Got something in the queue! value = %d\n",queue_val);
				queue_deq();
				path = gen_full_path(settings.shared_malloc_assoc_key, KEYPATH);
				sendBackupToClients(path,"queue data step 1 sending", 25);
				free(path);
				path = gen_full_path(settings.shared_malloc_slabs_key, KEYPATH);
				sendBackupToClients(path,"queue data step 2 sending", 25);
				free(path);
				path = gen_full_path(settings.shared_malloc_slabs_lists_key, KEYPATH);
				sendBackupToClients(path,"queue data step 3 sending", 25);
				free(path);

		}
		sleep(2);
	}
}

/*
 * Server backup thread. Waits for incoming communication.
 */
void *RunBackupServer(void *arg)
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    struct addr 		*addr = (struct addr*)arg;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    rv = getaddrinfo(NULL, addr->port, &hints, &servinfo);
    if (rv != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
    	sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
        {
            perror("server: socket\n");
            continue;
        }

    	rv = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (rv == -1)
        {
            perror("setsockopt\n");
            exit(1);
        }

        rv = bind(sockfd, p->ai_addr, p->ai_addrlen);
        if (rv == -1) {
            close(sockfd);
            perror("server: bind\n");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    rv = listen(sockfd, BACKLOG);
    if (rv == -1)
    {
        perror("listen\n");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    rv = sigaction(SIGCHLD, &sa, NULL);
    if (rv == -1)
    {
        perror("sigaction\n");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) // main accept() loop
    {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept\n");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        //Create receive thread
        pthread_t thread;
        rv = pthread_create(&thread, NULL , connection_handler, (void*) &new_fd);
        if(rv < 0)
        {
        	printf("Error creating receive thread\n");
        }
    }

    exit(0);
}

/*
 * Server connection handler thread.
 * Receives the memory backup within 3 steps - assoc, slabs and slabs_lists.
 */
void *connection_handler(void *socket_desc)
{
	int received = 0;
	int sock = *(int*)socket_desc;
	long data_size = 0;
	long original_data_size = 0;
	char msg[25];
	char data[MAXDATASIZE];
	int step = 0;
	data_size = 0;
	void *memcached1_slabs = NULL;
	void *memcached1_slabs_lists = NULL;
	void *memcached1_assoc = NULL;
	while (1)
	{
		received = 0;
		memset(data, 0, MAXDATASIZE);
		memset(msg, 0, 25);
		if (step == 0)
		{
			received = recv(sock, &msg, sizeof(char) * 25, 0);
			if (received == -1 || received != 25)
			{
				printf("error0\n");
			}
			if (strncmp(msg,"queue data step 1 sending",25) == 0)
			{
				received = recv(sock, &data_size, sizeof(long), 0);
				if (received == -1)
				{
					printf("error1\n");
				}
				original_data_size = data_size;
				step = 1;
				memcached1_assoc = shared_malloc(NULL, original_data_size,settings.shared_malloc_assoc_key,NO_LOCK);

				printf("Moving to step 1\n");
			}

		}
		if (step == 1 && data_size != 0)
		{
			while (data_size > 0)
			{
				memset(data, 0, MAXDATASIZE);
				if (data_size < MAXDATASIZE)
				{
					received = recv(sock, data, data_size, 0);
				}
				else
				{
					received = recv(sock, data, MAXDATASIZE, 0);
				}
				if (received == -1)
				{
					printf("error3\n");
				}
				memcpy((char *)memcached1_assoc + original_data_size - data_size, data,received);

				data_size -= received;

			}
			shared_free(memcached1_assoc, original_data_size);
			original_data_size = 0;
			printf("Finished step 1\n");
		}
		if (step == 1 && data_size == 0)
		{
			received = recv(sock, &msg, sizeof(char) * 25, 0);
			if (received == -1 || received != 25)
			{
				printf("error5\n");
			}
			if (strncmp(msg,"queue data step 2 sending",25) == 0)
			{
				received = recv(sock, &data_size, sizeof(long), 0);
				if (received == -1)
				{
					printf("error6\n");
				}
				original_data_size = data_size;
				step = 2;


				memcached1_slabs = shared_malloc(NULL, original_data_size, settings.shared_malloc_slabs_key, NO_LOCK);
			}
			printf("Moving to step 2\n");
		}
		if (step == 2 && data_size != 0)
		{
			while (data_size > 0)
			{
				memset(data, 0, MAXDATASIZE);
				if (data_size < MAXDATASIZE)
				{
					received = recv(sock, data, data_size, 0);
				}
				else
				{
					received = recv(sock, data, MAXDATASIZE, 0);
				}
				if (received == -1)
				{
					printf("error8\n");
				}

				memcpy((char *)memcached1_slabs + original_data_size - data_size, data,received);

				data_size -= received;

			}
			shared_free(memcached1_slabs, original_data_size);
			original_data_size = 0;
			printf("Finished step 2\n");
		}
		if (step == 2 && data_size == 0)
		{
			received = recv(sock, &msg, sizeof(char) * 25, 0);
			if (received == -1 || received != 25)
			{
				printf("error10\n");
			}
			if (strncmp(msg,"queue data step 3 sending",25) == 0)
			{
				received = recv(sock, &data_size, sizeof(long), 0);
				original_data_size = data_size;
				if (received == -1)
				{
					printf("error11\n");
				}
				step = 3;

				memcached1_slabs_lists = shared_malloc(NULL, original_data_size, settings.shared_malloc_slabs_lists_key, NO_LOCK);

			}
			printf("Moving to step 3\n");
		}
		if (step == 3 && data_size != 0)
		{
			while (data_size > 0)
			{
				memset(data, 0, MAXDATASIZE);
				if (data_size < MAXDATASIZE)
				{
					received = recv(sock, data, data_size, 0);
				}
				else
				{
					received = recv(sock, data, MAXDATASIZE, 0);
				}
				if (received == -1)
				{
					printf("error13\n");
				}

				memcpy((char *)memcached1_slabs_lists + original_data_size - data_size, data,received);

				data_size -= received;

			}
			shared_free(memcached1_slabs_lists, original_data_size);
			original_data_size = 0;
			printf("Finished step 3\n");
		}
		step = 0;
	}
    close(sock);
    printf("Downloaded backup successfully\n");
    return 0;
}
