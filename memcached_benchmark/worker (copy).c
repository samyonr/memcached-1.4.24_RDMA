
#include "worker.h"

pthread_mutex_t move_connection_lock = PTHREAD_MUTEX_INITIALIZER;

void* workerFunction(void* arg) {

  struct worker* worker = arg;
  printf("Creating worker on tid %u\n", (unsigned int)pthread_self());


/*  int s;
  cpu_set_t cpuset;
  pthread_t thread;
  thread = pthread_self(); */

//  sgenrand(worker->cpu_num, worker->config->random_seed);

  struct timeval timestamp;
  gettimeofday(&timestamp, NULL);
  int seed=(timestamp.tv_usec+worker->cpu_num)%2309;			 
  sgenrand(seed, &(worker->myMT19937p));
// sgenrand(worker->config->random_seed, &(worker->myMT19937p));
/* Set affinity mask to include CPUs 0 to 7 */
/*  CPU_ZERO(&cpuset);
  CPU_SET(worker->cpu_num, &cpuset);
  s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (s != 0) {
    printf("Couldn't set CPU affinity\n");
  } else {
    printf("Set tid %u affinity to CPU %d\n", (unsigned int)pthread_self(), worker->cpu_num);
  }
*/
  workerLoop(worker);
  printf("bye bye\n");
  return NULL;

}//End workerFunction()


void workerLoop(struct worker* worker) {

  event_base_priority_init(worker->event_base, 3);

  worker->event_map_0 = malloc(sizeof(struct event_map) * (worker->nConnections));
  worker->event_map_1 = malloc(sizeof(struct event_map) * (worker->nConnections));
  worker->event_map_2 = malloc(sizeof(struct event_map) * (worker->nConnections));
  worker->nEvents = (worker->nConnections);

  //Seed event for each fd
  int i;
  int max_sock = 0;
	for( i = 0; i < worker->nConnections; i++) {
		createEvents(i, worker, worker->connections[i]->sock, 0);
    	config->events_status[worker->connections[i]->sock].active = 1;
		max_sock = worker->connections[i]->sock;
  	}//End for i

	//create passive events for backup and backup2
	for( i = 0; i < worker->nConnections; i++) {
		createEvents(i, worker, ++max_sock, 1);
  	}//End for i
	for( i = 0; i < worker->nConnections; i++) {
		createEvents(i, worker, ++max_sock, 2);
  	}//End for i
  
  //TODO: delete
  struct timeval five_seconds = {5,0};
  struct event* ev = event_new(worker->event_base, -1, EV_TIMEOUT|EV_PERSIST, printCallback, NULL);
  event_priority_set(ev, 1);
  event_add(ev, &five_seconds);

  gettimeofday(&(worker->last_write_time), NULL);
  printf("starting receive base loop\n");
  //int error = event_base_loop(worker->event_base, 0);
  int error = event_base_dispatch(worker->event_base);
  if(error == -1) {
    printf("Error starting libevent\n");
  } else if(error == 1) {
    printf("No events registered with libevent\n");
  }

  printf("base loop done\n");

}//End workerLoop()


int pushRequest(struct worker* worker, struct request* request) {

//  printf("push: size %d head %d tail %d\n", worker->n_requests, worker->head, worker->tail);

  if(worker->n_requests == QUEUE_SIZE){
    printf("Reached queue size max\n");
    return 0;
  }
  
  worker->request_queue[worker->tail] = request;
  worker->tail = (worker->tail + 1) % QUEUE_SIZE;
  worker->n_requests++;

  return 1;

}//End pushRequest()

struct request* getNextRequest(struct worker* worker) {

  if(worker->n_requests == 0) {
    //printf("no more requests\n");
    return NULL;
  }

  struct request* request = worker->request_queue[worker->head];
  worker->head = (worker->head + 1) % QUEUE_SIZE;
  worker->n_requests--;

  return request;

}//End getNextRequest()

