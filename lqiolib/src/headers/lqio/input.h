/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* August 1991.                                                         */
/* March 2010.								*/
/************************************************************************/

/*
 * $Id: input.h 15969 2022-10-13 19:49:43Z greg $
 */

#if	!defined(LQIO_INPUT_H)
#define	LQIO_INPUT_H

/* Don't forget to fix labels.c */
/* Add a new scheduler type 'CFS' 2008*/
/* Add two new scheduler types for decisions  2016*/
typedef enum {
    SCHEDULE_CUSTOMER,
    SCHEDULE_DELAY,
    SCHEDULE_FIFO,
    SCHEDULE_LIFO,
    SCHEDULE_HOL,
    SCHEDULE_PPR,
    SCHEDULE_RAND,
    SCHEDULE_PS,
    SCHEDULE_POLL,
    SCHEDULE_BURST,
    SCHEDULE_UNIFORM,
    SCHEDULE_SEMAPHORE,
    SCHEDULE_CFS,
    SCHEDULE_RWLOCK,
    SCHEDULE_TIMEOUT,
    SCHEDULE_ABORT,
    SCHEDULE_RETRY,
    SCHEDULE_INF_RETRY
} scheduling_type;	      

typedef enum { DEFAULT_MATHERR, IGNORE_MATHERR, REPORT_MATHERR, ABORT_MATHERR } matherr_type;

/* Exit codes -- can be ored */
#define NORMAL_TERMINATION      (0)
#define INVALID_OUTPUT          (1 << 0)
#define INVALID_INPUT           (1 << 1)
#define INVALID_ARGUMENT        (1 << 2)
#define FILEIO_ERROR            (1 << 3)
#define EXCEPTION_EXIT	        0xff

#if defined(__cplusplus)
#include <map>
#include <string>

namespace LQIO {
    namespace SCHEDULE {
	extern const char * ABORT;
	extern const char * BURST;
	extern const char * CFS;
	extern const char * CUSTOMER;
	extern const char * DELAY;
	extern const char * FIFO;
	extern const char * HOL;
	extern const char * INF_RETRY;
	extern const char * LIFO;
	extern const char * POLL;
	extern const char * PPR;
	extern const char * PS;
	extern const char * RAND;
	extern const char * RETRY;
	extern const char * RWLOCK;
	extern const char * RWLOCK;
	extern const char * SEMAPHORE;
	extern const char * TIMEOUT;
	extern const char * UNIFORM;

	struct label_t {
	    const std::string str;
	    const std::string XML;
	    const char flag;
	};

    }
}

extern const std::map<const scheduling_type,const LQIO::SCHEDULE::label_t> scheduling_label; /*  */

#endif
#endif	/* LQIO_INPUT_H */
