
#include "request.h"
#ifdef GEM5
#include "m5op.h"
#endif

int sendRequest(struct request* request, int *old_sock) {

  //Send out all requests (only one unless multiget
 //// printf("entered sendRequest\n");
  struct request* sendRequest = request;
  int conn_err = 0;
  if(request->connection->protocol == TCP_MODE){
    tcpSendRequest(sendRequest, &conn_err, old_sock);
  } else if(request->connection->protocol == UDP_MODE){ 
    printf("UDP not working\n"); 
    exit(-1);
    udpSendRequest(sendRequest);
  } else {
    printf("Undefined protocol\n");
    exit(-1);
  }
  //struct request* sendRequest = request;
  //while(sendRequest != NULL) {
  ////  printf("request op %d\n", sendRequest->header.opcode);
  //  if(request->connection->protocol == TCP_MODE){
  //    tcpSendRequest(sendRequest);
  //  } else if(request->connection->protocol == UDP_MODE){
  //    udpSendRequest(sendRequest);
  //  } else {
  //    printf("Undefined protocol\n");
  //    exit(-1);
  //  }
  //  sendRequest = sendRequest->next_request;
  //}//End while

  if (conn_err == 1)
  {
     printf("tcp returning -1\n");
     return -1;
  }
  return 1;
}//End sendRequest()
  
void tcpSendRequest(struct request* request, int *conn_err, int *old_sock) {
  
  struct request* sendRequest = request;
  int server = request->connection_server;
  if (request->next_request != NULL)
  {
    printf("send ohhhhhhhh noooooooooooooooooooooooooooooo\n");
  }
  if (request->server_variant < request->worker->connection_server_variant[server]) //someone already handeled the variant
  {
    printf("server number %d, fd %d. on send request. updating variant and connection\n", server, request->connection->sock);
    request->server_variant++;
    request->connection = request->worker->connections[server];
    *conn_err = 1;
    return;
  }
#ifdef GEM5
      m5_work_begin(sendRequest->header.opcode, sendRequest->header.opaque); 
#endif
  if(request->bad_multiget)
  {
    printf("request->bad_multiget\n");
    while(sendRequest != NULL) {
       printf("request->bad_multiget in while\n");
      int totalSize = sendRequest->value_size + sendRequest->key_size + sendRequest->header.extras_length + sizeof(struct request_header);
      
      char* oneBigPacket = malloc(sizeof(char) * totalSize);
      char* ptr = oneBigPacket;

      memcpy(ptr, (char *) (& sendRequest->header), sizeof(struct request_header));
      ptr += sizeof(struct request_header);

      memcpy(ptr, sendRequest->extras, sendRequest->header.extras_length);
      ptr += sendRequest->header.extras_length;

      memcpy(ptr, sendRequest->key, sendRequest->key_size);
      ptr += sendRequest->key_size;

      memcpy(ptr, sendRequest->value, sendRequest->value_size);

      gettimeofday(&request->send_time, NULL);

      printf("sending request (writing block) for server %d, port %d, sock %d\n",request->connection_server,request->connection->port, request->connection->sock);
      int result = writeBlock(request->connection->sock, oneBigPacket, totalSize);
      
      free(oneBigPacket);

      if (result == -1)
      {
         printf("WOW - IM HERE???\n");
         exit(-1);
      }
      else
      {
        sendRequest = sendRequest->next_request;
      }
    }
  }
  else
  { 
    int totalSize = 0;
    while(sendRequest != NULL) {
      totalSize += sendRequest->value_size + sendRequest->key_size + sendRequest->header.extras_length + sizeof(struct request_header);
      sendRequest = sendRequest->next_request;
    }
    char* oneBigPacket = malloc(sizeof(char) * totalSize);
    char* ptr = oneBigPacket;

    sendRequest = request;
    while(sendRequest != NULL) {
      memcpy(ptr, (char *) (& sendRequest->header), sizeof(struct request_header));
      ptr += sizeof(struct request_header);

      memcpy(ptr, sendRequest->extras, sendRequest->header.extras_length);
      ptr += sendRequest->header.extras_length;

      memcpy(ptr, sendRequest->key, sendRequest->key_size);
      ptr += sendRequest->key_size;

      memcpy(ptr, sendRequest->value, sendRequest->value_size);
      sendRequest = sendRequest->next_request;
    }

    gettimeofday(&request->send_time, NULL);
    int result = writeBlock(request->connection->sock, oneBigPacket, totalSize);
    free(oneBigPacket);

    if (result == -1)
    {

      int a = pthread_mutex_lock(&move_connection_lock);
      printf("request - server number %d: lock set. fd %d, status %d\n",server, request->connection->sock,a);
      *conn_err = 1;
      if (request->server_variant != request->worker->connection_server_variant[server]) //someone already handeled the variant
      {
         printf("server number %d, fd %d. just updating variant\n", server, request->connection->sock);
         request->server_variant++;    //TODO: free lock   
         return;
      }
     
      //deleteEvents(request->connection->sock, request->worker->event_map, request->worker->nEvents);
      printf("server number %d: had write error on fd %d\n", server, request->connection->sock);
      *old_sock = request->connection->sock;

      if (request->worker->connection_server_variant[server] != 2)
      {
         request->worker->connection_server_variant[server]++;
         printf("server number %d: moving to variant %d \n", server, request->worker->connection_server_variant[server]);
         if (request->worker->connection_server_variant[server] == 1)
         {
            printf("server number %d: setting connection. old addrss - %s, new address - %s, old port - %d, new port - %d\n",
					server ,request->worker->config->server_ip_address[0] ,
					request->worker->config->server_ip_address_backup[0], 
					request->worker->config->server_port[0],request->worker->config->server_port_backup[0]);
	        request->worker->connections[server] = createConnection(request->worker->config->server_ip_address_backup[0], 
				  												    request->worker->config->server_port_backup[0], 
					  											    request->worker->config->protocol_mode, 
																    request->worker->config->naggles);
            printf("server number %d: new connection set\n", server);
		    printf("server number %d: previous sock: %d, new sock: %d\n",server, request->connection->sock, request->worker->connections[server]->sock);
            request->connection = request->worker->connections[server];
            request->server_variant++;
            createEvents(server, request->worker);
         }
         else if (request->worker->connection_server_variant[server] == 2)
         {
            printf("!!!!!!!!!!!!!!!!!!!!!! TRYING SECOND BACKUP for server numbr %d\n", server);
	        request->worker->connections[server] = createConnection(request->worker->config->server_ip_address_backup_2[0], 
																    request->worker->config->server_port_backup_2[0], 
																    request->worker->config->protocol_mode, 
																    request->worker->config->naggles);
            request->connection = request->worker->connections[server];
         }
         else
         {
            printf("connection server variant is not in range\n");
            exit(-1);
         }
      }
      printf("request - 1. server number %d: lock free. fd %d\n",server, request->connection->sock);
      int b = pthread_mutex_unlock(&move_connection_lock);
      printf("request - 2. server number %d: lock free. fd %d, status %d\n",server, request->connection->sock,b);
    }
  }
}//End tcpSendRequest