void sendCallback(int fd, short eventType, void* args) {
  struct worker* worker = args;
 if (fd > 55) { printf("send callback!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!, fd %d\n",fd);}
  increaseEventsLiveSendEvCounter(fd, worker, worker->nEvents);
  if (eventType & EV_TIMEOUT) {
	printf("send event with fd %d happend beceuse of timeout\n",fd);
  }

  struct timeval timestamp, timediff, timeadd;
  gettimeofday(&timestamp, NULL);
  timersub(&timestamp, &(worker->last_write_time), &timediff);
  double diff = timediff.tv_usec * 1e-6  + timediff.tv_sec;
  struct int_dist* interarrival_dist = worker->config->interarrival_dist;
  int interarrival_time  = 0;
  //Null interarrival_dist means no waiting
  if(interarrival_dist != NULL){
    if(worker->interarrival_time <= 0){
        interarrival_time = getIntQuantile(interarrival_dist); //In microseconds
        //   printf("new interarrival_time %d\n", interarrival_time);
        worker->interarrival_time = interarrival_time;
    } else {
        interarrival_time = worker->interarrival_time; 
    }
      if( interarrival_time/1.0e6 > diff){
			//printf("returning at interarrival_time/1.0e6 > diff\n");
			if (eventType & EV_TIMEOUT) {
			printf("debugging 1 for fd %d beceuse of timeout\n",fd);
			}else{
			sleep(interarrival_time/1.0e6);
	      		//return;
}
      }
  }		
		if (eventType & EV_TIMEOUT) {
			printf("debugging 2 for fd %d beceuse of timeout\n",fd);
		}
  worker->interarrival_time = -1;

  timeadd.tv_sec = 0; timeadd.tv_usec = interarrival_time; 
  timeradd(&(worker->last_write_time), &timeadd, &(worker->last_write_time));
  struct request* request = NULL;
  if(worker->incr_fix_queue_tail != worker->incr_fix_queue_head) {
    request = worker->incr_fix_queue[worker->incr_fix_queue_head];
    worker->incr_fix_queue_head = (worker->incr_fix_queue_head + 1) % INCR_FIX_QUEUE_SIZE;
//    printf("fixing\n");
  } else {
  //  printf(")preload %d warmup key %d\n", worker->config->pre_load, worker->warmup_key);
    if(worker->config->pre_load == 1 && worker->warmup_key < 0) {
      printf("here is how requests disappear\n");
      return;
    } else {
		if (eventType & EV_TIMEOUT) {
			printf("generating request for fd %d beceuse of timeout\n",fd);
		}
		if (fd > 55) { printf("generating request, fd %d\n",fd);}
      request = generateRequest(worker->config, worker);
    }
  }
  if(request->header.opcode == OP_SET){
//    printf("Generated SET request of size %d\n", request->value_size);
  }
  if( !pushRequest(worker, request) ) {
    //Queue is full, bail
    printf("Full queue\n");
    deleteRequest(request);
    return;
  }
int old_sock = 0;
  int sendResult = sendRequest(request, &old_sock);
  if (sendResult == -1)
 {
   deleteRequest(request);
}
  //sendWorkerRequest_sendCallback(request,worker, 0);
  
}//End sendCallback()

int sendWorkerRequest_sendCallback(struct request* request,struct worker* worker, int iteration)
{
  int old_sock = 0;
  if (iteration > 0)
  {
     printf("sendWorkerRequest_sendCallback, fd %d. iteration %d\n", request->connection->sock,iteration);
  }
  int sendResult = sendRequest(request, &old_sock);
  if (sendResult == -1 && iteration != 3)
  {
    printf("Resending again %d\n",iteration+1);
    return sendWorkerRequest_sendCallback(request,worker, ++iteration);
  }
  else if (sendResult == -1 && iteration == 3)
  {
    printf("failed resending 3 times\n");
	deleteRequest(request);
    return 0;
  }
  return 1;
}

