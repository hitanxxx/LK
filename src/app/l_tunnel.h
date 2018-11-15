#ifndef _L_TUNNEL_H_INCLUDED_
#define _L_TUNNEL_H_INCLUDED_

#define TUNNEL_TIMEOUT 20

#define 	TUNNEL_CLIENT	0x0200
#define		TUNNEL_SERVER	0x0201
#define		TUNNEL_SINGLE	0x0202

// default ipv4 recv buffer size 
#define 	TUNNEL_TRANSPORT_BUFFER	87380

typedef struct tunnel_t {
	queue_t				queue;
	connection_t * 		downstream;
	connection_t * 		upstream;

	uint32	  			https;
	float	  			http_response_version_n;
	meta_t 	   			established;
	meta_t 				local_recv_chain;

	// transport values
	char 	buffer_in[TUNNEL_TRANSPORT_BUFFER];
	char 	buffer_out[TUNNEL_TRANSPORT_BUFFER];
	meta_t	in;
	meta_t	out;
	uint32	out_busy;
	uint32	in_busy;
	uint32	out_recv_error;
	uint32	in_recv_error;

	http_request_head_t *	request_head;
	http_entitybody_t * 	request_body;
	http_response_head_t *  response_head;
	http_entitybody_t * 	response_body;

	uint32		trans_body_done;
} tunnel_t;

status tunnel_process_end( void );
status tunnel_process_init( void );

status tunnel_init( void );
status tunnel_end( void );

#endif
