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
int sendBackupToClients(void);
int BackupServer(void);
int BackupClient(char *clientHostname);
char** str_split(char* a_str, const char a_delim);
int receive(int sockfd, char *buf, int *numbytes);

#endif /* BACKUP_H_ */
