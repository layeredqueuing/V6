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
 * $Id: message.cc 13324 2018-06-15 15:43:32Z greg $
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <parasol.h>
#include <assert.h>
#include <lqio/input.h>
#include <lqio/error.h>
#include "lqsim.h"
#include "entry.h"
#include "message.h"
#include "task.h"
#include "instance.h"


/*
 * This function tags a message with the appropriate fields.
 */

Message * 
Message::init( const Entry * ep, tar_t * src )
{
    time_stamp   = ps_now; /* Tag send time.	*/
    client       = ep;
    reply_port   = -1;
    intermediate = NULL;
    target       = src;

    return this;
}
