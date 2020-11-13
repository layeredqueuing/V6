/*  -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/lqns/unit-test/poptest.cc $
 *
 * Population test ( for X[N] ).
 * ------------------------------------------------------------------------
 *
 * $Id: poptest.cc 13403 2018-10-19 14:40:04Z greg $
 */

#include "testmva.h"
#include <vector>
#include <set>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include "pop.h"
#include "vector.h"

char * myName;

static void usage ()
{
    cerr << myName << " [-[els]] n1 n2 n3 ... " << endl;
    exit( 1 );
}

int main ( int argc, char * argv[] )
{
    typedef enum { EXACT, LINEARIZER, SCHWEITZER } test_case_t;
    std::set<test_case_t> test_case;

    myName = argv[0];
	
    for ( ;; ) {
	const int c = getopt( argc, argv, "els" );
	if ( c == EOF) break;
	
	switch ( c ) {
	case 'e':
	    test_case.insert( EXACT );
	    break;

	case 'l':
	    test_case.insert( LINEARIZER );
	    break;

	case 's':
	    test_case.insert( SCHWEITZER );
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

    Population n(k);		// Limit.	
    Population N(k);			// Current.

    for ( unsigned i = optind; i < argc; ++i ) {
	N[i-optind+1] = atoi( argv[i] );
    }
    cout << "Limit: " << N << endl;

    /* Iterate over all populations. */

    return 0;
}
#include <vector.cc>

template class Vector<int>;
template class Vector<unsigned int>;
template class Vector<size_t>;
