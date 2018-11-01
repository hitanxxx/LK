#ifndef _L_SERV_H_INCLUDED_
#define _L_SERV_H_INCLUDED_

typedef status ( * serv_api_handler  ) ( void * data );
typedef struct serv_api_t {
	string_t 			name;
	serv_api_handler	handler;
} serv_api_t;
typedef struct serv_apis_t {
	queue_t			queue;
	serv_api_t 		*api;
} serv_apis_t;

status serv_init( void );
status serv_end( void );
status serv_api_find( string_t * key, serv_api_handler * handler );

#endif
