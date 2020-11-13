/* help.h	-- Greg Franks
 *
 * $Id: help.h 14000 2020-10-25 12:50:53Z greg $
 */

#ifndef _HELP_H
#define _HELP_H

void invalid_option( int );
void invalid_argument( int, const std::string&, const std::string& s="" );
void usage();
void man();
void help();

#endif
