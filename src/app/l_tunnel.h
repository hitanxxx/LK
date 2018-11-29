#ifndef _L_TUNNEL_H_INCLUDED_
#define _L_TUNNEL_H_INCLUDED_

#define 	TUNNEL_TIMEOUT 20
#define 	TUNNEL_TIMEOUT_CONNECT	3

#define 	TUNNEL_CLIENT	0x0200
#define		TUNNEL_SERVER	0x0201
#define		TUNNEL_SINGLE	0x0202

#define 	TUNNEL_HEADER_LENGTH	8192

typedef struct tunnel_t {
	queue_t				queue;
	connection_t * 		downstream;
	connection_t * 		upstream;

	uint32	  			https;
	float	  			http_response_version_n;
	meta_t 	   			meta_send;

	http_request_head_t *	request_head;
	http_entitybody_t * 	request_body;
	http_response_head_t *  response_head;
	http_entitybody_t * 	response_body;

	net_transport_t * in;
	net_transport_t * out;

	uint32		trans_body_done;
} tunnel_t;

status tunnel_process_end( void );
status tunnel_process_init( void );

status tunnel_init( void );
status tunnel_end( void );

#endif
