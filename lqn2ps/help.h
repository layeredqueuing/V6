/* help.h	-- Greg Franks
 *
 * $Id: help.h 14882 2021-07-07 11:09:54Z greg $
 */

#ifndef _HELP_H
#define _HELP_H

void usage( const bool = true );
void invalid_option( char c, char * optarg );
void man();

#endif
