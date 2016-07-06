/*
 * The code below is a modification of Mellanox's server and client examples.
 * Keeping their disclaimer.
 */

/*
 * Copyright (c) 2013 Mellanox Technologies®. All rights reserved.
 *
 * This software is available to you under a choice of one of two licenses.
 * You may choose to be licensed under the terms of the GNU General Public
 * License (GPL) Version 2, available from the file COPYING in the main
 * directory of this source tree, or the Mellanox Technologies® BSD license
 * below:
 *
 *      - Redistribution and use in source and binary forms, with or without
 *        modification, are permitted provided that the following conditions
 *        are met:
 *
 *      - Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 *      - Neither the name of the Mellanox Technologies® nor the names of its
 *        contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <pthread.h>
#include "queue.h"
#include "backup_rdma_accelio.h"
#include "libxio.h"

void create_basic_request(struct xio_msg *req);
void create_queue_data_request(struct xio_msg *req, int value);
void *RunBackupServerRDMA(void *arg);
int rdma_load_file_to_memory(const char *filename, char **result);
void *RunBackupClientRDMA(void *arg);
static int on_session_event_client(struct xio_session *session,
			    struct xio_session_event_data *event_data,
			    void *cb_user_context);
static int on_response_client(struct xio_session *session, struct xio_msg *rsp,
		       int last_in_rxq,
		       void *cb_user_context);
static int on_session_event_server(struct xio_session *session,
			    struct xio_session_event_data *event_data,
			    void *cb_user_context);
static int on_new_session_server(struct xio_session *session,
			  struct xio_new_session_req *req,
			  void *cb_user_context);
static int on_request_server(struct xio_session *session,
		      struct xio_msg *req,
		      int last_in_rxq,
		      void *cb_user_context);

#define QUEUE_DEPTH		512
#define PRINT_COUNTER		4000000
#define DISCONNECT_NR		(2 * PRINT_COUNTER)

#define MAX_RDMA_BACKUPS	3


static pthread_t g_serverThread;
static pthread_t g_clientThread;
int test_disconnect;
int g_backups_RDMA_count = 0;
int g_queue_depth;
int g_server_connected = 0;

/* server private data */
struct server_data {
	struct xio_context	*ctx;
	struct xio_connection	*connection;
	uint64_t		nsent;
	uint64_t		cnt;
	int			pad;
	int			ring_cnt;
	struct xio_msg		rsp_ring[QUEUE_DEPTH];	/* global message */
	struct xio_msg		single_rsp;
};

static struct xio_session_ops ses_ops = {
	.on_session_event		=  on_session_event_client,
	.on_session_established		=  NULL,
	.on_msg				=  on_response_client,
	.on_msg_error			=  NULL
};

struct session_data {
	struct xio_context	*ctx;
	struct xio_connection	*conn;
	uint64_t		cnt;
	uint64_t		nsent;
	uint64_t		nrecv;
	uint64_t		pad;
	struct xio_msg		req_ring[QUEUE_DEPTH];
	struct xio_msg		single_req;
};


/*---------------------------------------------------------------------------*/
/* on_session_event							     */
/*---------------------------------------------------------------------------*/
static int on_session_event_client(struct xio_session *session,
			    struct xio_session_event_data *event_data,
			    void *cb_user_context)
{
	struct session_data *session_data = (struct session_data *)
						cb_user_context;

	printf("session event: %s. reason: %s\n",
	       xio_session_event_str(event_data->event),
	       xio_strerror(event_data->reason));

	switch (event_data->event) {
	case XIO_SESSION_CONNECTION_TEARDOWN_EVENT:
		xio_connection_destroy(event_data->conn);
		break;
	case XIO_SESSION_TEARDOWN_EVENT:
		xio_session_destroy(session);
		xio_context_stop_loop(session_data->ctx);  /* exit */
		break;
	default:
		break;
	};
	return 0;
}