int sendWorkerRequest_receiveCallback(struct request* request,struct worker* worker, int iteration)
{
  int old_sock = 0;
  printf("sendWorkerRequest_receiveCallback, fd %d. iteration %d\n", request->connection->sock,iteration);
  int sendResult = sendRequest(request, &old_sock);
  if (sendResult == -1 && iteration != 3)
  {
    //deleteRequest(request);
    /*
    if( !pushRequest(worker, request) ) {
    //Queue is full, bail
    //printf("Full queue\n");
      printf("Request queue is full\n");
      deleteRequest(request);
      return 0;
    }
    */
    printf("Resending again %d\n",iteration+1);
    return sendWorkerRequest_receiveCallback(request,worker, ++iteration);
  }
  else if (sendResult == -1 && iteration == 3)
  {
    printf("failed resending 3 times\n");
	deleteRequest(request);
    return 0;
  }
  return 1;
}

void createEvents(int connection, struct worker* worker, fd, int variant)
{
	struct timeval five_seconds = {1,0};
    if (variant == 0)
    {
		printf("connection number %d: adding send event for variant 0 with fd %d\n",server, worker->connections[server]->sock);
		struct event* ev1 = event_new(worker->event_base, worker->connections[server]->sock, EV_PERSIST|EV_TIMEOUT, sendCallback, worker);
		event_priority_set(ev1, 2);
		event_add(ev1, &five_seconds);

		printf("connection number %d: adding read event for variant 0 with fd %d\n",server, worker->connections[server]->sock);
		struct event* ev2 = event_new(worker->event_base, worker->connections[server]->sock, EV_PERSIST|EV_TIMEOUT, receiveCallback, worker);
		event_priority_set(ev2, 3);
		event_add(ev2, &five_seconds);

		worker->event_map_0[connection].event_send = ev1;
		worker->event_map_0[connection].event_receive = ev2;
		worker->event_map_0[connection].fd = worker->connections[server]->sock;
		worker->event_map_0[connection].null_counter = 0;
		worker->event_map_0[connection].live_send_ev_counter = 0;
		worker->event_map_0[connection].live_receive_ev_counter = 0;
    }
	else if (variant == 1)
	{
		printf("connection number %d: adding send event for variant 1 with fd %d\n",server, worker->connections[server]->sock);
		struct event* ev1 = event_new(worker->event_base, worker->connections[server]->sock, EV_PERSIST|EV_TIMEOUT, sendCallback, worker);
		event_priority_set(ev1, 2);
		event_add(ev1, &five_seconds);

	/*
		printf("connection number %d: adding read event for variant 1 with fd %d\n",server, worker->connections[server]->sock);
		struct event* ev2 = event_new(worker->event_base, worker->connections[server]->sock, EV_READ|EV_PERSIST|EV_TIMEOUT, receiveCallback, worker);
		event_priority_set(ev2, 3);
		event_add(ev2, &five_seconds);
*/
		worker->event_map_1[server].event_send = ev1;
		//worker->event_map_1[server].event_receive = ev2;
		worker->event_map_1[server].fd = worker->connections[server]->sock;
		worker->event_map_1[server].null_counter = 0;
		worker->event_map_1[server].live_send_ev_counter = 0;
		worker->event_map_1[server].live_receive_ev_counter = 0;
	}
	else if (variant == 2)
	{
		printf("connection number %d: adding send event for variant 2 with fd %d\n",server, worker->connections[server]->sock);
		struct event* ev1 = event_new(worker->event_base, worker->connections[server]->sock, EV_WRITE|EV_PERSIST|EV_TIMEOUT, sendCallback, worker);
		event_priority_set(ev1, 2);
		event_add(ev1, &five_seconds);

		printf("connection number %d: adding read event for vairant 2 with fd %d\n",server, worker->connections[server]->sock);
		struct event* ev2 = event_new(worker->event_base, worker->connections[server]->sock, EV_READ|EV_PERSIST|EV_TIMEOUT, receiveCallback, worker);
		event_priority_set(ev2, 3);
		event_add(ev2, &five_seconds);

		worker->event_map_2[server].event_send = ev1;
		worker->event_map_2[server].event_receive = ev2;
		worker->event_map_2[server].fd = worker->connections[server]->sock;
		worker->event_map_2[server].null_counter = 0;
		worker->event_map_2[server].live_send_ev_counter = 0;
		worker->event_map_2[server].live_receive_ev_counter = 0;
	}
	else
	{
		printf("cant add more events\n");
		exit(-1);
	}
}

