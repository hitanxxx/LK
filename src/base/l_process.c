#include "lk.h"

process_t 	process_arr[MAXPROCESS];
uint32		process_num = 0;
uint32		process_id = 0xffff;

// process_get_run --------------
process_t * process_get_run( void )
{
	if( process_num < 1 ) {
		return NULL;
	}
	return &process_arr[process_id];
}
// process_broadcast_child -----------------
static status process_broadcast_child( int32 lsignal )
{
	uint32 i = 0;

	for( i = 0; i < process_num; i ++ ) {
		if( ERROR == kill( process_arr[i].pid, lsignal ) ) {
			err_log( "%s --- kill signal [%d] child", __func__, signal );
		}
		process_arr[i].exiting = 1;
	}
	return OK;
}
// process_perf_stop ----------
static void process_perf_stop ( void  )
{
	performance_process_stop( NULL );
}
// process_perf_start -------------
static void process_perf_start ( void )
{
	meta_t * t;
	json_t * json;

	if( OK != config_get( &t, L_PATH_PERFTEMP ) ) {
		err_log( "%s --- get perf data", __func__ );
		return;
	}
	if( OK != json_decode( &json, t->pos, t->last ) ) {
		err_log( "%s --- json decode", __func__ );
		meta_free( t );
		return;
	}
	performance_process_start( (void*)json );
	json_free( json );
	meta_free( t );
	return;
}
// process_worker_end ------------------
static void process_worker_end( void )
{
	dynamic_module_end( );
	debug_log( "%s --- worker process exiting", __func__ );
	exit(0);
}
// process_worker_run -------------------
static void process_worker_run( void )
{
	int32 timer;
	sigset_t    set;

	sigemptyset( &set );
	sigprocmask( SIG_SETMASK, &set, NULL );

	dynamic_module_init( );
	while(1) {
		if( sig_quit == 1 ) {
			process_worker_end( );
		}
		if( sig_perf == 1 ) {
			process_perf_start( );
			sig_perf = 0;
		}
		if( sig_perf_stop == 1 ) {
			process_perf_stop( );
			sig_perf_stop = 0;
		}
		timer_expire( &timer );
		event_loop( timer );
	}
}
// process_spawn ------------------
static pid_t process_spawn( process_t * pro )
{
	pid_t 		id;

	id = fork( );
	if( id < 0 ) {
		err_log( "%s --- fork error", __func__ );
		return ERROR;
	} else if ( id > 0 ) {

		// parent
		pro->pid = id;
	} else if ( id == 0 ) {

		// child
		process_id = pro->nid;
		process_worker_run( );
	}
	return id;
}
// reap_child ----------------------
static pid_t process_reap( void )
{
	int32 live = 0;
	uint32 i;
	pid_t pid;

	for( i = 0; i < process_num; i ++ ) {
		if( process_arr[i].pid == ERROR ) {
			continue;
		}
		if( process_arr[i].exited ) {
			if( !process_arr[i].exiting && !sig_quit ) {
				pid = process_spawn( &process_arr[i] );
				if( pid == ERROR ) {
					err_log( "%s --- spawn errno [%d]", __func__, errno );
					return ERROR;
				}
				process_arr[i].exited = 0;
				live = 1;
			}
		} else {
			if( process_arr[i].exiting ) {
				live = 1;
			}
		}
	}
	return live;
}
// process_worker_run -------------------
static status process_worker_start( void )
{
	uint32 i;

	for( i = 0; i < process_num; i++ ) {
		if( ERROR == process_spawn( &process_arr[i] ) ) {
			err_log( "%s --- process spawn error", __func__ );
			return ERROR;
		}
	}
	return OK;
}
// process_master_run -------------------------
status process_master_run( void )
{
	int32 live = 1;
	sigset_t    set;

    sigemptyset( &set );
    sigaddset( &set, SIGCHLD );
    sigaddset( &set, SIGINT );
	sigaddset( &set, SIGUSR1 );
	sigaddset( &set, SIGUSR2 );

    if( sigprocmask( SIG_BLOCK, &set, NULL ) == ERROR ) {
		err_log( "%s --- sigs_suppend_init", __func__ );
		return ERROR;
    }
	sigemptyset(&set);

	process_worker_start( );
	// master running wait signal
	while( 1 ) {
		sigsuspend( &set );

		if( sig_reap == 1 ) {
			sig_reap = 0;
			live = process_reap(  );
		}
		if( live == 0 && sig_quit == 1 ) {
			break;
		}
		if( sig_quit == 1 ) {
			process_broadcast_child( SIGINT );
			continue;
		}
	}
	return OK;
}
// process_single_run ---------------
void process_single_run( void )
{
	int32 timer;
	sigset_t    set;

	sigemptyset( &set );
	sigprocmask( SIG_SETMASK, &set, NULL );

	dynamic_module_init( );
	while(1) {
		if( sig_quit == 1 ) {
			dynamic_module_end();
			break;
		}
		if( sig_perf == 1 ) {
			process_perf_start( );
			sig_perf = 0;
		}
		if( sig_perf_stop == 1 ) {
			process_perf_stop( );
			sig_perf_stop = 0;
		}
		timer_expire( &timer );
		event_loop( timer );
	}
}
// process_init -----------------------
status process_init( void )
{
	uint32 i;
	uint32 process_number = 0;

	process_number = conf.worker_process;
	for( i = 0; i < process_number; i ++ ) {
		process_arr[i].nid = i;
		process_num ++;
		if( process_num >= MAXPROCESS ) {
			err_log( "%s --- process number too much", __func__ );
			return ERROR;
		}
	}
	return OK;
}
// process_end -----------------------
status process_end( void )
{
	return OK;
}
