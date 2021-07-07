/* help.h	-- Greg Franks
 *
 * $Id: help.h 14882 2021-07-07 11:09:54Z greg $
 */

#ifndef _HELP_H
#define _HELP_H

void invalid_option( int );
void invalid_argument( int, const std::string&, const std::string& s="" );
void usage();
void man();
void help();

#endif