static void process_response_client(struct session_data *session_data,
			     struct xio_msg *rsp)
{
	if (strcmp((char *)rsp->in.header.iov_base,"beacon response header") == 0)
	{
		if (++session_data->cnt == PRINT_COUNTER) {
			struct xio_iovec_ex	*isglist = vmsg_sglist(&rsp->in);
			int			inents = vmsg_sglist_nents(&rsp->in);

			printf("message: [%llu] - %s\n",
				   (unsigned long long)(rsp->request->sn + 1),
				   (char *)rsp->in.header.iov_base);
			printf("message: [%llu] - %s\n",
				   (unsigned long long)(rsp->request->sn + 1),
				   (char *)(inents > 0 ? isglist[0].iov_base : NULL));
			session_data->cnt = 0;
		}
	}
	else if (strcmp((char *)rsp->in.header.iov_base,"test header") == 0)
	{
		struct xio_iovec_ex	*isglist = vmsg_sglist(&rsp->in);
		int			inents = vmsg_sglist_nents(&rsp->in);

		printf("message: [%llu] - %s\n",
			   (unsigned long long)(rsp->request->sn + 1),
			   (char *)rsp->in.header.iov_base);
		printf("message: [%llu] - %s\n",
			   (unsigned long long)(rsp->request->sn + 1),
			   (char *)(inents > 0 ? isglist[0].iov_base : NULL));
	}
}

static int on_response_client(struct xio_session *session,
		       struct xio_msg *rsp,
		       int last_in_rxq,
		       void *cb_user_context)
{
	struct session_data *session_data = (struct session_data *)
						cb_user_context;
	struct xio_msg	    *req = rsp;
	int queue_val;


	session_data->nrecv++;

	/* process the incoming message */
	process_response_client(session_data, rsp);

	/* acknowledge xio that response is no longer needed */
	xio_release_response(rsp);

	if (test_disconnect) {
		if (session_data->nrecv == DISCONNECT_NR) {
			xio_disconnect(session_data->conn);
			return 0;
		}
		if (session_data->nsent == DISCONNECT_NR)
			return 0;
	}

	//check if there a message waiting in the queue
	if (!queue_empty())
	{
		queue_val = queue_frontelement();
		printf("Got something in the queue! value = %d\n",queue_val);
		queue_deq();
		create_queue_data_request(req, queue_val);
	}
	else
	{
		create_basic_request(req);
	}

	req->in.header.iov_base	  = NULL;
	req->in.header.iov_len	  = 0;
	vmsg_sglist_set_nents(&req->in, 0);

	/* resend the message */
	xio_send_request(session_data->conn, req);
	session_data->nsent++;

	return 0;
}


