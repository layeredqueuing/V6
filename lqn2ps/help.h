/* help.h	-- Greg Franks
 *
 * $Id: help.h 14000 2020-10-25 12:50:53Z greg $
 */

#ifndef _HELP_H
#define _HELP_H

void usage( const bool = true );
void invalid_option( char c, char * optarg );
void man();

#endif
