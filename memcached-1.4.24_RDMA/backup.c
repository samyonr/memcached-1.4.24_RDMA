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

#define PORT "8888" // the port client will be connecting to

#define MAXDATASIZE 1000 // max number of bytes we can get at once
void *get_in_addr(struct sockaddr *sa);
int closeSocket(int sockfd);
void sigchld_handler(int s);
void *connection_handler(void *socket_desc);
void *RunBackupServer(void *arg);
int connectToServer(char *clientHostname, int *sockfd);
int ae_load_file_to_memory(const char *filename, char **result);
int ae_load_memory_to_file(const char *filename, const char *data, const int size);

static pthread_t g_serverThread;
static int g_backups_count = 0;
static int g_client_socketfd[MAX_BACKUPS];

int ae_load_file_to_memory(const char *filename, char **result)
{
	int size = 0;
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

int ae_load_memory_to_file(const char *filename, const char *data, const int size)
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

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int connectToServer(char *clientHostname, int *sockfd)
{
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rv = getaddrinfo(clientHostname, "8888", &hints, &servinfo);
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

int closeSocket(int sockfd)
{
	close(sockfd);
	return 0;
}

#define BACKLOG 10     // how many pending connections queue will hold

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

int sendBackupToClients(void)
{
	int i;
	char *content;
	char *content_temp;
	long size, size_temp;
	ssize_t n;
	size = ae_load_file_to_memory("/tmp/memkey/assoc_key1", &content);
	if (size < 0)
	{
		puts("Error loading file");
		return 1;
	}
	for (i = 0; i < g_backups_count; i++)
	{
		size_temp = size;
		content_temp = content;
		send((g_client_socketfd[i]), &size_temp, sizeof(long), 0);
		do
		{
			// putchar(content[size-1]);
			//TODO: if open
			//char c = content[size_temp-1];
			n = send((g_client_socketfd[i]), content_temp, size_temp, 0);
			content_temp += n;
			size_temp -= n;
		} while(size_temp > 0);
	}

	return 0;
}


int BackupClient(char *clientHostname)
{
	if (g_backups_count >= MAX_BACKUPS)
	{
		printf("Maximal number of backups reached\n");
		return -1;
	}

    if (connectToServer(clientHostname, &g_client_socketfd[g_backups_count]) == 0)
    {
        g_backups_count++;
        return 0;
    }
    else
    {
    	printf("Error creating client connection\n");
    	return -1;
    }
}

int BackupServer(void)
{
	int rv;
    //Create backup server thread
    rv = pthread_create(&g_serverThread, NULL, RunBackupServer, NULL);
    if(rv < 0)
    {
    	printf("Error creating backup server thread\n");
    }
    return 0;
}


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

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    rv = getaddrinfo(NULL, PORT, &hints, &servinfo);
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

void *connection_handler(void *socket_desc)
{
	//int i = 0;
	int received = 0;
	int sock = *(int*)socket_desc;
	long data_size = 0;
	char data[MAXDATASIZE];
	while (1)
	{
		received = 0;
		data_size = 0;
		memset(data, 0, MAXDATASIZE);
		received = recv(sock, &data_size, sizeof(long), 0);
		if (received == -1)
		{
			printf("error0\n");
		}
		FILE *f = fopen("/tmp/memkey/assoc_key1_copy", "wb");
		if (f == NULL)
		{
			printf("error1\n");
			return 0;
		}
		while (data_size > 0)
		{
			received = recv(sock, data, MAXDATASIZE, 0);
			if (received == -1)
			{
				printf("error00\n");
			}
			data_size -= received;
			if (fwrite(data, sizeof(char), received, f) == 0)
			{
				printf("error2\n");
				fclose(f);
				return 0;
			}
		}

		fclose(f);
		printf("downloaded succesfully\n");
		/*
		if (send(sock, "Hello, world!", 13, 0) == -1)
		{
			perror("send\n");
		}
		while(i < 10000000)
		{
			send(sock, "a", 1, 0);
			i++;
		}*/
	}
    close(sock);
    return 0;
}
