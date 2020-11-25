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
 * $Id: message.cc 14131 2020-11-25 02:17:53Z greg $
 */

#include <parasol.h>
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
