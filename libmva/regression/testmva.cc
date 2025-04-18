/*  -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk-V6/libmva/regression/testmva.cc $
 *
 * Example from:
 *     author =   "Chandy, K. Mani and Neuse, Doug",
 *     title =    "Linearizer: A Heuristic Algorithm for Queueing
 *                 Network Models of Computing Systems",
 *     journal =  cacm,
 *     year =     1982,
 *     volume =   25,
 *     number =   2,
 *     pages =    "126--134",
 *     month =    feb
 *
 * ------------------------------------------------------------------------
 * $Id: testmva.cc 17200 2024-05-05 23:54:01Z greg $
 * ------------------------------------------------------------------------
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include "testmva.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cmath>
#include <getopt.h>
#include "prob.h"
#include "fpgoop.h"
#include "mvaexception.h"

static bool doIt( const solverId, Vector<Server *>& Q, const Population & NCust, const VectorMath<double>& thinkTime, const VectorMath<unsigned>& priority, const unsigned special );
static bool run( const unsigned solver_set, const unsigned special );
static void usage();
char * progname;

#if HAVE_GETOPT_LONG
const struct option longopts[] =
    /* name */ /* has arg */ /*flag */ /* val */
{
    { "all",             no_argument,       0, 'a' },
    { "bard-schweitzer", no_argument,       0, 'b' },
    { "debug-mva",	 no_argument,	    0, 'd' },
    { "exact-mva",       no_argument,       0, 'e' },
    { "fast-linearizer", no_argument,       0, 'f' },
    { "help",            no_argument,       0, 'h' },
    { "iterations",      required_argument, 0, 'i' },
    { "linearizer",      no_argument,       0, 'l' },
    { "no-check",        no_argument,       0, 'n' },
    { "print",           no_argument,       0, 'p' },
    { "silent",          no_argument,       0, 's' },
    { "test-number",     required_argument, 0, 't' },
    { "verbose",         no_argument,       0, 'v' },
    { 0, 0, 0, 0 }
};
#else
#warning No getopt_long
#endif
const char opts[]	= "abdefhi:lnpst:v";
const char * opthelp[]  = {
    /* "all",             */    "Test using all MVA solvers.",
    /* "bard-schweitzer", */    "Test using Bard-Schweitzer solver.",
    /* "debug",           */    "Enable debug code.",
    /* "exact-mva",       */    "Test using Exact MVA solver.",
    /* "fast-linearizer", */    "Test using the Fast Linearizer solver.",
    /* "help",            */    "Show this.",
    /* "iterations",      */    "Run the test ARG times.",
    /* "linearizer",      */    "Test using Generic Linearizer.",
    /* "no-check",        */    "Do not check solution against \"correct\" values.",
    /* "print"		  */	"Print out station info.",
    /* "silent",          */    "",
    /* "test",            */    "Select test ARG.  Arg is an integer.",
    /* "verbose",         */    "",
    0
};

/* -- */

static int silencio_flag = 0;			/* Don't print results if 1	*/
static int verbose_flag = 0;			/* Print iteration count if 1	*/
static int print_flag = 0;			/* Print station info.		*/
static int nocheck_flag = 0;			/* Don't check if 1.		*/


/*
 * Main line.
 */

int main (int argc, char *argv[])
{
    unsigned solver_set = 0;
    bool ok;
    unsigned long count = 1;
    unsigned special = 0;
	
    set_fp_abort();

    progname = argv[0];

    for ( ;; ) {
#if HAVE_GETOPT_LONG
	const int c = getopt_long( argc, argv, opts, longopts, NULL );
#else
	const int c = getopt( argc, argv, opts );
#endif
	if ( c == EOF ) break;

	switch( c ) {
	case 'a':
	    solver_set = EXACT_SOLVER_BIT|BARD_SCHWEITZER_SOLVER_BIT|LINEARIZER_SOLVER_BIT;
	    break;
			
	case 'b':
	    solver_set |= BARD_SCHWEITZER_SOLVER_BIT;
	    break;

	case 'd':
	    MVA::debug_D = true;
	    MVA::debug_L = true;
	    MVA::debug_P = true;
	    MVA::debug_U = true;
	    MVA::debug_W = true;
	    break;

	case 'e':
	    solver_set |= EXACT_SOLVER_BIT;
	    break;
			
	case 'f':
	    solver_set |= LINEARIZER2_SOLVER_BIT;
	    break;

	case 'h':
	    usage();
	    exit(0);

	case 'i':
	    if ( sscanf( optarg, "%lu", &count ) != 1 ) {
		std::cerr << "Bogus loop count: " << optarg << std::endl;
		exit( 1 );
	    }
	    break;
			
	case 'l':
	    solver_set |= LINEARIZER_SOLVER_BIT;
	    break;
			
	case 'n':
	    nocheck_flag = 1;
	    break;
			
	case 'p':
	    print_flag = 1;
	    break;
			    
	case 's':
	    silencio_flag = 1;
	    break;

	case 't':
	    if ( sscanf( optarg, "%u", &special ) != 1 ) {
		std::cerr << "Bogus \"special\": " << optarg << std::endl;
		exit( 1 );
	    }
	    break;
			
	case 'v':
	    verbose_flag = 1;
	    break;
			
	default:
	    usage();
	}
    }

    if ( !solver_set ) {
	solver_set = LINEARIZER_SOLVER_BIT;
    }




    if ( optind != argc ) {
	std::cerr << "Arg count." << std::endl;
    }

    ok = true;
    for ( unsigned long i = 0; i < count && ok; ++i ) {
	ok = run( solver_set, special );
    }
    if ( !ok ) { 
	std::cerr << argv[0] << " fails." << std::endl;
	return 1;
    } else {
	return 0;
    }
}


