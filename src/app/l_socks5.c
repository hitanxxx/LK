#include "lk.h"

// socks5_cycle_free ---------
static status socks5_cycle_free( socks5_cycle_t * cycle )
{
	if( cycle->up ) {
		net_free( cycle->up );
		cycle->up = NULL;
	}
	if( cycle->down ) {
		net_free( cycle->down );
		cycle->down = NULL;
	}
	l_safe_free( cycle );
	return OK;
}

// socks5_cycle_time_out --------
static void socks5_cycle_time_out( void * data )
{
	socks5_cycle_t * cycle;

	cycle = data;
	socks5_cycle_free( cycle );
}

// socks5_local_connect_test -----
static status socks5_local_connect_test( connection_t * c )
{
	int	err = 0;
    socklen_t  len = sizeof(int);

	if (getsockopt( c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len) == -1 ) {
		err = errno;
	}
	if (err) {
		err_log("%s --- remote connect test, [%d]", __func__, errno );
		return ERROR;
	}
	return OK;
}
// socks5_transport_out_recv ------
static status socks5_transport_out_recv( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;

	rc = net_transport( cycle->out, 0 );
	if( rc == ERROR ) {
		err_log("%s --- net transport out recv failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	cycle->up->read->timer.data = (void*)cycle;
	cycle->up->read->timer.handler = socks5_cycle_time_out;
	timer_add( &cycle->up->read->timer, 3 );
	return rc;
}
// socks5_transport_out_send ------
static status socks5_transport_out_send( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;

	rc = net_transport( cycle->out, 1 );
	if( rc == ERROR ) {
		err_log("%s --- net transport out send failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	cycle->up->read->timer.data = (void*)cycle;
	cycle->up->read->timer.handler = socks5_cycle_time_out;
	timer_add( &cycle->up->read->timer, 3 );
	return rc;
}
// socks5_transport_in_recv ------
static status socks5_transport_in_recv( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;

	rc = net_transport( cycle->in, 0 );
	if( rc == ERROR ) {
		err_log("%s --- net transport in recv failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	cycle->down->read->timer.data = (void*)cycle;
	cycle->down->read->timer.handler = socks5_cycle_time_out;
	timer_add( &cycle->down->read->timer, 3 );
	return rc;
}
// socks5_transport_in_send ------
static status socks5_transport_in_send( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;

	rc = net_transport( cycle->in, 1 );
	if( rc == ERROR ) {
		err_log("%s --- net transport in send failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	cycle->down->read->timer.data = (void*)cycle;
	cycle->down->read->timer.handler = socks5_cycle_time_out;
	timer_add( &cycle->down->read->timer, 3 );
	return OK;
}
// socks5_local_pipe -------
static status socks5_local_pipe( event_t * ev )
{
	connection_t * down;
	socks5_cycle_t * cycle;

	down = ev->data;
	cycle = down->data;
	if( OK != net_transport_alloc( &cycle->in ) ) {
		err_log("%s --- net_transport in alloc", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	if( OK != net_transport_alloc( &cycle->out ) ) {
		err_log("%s --- net_transport out alloc", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}

	cycle->in->recv_connection = cycle->down;
	cycle->in->send_connection = cycle->up;

	cycle->out->recv_connection = cycle->up;
	cycle->out->send_connection = cycle->down;

	cycle->down->read->handler = socks5_transport_in_recv;
	cycle->up->write->handler = socks5_transport_in_send;

	cycle->down->write->handler = socks5_transport_out_send;
	cycle->up->read->handler = socks5_transport_out_recv;

	event_opt( cycle->up->read, EVENT_READ );
	event_opt( cycle->down->write, EVENT_WRITE );
	event_opt( cycle->down->read, EVENT_READ );
	event_opt( cycle->up->write, EVENT_WRITE );

	return AGAIN;
}
// socks5_local_response ----
static status socks5_local_response( event_t * ev )
{
	connection_t * down;
	socks5_cycle_t * cycle;

	down = ev->data;
	cycle = down->data;
	status rc;
	rc = down->send_chain( down, down->meta );
	if( rc == ERROR ) {
		err_log( "%s ---  send failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	} else if( rc == DONE ) {
		timer_del( &down->write->timer );
		debug_log ( "%s --- socks5 local response send success", __func__ );
		down->read->handler = socks5_local_pipe;
		return down->read->handler( down->read );
	}
	down->write->timer.data = (void*)cycle;
	down->write->timer.handler = socks5_cycle_time_out;
	timer_add( &down->write->timer, 3 );
	return rc;
}
// socks5_local_start -----
static status socks5_local_start( event_t * ev )
{
	socks5_cycle_t * cycle;
	connection_t* up, *down;

	up = ev->data;
	cycle = up->data;
	down = cycle->down;


	down->meta->last = down->meta->pos = down->meta->start;
	*down->meta->last++ = 0x05;
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x01;

	//
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x00;

	*down->meta->last++ = 0x10;
	*down->meta->last++ = 0x10;

	debug_log("%s ---- socks5 response len [%d]", __func__, meta_len( down->meta->pos, down->meta->last ) );

	up->read->handler = NULL;
	up->write->handler = NULL;
	event_opt( down->write, EVENT_WRITE );
	down->write->handler = socks5_local_response;
	return down->write->handler( down->write );
}
// socks5_local_connect_check ----
static status socks5_local_connect_check( event_t * ev )
{
	socks5_cycle_t * cycle;
	connection_t* up;
	status rc;

	up = ev->data;
	cycle = up->data;
	if( OK != socks5_local_connect_test( up ) ) {
		err_log( "%s --- socks5 local connect failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	debug_log ( "%s --- connect success", __func__ );
	net_nodelay( up );
	timer_del( &up->write->timer );

	up->write->handler = socks5_local_start;
	return up->write->handler( up->write );
}
// socks5_local_connect_start ------
static status socks5_local_connect_start( event_t * ev )
{
	status rc;
	socks5_cycle_t * cycle;
	connection_t* up;

	up = ev->data;
	cycle = up->data;
	rc = event_connect( up->write );
	if( rc == ERROR ) {
		err_log( "%s --- connect error", __func__ );
		socks5_cycle_free(cycle);
		return ERROR;
	}
	up->write->handler = socks5_local_connect_check;
	event_opt( up->write, EVENT_WRITE );

	if( rc == AGAIN ) {
		debug_log("%s --- connect again", __func__ );
		up->write->timer.data = (void*)cycle;
		up->write->timer.handler = socks5_cycle_time_out;
		timer_add( &up->write->timer, 3 );
		return AGAIN;
	}
	return up->write->handler( up->write );
}
// socks5_connect -----------
static status socks5_connect( event_t * ev )
{
	connection_t * c, * up;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;

	debug_log("%s --- socks5_connect", __func__  );
	if( OK != net_alloc( &cycle->up ) ) {
		err_log("%s --- net alloc up", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	cycle->up->send = sends;
	cycle->up->recv = recvs;
	cycle->up->send_chain = send_chains;
	cycle->up->recv_chain = NULL;
	cycle->up->data = (void*)cycle;

	if( !cycle->up->meta ) {
		if( OK != meta_alloc( &cycle->up->meta, 4096 ) ) {
			err_log( "%s --- up meta alloc", __func__ );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	}

	// getaddrinfo
	struct addrinfo * res = NULL;
	string_t ip, port;
	char ipstr[100] = {0}, portstr[20] = {0};

	snprintf( ipstr, sizeof(ipstr), "%d.%d.%d.%d", (unsigned char )cycle->request.dst_addr[0],
	(unsigned char )cycle->request.dst_addr[1],
	(unsigned char )cycle->request.dst_addr[2],
	(unsigned char )cycle->request.dst_addr[3]
	);
	snprintf( portstr, sizeof(portstr), "%d", ntohs(*(int32*)cycle->request.dst_port) );
	ip.data = ipstr;
	ip.len = l_strlen(ipstr);
	port.data = portstr;
	port.len = l_strlen(portstr);
	debug_log("%s --- ip [%.*s] port [%.*s]", __func__ ,
	ip.len, ip.data,
	port.len, port.data
	);
	res = net_get_addr( &ip, &port );
	if( !res ) {
		err_log("%s --- get up address failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	memset( &cycle->up->addr, 0, sizeof(struct sockaddr_in) );
	memcpy( &cycle->up->addr, res->ai_addr, sizeof(struct sockaddr_in) );
	freeaddrinfo( res );

	timer_del( &cycle->down->read->timer );
	cycle->down->read->handler = NULL;
	cycle->down->write->handler = NULL;

	cycle->up->read->handler = NULL;
	cycle->up->write->handler = socks5_local_connect_start;

	return cycle->up->write->handler( cycle->up->write );
}
// socks5_process_request -------------
static status socks5_process_request( event_t * ev )
{
	char * p = NULL;
	connection_t * c;
	socks5_cycle_t * cycle = NULL;
	ssize_t rc;
	char host_len = 0;
	int32 i = 0;

	enum{
		ver = 0,
		cmd,
		rsv,
		atyp,
		dst_addr_start,
		dst_addr,
		dst_host,
		dst_port,
		dst_port_end
	} state;

	c = ev->data;
	cycle = c->data;

	while( 1 ) {
		if( c->meta->pos == c->meta->last )  {
			rc = c->recv( c, c->meta->last, meta_len( c->meta->last, c->meta->end ) );
			if( rc == ERROR ) {
				err_log("%s --- recv socks5 request failed", __func__ );
				socks5_cycle_free( cycle );
				return ERROR;
			} else if ( rc == AGAIN ) {
				c->read->timer.data = (void*)cycle;
				c->read->timer.handler = socks5_cycle_time_out;
				timer_add( &c->read->timer, 3 );

				debug_log("%s --- recv socks5 again", __func__ );
				return AGAIN;
			} else {
				c->meta->last += rc;
			}
		}

		rc = meta_len( c->meta->pos, c->meta->last );
		debug_log("%s --- recvd [%d] bytes", __func__, rc );

		state = cycle->request.state;
		for( ; c->meta->pos < c->meta->last; c->meta->pos ++ ) {
			p = c->meta->pos;

			if( state == ver ) {
				cycle->request.ver = *p;
				debug_log("%s --- socks5 process ver [%x]", __func__, cycle->request.ver );
				state = cmd;
				continue;
			}
			if( state == cmd ) {
				cycle->request.cmd = *p;
				debug_log("%s --- socks5 process cmd [%x]", __func__, cycle->request.cmd );
				state = rsv;
				continue;
			}
			if( state == rsv ) {
				cycle->request.rsv = *p;
				debug_log("%s --- socks5 process rsv [%x]", __func__, cycle->request.rsv );
				state = atyp;
				continue;
			}
			if( state == atyp ) {
				cycle->request.atyp = *p;
				debug_log("%s --- socks5 process atyp [%x]", __func__, cycle->request.atyp );
				state = dst_addr_start;
				continue;
			}
			if( state == dst_addr_start ) {
				cycle->request.dst_addr[cycle->request.offset++] = *p;
				debug_log("%s --- socks5 process dst addr-0 [%x]", __func__, cycle->request.dst_addr[0] );
				state = dst_addr;
				continue;
			}
			if( state == dst_addr ) {
				switch( cycle->request.atyp ) {
					case (0x01):
						debug_log("%s --- offset [%d]  p [%x]", __func__, cycle->request.offset, *p );
						if( cycle->request.offset >= 4 ) {
							state = dst_port;
						} else {
							cycle->request.dst_addr[cycle->request.offset++] = *p;
						}
						break;
					case (0x03):
						host_len = *p;
						state = dst_host;
						break;
					case (0x04):
						if( cycle->request.offset >= 16 ) {
							state = dst_port;
						} else {
							cycle->request.dst_addr[cycle->request.offset++] = *p;
						}

						break;
					default:
						break;
				}
				//continue;
			}
			if( state == dst_host ) {
				if( cycle->request.offset >= (int32)host_len ) {
					state = dst_port;
				} else {
					cycle->request.dst_addr[cycle->request.offset ++] = *p;
				}
				continue;
			}
			if( state == dst_port ) {
				debug_log("%s --- port[0] %x", __func__, *p );
				cycle->request.dst_port[0] = *p;
				state = dst_port_end;
				continue;
			}
			if( state == dst_port_end ) {
				debug_log("%s --- port[1] %x", __func__, *p );
				cycle->request.dst_port[1] = *p;

				debug_log("%s --- process request success", __func__ );
				debug_log("%s --- version [%x]", __func__, cycle->request.ver );
				debug_log("%s --- cmd [%x]", __func__, cycle->request.cmd );
				debug_log("%s --- rsv [%x]", __func__, cycle->request.rsv );
				debug_log("%s --- atyp[%x]", __func__, cycle->request.atyp);
				debug_log("%s --- dst addr [%x, %x, %x, %x]", __func__, cycle->request.dst_addr[0],
					cycle->request.dst_addr[1],
					cycle->request.dst_addr[2],
					cycle->request.dst_addr[3]);
				debug_log("%s --- dst port [%x-%x]", __func__, cycle->request.dst_port[0],
					cycle->request.dst_port[1] );

				debug_log("%s --- dst addr [%d %d %d %d]", __func__,
					(unsigned char)cycle->request.dst_addr[0],
					(unsigned char)cycle->request.dst_addr[1],
					(unsigned char)cycle->request.dst_addr[2],
					(unsigned char)cycle->request.dst_addr[3]
				);

				debug_log("%s --- dst port [%d]", __func__, ntohs(*(int32*)cycle->request.dst_port ));

				cycle->request.state = 0;
				c->read->handler = socks5_connect;
				return c->read->handler( c->read );
			}
		}
		cycle->request.state = state;
	}
	return OK;
}

// socks5_local_auth_replay ------------
static status socks5_local_auth_replay ( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle = NULL;

	c = ev->data;
	cycle = c->data;

	rc = c->send_chain( c, c->meta );
	if( rc == ERROR ) {
		err_log("%s --- send auth replay failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	} else if ( rc == DONE ) {
		debug_log("%s --- send auth replay success", __func__ );
		timer_del( &c->read->timer );

		c->meta->pos = c->meta->last = c->meta->start;

		c->write->handler = NULL;
		c->read->handler = socks5_process_request;
		return c->read->handler( c->read );
	}
	c->read->timer.data = (void*)cycle;
	c->read->timer.handler = socks5_cycle_time_out;
	timer_add( &c->read->timer, 3 );
	return AGAIN;
}
// socks5_local_auth_replay_prepare ----------
static status socks5_local_auth_replay_prepare( event_t * ev )
{
	connection_t * c;
	c = ev->data;

	/*
		1 byte 		1 byte
		version | ack authentication method

	*/
	c->meta->pos = c->meta->last = c->meta->start;
	*c->meta->last ++ = 0x05;
	*c->meta->last ++ = 0x00;

	debug_log("%s --- socks5 auth replay [%d]", __func__, meta_len( c->meta->pos, c->meta->last ) );
	debug_log("%s --- socks5 replay [%x,%x]", __func__, *c->meta->pos, * (c->meta->pos + 1) );

	c->read->handler = NULL;
	c->write->handler = socks5_local_auth_replay;
	event_opt( c->write, EVENT_WRITE );

	return c->write->handler( c->write );
}

// socks5_local_state_init -----------
static status socks5_local_state_init( event_t * ev )
{
	connection_t * c;

	char buff[1024] = {0};
	char * p = NULL;
	int32 i = 0;
	ssize_t rc;
	socks5_cycle_t * cycle = NULL;

	c = ev->data;
	/*
		1 byte		1 byte					1-255 byte
		version | authentication method | auth method list

	*/
	cycle = l_safe_malloc( sizeof(socks5_cycle_t) );
	if( !cycle ) {
		err_log("%s --- l safe malloc cycle", __func__ );
		return ERROR;
	}
	memset( cycle, 0, sizeof(socks5_cycle_t) );
	cycle->down = c;

	c->data = (void*)cycle;
	if( !c->meta ) {
		if( OK != meta_alloc( &c->meta, SOCKS5_META_LENGTH ) ) {
			err_log( "%s --- c meta alloc", __func__ );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	}
	c->send = sends;
	c->recv = recvs;
	c->send_chain = send_chains;
	c->recv_chain = NULL;
	debug_log("%s --- meta space [%d]", __func__, meta_len( c->meta->last, c->meta->end ) );
	rc = c->recv( c, c->meta->last, meta_len( c->meta->last, c->meta->end ) );
	if( rc == ERROR ) {
		err_log("%s --- recv socks5 auth failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	} else if ( rc == AGAIN ) {
		debug_log("%s --- recv socks5 again", __func__ );
		return AGAIN;
	} else {
		c->meta->last += rc;
	}

	rc = meta_len( c->meta->pos, c->meta->last );
	debug_log("%s --- recvd [%d] bytes", __func__, rc );
	if( rc < 3 ) {
		return AGAIN;
	} else {
		debug_log("%s --- socks version [%x]", __func__, buff[0] );
		debug_log("%s --- authentication method num [%x]", __func__, buff[1] );
		if( buff[1] > 0x00 ) {
			for( p = &buff[2]; i < buff[1]; i ++, p ++ ) {
				debug_log("%s --- authenticaition methods [%x]", __func__, *p );
			}
		} else {
			debug_log("%s --- have't authentication method", __func__ );
		}

		c->read->handler = socks5_local_auth_replay_prepare;
		return c->read->handler( c->read );
	}
}
// socks5_local_init -----------
status socks5_local_init( void )
{
	listen_add( 3333, socks5_local_state_init, TCP );
	return OK;
}
// socks5_local_end -----------
status socks5_local_end( void )
{
	return OK;
}