int rdma_load_file_to_memory(const char *filename, char **result)
{
	int size = 0;
	FILE *f = fopen(filename, "rb");
	if (f == NULL)
	{
		*result = NULL;
		return -1; // -1 means file opening fail
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	*result = (char *)malloc(size+1);
	if (size != fread(*result, sizeof(char), size, f))
	{
		free(*result);
		return -2; // -2 means file reading fail
	}
	fclose(f);
	(*result)[size] = 0;
	free(*result);
	return size;
}

/*---------------------------------------------------------------------------*/
/* ring_get_next_msg							     */
/*---------------------------------------------------------------------------*/
static inline struct xio_msg *ring_get_next_msg(struct server_data *sd)
{
	struct xio_msg *msg = &sd->single_rsp;
	long size;
	//char *content;

	free(msg->out.header.iov_base);
	free(msg->out.data_iov.sglist[0].iov_base);


	msg->out.header.iov_base =
		strdup("beacon response header");
	msg->out.header.iov_len =
		strlen((const char *)
			msg->out.header.iov_base) + 1;

	msg->out.sgl_type	   = XIO_SGL_TYPE_IOV;
	msg->out.data_iov.max_nents = XIO_IOVLEN;


	size = 0;
	//rdma_load_file_to_memory("/tmp/memkey/slabs_lists_key1", &content);
	if (size < 0)
	{
		puts("Error loading file");
		exit(0);
	}


	msg->out.data_iov.sglist[0].iov_base =
				strdup("beacon response body");

	msg->out.data_iov.sglist[0].iov_len =
		strlen((const char *)
			msg->out.data_iov.sglist[0].iov_base) + 1;
	msg->out.data_iov.nents = 1;
	//free(content);

	return msg;
}

/*---------------------------------------------------------------------------*/
/* process_request							     */
/*---------------------------------------------------------------------------*/
static void process_request_server(struct server_data *server_data,
			    struct xio_msg *req)
{
	struct xio_iovec_ex	*sglist = vmsg_sglist(&req->in);
	char			*str;
	int			nents = vmsg_sglist_nents(&req->in);
	int			len, i;
	char			tmp;

	if (strcmp((char *)req->in.header.iov_base,"beacon request header") == 0)
	{
		/* note all data is packed together so in order to print each
		 * part on its own NULL character is temporarily stuffed
		 * before the print and the original character is restored after
		 * the printf
		 */
		if (++server_data->cnt == PRINT_COUNTER) {
			str = (char *)req->in.header.iov_base;
			len = req->in.header.iov_len;
			if (str) {
				if (((unsigned)len) > 64)
					len = 64;
				tmp = str[len];
				str[len] = '\0';
				printf("message header : [%llu] - %s\n",
					   (unsigned long long)(req->sn + 1), str);
				str[len] = tmp;
			}
			for (i = 0; i < nents; i++) {
				str = (char *)sglist[i].iov_base;
				len = sglist[i].iov_len;
				if (str) {
					if (((unsigned)len) > 64)
						len = 64;
					tmp = str[len];
					str[len] = '\0';
					printf("message data: [%llu][%d][%d] - %s\n",
						   (unsigned long long)(req->sn + 1),
						   i, len, str);
					str[len] = tmp;
				}
			}
			server_data->cnt = 0;
		}
		req->in.header.iov_base	  = NULL;
		req->in.header.iov_len	  = 0;
		vmsg_sglist_set_nents(&req->in, 0);
	}
	else
	{
		str = (char *)req->in.header.iov_base;
		len = req->in.header.iov_len;
		if (str) {
			if (((unsigned)len) > 64)
				len = 64;
			tmp = str[len];
			str[len] = '\0';
			printf("message header : [%llu] - %s\n",
				   (unsigned long long)(req->sn + 1), str);
			str[len] = tmp;
		}
		for (i = 0; i < nents; i++) {
			printf("message data : %d\n", *(int *)sglist[i].iov_base);
			/*
			str = (char *)sglist[i].iov_base;
			len = sglist[i].iov_len;
			if (str) {
				if (((unsigned)len) > 64)
					len = 64;
				tmp = str[len];
				str[len] = '\0';
				printf("message data: [%llu][%d][%d] - %s\n",
					   (unsigned long long)(req->sn + 1),
					   i, len, str);
				str[len] = tmp;
			}*/
		}
		server_data->cnt = 0;
		req->in.header.iov_base	  = NULL;
		req->in.header.iov_len	  = 0;
		vmsg_sglist_set_nents(&req->in, 0);
	}
}

/*---------------------------------------------------------------------------*/
/* on_session_event	server						     */
/*---------------------------------------------------------------------------*/
static int on_session_event_server(struct xio_session *session,
			    struct xio_session_event_data *event_data,
			    void *cb_user_context)
{
	struct server_data *server_data = (struct server_data *)cb_user_context;

	printf("session event: %s. session:%p, connection:%p, reason: %s\n",
	       xio_session_event_str(event_data->event),
	       (void *)session, (void *)event_data->conn,
	       xio_strerror(event_data->reason));

	switch (event_data->event) {
	case XIO_SESSION_NEW_CONNECTION_EVENT:
		server_data->connection = event_data->conn;
		break;
	case XIO_SESSION_CONNECTION_TEARDOWN_EVENT:
		xio_connection_destroy(event_data->conn);
		server_data->connection = NULL;
		break;
	case XIO_SESSION_TEARDOWN_EVENT:
		xio_session_destroy(session);
		xio_context_stop_loop(server_data->ctx);  /* exit */
		break;
	default:
		break;
	};

	return 0;
}

/*---------------------------------------------------------------------------*/
/* on_new_session							     */
/*---------------------------------------------------------------------------*/
static int on_new_session_server(struct xio_session *session,
			  struct xio_new_session_req *req,
			  void *cb_user_context)
{
	struct server_data *server_data = (struct server_data *)cb_user_context;

	/* automatically accept the request */
	printf("new session event. session:%p\n", (void*)session);

	if (!server_data->connection)
		xio_accept(session, NULL, 0, NULL, 0);
	else
		xio_reject(session, (enum xio_status)EISCONN, NULL, 0);

	return 0;
}

/*---------------------------------------------------------------------------*/
/* on_request callback							     */
/*---------------------------------------------------------------------------*/
static int on_request_server(struct xio_session *session,
		      struct xio_msg *req,
		      int last_in_rxq,
		      void *cb_user_context)
{
	struct server_data *server_data = (struct server_data *)cb_user_context;
	struct xio_msg	   *rsp = ring_get_next_msg(server_data);

	/* process request */
	process_request_server(server_data, req);

	/* attach request to response */
	rsp->request = req;

	xio_send_response(rsp);
	server_data->nsent++;

	if (test_disconnect) {
		if (server_data->nsent == DISCONNECT_NR) {
			xio_disconnect(server_data->connection);
			return 0;
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------*/
/* asynchronous callbacks						     */
/*---------------------------------------------------------------------------*/
static struct xio_session_ops  server_ops __attribute__ ((unused)) = {
	.on_session_event		=  on_session_event_server,
	.on_new_session			=  on_new_session_server,
	.on_msg_send_complete		=  NULL,
	.on_msg				=  on_request_server,
	.on_msg_error			=  NULL
};

void create_basic_request(struct xio_msg *req)
{
	req->out.header.iov_base =
		strdup("beacon request header");
	req->out.header.iov_len =
		strlen((const char *)
			req->out.header.iov_base) + 1;
	req->in.sgl_type		  = XIO_SGL_TYPE_IOV;
	req->in.data_iov.max_nents = XIO_IOVLEN;

	req->out.sgl_type	   = XIO_SGL_TYPE_IOV;
	req->out.data_iov.max_nents = XIO_IOVLEN;

	req->out.data_iov.sglist[0].iov_base =
		strdup("beacon request body");

	req->out.data_iov.sglist[0].iov_len =
		strlen((const char *)
		  req->out.data_iov.sglist[0].iov_base)
		   + 1;

	req->out.data_iov.nents = 1;
}

void create_queue_data_request(struct xio_msg *req, int value)
{
	//int length = snprintf(NULL, 0, "queue data request body, value=%d", value);
	void * p;


	req->out.header.iov_base =
		strdup("queue data request header");
	req->out.header.iov_len =
		strlen((const char *)
			req->out.header.iov_base) + 1;
	req->in.sgl_type		  = XIO_SGL_TYPE_IOV;
	req->in.data_iov.max_nents = XIO_IOVLEN;

	req->out.sgl_type	   = XIO_SGL_TYPE_IOV;
	req->out.data_iov.max_nents = XIO_IOVLEN;

	req->out.data_iov.sglist[0].iov_base = malloc(sizeof(int));
	p = req->out.data_iov.sglist[0].iov_base;
	((int *)p)[0] = value;
	req->out.data_iov.sglist[0].iov_len = sizeof(int);
	//req->out.data_iov.sglist[0].iov_base = malloc(sizeof(char) * (length + 1));
	//snprintf(req->out.data_iov.sglist[0].iov_base, length+1, "queue data request body, value=%d", value);

	/*req->out.data_iov.sglist[0].iov_len =
		strlen((const char *)
		  req->out.data_iov.sglist[0].iov_base)
		   + 1;
	*/

	req->out.data_iov.nents = 1;
}

/*---------------------------------------------------------------------------*/
/* main									     */
/*---------------------------------------------------------------------------*/
int BackupServerRDMA(void)
{
	int rv;
    //Create backup server thread
    rv = pthread_create(&g_serverThread, NULL, RunBackupServerRDMA, NULL);
    if(rv < 0)
    {
    	printf("Error creating backup server thread\n");
    }
    return 0;
}


void *RunBackupServerRDMA(void *arg)
{
	struct xio_server	*server;	/* server portal */
	struct server_data	server_data;
	char			url[256];
	struct	xio_msg		*rsp;

	/* initialize library */
	xio_init();

	/* create "hello world" message */
	memset(&server_data, 0, sizeof(server_data));

	rsp = &server_data.single_rsp;
	rsp->out.header.iov_base =
		strdup("hello world header response");
	rsp->out.header.iov_len =
		strlen((const char *)
			rsp->out.header.iov_base) + 1;

	rsp->out.sgl_type	   = XIO_SGL_TYPE_IOV;
	rsp->out.data_iov.max_nents = XIO_IOVLEN;

	rsp->out.data_iov.sglist[0].iov_base =
		strdup("hello world data response");

	rsp->out.data_iov.sglist[0].iov_len =
		strlen((const char *)
		       rsp->out.data_iov.sglist[0].iov_base) + 1;
	rsp->out.data_iov.nents = 1;

	/* create thread context for the client */
	server_data.ctx	= xio_context_create(NULL, 0, -1);

	/* create url to connect to */
	sprintf(url, "rdma://%s:%s", "10.0.0.1", "5555");//TODO: make configurable

	/* bind a listener server to a portal/url */
	server = xio_bind(server_data.ctx, &server_ops,
			  url, NULL, 0, &server_data);
	if (server) {
		printf("listen to %s\n", url);
		xio_context_run_loop(server_data.ctx, XIO_INFINITE);

		/* normal exit phase */
		fprintf(stdout, "exit signaled\n");

		/* free the server */
		xio_unbind(server);
	}

	/* free the message */

	free(rsp->out.header.iov_base);
	free(rsp->out.data_iov.sglist[0].iov_base);

	/* free the context */
	xio_context_destroy(server_data.ctx);

	xio_shutdown();

	exit(0);
}

int BackupClientRDMA()
{
	if (g_backups_RDMA_count >= MAX_RDMA_BACKUPS)
	{
		printf("Maximal number of backups reached\n");
		return -1;
	}
	int rv;
    //Create backup server thread
    rv = pthread_create(&g_clientThread, NULL, RunBackupClientRDMA, NULL);
    if(rv < 0)
    {
    	printf("Error creating backup client thread\n");
    	return -1;
    }
    g_backups_RDMA_count++;
    return 0;
}

void *RunBackupClientRDMA(void *arg)
{
	struct xio_session		*session;
	char				url[256];
	struct session_data		session_data;
	int opt, optlen;
	struct xio_session_params	params;
	struct xio_connection_params	cparams;
	struct xio_msg			*req;


	test_disconnect = 0;

	memset(&session_data, 0, sizeof(session_data));
	memset(&params, 0, sizeof(params));
	memset(&cparams, 0, sizeof(cparams));

	/* initialize library */
	xio_init();

	/* get minimal queue depth */
	xio_get_opt(NULL, XIO_OPTLEVEL_ACCELIO,
		    XIO_OPTNAME_SND_QUEUE_DEPTH_MSGS,
		    &opt, &optlen);
	g_queue_depth = QUEUE_DEPTH > opt ? opt : QUEUE_DEPTH;

	/* create thread context for the client */
	session_data.ctx = xio_context_create(NULL, 0, -1);

	/* create url to connect to */
	sprintf(url, "rdma://%s:%s", "10.0.0.1", "5555");//TODO: make configurable

	params.type		= XIO_SESSION_CLIENT;
	params.ses_ops		= &ses_ops;
	params.user_context	= &session_data;
	params.uri		= url;

	session = xio_session_create(&params);

	cparams.session			= session;
	cparams.ctx			= session_data.ctx;
	cparams.conn_user_context	= &session_data;

	/* connect the session  */
	session_data.conn = xio_connect(&cparams);

	/* create "hello world" message */
	req = &session_data.single_req;
	create_basic_request(req);

	g_server_connected = 1;
	xio_send_request(session_data.conn, req);
	session_data.nsent++;

	/* event dispatcher is now running */
	xio_context_run_loop(session_data.ctx, XIO_INFINITE);

	/* normal exit phase */
	fprintf(stdout, "exit signaled\n");

	/* free the message */
	free(req->out.header.iov_base);
	free(req->out.data_iov.sglist[0].iov_base);

	/* free the context */
	xio_context_destroy(session_data.ctx);

	xio_shutdown();

	printf("good bye\n");
	return 0;
}
