/*
 * Added as part of the memcached-1.4.24_RDMA project.
 * C implementation of Queue Data Structure using Linked List
 * source: http://www.sanfoundry.com/c-program-queue-using-linked-list/
 * not thread safe
 */

#ifndef QUEUE_H_
#define QUEUE_H_

int queue_frontelement(void);
void queue_enq(int data);
void queue_deq(void);
int queue_empty(void);
void queue_display(void);
void queue_create(void);
void queue_queuesize(void);




#endif /* QUEUE_H_ */