void deleteEvents(int fd, struct event_map* event_map, int nEvents)
{
  int i;
  for (i = 0; i < nEvents; i++)
  {
    if (event_map[i].fd == fd)
    {
      printf("deleting event with fd %d\n", fd);
      event_free(event_map[i].event_receive);
      event_free(event_map[i].event_send);
      event_map[i].fd = 0;
    }
  }
}

void increaseEventsLiveReceiveEvCounter(int fd, struct worker* worker, int nEvents)
{
	int i;
	struct event_map* event_map;

	event_map = worker->event_map_0;
	for (i = 0; i < nEvents; i++)
	{
		if (event_map[i].fd == fd)
		{
			event_map[i].live_receive_ev_counter++;
			if (event_map[i].live_receive_ev_counter % 1000000 == 0)
			{
				printf("fd %d from variant 0 has live counter - receive - %d\n", fd, event_map[i].live_receive_ev_counter);
			}
			return;
		}
	}

	event_map = worker->event_map_1;
	for (i = 0; i < nEvents; i++)
	{
		if (event_map[i].fd == fd)
		{
			event_map[i].live_receive_ev_counter++;
			if (event_map[i].live_receive_ev_counter % 1000000 == 0)
			{
				printf("fd %d from variant 0 has live counter - receive - %d\n", fd, event_map[i].live_receive_ev_counter);
			}
			return;
		}
	}

	event_map = worker->event_map_2;
	for (i = 0; i < nEvents; i++)
	{
		if (event_map[i].fd == fd)
		{
			event_map[i].live_receive_ev_counter++;
			if (event_map[i].live_receive_ev_counter % 1000000 == 0)
			{
				printf("fd %d from variant 0 has live counter - receive - %d\n", fd, event_map[i].live_receive_ev_counter);
			}
			return;
		}
	}
	printf ("fd %d was not found\n", fd);
	exit(-1);
}

void increaseEventsLiveSendEvCounter(int fd, struct worker* worker, int nEvents)
{
	int i;
	struct event_map* event_map;

	event_map = worker->event_map_0;
	for (i = 0; i < nEvents; i++)
	{
		if (event_map[i].fd == fd)
		{
			event_map[i].live_send_ev_counter++;
			if (event_map[i].live_send_ev_counter % 100000 == 0)
			{
				printf("fd %d from variant 0 has live counter - send - %d\n", fd, event_map[i].live_send_ev_counter);
			}
			return;
		}
	}

	event_map = worker->event_map_1;
	for (i = 0; i < nEvents; i++)
	{
		if (event_map[i].fd == fd)
		{
			event_map[i].live_send_ev_counter++;
			if (event_map[i].live_send_ev_counter % 100000 == 0)
			{
				printf("fd %d from variant 0 has live counter  - send - %d\n", fd, event_map[i].live_send_ev_counter);
			}
			return;
		}
	}

	event_map = worker->event_map_2;
	for (i = 0; i < nEvents; i++)
	{
		if (event_map[i].fd == fd)
		{
			event_map[i].live_send_ev_counter++;
			if (event_map[i].live_send_ev_counter % 100000 == 0)
			{
				printf("fd %d from variant 0 has live counter  - send - %d\n", fd, event_map[i].live_send_ev_counter);
			}
			return;
		}
	}
	printf ("fd %d was not found\n", fd);
	exit(-1);
}

