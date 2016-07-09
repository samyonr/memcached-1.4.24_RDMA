/*
 * backup_rdma_accelio.h
 *
 *  Created on: Apr 4, 2016
 *      Author: a315
 */

#ifndef BACKUP_RDMA_ACCELIO_H_
#define BACKUP_RDMA_ACCELIO_H_



int BackupServerRDMA(char *clientHostname, uint64_t port);
int BackupClientRDMA(char *clientHostname, uint64_t port);

#endif /* BACKUP_RDMA_ACCELIO_H_ */
