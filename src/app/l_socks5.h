#ifndef _L_SOCKS5_H_INCLUDED_
#define _L_SOCKS5_H_INCLUDED_


#define SOCKS5_META_LENGTH	4096


typedef struct socks5_request_t {
	int32		state;
	char		ver;
	char		cmd;
	char		rsv;
	char		atyp;
	
	char		dst_addr[100];
	int32 		offset;
	char		dst_port[2];
} socks5_request_t;

typedef struct socks5_response_t {
	int32		ver;
	int32		rep;
	int32		rsv;
	int32		atyp;
	int32		bnd_addr[100];
	int32		bnd_port[2];
} socks5_response_t;


typedef struct socks5_cycle_t {
	socks5_request_t		request;
	socks5_response_t 		response;
	connection_t * 		down;
	connection_t * 		up;
	
	net_transport_t * in;
	net_transport_t * out;
} socks5_cycle_t;

status socks5_local_init( void );
status socks5_local_end( void );


#endif