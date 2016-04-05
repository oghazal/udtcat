/*
 *  udtcat is simple file transfer tool over the reliable UDP-based Data Transfer (UDT) protocol.  
 *
 *  Copyright (C) 2016  Obieda Ghazal <obieda.ghazal@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <assert.h>
#include <inttypes.h>
#include <errno.h>     /* EINTR */
#include <sys/epoll.h> /* EPOLLIN | EPOLLERR */
#include <sys/types.h>
#include <sys/socket.h>
#include "udt-wrapper.h"
/* defaults */
#define DEFAULT_PORT        1988
#define MAX_HOSTNAME_LEN    32
#define PORT_STRING_LEN     8
#define MAX_LISTEN_Q        20
#define SEND_BUFFER_SIZE    1024
#define RECV_BUFFER_SIZE    1024 /* receive buffer size should be equal or 
                                                greater than send buffer size */
/* error code list */
/* TODO: use or remove the error codes */
#define ERR_UNKNOWN_OPT	        -1
#define ERR_UDT_STARTUP_FAILED  -2
#define ERR_INVALID_SO_CREATED  -3
#define ERR_BIND_FAILED	        -3
#define ERR_LISTEN_FAILED       -5
#define ERR_CONNECT_FAILED      -6
#define ERR_GETADDRINFO_FAILED  -7
/* some helpers */
#define TRUE    1
#define FALSE   !(TRUE)
#define OPERATING_MODE_NOTSET  -1
#define OPERATING_MODE_SERVER  1
#define OPERATING_MODE_CLIENT  0
#define UDT_ERR_CONN_LOST      2001
/* udtcat version */
#define UDTCAT_VERSION	"0.1"

/* prototypes */
int send_msg(int socket, const char *msg, int msg_len);
int recv_msg(int socket, char *buffer, int buffer_size);
int server_mode(const char* const listen_port_string);
int client_mode(const char* const server_name, const char* const port_string);
void *recv_handler(void *recv_info_param);
void signal_handler(int signum);
void siguser1_handler(int signum);
void print_total_sent_recvd_bytes(void);
void print_version(void);
void usage(void);

/* global variables  */
int server_fd =	-1,
    client_fd =	-1,
    operating_mode 	= OPERATING_MODE_NOTSET;
/* last errors in send_msg() and recv_mesg() */
int last_recv_err_code = -1,
    last_send_err_code = -1;
/* counters for sent and received bytes*/
uint64_t total_recv_bytes = 0,
         total_sent_bytes = 0;
/* exit flag. set in the signal_handler() */
sig_atomic_t exit_flag     = FALSE,
             print_st_flag = FALSE; /* set in siguser1_handler */
/* print total sent and received bytes flag */
int print_total_sent_recvd_bytes_on_exit_flag = FALSE;
/* main thread id (reader thread)*/             
pthread_t main_tid = 0;
/* received information struct. it used to pass receive information as
   argument to the (thread) function recv_handler().
*/
typedef struct recv_info
{
	int  socket_fd;
	char *buffer;
	int  buffer_size;
}recv_info_t;

/* SIGTERM and SIGINT handler.
   set_exit flag upon receiving signal.
*/
void signal_handler(int signum)
{
	exit_flag = TRUE;
	(void)signum; /* suppress compiler warning */
	return;
}

/* SIGUSR1 handler. Set print_st_flag on SIGUSR1 arrival.*/
void siguser1_handler(int signum)
{
	print_st_flag = TRUE;
	(void)signum; /* suppress compiler warning */
	return;
}

/* print program usage */
void usage(void)
{
	printf("usage: udtcat [OPTIONS..] [HOSTNAME]\n"
		   "-l listen for connections\n"
		   "-p port number to listen on or connect to.\n"
		   "-s print total sent and received bytes before exit.\n"
		   "-h display usage.\n"
		   "-v print udtcat version.\n");
	return;
}

/* print received and sent bytes to stderr */
void print_total_sent_recvd_bytes(void)
{
	fprintf(stderr, "\n* Total received bytes:" "%"  PRIu64
		            "\n* Total sent bytes:" "%" PRIu64,
		            total_recv_bytes, 
		            total_sent_bytes);
	return;
}

