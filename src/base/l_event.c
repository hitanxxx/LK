#include "lk.h"

static queue_t		queue_accept;
queue_t				queue_event;

static int32		event_fd = 0;
static struct epoll_event * events = NULL;

static uint32		accept_mutex_open = 0;
static uint32		accept_event_open = 0;
sem_t*				accept_mutex = NULL;
pid_t				accept_mutex_user = 0;

// event_connect ---------------
status event_connect( event_t * event )
{
	int32 fd, rc;
	int  reuse_addr = 1;
	connection_t * c;

	c = (connection_t*)event->data;

	fd = socket(AF_INET, SOCK_STREAM, 0 );
	if( fd < 0 ) {
		err_log( "%s --- socket", __func__ );
		return ERROR;
	}
	net_non_blocking( fd );

	if ( ERROR == setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuse_addr, sizeof(int)) ) {
		err_log( "%s --- setsockopt SO_REUSEADDR", __func__ );
		close( fd );
		return ERROR;
	}

	rc = connect( fd, (struct sockaddr*)&c->addr, sizeof(struct sockaddr_in) );
	if( rc == ERROR ) {
		if( errno != EINPROGRESS ) {
			err_log( "%s --- connect failed, [%d]", __func__, errno );
			close(fd);
			return ERROR;
		}
	}
	c->fd = fd;
	return OK;
}
// event_accept ----------------
status event_accept( event_t * event )
{
	connection_t * c, *listen_c;
	struct sockaddr_in client_addr;
	socklen_t len;
	int32 fd;
	listen_t * ls;

	listen_c = event->data;
	ls = listen_c->data;

	while( 1 ) {
		len = sizeof( struct sockaddr_in );
		memset(&client_addr, 0, sizeof(struct sockaddr_in));
		fd = accept( listen_c->fd, (struct sockaddr *)&client_addr, &len );
		if( ERROR == fd ) {
			if( errno == ECONNABORTED ) {
				continue;
			}
			if( errno == EAGAIN || errno == EWOULDBLOCK ) {
				return AGAIN;
			}
		}
		if( ERROR == net_alloc( &c ) ) {
			err_log( "%s --- net_alloc", __func__ );
			close( fd );
			return ERROR;
		}
		c->fd = fd;
		memcpy( &c->addr, &client_addr, sizeof(struct sockaddr_in) );
		net_non_blocking(c->fd);

		c->recv = recvs;
		c->send = sends;
		c->recv_chain = NULL;
		c->send_chain = send_chains;

		if( ls->type == HTTPS ){
			c->ssl_flag = 1;
		}
		c->read->handler = ls->handler;

		event_opt( c->read, EVENT_READ );
		if( accept_mutex_open ) {
			queue_insert( &queue_event, &c->read->queue );
		} else {
			c->read->handler( c->read );
		}
	}
	return OK;
}
// event_opt ----------------
status event_opt( event_t * event, net_events flag )
{
	event_t * e;
	struct epoll_event ev;
	int32 op;
	connection_t * c;
	net_events events, prev;

	c = event->data;
	events = flag;

	if( event->f_active == 1 ) {
		return OK;
	}

	if( flag == EVENT_WRITE ) {
		e = c->read;
		prev = EPOLLIN; //|EPOLLRDHUP;
	} else if ( flag == EVENT_READ ) {
		e = c->write;
		prev = EPOLLOUT;
	} else {
		err_log( "%s --- not support flag", __func__ );
		return ERROR;
	}

	if( e->f_active ) {
		op = EPOLL_CTL_MOD;
		events |= prev;
	} else {
		op = EPOLL_CTL_ADD;
	}

	ev.data.ptr = (void*)c;
	ev.events =  EPOLLET | events;

	if( OK != epoll_ctl( event_fd, op, c->fd, &ev ) ) {
		err_log( "%s --- EPOLL_CTL_MOD [%d] EPOLL_CTL_ADD [%d]", __func__, EPOLL_CTL_MOD, EPOLL_CTL_DEL );
		err_log( "%s --- epoll ctl [%d] failed, errno [%d]", __func__, op, errno );
		return ERROR;
	}
	event->f_active = 1;
	c->f_active = 1;

	return OK;
}
// event_close ----------
status event_close( event_t * event, uint32 flag )
{
	struct epoll_event ev;
	connection_t * c;
	net_events prev_want;
	event_t *e, *prev;
	uint32	need_active;

	c = event->data;

	if( flag == EVENT_READ ) {
		e = c->read;
		prev = c->write;
		prev_want = EPOLLOUT;
	} else if( flag == EVENT_WRITE ) {
		e = c->write;
		prev = c->read;
		prev_want = EPOLLIN;
	} else {
		debug_log( "%s --- not support flag", __func__ );
		return ERROR;
	}

	// if ev dont need to del
	if( !e->f_active ) {
		return OK;
	}
	// if need del, get reverse
	if( prev->f_active ) {
		need_active = 1;
	}
	// del all
	ev.data.ptr = (void*)c;
	ev.events = EPOLLET | EPOLLIN | EPOLLOUT;

	if( OK != epoll_ctl( event_fd, EPOLL_CTL_DEL, c->fd, &ev ) ) {
		err_log( "%s --- epoll del failed, errno [%d]", __func__, errno );
		return ERROR;
	}
	c->read->f_active = 0;
	c->write->f_active = 0;
	// add prev
	if( need_active ) {
		ev.data.ptr = (void*)c;
		ev.events =  EPOLLET | prev_want;

		if( OK != epoll_ctl( event_fd, EPOLL_CTL_ADD, c->fd, &ev ) ) {
			err_log( "%s --- epoll del failed, errno [%d]", __func__, errno );
			return ERROR;
		}
		prev->f_active = 1;
	}
	return OK;
}
// event_accept_event_opt ------------------
static status event_accept_event_opt( uint32 opt )
{
	uint32 i;
	listen_t * listen_head;

	for( i = 0; i < listens->elem_num; i ++ ) {
		listen_head = mem_list_get( listens, i+1 );
		if( conf.reuse_port && process_num > 1 ) {
			listen_head = mem_list_get( listen_head->list, process_id + 1 );
		}
		if( opt ) {
			event_opt( listen_head->c->read, EVENT_READ );
		} else {
			event_close( listen_head->c->read, EVENT_READ );
		}
	}
	return OK;
}
// event_loop -------------------------
status event_loop( time_t time_out )
{
	event_t * event;
	int32 i, action_num;
	connection_t * c;
	uint32 ev;
	queue_t	* queue, *next;
	int32 rc;

	if( accept_mutex_open ) {
		rc = sem_trywait( accept_mutex );
		if( rc == ERROR ) {
			if( accept_event_open ) {
				event_accept_event_opt( 0 );
				accept_event_open = 0;
			}
			time_out = 500;
		} else {
			accept_mutex_user = process_arr[process_id].pid;
			if( !accept_event_open ) {
				event_accept_event_opt( 1 );
				accept_event_open = 1;
			}
		}
	}

	action_num = epoll_wait( event_fd, events, MAXEVENTS, (int)time_out );
	l_time_update( );
	if( action_num == ERROR ) {
		if( errno == EINTR ) {
			debug_log("%s --- epoll_wait interrupted by signal", __func__ );
			return OK;
		}
		err_log( "%s --- epoll_wait failed, [%d]", __func__ );
		return ERROR;
	} else if( action_num == 0 ) {
		if( time_out != -1 ) {
			return OK;
		}
		err_log( "%s --- epoll_wait return 0", __func__ );
		return ERROR;
	}

	for( i = 0; i < action_num; i ++ ) {
		c = (connection_t*)events[i].data.ptr;
		if( !c->f_active ) {
			continue;
		}
		ev = events[i].events;
		event = c->read;
		if( (ev & EPOLLIN) && event->f_active ) {
			queue = event->f_accept ? &queue_accept : &queue_event;
			if( accept_mutex_open ) {
				queue_insert( queue, &event->queue );
			} else {
				if( event->handler ) {
					event->handler( event );
				}
			}
		}
		event = c->write;
		if( (ev & EPOLLOUT) && event->f_active ) {
			if( accept_mutex_open ) {
				queue_insert( &queue_event, &event->queue );
			} else {
				if( event->handler ) {
					event->handler( event );
				}
			}
		}
	}
	// deal with accept queue
	if( !queue_empty( &queue_accept ) ) {
		queue = queue_head( &queue_accept );
		while( queue != queue_tail( &queue_accept ) ) {
			next = queue_next( queue );
			queue_remove( queue );
			event = l_get_struct( queue, event_t, queue );
			if( event->handler ) {
				event->handler( event );
			}
			queue = next;
		}
	}

	if( accept_mutex_open ) {
		if( accept_event_open == 1 ) {
			sem_post( accept_mutex );
			accept_mutex_user = 0;
		}
	}

	// deal with event queue
	if( !queue_empty( &queue_event ) ) {
		queue = queue_head( &queue_event );
		while( queue != queue_tail( &queue_event ) ) {
			next = queue_next( queue );

			queue_remove( queue );
			event = l_get_struct( queue, event_t, queue );
			if( event->handler ) {
				event->handler( event );
			}
			queue = next;
		}
	}
	return OK;
}
// event_process_init ------------------
status event_process_init( void )
{
	uint32 i;
	listen_t * listen_head, *listen;
	connection_t * c;

	queue_init( &queue_accept );
	queue_init( &queue_event );
	events = (struct epoll_event*)l_safe_malloc( sizeof(struct epoll_event) * MAXEVENTS );
	if( !events ) {
		err_log( "%s --- l_safe_malloc events", __func__ );
		return ERROR;
	}
	event_fd = epoll_create1(0);
	if( event_fd == ERROR ) {
		err_log( "%s --- cpoll create", __func__ );
		return ERROR;
	}

	if( conf.perf_switch ) {
		if( process_id != process_num - 1 && process_id != 0xffff ) {
			// goto return
		} else {
			for( i = 0; i < listens->elem_num; i ++ ) {
				listen_head = mem_list_get( listens, i+1 );
				if( OK != net_alloc( &c ) ) {
					err_log( "%s --- net alloc", __func__ );
					return ERROR;
				}
				c->fd = listen_head->fd;
				c->read->f_accept = 1;
				c->read->handler = event_accept;
				c->data = listen_head;
				listen_head->c = c;
				event_opt( listen_head->c->read, EVENT_READ );
			}
		}
		return OK;
	}
	for( i = 0; i < listens->elem_num; i ++ ) {
		listen_head = mem_list_get( listens, i+1 );
		if( conf.reuse_port && process_num > 1 ) {
			listen = mem_list_get( listen_head->list, process_id + 1 );
			if( OK != net_alloc( &c ) ) {
				err_log( "%s --- net alloc", __func__ );
				return ERROR;
			}
			c->fd = listen->fd;
			c->read->f_accept = 1;
			c->read->handler = event_accept;
			c->data = listen;
			listen->c = c;
			if( !accept_mutex_open ) {
				event_opt( c->read, EVENT_READ );
			}
		} else {
			if( OK != net_alloc( &c ) ) {
				err_log( "%s --- net alloc", __func__ );
				return ERROR;
			}
			c->fd = listen_head->fd;
			c->read->f_accept = 1;
			c->read->handler = event_accept;
			c->data = listen_head;
			listen_head->c = c;
			if( !accept_mutex_open ) {
				event_opt( c->read, EVENT_READ );
			}
		}
	}
	return OK;
}
// event_process_end -------------------
status event_process_end( void )
{
	if( event_fd ) {
		close( event_fd );
	}
	if( events ) {
		free( events );
	}
	return OK;
}
// event_init -----------------
status event_init( void )
{
	if( !conf.perf_switch && conf.accept_mutex &&
		!conf.reuse_port && process_num > 1 ) {
		accept_mutex_open = 1;
		accept_mutex = (sem_t*) mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE,
		MAP_ANON|MAP_SHARED, -1, 0);
		if( accept_mutex == MAP_FAILED ) {
			err_log("%s --- mmap shm failed, [%d]", __func__, errno );
			return ERROR;
		}
		sem_init( accept_mutex, 1, 1 );
	}
	return OK;
}
// event_end -----------------
status event_end( void )
{
	if( accept_mutex_open ) {
		sem_destroy( accept_mutex );
		munmap((void *) accept_mutex, sizeof(sem_t) );
	}
	accept_mutex_open = 0;
	return OK;
}
