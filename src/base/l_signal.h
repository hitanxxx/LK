#ifndef _L_SIGNAL_H_INCLUDED_
#define _L_SIGNAL_H_INCLUDED_

extern sig_atomic_t		sig_quit;
extern sig_atomic_t		sig_reap;
extern sig_atomic_t 	sig_perf;
extern sig_atomic_t		sig_perf_stop;

status l_signal_init( void );
status l_signal_end( void );

#endif