/* print udtcat version */
void print_version(void)
{
	fprintf(stderr, "udtcat: version %s ( http://github.com/oghazal/udtcat )\n", 
		             UDTCAT_VERSION);
	return;
}

/* brief : send message 'msg' of length 'msg_len' to connected 'socket'.
   return: on success the total bytes that have been sent, -1 otherwise. 
 */
int send_msg(int socket, const char *msg, int msg_len)
{
	int retval = 0;
    /* send the length of the message */
	if(udt_send(socket, (char*)&msg_len, sizeof(msg_len), 0) == UDT_ERROR)
	{
		(void)fprintf(stderr, "udtcat: could not send the size of msg: %s\n", 
    	                 	   udt_getlasterror_message());
		/* save last error code */
		last_send_err_code = udt_getlasterror_code();
		retval = -1;
		goto end;
	}
	/* send the actual message */
	if((retval = udt_send(socket, msg, msg_len, 0)) == UDT_ERROR)
	{
		(void)fprintf(stderr, "udtcat: could not send the given msg: %s\n", 
    	                      udt_getlasterror_message());
		last_send_err_code = udt_getlasterror_code();
		retval = -1;
	}
 end:
	return retval;
}

/* brief : receive message from connected 'socket', store it
           in 'buffer' of the size 'buffer_size'.
   return: on success the total bytes that have been received, -1 otherwise.
 */
int recv_msg(int socket, char *buffer, int buffer_size)
{
	int retval 	= 0,
		msg_len = 0;
    /* receive data length */
	if ((retval = udt_recv(socket, (char*)&msg_len, sizeof(msg_len), 0)) == UDT_ERROR)
	{
		/* save last recv error code */
		last_recv_err_code = udt_getlasterror_code();
		/* if connection is lost, don't print anything. */
		if(last_recv_err_code != UDT_ERR_CONN_LOST) 
		{
			(void)fprintf(stderr, "udtcat: could not receive data length: %s\n", 
	            				   udt_getlasterror_message());	
		}
		retval = -1;
		goto end;
	}
	/* check received message length */
	assert(msg_len <= buffer_size && msg_len >= 0);
	/* receive the message  */
	if((retval = udt_recv(socket, buffer, msg_len, 0)) == UDT_ERROR)
	{
		/* save last recv error code */
		last_recv_err_code = udt_getlasterror_code();
		/* if connection is lost, don't print anything. */
		if(last_recv_err_code != UDT_ERR_CONN_LOST) 
		{
			(void)fprintf(stderr, "udtcat: could not receive data: %s\n", 
                         		  udt_getlasterror_message());
			retval = -1;
			goto end;
		}
	}
 end:
	return retval;
}

/* brief : receive message from client. 
   params: recv_info_param (recv_info_t) which should have valid socket_fd to 
           receive from, and valid buffer to fill the message into it.
   return: always NULL
 */
void *recv_handler(void *recv_info_param)
{
	int recved_bytes = 0;
	recv_info_t *info =  (recv_info_t*)recv_info_param;
	while(TRUE)
	{
		if((recved_bytes = recv_msg(info -> socket_fd, info -> buffer, info ->  buffer_size)) == -1)
		{
			if(last_recv_err_code == UDT_ERR_CONN_LOST)
			{
				/* no point of living. connection is closed */
				if(pthread_kill(main_tid, SIGTERM)) /* signal the main thread (reader thread)*/
				{
					(void)fprintf(stderr, "pthread_kill() error. exiting...\n");
						  exit(EXIT_FAILURE);
				}
				break;
			}
			/* TODO: on some errors program should break on others it should not*/
			break; 
			/*continue;*/
		}
	    /* print message to stdout */
	    (void)write(STDOUT_FILENO, info -> buffer, recved_bytes);
	    /* increment receive counter */
	    if(recved_bytes > 0)
	    	total_recv_bytes += recved_bytes;
	}
	return NULL;
}

/* brief : run the program in server mode.
   return: 0 on success, -1 otherwise.
 */
