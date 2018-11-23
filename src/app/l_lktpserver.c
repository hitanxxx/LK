#include "lk.h"

static lktp_manager_t * server = NULL;
static timer_msg_t  	timer_connect;

status lktpserv_over( lktp_manager_t * lktp, int32 status );
status lktpserv_send( event_t * ev );
static void lktp_connect( void * data );
static status lktpserv_recv( event_t * ev );

static status api_lktp_server_recv_syn( void * data );
static status api_lktp_client_recv_ack( void * data );
static status api_lktp_server_recv_heartbeat ( void * data );

// --------
static serv_api_t api_arr_lktp_basic[] = {
	{ string("lktp_syn"),			    api_lktp_server_recv_syn },
	{ string("lktp_ack"),   			api_lktp_client_recv_ack },
    { string("lktp_heartbeat"),   		api_lktp_server_recv_heartbeat },
	{ string_null,						NULL			}
};

// lktpserv_client_send_heartbeat ------
static void lktpserv_client_send_heartbeat( void * data )
{
    lktp_manager_t * lktp;

    lktp = data;
    string_t api = string("lktp_heartbeat");

    lktp = data;
    lktp->lktp_request = (lktp_request_t*)l_safe_malloc( sizeof(lktp_request_t) );
    if( !lktp->lktp_request ) {
        err_log("%s --- malloc lktp request", __func__ );
        lktpserv_over( lktp, ERROR );
        return;
    }
    if( OK != l_mem_create_page( &lktp->lktp_request->page, 4096 ) ) {
        err_log("%s --- lktp_request create memp page", __func__ );
        lktpserv_over( lktp, ERROR );
        return;
    }
    lktp->lktp_request->api = &api;
    lktp->lktp_request->body_raw = NULL;
    if( OK != lktp_request_create( lktp->lktp_request ) ) {
        err_log("%s --- lktp_request_create failed", __func__ );
        lktpserv_over( lktp, ERROR );
        return;
    }
    lktp->c->write->handler = lktpserv_send;
    lktp->c->write->handler( lktp->c->write );
}
// api_lktp_client_recv_ack ------
static status api_lktp_client_recv_ack( void * data )
{
    lktp_manager_t * lktp;

    lktp = data;
    return OK;
}
// api_lktp_server_recv_heartbeat  -------
static status api_lktp_server_recv_heartbeat ( void * data )
{
    lktp_manager_t * lktp;
    string_t api = string("lktp_ack");

    lktp = data;
    lktp->lktp_request = (lktp_request_t*)l_safe_malloc( sizeof(lktp_request_t) );
    if( !lktp->lktp_request ) {
        err_log("%s --- malloc lktp request", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    }
    if( OK != l_mem_create_page( &lktp->lktp_request->page, 4096 ) ) {
        err_log("%s --- lktp_request create memp page", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    }
    lktp->lktp_request->api = &api;
    lktp->lktp_request->body_raw = NULL;
    if( OK != lktp_request_create( lktp->lktp_request ) ) {
        err_log("%s --- lktp_request_create failed", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    }
    lktp->c->write->handler = lktpserv_send;
    return lktp->c->write->handler( lktp->c->write );
}
// api_lktp_server_recv_syn --------
static status api_lktp_server_recv_syn( void * data )
{
    lktp_manager_t * lktp;
    string_t api = string("lktp_ack");

    lktp = data;
	debug_log("%s --- ", __func__ );
    lktp->lktp_request = (lktp_request_t*)l_safe_malloc( sizeof(lktp_request_t) );
    if( !lktp->lktp_request ) {
        err_log("%s --- malloc lktp request", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    }
    if( OK != l_mem_create_page( &lktp->lktp_request->page, 4096 ) ) {
        err_log("%s --- lktp_request create memp page", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    }
    lktp->lktp_request->api = &api;
    lktp->lktp_request->body_raw = NULL;
    if( OK != lktp_request_create( lktp->lktp_request ) ) {
        err_log("%s --- lktp_request_create failed", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    }
    lktp->c->write->handler = lktpserv_send;
    return lktp->c->write->handler( lktp->c->write );
}
// lktpserv_timeout --------
static void lktpserv_timeout( void * data )
{
    lktp_manager_t * lktp;

    lktp = data;
    lktpserv_over( lktp, ERROR );
}
// lktpserv_over ----------
status lktpserv_over( lktp_manager_t * lktp, int32 status )
{
    // delete timers
    timer_del( &lktp->timer_message );
    if( lktp->lktp_request ) {
        if( lktp->lktp_request->page ) {
            l_mem_free_page( lktp->lktp_request->page );
        }
        lktp->lktp_request->page = NULL;
        l_safe_free( lktp->lktp_request );
    }
    lktp->lktp_request = NULL;

    if( lktp->lktp_head ) {
        lktp_head_free( lktp->lktp_head );
    }
    lktp->lktp_head = NULL;

    if( lktp->lktp_body ) {
        lktp_body_free( lktp->lktp_body );
    }
    lktp->lktp_body = NULL;

	if( status == OK ) {
		lktp->c->meta->pos = lktp->c->meta->last = lktp->c->meta->start;
		return OK;
	}

    if( lktp->c ) {
        net_free( lktp->c );
    }
    lktp->c = NULL;
    l_safe_free( lktp );

	if( conf.lktp_mode == LKTP_CLIENT ) {
		// don't care the status, just reconnect
		timer_connect.data = NULL;
	    timer_connect.handler = lktp_connect;
	    timer_add( &timer_connect, LKTP_TIMEOUT );
	}
    return OK;
}
// lktpserv_send ------
status lktpserv_send( event_t * ev )
{
    status rc;
    connection_t * c;
    lktp_manager_t * lktp;

    c = ev->data;
    lktp = c->data;

    rc = c->send_chain( c, lktp->lktp_request->lktp_request_chain );
    if( rc == ERROR ) {
        err_log("%s --- send chain faild", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    } else if ( rc == DONE ) {
        timer_del( &c->write->timer );

        lktpserv_over( lktp, OK );

        // if client , need add timer for recv
		if( conf.lktp_mode == LKTP_CLIENT ) {
			lktp->timer_message.data = (void*)lktp;
	        lktp->timer_message.handler = lktpserv_timeout;
	        timer_add( &lktp->timer_message, LKTP_TIMEOUT );
		}

        lktp->c->write->handler = NULL;
        lktp->c->read->handler = lktpserv_recv;
        return OK;
    }
    c->write->timer.data = (void*)lktp;
    c->write->timer.handler = lktpserv_timeout;
    timer_add( &c->write->timer, LKTP_TIMEOUT );
    return rc;
}
// lktpserv_api_do ------
static status lktpserv_api_do( event_t * ev )
{
    connection_t * c;
    lktp_manager_t * lktp;

    c = ev->data;
    lktp = c->data;

    return lktp->handler( (void*)lktp );
}
// lktpserv_api_find ------
static status lktpserv_api_find( event_t * ev )
{
    connection_t * c;
    lktp_manager_t * lktp;

    c = ev->data;
    lktp = c->data;

    c->read->handler = NULL;
	debug_log("%s --- find api: [%.*s], goto api function", __func__, lktp->lktp_head->api.len, lktp->lktp_head->api.data );
    if( OK == serv_api_find( &lktp->lktp_head->api, &lktp->handler ) ) {
		if( conf.lktp_mode == LKTP_CLIENT ) {
			// wait LKTP_TIMEOUT_HEARTBEAT second, send next heartbeat
		    lktp->timer_message.data = (void*)lktp;
		    lktp->timer_message.handler = lktpserv_client_send_heartbeat;
		    timer_add( &lktp->timer_message, LKTP_TIMEOUT_HEARTBEAT );
		}
        return lktpserv_api_do( c->write );
    } else {
        err_log("%s --- not find api, discard this connection", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    }
    return OK;
}
// lktpserv_recv_body --------
static status lktpserv_recv_body( event_t * ev )
{
    connection_t * c;
    lktp_manager_t * lktp;
    status rc;

    c = ev->data;
    lktp = c->data;

    rc = lktp->lktp_body->handler( lktp->lktp_body );
    if( rc == ERROR ) {
        err_log("%s --- lktp_body process faield", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    } else if ( rc == DONE ) {
        timer_del( &c->read->timer );

        return lktpserv_api_find( c->read );
    }
    c->read->timer.data = (void*)lktp;
    c->read->timer.handler = lktpserv_timeout;
    timer_add( &c->read->timer, LKTP_TIMEOUT );
    return rc;
}
// lktpserv_recv_header ----------
static status lktpserv_recv_header( event_t * ev )
{
    connection_t * c;
    lktp_manager_t * lktp;
    status rc;

    c = ev->data;
    lktp = c->data;

    rc = lktp->lktp_head->handler( lktp->lktp_head );
    if( rc == ERROR ) {
        err_log("%s --- lktp_head process failed", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    } else if ( rc == DONE ) {
        timer_del( &c->read->timer );

        if( lktp->lktp_head->body_length_n ) {
            if( OK != lktp_body_create( c, &lktp->lktp_body ) ) {
                err_log("%s --- lktp_body process body", __func__ );
                lktpserv_over( lktp, ERROR );
                return ERROR;
            }
            c->read->handler = lktpserv_recv_body;
        } else {
            c->read->handler = lktpserv_api_find;
        }
        return c->read->handler( c->read );
    }
    c->read->timer.data = (void*)lktp;
    c->read->timer.handler = lktpserv_timeout;
    timer_add( &c->read->timer, LKTP_TIMEOUT );
    return rc;
}
// lktpserv_recv --------
static status lktpserv_recv( event_t * ev )
{
    connection_t * c;
    lktp_manager_t * lktp;

    c = ev->data;
    lktp = c->data;
    if( OK != lktp_head_create( c, &lktp->lktp_head ) ) {
        err_log("%s --- lktp_head_create failed", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    }
    c->read->handler = lktpserv_recv_header;
    return c->read->handler( c->read );
}
// lktpserv_init_connection ---------
static status lktpserv_init_connection( event_t * ev )
{
    connection_t * c;
    lktp_manager_t * lktp = NULL;

    c = ev->data;
    lktp = (lktp_manager_t*)l_safe_malloc( sizeof(lktp_manager_t) );
    if( !lktp ) {
        err_log("%s --- l_safe_malloc lktp", __func__ );
        return ERROR;
    }
    memset( lktp, 0, sizeof(lktp_manager_t) );
    lktp->c = c;
    c->data = (void*)lktp;
	if( !c->meta ) {
		if( OK != meta_alloc( &c->meta, 4096 ) ) {
			err_log( "%s --- c meta alloc", __func__ );
			lktpserv_over( lktp, ERROR );
			return ERROR;
		}
	}
	event_opt( c->write, EVENT_WRITE );
	c->write->handler = NULL;
    c->read->handler = lktpserv_recv;
    return c->read->handler( c->read );
}

// lktp_register ------
static status lktp_register( event_t * ev )
{
	connection_t * c;
	lktp_manager_t * lktp;
    string_t api = string("lktp_syn");

	c = ev->data;
	lktp = c->data;
    lktp->lktp_request = (lktp_request_t*)l_safe_malloc( sizeof(lktp_request_t) );
    if( !lktp->lktp_request ) {
        err_log("%s --- malloc lktp request", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    }
    if( OK != l_mem_create_page( &lktp->lktp_request->page, 4096 ) ) {
        err_log("%s --- lktp_request create memp page", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    }
    lktp->lktp_request->api = &api;
    lktp->lktp_request->body_raw = NULL;
    if( OK != lktp_request_create( lktp->lktp_request ) ) {
        err_log("%s --- lktp_request_create failed", __func__ );
        lktpserv_over( lktp, ERROR );
        return ERROR;
    }

    c->write->handler = lktpserv_send;
    return c->write->handler( c->write );
}
// lktp_connect_check_test ------
static status lktp_connect_check_test( connection_t * c )
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
// lktp_connect_check -----
static status lktp_connect_check( event_t * ev )
{
	connection_t * c;
	lktp_manager_t * lktp;

	c = ev->data;
	lktp = c->data;
    if( OK != lktp_connect_check_test( c ) ) {
		err_log( "%s --- tunnel connect remote", __func__ );
		lktpserv_over( lktp, ERROR );
        return ERROR;
	}
    debug_log("%s --- connect success", __func__ );
    timer_del( &c->write->timer );
    net_nodelay( c );

	server->status = LKTP_ESTABLE;
	event_opt( lktp->c->read, EVENT_READ );

    c->write->handler = lktp_register;
    return c->write->handler( c->write );
}
// lktp_connect -----
static void lktp_connect( void * data )
{
	uint32 port = 5555;
	char arr[128] = {0};
	status rc;

    l_unused( data );
	server = (lktp_manager_t*)l_safe_malloc( sizeof(lktp_manager_t) );
    if( !server ) {
        err_log("%s --- malloc lktp", __func__ );
		timer_connect.data = NULL;
	    timer_connect.handler = lktp_connect;
	    timer_add( &timer_connect, LKTP_TIMEOUT );
        return;
    }
    memset( server, 0, sizeof(lktp_manager_t) );
	server->status = LKTP_CONNECT;

    if( OK != net_alloc( &server->c ) ) {
        err_log("%s --- net alloc", __func__ );
        lktpserv_over( server, ERROR );
        return;
    }
    if( !server->c->meta ) {
		if( OK != meta_alloc( &server->c->meta, 4096 ) ) {
			err_log( "%s --- net meta alloc", __func__ );
            lktpserv_over( server, ERROR );
            return;
		}
	}
	server->c->send = sends;
	server->c->recv = recvs;
	server->c->send_chain = send_chains;
	server->c->recv_chain = NULL;
    server->c->data = (void*)server;

    memset( &server->addr, 0, sizeof(struct sockaddr_in) );
	snprintf( arr, sizeof(arr), "%.*s", conf.lktp_serverip.len, conf.lktp_serverip.data );
	debug_log("%s --- connect %s:%d", __func__, arr, port );
	server->c->addr.sin_family = AF_INET;
	server->c->addr.sin_port = htons( port );
	server->c->addr.sin_addr.s_addr = inet_addr( arr );

    rc = event_connect( server->c->write );
    if( rc == ERROR ) {
        err_log("%s --- connect remote failed, [%d]", __func__ );
        lktpserv_over( server, ERROR );
        return;
    }
    server->c->write->handler = lktp_connect_check;
    event_opt( server->c->write, EVENT_WRITE );
    if( rc == AGAIN ) {
        server->c->write->timer.data = NULL;
        server->c->write->timer.handler = lktpserv_timeout;
        timer_add( &server->c->write->timer, LKTP_TIMEOUT );
        return;
    }
    lktp_connect_check( server->c->write );
}
// lktpserv_init --------
status lktpserv_init( void )
{
    uint32 i;

    for( i = 0; api_arr_lktp_basic[i].handler; i ++ ) {
		serv_api_register( &api_arr_lktp_basic[i] );
	}

	if( conf.lktp_mode == LKTP_SERVER ) {
		listen_add( 5555, lktpserv_init_connection, LKTP );
	}
	if( conf.lktp_mode == LKTP_CLIENT ) {
		timer_connect.data = NULL;
	    timer_connect.handler = lktp_connect;
	    timer_add( &timer_connect, LKTP_TIMEOUT );
	}
    return OK;
}
// lktpserv_end ------
status lktpserv_end( void )
{
    return OK;
}
