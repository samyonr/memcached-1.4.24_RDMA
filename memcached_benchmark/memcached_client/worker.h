#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>
#include <malloc.h>
#include <event2/event.h>
#include <sys/time.h>
#include <errno.h>
#include "config.h"
#include "conn.h"
#include "request.h"
#include "response.h"
#include "generate.h"

#include "mt19937p.h"

#define QUEUE_SIZE 1000000
#define INCR_FIX_QUEUE_SIZE 1000

extern pthread_mutex_t move_connection_lock;

struct event_map {
  struct event *event_receive;
  int fd;
  struct event *event_send;
};

struct worker {
  
  struct config* config;
  pthread_t thread;
  struct event_map* event_map;
  int nEvents;
  struct event_base* event_base;
  struct conn** connections;
  int* connection_server;
  int* connection_server_variant;
  int nConnections;
  int cpu_num;
  struct timeval last_write_time;
  int interarrival_time;

  //Circular queue
  struct request* request_queue[QUEUE_SIZE];
  int head;
  int tail;
  int n_requests;
  int current_request_id;

  struct request* incr_fix_queue[INCR_FIX_QUEUE_SIZE];
  int incr_fix_queue_head;
  int incr_fix_queue_tail;
  struct mt19937p myMT19937p;
  int warmup_key;
  int warmup_key_check;	
  int received_warmup_keys;

};


void sendCallback(int fd, short eventType, void* args);
void receiveCallback(int fd, short eventType, void* args);
void* workerFunction(void* arg);
void workerLoop(struct worker* worker);
void createWorkers(struct config* config);
struct worker* createWorker(struct config* config, int cpuNum);
int pushRequest(struct worker* worker, struct request* request);
void createEvents(int server, struct worker* worker);
void deleteEvents(int fd, struct event_map* event_map, int nEvents);
int sendWorkerRequest(struct request* request,struct worker* worker, int iteration);
int sendWorkerRequest_sendCallback(struct request* request,struct worker* worker, int iteration);

#endif