int server_mode(const char* const listen_port_string)
{
	struct addrinfo server_addr_hints,
	    	 		*server_addr_res;

	char recv_buffer[RECV_BUFFER_SIZE],
	 	 send_buffer[SEND_BUFFER_SIZE];

	int addrlen    = sizeof(struct sockaddr_storage),
	    sent_bytes = 0,
		send_buffer_len = 0,
		/* udt epoll variables */
		epoll_readfds = 0,
		epoll_id = 0,
		epoll_rnum = 1,
		epoll_timeout_ms = 1000,
  		epoll_events = EPOLLIN | EPOLLERR;

    pthread_t recv_handler_thread;
    recv_info_t client_info = {0, NULL, 0};
    struct sockaddr_storage client_addr;	    	 		

	(void)memset(&server_addr_hints, 0, sizeof(struct addrinfo));
	server_addr_hints.ai_flags    = AI_PASSIVE;
	server_addr_hints.ai_family   = AF_INET;
	server_addr_hints.ai_socktype = SOCK_STREAM;
    /* find host information */
	if(getaddrinfo(NULL, listen_port_string, 
		                 &server_addr_hints, 
		                 &server_addr_res) < 0)
	{
		perror("udtcat: could not get the address information");
		return ERR_GETADDRINFO_FAILED;
	}
	/* create communication endpoint */
	server_fd = udt_socket(server_addr_res->ai_family, 
		                   server_addr_res->ai_socktype, 
		                   server_addr_res->ai_protocol);

	if(server_fd == UDT_INVALID_SOCK)
	{
		(void)fprintf(stderr, "udtcat: could not create valid socket to communicate: %s\n",
			                   udt_getlasterror_message());
		return ERR_INVALID_SO_CREATED;
	}
   	/* bind the address to the socket */
	if (udt_bind(server_fd, 
		        server_addr_res->ai_addr, 
		        server_addr_res->ai_addrlen))
   	{
   		(void)fprintf(stderr, "udtcat: could not bind address to socket: %s\n", 
   			                   udt_getlasterror_message());
      	return ERR_BIND_FAILED;
   	}
   	/* free address info; unnecessary after this point */
	freeaddrinfo(server_addr_res);
	/* listen to the incoming connection */
	if (udt_listen(server_fd, MAX_LISTEN_Q))
	{
		(void)fprintf(stderr, "udtcat: could not listen to the port: %s\n", 
			                   udt_getlasterror_message());
		return ERR_LISTEN_FAILED;
	}
	/* wait for connections */
	/* TODO: check the epoll usage again */
	epoll_id = udt_epoll_create(); 
   	if(udt_epoll_add_usock(epoll_id, server_fd, &epoll_events) == UDT_ERROR) 
   	{
 		(void)fprintf(stderr, "udtcat: epoll_add_usock() error: %s\n", 
			                   udt_getlasterror_message());
  	}
  	/* epoll loop */
  	while(1) 
  	{
  		if(udt_epoll_wait2(epoll_id, &epoll_readfds, &epoll_rnum, NULL, 
   	     	                NULL, epoll_timeout_ms, NULL, NULL, NULL, NULL))
   	    {
   	    	if(exit_flag)
   	    		return 1;
   	    	/* timed out */
   	     	continue;
   	    }
     	if(udt_epoll_release(epoll_id))
     	{
     		fprintf(stderr, "udtcat: epoll_release() error\n");
     	}
     	break;
    } 
	/* accept the new connections */
	client_fd = udt_accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
	if (client_fd == UDT_INVALID_SOCK)
    {
        (void)fprintf(stderr, "udtcat: could not accept the new con connection: %s\n", 
        	                   udt_getlasterror_message());
        return ERR_LISTEN_FAILED;
    }
    /* set client information  */
    client_info.socket_fd = client_fd;
	client_info.buffer = recv_buffer; 
	client_info.buffer_size = sizeof(recv_buffer);
    /* create the receiving thread */
    if(pthread_create(&recv_handler_thread, NULL, recv_handler, &client_info) < 0)
    {
    	perror("udtcat: server_mode()-> pthread_create() has failed");
    	return -1;
    }
    /* detach the thread */
    if(pthread_detach(recv_handler_thread) < 0)
    {
    	perror("udtcat: server_mode()-> pthread_detach() has failed");
    }
    /* receive the message length */
	while(((send_buffer_len = read(STDIN_FILENO, send_buffer, sizeof(send_buffer))) > 0 && 
		    !exit_flag) || print_st_flag)
    {
    	/* print sent/recv information and loop again*/
    	if(print_st_flag)
    	{
    		print_st_flag = FALSE;
    		print_total_sent_recvd_bytes();
    		continue;
    	}
    	if(send_buffer_len < 1)
    		break;
    	/* send the read message to the client */
    	if( (sent_bytes = send_msg(client_fd, send_buffer, send_buffer_len)) == -1)
    		continue;
    	/* increment sent bytes counter */
   		total_sent_bytes += sent_bytes;
    }
    /* work is done, close the socket*/
    (void)udt_close(client_fd);
	return 0;
}