//Each UDP datagram contains a simple frame header, followed by data in the
//same format as the TCP protocol described above. In the current
//implementation, requests must be contained in a single UDP datagram, but
//responses may span several datagrams. (The only common requests that would
//span multiple datagrams are huge multi-key "get" requests and "set"
//requests, both of which are more suitable to TCP transport for reliability
//reasons anyway.)
//
//The frame header is 8 bytes long, as follows (all values are 16-bit integers
//in network byte order, high byte first):
//
//0-1 Request ID
//2-3 Sequence number
//4-5 Total number of datagrams in this message
//6-7 Reserved for future use; must be 0
void udpSendRequest(struct request* request) {

 int totalSize = request->value_size + request->key_size + request->header.extras_length + sizeof(struct request_header) + 8;

 char* oneBigPacket = malloc(totalSize);
 char* ptr = oneBigPacket;

 int requestId = request->worker->current_request_id;
 request->id = requestId;
 request->worker->current_request_id = (requestId + 1) % 0xFFFF;

 oneBigPacket[0] = (char)(requestId & 0xFFFF) >> 16;
 oneBigPacket[1] = (char)(requestId & 0xFF);
 oneBigPacket[2] = 0x00;
 oneBigPacket[3] = 0x00;
 oneBigPacket[4] = 0x00;
 oneBigPacket[5] = 0x01;
 oneBigPacket[6] = 0x00;
 oneBigPacket[7] = 0x00;

 ptr += 8;

 memcpy(ptr, (char *) (& request->header), sizeof(struct request_header));
 ptr += sizeof(struct request_header);

 memcpy(ptr, request->extras, request->header.extras_length);
 ptr += request->header.extras_length;

 memcpy(ptr, request->key, request->key_size);
 ptr += request->key_size;

 memcpy(ptr, request->value, request->value_size);

 int fd = request->connection->sock;
 gettimeofday(&request->send_time, NULL);
 writeBlock(fd, oneBigPacket, totalSize);
 free(oneBigPacket);

}//End udpSendRequest()

void deleteRequest(struct request* request) {

  struct request* currentRequest = request;
  while(currentRequest != NULL) {

    if(currentRequest->value != NULL){
      free(currentRequest->value);
    }

    if(currentRequest->extras != NULL){
      free(currentRequest->extras);
    } 


    struct request* nextRequest;
    nextRequest = currentRequest->next_request;
    free(currentRequest);
    currentRequest = nextRequest;

  }//End while

}//End deleteRequest()

