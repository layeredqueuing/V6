/*  -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/lqns/unit-test/popitertest.cc $
 *
 * Population iterator testor.  See usage().
 * ------------------------------------------------------------------------
 *
 * $Id: popitertest.cc 13409 2018-10-19 20:01:31Z greg $
 */

#include "testmva.h"
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include "server.h"
#include "multserv.h"
#include "pop.h"
#include "vector.h"

char * myName;

static void usage ()
{
	cerr << myName << " [-a [-m<servers>]] [-b [-m<servers>] [-j<class>]] n1 n2 n3 ... " << endl;
	exit( 1 );
}

int main ( int argc, char * argv[] )
{
	int c;
	unsigned m = 1;
	unsigned j = 1;
	Population::Iterator * next;
	Server * aServer = 0;
	enum { A_POP, B_POP, GENERAL_POP } type = GENERAL_POP;

	myName = argv[0];
	
	while (( c = getopt( argc, argv, "abm:j:" )) != EOF) {
		switch ( c ) {
		case 'a':
			type = A_POP;
			break;

		case 'b':
			type = B_POP;
			break;

		case 'j':
			j = atoi( optarg );
			if ( !j ) {
				cerr << "Bad value for j:" << optarg << endl;
				usage();
			}
			break;

		case'm':
			m = atoi( optarg );
			if ( !m ) {
				cerr << "Bad value for m:" << optarg << endl;
				usage();
			}
			break;

		default:
			cerr << "Unkown option." << endl;
			usage();
		}
	}

	const unsigned k = argc - optind;

	if ( k == 0 ) {
		cerr << "Arg count." << endl;
		usage();
	}

	Population NCust(k);		// Limit.	
	Population N(k);			// Current.

	for ( unsigned i = optind; i < argc; ++i ) {
		NCust[i-optind+1] = atoi( argv[i] );
	}
	cout << "Limit: " << NCust << endl;

	switch ( type ) {
	case A_POP:
	    aServer = new Conway_Multi_Server( m );
	    next = new A_Iterator( *aServer, j, NCust, 0 );
	    break;

	case B_POP:
	    aServer = new Conway_Multi_Server( m );
	    next = new B_Iterator( *aServer, NCust, 0 );
	    break;

	default:
	    next = new Population::Iterator( NCust );
	    break;
	}

	
	for ( unsigned count = 1; (* next)( N ); ++count ) {
		cout << setw(3) << count << ": " << N << endl;
	}
	if ( aServer ) {
	    delete aServer;
	}
	return 0;
}
#include <vector.cc>

template class Vector<double>;
template class Vector<unsigned int>;
template class Vector<unsigned long>;
template class Vector<Server *>;
template class VectorMath<unsigned int>;
template class VectorMath<double>;
template class Vector<Vector<unsigned> >;
template class Vector<VectorMath<double> >;
template class Vector<VectorMath<unsigned> >;

