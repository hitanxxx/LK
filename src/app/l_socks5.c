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

// socks5_time_out --------
static void socks5_time_out( void * data )
{
	connection_t * c;
	
	c = data;
	
	net_free( c );
}

// socks5_connect -----------
static status socks5_connect( event_t * ev )
{
	connection_t * c;
	socks5_cycle_t * cycle;
	
	c = ev->data;
	cycle = c->data;
	
	debug_log("%s --- socks5_ connect", __func__  );
	socks5_cycle_free( cycle );
	
	return OK;
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
				cycle->request.dst_port[0] = *p;
				state = dst_port_end;
				continue;
			}
			if( state == dst_port_end ) {
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
				debug_log("%s --- dst port [%d]", __func__, *(int32*)&cycle->request.dst_port[0] );
				
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
	
	rc = c->send_chain( c, c->meta );
	if( rc == ERROR ) {
		err_log("%s --- send auth replay failed", __func__ );
		net_free( c );
		return ERROR;
	} else if ( rc == DONE ) {
		debug_log("%s --- send auth replay success", __func__ );
		timer_del( &c->read->timer );
		
		c->meta->pos = c->meta->last = c->meta->start;
		
		cycle = l_safe_malloc( sizeof(socks5_cycle_t) );
		if( !cycle ) {
			err_log("%s --- safe malloc cycle", __func__ );
			net_free( c );
			return ERROR;
		}
		memset( cycle, 0, sizeof(socks5_cycle_t) );
		memset( &cycle->request, 0, sizeof(socks5_request_t) );
		
		c->data = (void*)cycle;
		cycle->down = c;
		
		c->write->handler = NULL;
		c->read->handler = socks5_process_request;
		return c->read->handler( c->read );
	}
	c->read->timer.data = (void*)c;
	c->read->timer.handler = socks5_time_out;
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
	
	c = ev->data;
	/*
		1 byte		1 byte					1-255 byte
		version | authentication method | auth method list 
		
	*/
	if( !c->meta ) {
		if( OK != meta_alloc( &c->meta, SOCKS5_META_LENGTH ) ) {
			err_log( "%s --- c meta alloc", __func__ );
			net_free(c);
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
		net_free( c );
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