int generateUID(struct worker* worker) {

  struct config* config = worker->config;

  uint32_t uid = __sync_fetch_and_add(&(config->current_request_uid), 1);
  
  return uid;

}


struct request* createRequest(int requestType, struct conn* conn, struct worker* worker, char* key, char* value, int type, int connection_server) {

  struct request* request = malloc(sizeof(struct request));
  request->worker = worker;
  request->bad_multiget = 0;

  if(conn == NULL){
    printf("Tried to give request a null connection\n");
    exit(-1);
  }
  request->connection = conn;
  request->connection_server = connection_server;
  request->server_variant = worker->connection_server_variant[connection_server];

  int keyLength = 0;
  if(key != NULL) {
    keyLength = strlen(key);
  }

  if(keyLength > MAX_KEY_LENGTH) {
    printf("The key is too long!\nkey: %s\nlength: %d\n", key, keyLength);
    exit(-1);
  }

  int valueLength = 0;
  if(value != NULL) {
    valueLength = strlen(value);
  } 

  if(valueLength > MAX_VALUE_LENGTH) {
    printf("The value is too long!\nvalue: %s\nlength: %d\n", value, valueLength);
  }
  request->request_type = type;

  struct request_header* request_header = &(request->header);
  request_header->magic = MAGIC_REQUEST;
  request_header->data_type = 0;

  request_header->reserved[0] = 0;
  request_header->reserved[1] = 0;

  memset(&request_header->CAS, 0, 8);
  // We are using the opaque field for UIDs that will be sent back to use
  // in the response packet
  request_header->opaque = generateUID(worker);

