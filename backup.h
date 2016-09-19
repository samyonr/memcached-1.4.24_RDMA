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
#ifndef BACKUP_H_
#define BACKUP_H_

#define MAX_BACKUPS 3

/*
 * Receives an address to listen too and starts the RunBackupServer thread
 */
int BackupServer(char *clientHostnamePortwithPort);
/*
 * Receives an address to connect too, perform the connection and starts the RunBackupClient thread
 */
int BackupClient(char *clientHostnamePortwithPort);
/*
 * splits string accroding to the given delimiter
 */
char** str_split(char* a_str, const char a_delim);
/*
 * receivs data from sockfd, and returns it in buf.
 * numbytes is the number of received bytes.
 * The received data is null terminated.
 */
int receive(int sockfd, char *buf, int *numbytes);
/*
 * Load the given file into memory (RAM) and sends it in chunks via BSD Socket
 */
long ae_load_file_to_memory(const char *filename, char **result);
/*
 * Loads the given data into a file.
 */
long ae_load_memory_to_file(const char *filename, const char *data, const int size);

struct addr {
	char		*ip;
	char		*port;
};

#endif /* BACKUP_H_ */