void increaseOrDeleteEventsNullCounter(int fd, struct worker* worker, int nEvents)
{
	int i;
	struct event_map* event_map;

	event_map = worker->event_map_0;
	for (i = 0; i < nEvents; i++)
	{
		if (event_map[i].fd == fd)
		{
			event_map[i].null_counter++;
			if (event_map[i].null_counter > 100000)
			{
				printf("fd %d from variant 0 has null counter %d\n", fd, event_map[i].null_counter);
				printf("deleting event with fd %d\n", fd);
			    event_free(event_map[i].event_receive);
			    event_free(event_map[i].event_send);
				//TODO: free the comm itself?
			    event_map[i].fd = 0;
				event_map[i].null_counter = 0;
			}
			return;
		}
	}

	event_map = worker->event_map_1;
	for (i = 0; i < nEvents; i++)
	{
		if (event_map[i].fd == fd)
		{
			event_map[i].null_counter++;
			if (event_map[i].null_counter > 100000)
			{
				printf("fd %d from variant 1 has null counter %d\n", fd, event_map[i].null_counter);
				printf("deleting event with fd %d\n", fd);
			    event_free(event_map[i].event_receive);
			    event_free(event_map[i].event_send);
				//TODO: free the comm itself?
			    event_map[i].fd = 0;
				event_map[i].null_counter = 0;
			}
			return;
		}
	}

	event_map = worker->event_map_2;
	for (i = 0; i < nEvents; i++)
	{
		if (event_map[i].fd == fd)
		{
			event_map[i].null_counter++;
			if (event_map[i].null_counter > 100000)
			{
				printf("fd %d from variant 2 has null counter %d\n", fd, event_map[i].null_counter);
				printf("deleting event with fd %d\n", fd);
			    event_free(event_map[i].event_receive);
			    event_free(event_map[i].event_send);
				//TODO: free the comm itself?
			    event_map[i].fd = 0;
				event_map[i].null_counter = 0;
	 		}
			return;
		}
	}
	printf ("fd %d was not found\n", fd);
	exit(-1);
}

void receiveCallback(int fd, short eventType, void* args) {
  struct worker* worker = args;
  increaseEventsLiveReceiveEvCounter(fd, worker, worker->nEvents);
  if (eventType & EV_TIMEOUT) {
	printf("receive event with fd %d happend beceuse of timeout\n",fd);
  }
  

  struct request* request = getNextRequest(worker);
  if(request == NULL) { 
    //printf("Error: Tried to get a null request, fd %d\n",fd);
    //exit(-1);
	//increaseOrDeleteEventsNullCounter(fd, worker, worker->nEvents);
    return;
  }
  struct timeval readTimestamp, timediff;
  gettimeofday(&readTimestamp, NULL);
  timersub(&readTimestamp, &(request->send_time), &timediff);
  double diff = timediff.tv_usec * 1e-6  + timediff.tv_sec;

  int old_sock = 0;
  
  int result = receiveResponse(request, diff, &old_sock);
  if (result == -1 && 1==0)
  {
    if( !pushRequest(worker, request) ) {
       //Queue is full, bail
       printf("Full queue\n");
       deleteRequest(request);
       return;  
    }
	printf("sending worker request in receive-callback, fd %d. iteration %d\n", request->connection->sock,0);
    int res = sendWorkerRequest_receiveCallback(request,worker, 0);
    if (res != 1)
    {
      return;
    }
  }
  else
  {
    deleteRequest(request);
  }
  worker->received_warmup_keys++;
  if(worker->config->pre_load == 1 && worker->config->dep_dist != NULL && worker->received_warmup_keys == worker->config->keysToPreload){
    printf("You are warmed up, sir\n");
    exit(0);
  }
}//End receiveCallback()

void readF(int* temp){
 FILE *fp;
 char buff[4]={0};
 fp = fopen("cpu.txt","r");
 if(!fp) return;
 *temp=0; 
 while (fgets(buff,4, fp)!=NULL){
        printf("%s",buff);
         *temp=atoi(buff);
 } 
 fclose(fp);	
}

void writeF(int temp){
 FILE *fp;
 fp = fopen("cpu.txt","a");
 fprintf(fp, "%d\n",temp);
 fclose(fp);
}