  switch(requestType) {

    case STAT:{

      request_header->opcode = OP_STAT;

      break;

    }//End case STAT
    case ADD:{

      int body_length = 0;

      request_header->opcode = OP_ADD;

      request_header->key_length[0] = ((unsigned int)(strlen(key) & 0xff00))>>8;
      request_header->key_length[1] = (strlen(key) & 0xff);

      request->key = key;
      request->key_size = keyLength;
      request->value = value;
      request->value_size = valueLength;

      //Extra information
      request_header->extras_length = (char)8;
      request->extras = malloc(8);
      request->extras[0] = 0xde;
      request->extras[1] = 0xad;
      request->extras[2] = 0xbe;
      request->extras[3] = 0xef;
      request->extras[4] = 0;
      request->extras[5] = 0;
      request->extras[6] = 0;
      request->extras[7] = 0;

      body_length = 8 + keyLength + valueLength;
      #if DEBUG
      printf("body_length %d\n", body_length);
      #endif

      request_header->total_body_length[3] = (body_length & 0xff);
      request_header->total_body_length[2] = ((unsigned int)(body_length & 0xff00))>>8;
      request_header->total_body_length[1] = ((unsigned int)(body_length & 0xff0000))>>16;
      request_header->total_body_length[0] = ((unsigned int)(body_length & 0xff000000))>>24;
      break;

    }//End case REP
    case REP:{

      int body_length = 0;

      request_header->opcode = OP_REP;

      request_header->key_length[0] = ((unsigned int)(strlen(key) & 0xff00))>>8;
      request_header->key_length[1] = (strlen(key) & 0xff);

      request->key = key;
      request->key_size = keyLength;
      request->value = value;
      request->value_size = valueLength;

      //Extra information
      request_header->extras_length = (char)8;
      request->extras = malloc(8);
      request->extras[0] = 0xde;
      request->extras[1] = 0xad;
      request->extras[2] = 0xbe;
      request->extras[3] = 0xef;
      request->extras[4] = 0;
      request->extras[5] = 0;
      request->extras[6] = 0;
      request->extras[7] = 0;

      body_length = 8 + keyLength + valueLength;
      #if DEBUG
      printf("body_length %d\n", body_length);
      #endif

      request_header->total_body_length[3] = (body_length & 0xff);
      request_header->total_body_length[2] = ((unsigned int)(body_length & 0xff00))>>8;
      request_header->total_body_length[1] = ((unsigned int)(body_length & 0xff0000))>>16;
      request_header->total_body_length[0] = ((unsigned int)(body_length & 0xff000000))>>24;
      break;

    }//End case REP
    case DEL:{

      request_header->opcode = OP_DEL;

      request_header->key_length[0] = ((unsigned int)(strlen(key) & 0xff00))>>8;
      request_header->key_length[1] = (strlen(key) & 0xff);

      request->key = key;
      request->key_size = keyLength;

      request_header->extras_length = 0;
      request->extras = NULL;

      request->value = NULL;
      request->value_size = 0;
      request->extras = NULL;

      int body_length = keyLength;

      request_header->total_body_length[3] = (body_length & 0xff);
      request_header->total_body_length[2] = ((unsigned int)body_length & 0xff00)>>8;
      request_header->total_body_length[1] = ((unsigned int)body_length & 0xff0000)>>16;
      request_header->total_body_length[0] = ((unsigned int)body_length & 0xff000000)>>24;

      break;

    }//End case DEL
    case INCR:{

      request_header->opcode = OP_INCR;

      request_header->key_length[0] = ((unsigned int)(strlen(key) & 0xff00))>>8;
      request_header->key_length[1] = (strlen(key) & 0xff);

      request->key = key;
      request->key_size = keyLength;

      //Extra information
      // 8 byte value to add / subtract
      // 8 byte initial value (unsigned)
      // 4 byte expiration time

      request_header->extras_length = (char)20;
      request->extras = malloc(20);
      //request->extras[12] = 1;//value++
      //request->extras[3] = 0xff;
      //request->extras[2] = 0xff;
      //request->extras[1] = 0xff;
      //request->extras[0] = 0xff;
      request->extras[7] = 1;//value++
      request->extras[16] = 0xff;
      request->extras[17] = 0xff;
      request->extras[18] = 0xff;
      request->extras[19] = 0xff;
      //Right now, the expiration is 0, so we'll have to follow up the incr with a set if it fails.

      request->value = NULL;
      request->value_size = 0;

      int body_length = keyLength + 20;

      request_header->total_body_length[3] = (body_length & 0xff);
      request_header->total_body_length[2] = ((unsigned int)body_length & 0xff00)>>8;
      request_header->total_body_length[1] = ((unsigned int)body_length & 0xff0000)>>16;
      request_header->total_body_length[0] = ((unsigned int)body_length & 0xff000000)>>24;

      break;

    }//End case INCR
    case SET:{

      int body_length = 0;

      request_header->opcode = OP_SET;

      request_header->key_length[0] = ((unsigned int)(strlen(key) & 0xff00))>>8;
      request_header->key_length[1] = (strlen(key) & 0xff);

      request->key = key;
      request->key_size = keyLength;
      request->value = value;
      request->value_size = valueLength;

      //Extra information
      request_header->extras_length = (char)8;
      request->extras = malloc(8);
      request->extras[0] = 0xde;
      request->extras[1] = 0xad;
      request->extras[2] = 0xbe;
      request->extras[3] = 0xef;
      request->extras[4] = 0;
      request->extras[5] = 0;
      request->extras[6] = 0;
      request->extras[7] = 0;

      body_length = 8 + keyLength + valueLength;
      #if DEBUG
      printf("body_length %d\n", body_length);
      #endif

      request_header->total_body_length[3] = (body_length & 0xff);
      request_header->total_body_length[2] = ((unsigned int)(body_length & 0xff00))>>8;
      request_header->total_body_length[1] = ((unsigned int)(body_length & 0xff0000))>>16;
      request_header->total_body_length[0] = ((unsigned int)(body_length & 0xff000000))>>24;
      break;
    }
  case GET:{

      int body_length = 0;

      request_header->opcode = OP_GET;
      request_header->key_length[0] = ((unsigned int)(keyLength & 0xff00))>>8;
      request_header->key_length[1] = (keyLength & 0xff);

      request->key = key;
      request->key_size = keyLength;
      request->header.extras_length = (char)0;

      request->value = NULL;
      request->value_size = 0;
      request->extras = NULL;

      body_length = keyLength;

      request_header->total_body_length[3] = (body_length & 0xff);
      request_header->total_body_length[2] = ((unsigned int)(body_length & 0xff00))>>8;
      request_header->total_body_length[1] = ((unsigned int)(body_length & 0xff0000))>>16;
      request_header->total_body_length[0] = ((unsigned int)(body_length & 0xff000000))>>24;
      //printf("In get\n");
      //printf("body_length is %d\n", body_length);
      //int i;
      //for(i = 0; i < 4; i++){
      //  printf("%8x ", request_header->total_body_length[i]);
      //}
      //printf("\n");


      break;
    }
  case GETQ:{

      int body_length = 0;

      request_header->opcode = OP_GETQ;
      request_header->key_length[0] = ((unsigned int)(keyLength & 0xff00))>>8;
      request_header->key_length[1] = (keyLength & 0xff);

      request->key = key;
      request->key_size = keyLength;
      request->header.extras_length = (char)0;

      request->value = NULL;
      request->value_size = 0;
      request->extras = NULL;

      body_length = keyLength;

      request_header->total_body_length[3] = (body_length & 0xff);
      request_header->total_body_length[2] = ((unsigned int)body_length & 0xff00)>>8;
      request_header->total_body_length[1] = ((unsigned int)body_length & 0xff0000)>>16;
      request_header->total_body_length[0] = ((unsigned int)body_length & 0xff000000)>>24;

      break;
    }

  }//End switch

  return request;

}//End generateRequest()

