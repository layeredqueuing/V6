/* help.h	-- Greg Franks
 *
 * $Id: help.h 15171 2021-12-08 03:02:09Z greg $
 */

#ifndef _HELP_H
#define _HELP_H

void usage( const bool = true );
void invalid_option( char c, char * optarg );
void man();
#endif
