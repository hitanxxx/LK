#include "lk.h"

sig_atomic_t		sig_quit = 0;
sig_atomic_t		sig_reap = 0;
sig_atomic_t 		sig_perf = 0;
sig_atomic_t		sig_perf_stop = 0;

// l_signal_child_status -------------------
static void l_signal_child_status( void )
{
	int32 pid;
	uint32 i;

	for( ;; ) {
		pid = waitpid( -1, NULL, WNOHANG );
		if( pid == 0 ) {
			return;
		}
		if( pid == -1 ) {
			if( errno == EINTR ) continue;

			return;
		}
		for( i = 0; i < process_num; i ++ ) {
			if( process_arr[i].pid == pid ) {
				err_log("%s --- child [%d] died", __func__, pid );
				process_arr[i].exiting = 0;
				process_arr[i].exited = 1;
				break;
			}
		}
	}
	return;
}
// l_signal_handler --------------------------
static void l_signal_handler( int32 lsignal )
{
	if( process_id == 0xffff ) {
		if( lsignal == SIGINT || lsignal == SIGHUP ) {
			sig_quit = 1;
		} else if ( lsignal == SIGCHLD ) {
			sig_reap = 1;
		}
	} else {
		if( lsignal == SIGINT ) {
			sig_quit = 1;
		} else if( lsignal == SIGUSR1 ) {
			sig_perf = 1;
		} else if ( lsignal == SIGUSR2 ) {
			sig_perf_stop = 1;
		}
	}
	
	if( lsignal == SIGCHLD ) {
		if( process_id == 0xffff ) {
			l_signal_child_status( );
		}
	}
	return;
}
// l_signal_init --------------------
status l_signal_init( void )
{
	uint32 i;
	struct sigaction sa;
	int32 sig_arr[] = {
		SIGINT,
		SIGHUP,
		SIGCHLD,
		SIGPIPE,
		SIGUSR1,
		SIGUSR2,
		0
	};

	for( i = 0; sig_arr[i]; i ++ ) {
		memset( &sa, 0, sizeof(struct sigaction) );
		sigemptyset( &sa.sa_mask );
		sa.sa_handler = l_signal_handler;
		sa.sa_flags = SA_SIGINFO;
		sigemptyset(&sa.sa_mask);
		if( sigaction( sig_arr[i], &sa, NULL ) == -1 ) {
			err_log( "%s --- set sig handler [%d]", __func__, sig_arr[i] );
			return ERROR;
		}
	}
	return OK;
}
// l_signal_end ---------------------
status l_signal_end( void )
{
	return OK;
}