/*
 * run one iteration.
 */

static bool 
run( const unsigned solver_set, const unsigned special )
{
    Population N;
    VectorMath<double> Z;
    VectorMath<unsigned> priority;
    Vector<Server *> Q;
    bool ok = true;

    try {
	test( N, Q, Z, priority, special );
    }
    catch ( std::runtime_error& error ) {
	std::cerr << "runtime error - " << error.what() << std::endl;
	return 0;
    }
	
    if ( print_flag ) {
	for ( unsigned j = 1; j <= Q.size(); ++j ) {
	    std::cout << *Q[j] << std::endl;
	}
    }

    for ( unsigned i = 0; i <= BARD_SCHWEITZER_SOLVER; ++i ) {
	if ( (1 << i) & solver_set ) {
	    ok = doIt( (solverId)i, Q, N, Z, priority, special ) && ok;
	}
    }

    for ( unsigned j = 1; j <= Q.size(); ++j ) {
	delete Q[j];
    }

    return ok;
}


/*
 * run solver.
 */

static const char * names[] =
{
    "Exact MVA",
    "Linearizer",
    "Fast Linearizer",
    "Bard-Schweitzer",
    "Experimental",
};

static bool
doIt( solverId solver, Vector<Server *>& Q, const Population & N, const VectorMath<double> &Z, 
      const VectorMath<unsigned>& priority, const unsigned special )
{
    bool ok = true;
    MVA * model;

    switch ( solver ) {
    case EXACT_SOLVER:
	model = new ExactMVA( Q, N, Z, priority );
	break;
    case LINEARIZER_SOLVER:
	model = new Linearizer( Q, N, Z, priority );
	break;
    case LINEARIZER2_SOLVER:
	model = new Linearizer2( Q, N, Z, priority );
	break;
    case BARD_SCHWEITZER_SOLVER:
	model = new Schweitzer( Q, N, Z, priority );
	break;
    }

    try {
	model->solve();
    }
    catch ( LibMVA::not_implemented& error ) {
	std::cerr << error.what() << std::endl;
	ok = false;
    }
    catch ( floating_point_error& error ) {
	std::cerr << "floating point error - " << error.what() << std::endl;
	ok = false;
    }
    catch ( std::runtime_error& error ) {
	std::cerr << "runtime error - " << error.what() << std::endl;
	ok = false;
    }
    catch ( std::logic_error& error ) {
	std::cerr << "logic error - " << error.what() << std::endl;
	ok = false;
    }
    if ( ok ) {
	if ( !silencio_flag ) {
	    std::cout << names[(int)solver] << " solver." << std::endl;
	    std::cout.precision(4);
	    std::cout << *model;
	    special_check( std::cout, *model, special );
	}
	if ( verbose_flag ) {
	    std::cout << "Number of iterations of core step: " << model->iterations() << std::endl;
	}

	if ( !nocheck_flag ) {
	    ok = check( (int)solver, *model, special );
	}
    }
	
    delete model;
    return ok;
}

static void
usage() 
{
    std::cerr << "Usage: " << progname;

#if HAVE_GETOPT_LONG
    std::cerr << " [option]" << std::endl << std::endl;
    std::cerr << "Options" << std::endl;
    const char ** p = opthelp;
    for ( const struct option *o = longopts; (o->name || o->val) && *p; ++o, ++p ) {
	std::string s;
	if ( o->name ) {
	    s = "--";
	    s += o->name;
	    switch ( o->val ) {
	    }
	} else {
	    s = " ";
	}
	if ( isascii(o->val) && isgraph(o->val) ) {
	    std::cerr << " -" << static_cast<char>(o->val) << ", ";
	} else {
	    std::cerr << "     ";
	}
	std::cerr.setf( std::ios::left, std::ios::adjustfield );
	std::cerr << std::setw(24) << s << *p << std::endl;
    }
#else
    const char * s;
    std::cerr << " [-";
    for ( s = opts; *s; ++s ) {
	if ( *(s+1) == ':' ) {
	    ++s;
	} else {
	    std::cerr.put( *s );
	}
    }
    std::cerr << ']';

    for ( s = opts; *s; ++s ) {
	if ( *(s+1) == ':' ) {
	    std::cerr << " [-" << *s;
	    switch ( *s ) {
	    }
	    std::cerr << ']';
	    ++s;
	}
    }
#endif
    std::cerr << std::endl;
}