void printCallback(int fd, short eventType, void* args)
{
	printf("!!!!!!!!!!!!!!!!5 seconds passed!!!!!!!!!!!!!!!!!!!!!\n");
}
void createWorkers(struct config* config) {

  config->workers = malloc(sizeof(struct worker*)*config->n_workers);
  int i;

	if(config->n_workers > config->n_connections_total ) {
		printf("Overridge n_connections_total because < n_workers\n");
    	config->n_connections_total = config->n_workers;
	}

	//multiply by 3 because we allocate all in advance for the main run and for two backups.
	config->events_state = malloc((sizeof(struct events_state) * (config->n_connections_total * 3)) + 30 ); //plus 30 is just a buffer, should be ok for up to 8 threads
	for (i = 0; i < (config->n_connections_total * 3) + 30; i++)
	{
		config->events_state[i].active = 0;
		//the first fd index is 6 if only 1 worker used, 9 if 2 worker, 12 for 3 workers and so on.
		//not sure why, but probably it's because of fds used for threads. main thread taks 3 fds, and each worker thread takes another 3 fds.
		//what means that (((config->n_workers-1) * 3) + 6)-1 first cells will never be active
		config->events_state[i].fd = i;
	}

	for( i = 0; i < config->n_workers; i++) {
		config->workers[i] = createWorker(config, i);
	}
	int total_connections = 0;
  for(i = 0; i < config->n_workers; i++) {
    int num_worker_connections = config->n_connections_total/config->n_workers + (i < config->n_connections_total % config->n_workers);
    total_connections += num_worker_connections;
    printf("total_connections %d\n", total_connections);
    config->workers[i]->connections = malloc(sizeof(struct conn*) * num_worker_connections);
	config->workers[i]->connection_server = malloc(sizeof(int) * num_worker_connections);
	config->workers[i]->connection_server_variant = malloc(sizeof(int) * num_worker_connections);
    config->workers[i]->nConnections = num_worker_connections;
    config->workers[i]->received_warmup_keys = 0;
    int j;
    int server=i % config->n_servers; 
    for(j = 0; j < num_worker_connections; j++) {
      config->workers[i]->connections[j] = createConnection(config->server_ip_address[server], config->server_port[server], config->protocol_mode, config->naggles);
	  config->workers[i]->connection_server[j] = server;
	  config->workers[i]->connection_server_variant[j] = 0; //0 - original, 1 - backup_1, 2 - backup_2
    }
    int rc;
    //Create receive thread
    rc = pthread_create(&config->workers[i]->thread, NULL, workerFunction, config->workers[i]);
    if(rc) {
      printf("Error creating receive thread\n");
    }
  }
  printf("Created %d connections total\n", total_connections);
}//createWorkers()


struct worker* createWorker(struct config* config, int cpuNum) {

  //Create connections
  evthread_use_pthreads();
  struct worker* worker = malloc(sizeof(struct worker));
  worker->event_base = event_base_new();

  worker->config = config;
  worker->head = 0;
  worker->tail = 0;
  worker->n_requests = 0;
  worker->cpu_num = cpuNum;
  worker->interarrival_time = 0;
  worker->incr_fix_queue_tail = 0; // THSES probably need to be fixed
  worker->incr_fix_queue_head = 0;
  if(config->dep_dist != NULL && config->pre_load) {
    worker->warmup_key = config->keysToPreload-1;
    worker->warmup_key_check = 0;
  }


  return worker;

}//End createWorker()

int changeServer(struct request* request, int server)
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
		createEvents(server, request->worker, 1);
		return 1;
	}
	else if (request->worker->connection_server_variant[server] == 2)
	{
		printf("server number %d: setting connection. old addrss - %s, new address - %s, old port - %d, new port - %d\n",
			server ,request->worker->config->server_ip_address_backup[0] ,
			request->worker->config->server_ip_address_backup_2[0], 
			request->worker->config->server_port_backup[0],request->worker->config->server_port_backup_2[0]);
		request->worker->connections[server] = createConnection(request->worker->config->server_ip_address_backup_2[0], 
			request->worker->config->server_port_backup_2[0], 
			request->worker->config->protocol_mode, 
			request->worker->config->naggles);
		printf("server number %d: new connection set\n", server);
		printf("server number %d: previous sock: %d, new sock: %d\n",server, request->connection->sock, request->worker->connections[server]->sock);
		request->connection = request->worker->connections[server];
		request->server_variant++;
		createEvents(server, request->worker, 2);
		return 1;
	}
	else
	{
		printf("server number %d: failed moving to variant %d - no such variant\n", server, request->worker->connection_server_variant[server]);
		return -1;
	}
}


