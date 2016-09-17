/*
 * backup.h
 *
 *  Created on: Mar 6, 2016
 *      Author: a315
 */

#ifndef BACKUP_H_
#define BACKUP_H_

#define MAX_BACKUPS 3

//void backup_init(void);
//bool create_backup(void);
int BackupServer(char *clientHostnamePortwithPort);
int BackupClient(char *clientHostnamePortwithPort);
char** str_split(char* a_str, const char a_delim);
int receive(int sockfd, char *buf, int *numbytes);
long ae_load_file_to_memory(const char *filename, char **result);
long ae_load_memory_to_file(const char *filename, const char *data, const int size);

struct addr {
	char		*ip;
	char		*port;
};

#endif /* BACKUP_H_ */
