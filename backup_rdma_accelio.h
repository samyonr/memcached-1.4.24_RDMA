
/*
 * Added as part of the memcached-1.4.24_RDMA project.
 * Implementing backup system via RDMA using Accelio.
 * BackupClientRDMA method receives the address to connect too,
 * and starts the RunBackupClientRDMA thread.
 * RunBackupClientRDMA creates a connection and starts an event loop.
 * On receiving response to a generic beacon message, the client samples the queue,
 * and if the queue is not empty, starts the backup session.
 * BackupServer receives the address to listen too,
 * and starts the RunBackupServerRDMA thread.
 * RunBackupServerRDMA starts and event loop, and responds to clients messages.
 */

#ifndef BACKUP_RDMA_ACCELIO_H_
#define BACKUP_RDMA_ACCELIO_H_


/*
 * Receives an address to listen too and starts the RunBackupServerRDMA thread
 */
int BackupServerRDMA(char *clientHostnamePortwithPort);
/*
 * Receives an address to connect too and starts the RunBackupClientRDMA thread
 */
int BackupClientRDMA(char *clientHostnamePortwithPort);

#endif /* BACKUP_RDMA_ACCELIO_H_ */
