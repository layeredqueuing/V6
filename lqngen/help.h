/* help.h	-- Greg Franks
 *
 * $Id: help.h 16945 2024-01-26 13:02:36Z greg $
 */

#ifndef _HELP_H
#define _HELP_H

void invalid_option( int );
void invalid_argument( int, const std::string&, const std::string& s="" );
void usage();
void man();
void help();

#endif
