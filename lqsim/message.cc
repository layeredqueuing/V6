/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* May 1996								*/
/* June 2009								*/
/************************************************************************/

/*
 * Input output processing.
 *
 * $Id: message.cc 15459 2022-03-09 21:53:20Z greg $
 */

#include "lqsim.h"
#include "entry.h"
#include "message.h"


/*
 * This function tags a message with the appropriate fields.
 */

Message * 
Message::init( const Entry * ep, tar_t * src )
{
    time_stamp   = ps_now; /* Tag send time.	*/
    client       = ep;
    reply_port   = -1;
    intermediate = nullptr;
    target       = src;

    return this;
}
