/*
 * Added as part of the memcached-1.4.24_RDMA project.
 * C implementation of Queue Data Structure using Linked List
 * source: http://www.sanfoundry.com/c-program-queue-using-linked-list/
 * not thread safe
 */
#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

struct node
{
    int info;
    struct node *ptr;
}*front,*rear,*temp,*front1;

/*
struct queue
{
	struct node* front;
	struct node* rear;
	struct node* temp;
	struct node* front1;
};
*/

int count = 0;

/* Create an empty queue */
void queue_create()
{
    front = rear = NULL;
}

/* Returns queue size */
void queue_queuesize()
{
    printf("Queue size : %d\n", count);
}

/* Enqueing the queue */
void queue_enq(int data)
{
    if (rear == NULL)
    {
        rear = (struct node *)malloc(1*sizeof(struct node));
        rear->ptr = NULL;
        rear->info = data;
        front = rear;
    }
    else
    {
        temp=(struct node *)malloc(1*sizeof(struct node));
        rear->ptr = temp;
        temp->info = data;
        temp->ptr = NULL;

        rear = temp;
    }
    count++;
}

/* Displaying the queue elements */
void queue_display()
{
    front1 = front;

    if ((front1 == NULL) && (rear == NULL))
    {
        printf("Queue is empty\n");
        return;
    }
    while (front1 != rear)
    {
        printf("%d ", front1->info);
        front1 = front1->ptr;
    }
    if (front1 == rear)
        printf("%d", front1->info);
}

/* Dequeing the queue */
void queue_deq()
{
    front1 = front;

    if (front1 == NULL)
    {
        printf("Error: Trying to display elements from empty queue\n");
        return;
    }
    else
        if (front1->ptr != NULL)
        {
            front1 = front1->ptr;
            printf("Dequed value : %d\n", front->info);
            free(front);
            front = front1;
        }
        else
        {
            printf("Dequed value : %d\n", front->info);
            free(front);
            front = NULL;
            rear = NULL;
        }
        count--;
}

/* Returns the front element of queue */
int queue_frontelement()
{
    if ((front != NULL) && (rear != NULL))
        return(front->info);
    else
        return 0;
}

/* Display if queue is empty or not */
int queue_empty()
{
     if ((front == NULL) && (rear == NULL))
        return 1;
    else
       return 0;
}

