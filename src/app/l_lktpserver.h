#ifndef _L_LKTPSERVER_H_INCLUDED_
#define _L_LKTPSERVER_H_INCLUDED_

#define LKTP_CONNECT 	0X2001
#define LKTP_ESTABLE	0X2002

#define LKTP_CLIENT		0x0001
#define LKTP_SERVER		0x0002

#define LKTP_TIMEOUT 			2
#define LKTP_TIMEOUT_HEARTBEAT	5

typedef struct lktp_manager_t {

	struct sockaddr_in 	addr;

	status 				status;
	connection_t * 		c;
	timer_msg_t 			timer_message;

	lktp_request_t * 	lktp_request;
	meta_t	*			lktp_request_send_chain;

    lktp_head_t*        lktp_head;
    lktp_body_t*        lktp_body;

	serv_api_handler	handler;
} lktp_manager_t;

status lktpserv_init( void );
status lktpserv_end( void );

#endif
