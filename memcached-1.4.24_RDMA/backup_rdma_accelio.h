/*
 * backup_rdma_accelio.h
 *
 *  Created on: Apr 4, 2016
 *      Author: a315
 */

#ifndef BACKUP_RDMA_ACCELIO_H_
#define BACKUP_RDMA_ACCELIO_H_



int BackupServerRDMA(char *clientHostname, char *port);
int BackupClientRDMA(char *clientHostname, char *port);

#endif /* BACKUP_RDMA_ACCELIO_H_ */