/* brief : run the program in client mode.
   return: 0 on success, -1 otherwise.
 */
int client_mode(const char* const server_name, const char* const port_string)
{
	char send_buffer[SEND_BUFFER_SIZE],
	     recv_buffer[RECV_BUFFER_SIZE];

	struct addrinfo host_addr_hints, 
	                *host_addr_res;

	int buffer_len  = 0,
	 	sent_bytes;   

	pthread_t recv_handler_thread;
	recv_info_t server_info = {0, NULL, 0};

	/* create communication endpoint */
	server_fd = udt_socket(AF_INET, SOCK_STREAM, 0); 
	if(server_fd == UDT_INVALID_SOCK)
	{
		(void)fprintf(stderr, "udtcat: could not create valid socket to communicate: %s\n",
			                   udt_getlasterror_message());
		return ERR_INVALID_SO_CREATED;
	}
    /* initialize host_addr_hints */
    (void)memset(&host_addr_hints, 0, sizeof(struct addrinfo));
    /* find host information */
    if(getaddrinfo(server_name, port_string, &host_addr_hints, &host_addr_res) < 0)
    {
        perror("udtcat: could not get the address information");
        return ERR_GETADDRINFO_FAILED;
    }
    /* connect to host (implicit bind) */
    if(udt_connect(server_fd, host_addr_res->ai_addr, host_addr_res->ai_addrlen))
    {
        (void)fprintf(stderr, "udtcat: could not connect to server: %s\n", 
        	                   udt_getlasterror_message());
        return ERR_CONNECT_FAILED;
    }
     /* free peer address info; unnecessary after this point */
    freeaddrinfo(host_addr_res);  
    /* set server information */
    server_info.socket_fd = server_fd;
	server_info.buffer = recv_buffer; 
	server_info.buffer_size = sizeof(recv_buffer);
 	/* create the receiving thread */
    if(pthread_create(&recv_handler_thread, NULL, recv_handler, &server_info ) < 0)
    {
    	perror("udtcat: server_mode()-> pthread_create() has failed");
    	return -1;
    }
    /* detach the thread */
    if(pthread_detach(recv_handler_thread) < 0)
    {
    	perror("server_mode()-> pthread_detach() has failed");
    } 
    /* send main loop */
    while(((buffer_len = read(STDIN_FILENO, send_buffer, sizeof(send_buffer))) > 0 && 
    	    !exit_flag) || print_st_flag)
    {
    	/* print sent/recv information and loop again*/
    	if(print_st_flag)
    	{
    		print_st_flag = FALSE;
    		print_total_sent_recvd_bytes();
    		if(buffer_len == EINTR)
    			continue;
    	}
   	   	/* send the read message to the server */
    	if( (sent_bytes = send_msg(server_fd, send_buffer, buffer_len)) == -1)
    		continue;
    	/* increment sent byte counter */
   		total_sent_bytes += sent_bytes;
    }
    /* work is done, close the socket*/
    (void)udt_close(server_fd);
	return 0;
}

/* main entry point. */
/* breif : parse arguments, initialize udt library, run client or server mode,
           block on read(), and clean up the library before exit.
   notes : the main() will call client_mode() or server_mode() according to the passed
           arguments. client_mode() and server_mode will  create  and  detach receiving 
           thread (recv_handler) which will block on recv_msg(), where main will  block 
           on read().
 */
