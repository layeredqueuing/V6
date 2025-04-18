/* -*- c++ -*- */
/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* September 1991.							*/
/************************************************************************/
#ifndef _LQSIM_LQSIM_H
#define _LQSIM_LQSIM_H

/*
 * Global vars for setting up simulation.
 *
 * $Id: lqsim.h 17458 2024-11-12 11:54:17Z greg $
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <cstdio>
#include <regex>

#define BUG_289		0	// Parasol
#define	BUG_313		1

#if BUG_289

#else
#include <parasol/parasol.h>
#endif

#define	MAX_PHASES	3
#define MAX_TASKS	16384
#define	MAX_PORTS	16384
#define	MAX_NODES	32768

#define	LINKS_MESSAGE_SIZE	1

#define EPSILON 0.000001

extern bool debug_flag;			/* Debugging flag set?      	*/
extern bool debug_interactive_stepping;
extern bool rtf_flag;			/* Output in RTF.		*/
extern bool raw_stat_flag;		/* Verbose text output?	    	*/
extern bool verbose_flag;		/* Verbose text output?	    	*/
extern bool no_execute_flag;		/* Run simulation if false	*/
extern bool timeline_flag;		/* Generate output for timeline	*/
extern bool trace_msgbuf_flag;		/* Observe msg buffer operation	*/
extern bool reload_flag;		/* Reload results from LQX run.	*/
extern bool restart_flag;		/* Restart and mussing run 	*/
extern bool quorum_delayed_calls;	/* Quorum reply (BUG_311)	*/
extern bool check_stacks;		/* Test for stack overrun	*/

extern unsigned long watched_events;	/* Observe these events.	*/

extern int trace_driver;		/* trace sim. drriver.		*/

extern int scheduling_model;		/* Slice/Natural scheduling.	*/

extern char * histogram_output_file;	/* File name for histogram data	*/

extern std::regex processor_match_pattern;	/* Pattern to match.	    */
extern std::regex task_match_pattern;		/* Pattern to match.	    */

extern FILE * stddbg;

void * my_malloc( size_t size );
void * my_realloc( void * ptr, size_t size );
void report_matherr( FILE * output );

extern int nice_value;
extern bool deferred_exception;		/* Fault detected in thread	*/

/*
 * These items are used with bit-tests.
 *   bit 0 == default/custom
 *   bit 1 == slice/natural
 */

#define SCHEDULE_SLICE		0x0
#define	SCHEDULE_CUSTOM		0x1
#define	SCHEDULE_NATURAL	0x2
#define SCHEDULE_CUSTOM_NATURAL	0x3

/* For open queues. */
#define DEFAULT_QUEUE_SIZE	1024

typedef enum
{
    ASYNC_INTERACTION_INITIATED,
    SYNC_INTERACTION_INITIATED,
    SYNC_INTERACTION_ESTABLISHED,
    SYNC_INTERACTION_REPLIES,
    SYNC_INTERACTION_COMPLETED,
    SYNC_INTERACTION_ABORTED,
    SYNC_INTERACTION_FORWARDED,
    WORKER_DISPATCH,
    WORKER_IDLE,
    TASK_CREATED,
    TASK_IS_READY,
    TASK_IS_RUNNING,			/* doing message passing... */
    TASK_IS_COMPUTING,			/* executing virtual code... */
    TASK_IS_WAITING,
    THREAD_START,
    THREAD_ENQUEUE_MSG,
    THREAD_DEQUEUE_MSG,
    THREAD_IDLE,
    THREAD_CREATE,
    THREAD_REAP,
    THREAD_STOP,
    ACTIVITY_START,
    ACTIVITY_EXECUTE,
    ACTIVITY_FORK,
    ACTIVITY_JOIN,
    DEQUEUE_READER,
    DEQUEUE_WRITER,
    ENQUEUE_READER,
    ENQUEUE_WRITER,
    QUEUE_TIMEOUT,
    MSG_RETRY,
    MSG_ABORT
} trace_events;

#define ASYNC_INTERACTION_INITIATED_BIT (1<<ASYNC_INTERACTION_INITIATED)
#define SYNC_INTERACTION_INITIATED_BIT (1<<SYNC_INTERACTION_INITIATED)
#define SYNC_INTERACTION_ESTABLISHED_BIT (1<<SYNC_INTERACTION_ESTABLISHED)
#define SYNC_INTERACTION_REPLIES_BIT (1<<SYNC_INTERACTION_REPLIES)
#define SYNC_INTERACTION_COMPLETED_BIT (1<<SYNC_INTERACTION_COMPLETED)
#define SYNC_INTERACTION_ABORTED_BIT (1<<SYNC_INTERACTION_ABORTED)
#define SYNC_INTERACTION_FORWARDED_BIT (1<<SYNC_INTERACTION_FORWARDED)
#define WORKER_DISPATCH_BIT (1<<WORKER_DISPATCH)
#define WORKER_IDLE_BIT (1<<WORKER_IDLE)
#define TASK_CREATED_BIT (1<<TASK_CREATED)
#define TASK_IS_READY_BIT (1<<TASK_IS_READY)
#define TASK_IS_RUNNING_BIT (1<<TASK_IS_RUNNING)
#define TASK_IS_COMPUTING_BIT (1<<TASK_IS_COMPUTING)
#define TASK_IS_WAITING_BIT (1<<TASK_IS_WAITING)
#define THREAD_START_BIT (1<<THREAD_START)
#define THREAD_ENQUEUE_MSG_BIT (1<<THREAD_ENQUEUE_MSG)
#define THREAD_DEQUEUE_MSG_BIT (1<<THREAD_DEQUEUE_MSG)
#define THREAD_IDLE_BIT (1<<THREAD_IDLE)
#define THREAD_CREATE_BIT (1<<THREAD_CREATE)
#define THREAD_REAP_BIT (1<<THREAD_REAP)
#define THREAD_STOP_BIT (1<<THREAD_STOP)
#define ACTIVITY_START_BIT (1<<ACTIVITY_START)
#define ACTIVITY_EXECUTE_BIT (1<<ACTIVITY_EXECUTE)
#define ACTIVITY_FORK_BIT (1<<ACTIVITY_FORK)
#define ACTIVITY_JOIN_BIT (1<<ACTIVITY_JOIN)

#endif
