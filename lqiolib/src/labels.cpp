/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* November 1995							*/
/************************************************************************/

/*
 * $Id: labels.cpp 15900 2022-09-24 12:54:03Z greg $
 */

#include "input.h"
#include "labels.h"

namespace LQIO {

    const char * cv_square_str			= "Squared coefficient of variation of execution segments:";
    const char * forwarding_probability_str	= "Forwarding probability from entry to entry:";
    const char * max_service_time_str		= "Service time limit:";
    const char * open_arrival_rate_str		= "Open arrival rates per entry:";
    const char * phase_type_str			= "Phase type flags:";
    const char * processor_info_str		= "Processor identifiers and scheduling algorithms:";
    const char * group_info_str			= "Group identifiers:";
    const char * rendezvous_rate_str		= "Mean number of rendezvous from entry to entry:";
    const char * send_no_reply_rate_str		= "Mean number of non-blocking sends from entry to entry:";
    const char * service_demand_str		= "Entry execution demands:";
    const char * task_info_str			= "Task information:";
    const char * think_time_str			= "Entry think times:";
    const char * histogram_str			= "Service time distributions for entries and activities:";
    const char * join_delay_str			= "Mean delay for a join:";
    const char * loss_probability_str		= "Arrival Loss Probabilities:";
    const char * mean_life_time_str		= "Mean life times";
    const char * open_wait_str			= "Waiting time at tasks with open arrivals:";
    const char * overtaking_prob_str		= "Overtaking Probabilities:";
    const char * semaphore_hold_time_str	= "Semaphore holding times:";
    const char * semaphore_hold_variance_str	= "Semaphore holding time variance:";

    const char * fwd_waiting_time_str		= "Mean delay for a forwarded rendezvous:";
    const char * fwd_waiting_time_variance_str	= "Variance of delay for a forwarded rendezvous:";
    const char * rwlock_hold_time_str		= "RWLock holding times:";
    const char * rwlock_reader_hold_time_str	= "RWLock reader holding times:";
    const char * rwlock_reader_hold_variance_str= "RWLock reader holding time variance:";
    const char * rwlock_writer_hold_time_str	= "RWLock writer holding times:";
    const char * rwlock_writer_hold_variance_str= "RWLock writer holding time variance:";
    const char * service_time_exceeded_str	= "Probability maximum service time exceeded:";		/*  */
    const char * service_time_str		= "Service times:";
    const char * snr_waiting_time_str		= "Mean delay for a send-no-reply request:";	
    const char * snr_waiting_time_variance_str	= "Variance of delay for a send-no-reply request:";
    const char * throughput_bounds_str		= "Type 1 throughput bounds: ";
    const char * throughput_str			= "Throughputs and utilizations per phase:";
    const char * utilization_str		= "Utilization and waiting per phase for processor: ";
    const char * variance_str			= "Service time variance (per phase)\nand squared coefficient of variation (over all phases):";
    const char * waiting_time_str		= "Mean delay for a rendezvous:";	
    const char * waiting_time_variance_str	= "Variance of delay for a rendezvous:";
	
    const char * decision_info_str              = "Decision Identifier:";
    const char * decision_path_info_str         = "Decision Path identifier:";
    const char * timeout_prob_str               = "Timeout Probability:";
    const char * timeout_prob_variance_str      = "Timeout Probability variance:";
    const char * success_prob_str               = "Success path Probability:";
    const char * success_prob_variance_str      = "Success path Probability variance:";
    const char * abort_prob_str                 = "Abort Probability:";
    const char * abort_prob_variance_str        = "Abort Probability variance:";
    const char * number_of_retries_str          = "Mean number of retries:";
    const char * number_of_retries_variance_str = "Mean number of retries variance:";

}

/*
 * Scheduling types.
 */

const char * LQIO::SCHEDULE::ABORT      = "abort";
const char * LQIO::SCHEDULE::BURST	= "burst";
const char * LQIO::SCHEDULE::CFS	= "cfs";
const char * LQIO::SCHEDULE::CUSTOMER	= "ref";
const char * LQIO::SCHEDULE::DELAY	= "inf";
const char * LQIO::SCHEDULE::FIFO	= "fcfs";
const char * LQIO::SCHEDULE::HOL	= "hol";
const char * LQIO::SCHEDULE::INF_RETRY  = "inf-retry";
const char * LQIO::SCHEDULE::LIFO	= "lcfs";
const char * LQIO::SCHEDULE::POLL	= "poll";
const char * LQIO::SCHEDULE::PPR	= "pri";
const char * LQIO::SCHEDULE::PS	    	= "ps";
const char * LQIO::SCHEDULE::RAND	= "rand";
const char * LQIO::SCHEDULE::RETRY      = "retry";
const char * LQIO::SCHEDULE::RWLOCK	= "rwlock";
const char * LQIO::SCHEDULE::SEMAPHORE  = "semaphore";
const char * LQIO::SCHEDULE::TIMEOUT    = "timeout";
const char * LQIO::SCHEDULE::UNIFORM	= "uniform";

const std::map<const scheduling_type,const LQIO::SCHEDULE::label_t> scheduling_label = 
{
    { SCHEDULE_ABORT,       { "ABORT",        LQIO::SCHEDULE::ABORT,       'T' } },
    { SCHEDULE_BURST,       { "BURST",	      LQIO::SCHEDULE::BURST,       'b' } },
    { SCHEDULE_CFS,         { "CFS",	      LQIO::SCHEDULE::CFS,         'c' } },
    { SCHEDULE_CUSTOMER,    { "CUST",	      LQIO::SCHEDULE::CUSTOMER,    'r' } },
    { SCHEDULE_DELAY,       { "DELAY",	      LQIO::SCHEDULE::DELAY,       'i' } },
    { SCHEDULE_FIFO,        { "FCFS",	      LQIO::SCHEDULE::FIFO,        'f' } },
    { SCHEDULE_HOL,         { "HOL",	      LQIO::SCHEDULE::HOL,         'h' } },
    { SCHEDULE_INF_RETRY,   { "INF_RETRY",    LQIO::SCHEDULE::INF_RETRY,   'R' } },
    { SCHEDULE_LIFO,        { "LCFS",	      LQIO::SCHEDULE::LIFO,        'l' } },
    { SCHEDULE_POLL,        { "POLL",	      LQIO::SCHEDULE::POLL,        'P' } },	/* Task */
    { SCHEDULE_PPR,         { "PPR",	      LQIO::SCHEDULE::PPR,         'p' } },
    { SCHEDULE_PS,          { "PS",	      LQIO::SCHEDULE::PS,          's' } },
    { SCHEDULE_RAND,        { "RAND",	      LQIO::SCHEDULE::RAND,        'r' } },
    { SCHEDULE_RETRY,       { "RETRY",        LQIO::SCHEDULE::RETRY,       'R' } },
    { SCHEDULE_RWLOCK,      { "RWLOCK",	      LQIO::SCHEDULE::RWLOCK,      'w' } },
    { SCHEDULE_SEMAPHORE,   { "SEMAPHORE",    LQIO::SCHEDULE::SEMAPHORE,   'S' } },
    { SCHEDULE_TIMEOUT,     { "TIMEOUT",      LQIO::SCHEDULE::TIMEOUT,     'T' } },
    { SCHEDULE_UNIFORM,     { "UNIFORM",      LQIO::SCHEDULE::UNIFORM,     'u' } },
};