int main(int argc, char **argv)
{
	int	listen_flag =  FALSE,
		option 		=  0,
		retval 		=  0;

	char port_string [PORT_STRING_LEN],
		 hostname [MAX_HOSTNAME_LEN];

	struct sigaction sa_sigset;
	/* at least hostname or listen flag should be provided */
	if(argc < 2)
	{
		(void)fprintf(stderr, "udtcat: please specify host to connect.\n");
		usage();
		retval = -1;
		goto end;
	}
	/* set defaults */
	(void)snprintf(port_string, sizeof(port_string), "%i", DEFAULT_PORT);
	snprintf(hostname, sizeof(hostname), "%s", argv[argc - 1]);
	/* install signal handlers */
  	if(sigemptyset(&sa_sigset.sa_mask))
  	{
  		perror("sigemptyset() error");
  		retval = -1;
  		goto end;
  	}
  	sa_sigset.sa_flags = 0;
  	sa_sigset.sa_handler = signal_handler;
  	/* SIGINT handler */
  	if(sigaction(SIGINT,  &sa_sigset, NULL))
  	{
  		perror("sigaction() SIGINT handler installation error");
  		retval = -1;
  		goto end;
  	}
  	/* SIGTERM handler */
  	if(sigaction(SIGTERM, &sa_sigset, NULL))
  	{
  		perror("sigaction() SIGTERM handler installation error");
  		retval = -1;
  		goto end;
  	}
  	/* SIGUSR1 handler */
  	sa_sigset.sa_handler = siguser1_handler;
  	if(sigaction(SIGUSR1, &sa_sigset, NULL))
  	{
  		perror("sigaction() SIGUSR1 handler installation error");
  		retval = -1;
  		goto end;
  	}
  	/* ignore some boring signals */
  	sa_sigset.sa_handler = SIG_IGN;
  	if(sigaction(SIGPIPE, &sa_sigset, NULL))
  	{
  		perror("sigaction() SIGPIPE ignore error");
  		retval = -1;
  		goto end;
  	}
  	if(sigaction(SIGURG, &sa_sigset, NULL))
  	{
  		perror("sigaction() SIGURG ignore error");
  		retval = -1;
  		goto end;
  	}
  	/* save the main thread id */
	main_tid = pthread_self();
  	/* prevent getopt() from printing error messages to stderr */
	opterr = 0; 
	/* parse arguments */
	while((option = getopt(argc, argv, "lvshp:")) != -1)
	{
		switch(option)
		{
			case 'l':
				listen_flag = TRUE;
				break;

			case 's':
				print_total_sent_recvd_bytes_on_exit_flag = TRUE;
				break;

			case 'p':
				(void)strncpy(port_string, optarg, sizeof(port_string));
				break;

			case 'h':
				usage();
				return EXIT_SUCCESS;

			case 'v':
				print_version();
				return EXIT_SUCCESS;

			default:
				(void)fprintf(stderr, "udtcat: unknown option\n");
				usage();
				return ERR_UNKNOWN_OPT;
		}
	}
    /* receive buffer size should be equal or greater than send buffer size */
    assert(RECV_BUFFER_SIZE >= SEND_BUFFER_SIZE);
	/* initialize the udt library */
	if(udt_startup())  
	{
		(void)fprintf(stderr, "udtcat: udt library initialization failed: %s\n", 
			                   udt_getlasterror_message());
		return ERR_UDT_STARTUP_FAILED;
	}
	/* select mode base on the listen flag -l */
	if(listen_flag)
	{
		operating_mode = OPERATING_MODE_SERVER;
		retval = server_mode(port_string);
	}
	else
	{
		operating_mode = OPERATING_MODE_CLIENT;
		retval = client_mode(hostname, port_string);
	}
	/* library clean up */
	if(udt_cleanup())
	{
		(void)fprintf(stderr, "udtcat: could not release the udt library: %s\n", 
			                   udt_getlasterror_message());
	}

 end:	
    /* check on failure*/
	if(retval)
		return (EXIT_FAILURE);		
	/* print sent/recv bytes */
	if(print_total_sent_recvd_bytes_on_exit_flag)
		print_total_sent_recvd_bytes();
	/* ma'a salama */
	return (EXIT_SUCCESS);
}
