/* help.h	-- Greg Franks
 *
 * $Id: help.h 16945 2024-01-26 13:02:36Z greg $
 */

#ifndef _HELP_H
#define _HELP_H

void usage( const bool = true );
void invalid_option( char c, char * optarg );
void man();
#endif
