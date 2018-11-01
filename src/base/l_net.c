#include "lk.h"

static queue_t usable;
static queue_t use;
static connection_t * pool = NULL;
static event_t * read_pool = NULL;
static event_t * write_pool = NULL;

// net_alloc ----------------------------------------------------------------------------------------------------------------------------------
status net_alloc( connection_t ** c )
{
	connection_t * new;
	queue_t * q;

	if( 1 == queue_empty( &usable ) ) {
		err_log( "%s --- usbale empty", __func__ );
		return ERROR;
	}

	q = queue_head( &usable );
	queue_remove( q );
	queue_insert_tail( &use, q );
	new = l_get_struct( q, connection_t, queue );
	*c = new;
	return OK;
}
// net_free -----------------------------------------------------------------------------------------------------------------------------------
status net_free( connection_t * c )
{
	queue_remove( &c->queue );
	queue_insert_tail( &usable, &c->queue );

	if( c->fd ) {
		event_close( c->read, EVENT_READ );
		event_close( c->write, EVENT_WRITE );
		close( c->fd );
		c->fd = 0;
	}
	c->data = NULL;

	memset( &c->addr, 0, sizeof(struct sockaddr_in) );
	c->read->handler = NULL;
	c->write->handler = NULL;
	timer_del( &c->read->timer );
	timer_del( &c->write->timer );
	c->read->f_active = 0;
	c->write->f_active = 0;

	c->f_active = 0;
	c->ssl_flag = 0;
	c->ssl = NULL;
	return OK;
}
// net_non_blocking ---------------------------------------------------------------------------------------------------------------------
status net_non_blocking( int fd )
{
	int32 nb;
	nb = 1;
	return ioctl( fd, FIONBIO, &nb );
}
// net_fastopen ------------
status net_fastopen( connection_t * c )
{
	int  tcp_fastopen = 1;

	if( ERROR == setsockopt(c->fd, IPPROTO_TCP, TCP_FASTOPEN,
			(const void *) &tcp_fastopen, sizeof(tcp_fastopen)) ) {
		err_log( "%s --- set fastopen, [%s]", __func__, strerror(errno) );
		return ERROR;
	}
	return OK;
}
// net_nodelay -------------
status net_nodelay( connection_t * c )
{
	int  tcp_nodelay = 1;

	if( ERROR == setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
			(const void *) &tcp_nodelay, sizeof(tcp_nodelay)) ) {
		err_log( "%s --- set nodelay, [%s]", __func__, strerror(errno) );
		return ERROR;
	}
	return OK;
}
// net_nopush ---------------------
status net_nopush( connection_t * c )
{
    int  tcp_cork = 1;

	if( ERROR == setsockopt(c->fd, IPPROTO_TCP, TCP_CORK,
			(const void *) &tcp_cork, sizeof(tcp_cork)) ) {
		err_log( "%s --- set nopush, [%s]", __func__, strerror(errno) );
		return ERROR;
	}
	return OK;
}
// net_get_addr ------------------
struct addrinfo * net_get_addr( string_t * ip, string_t * port )
{
	struct addrinfo hints, *res;
	int rc;
	char name[100] = {0};
	char serv[10] = {0};

	memset( &hints, 0, sizeof( struct addrinfo ) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	memcpy( name, ip->data, ip->len );
	memcpy( serv, port->data, port->len );

	rc = getaddrinfo( name, serv, &hints, &res );
	if( 0 != rc ) {
		err_log( "%s --- getaddrinfo error", __func__ );
		return NULL;
	}
	if( NULL == res ) {
		err_log( "%s --- getaddrinfo empty", __func__ );
		return NULL;
	}
	return res;
}
// net_init ----------------------------------------------------------------------------------------------------------------------------------
status net_init( void )
{
	uint32 i;

	queue_init( &usable );
	queue_init( &use );

	pool = ( connection_t *) malloc ( sizeof(connection_t) * MAXCON );
	if( !pool ) {
		err_log( "%s --- malloc pool", __func__ );
		return ERROR;
	}
	memset( pool, 0, sizeof(connection_t) * MAXCON );

	read_pool = (event_t*)malloc( sizeof(event_t)* MAXCON );
	if( !read_pool ) {
		err_log( "%s --- malloc read pool", __func__ );
		return ERROR;
	}
	memset( read_pool, 0, sizeof(event_t)*MAXCON );

	write_pool = (event_t*)malloc( sizeof(event_t)*MAXCON );
	if( !write_pool ) {
		err_log( "%s --- malloc write pool", __func__ );
		return ERROR;
	}
	memset( write_pool, 0, sizeof(event_t)*MAXCON );

	for( i = 0; i < MAXCON; i ++ ) {
		pool[i].read = &read_pool[i];
		pool[i].write = &write_pool[i];
		pool[i].read->data = (void*)&pool[i];
		pool[i].write->data = (void*)&pool[i];

		queue_insert_tail( &usable, &pool[i].queue );
	}
	return OK;
}
// net_end ------------------------------------------------------------------------------------------------------------------------------------
status net_end( void )
{
	uint32 i;
	if( read_pool ){
		free( read_pool );
	}
	if( write_pool ) {
		free( write_pool );
	}

	for( i = 0; i < MAXCON; i ++ ) {
		if( pool[i].meta ) {
			meta_free( pool[i].meta );
			pool[i].meta = NULL;
		}
	}
	if( pool ) {
		free( pool );
	}
	return OK;
}
