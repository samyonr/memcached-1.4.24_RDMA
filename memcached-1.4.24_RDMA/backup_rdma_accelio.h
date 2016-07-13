/*
 * backup_rdma_accelio.h
 *
 *  Created on: Apr 4, 2016
 *      Author: a315
 */

#ifndef BACKUP_RDMA_ACCELIO_H_
#define BACKUP_RDMA_ACCELIO_H_



int BackupServerRDMA(char *clientHostnamePortwithPort);
int BackupClientRDMA(char *clientHostnamePortwithPort);

#endif /* BACKUP_RDMA_ACCELIO_H_ */
