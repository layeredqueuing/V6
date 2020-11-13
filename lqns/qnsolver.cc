/*
 * $Id: qnsolver.cc 14000 2020-10-25 12:50:53Z greg $
 */

#include <lqio/pmif_document.h>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <getopt.h>
#include <libgen.h>
#include "dim.h"
#include "fpgoop.h"
#include "mva.h"
#include "vector.h"
#include "server.h"
#include "prob.h"


typedef enum {EXACT_MVA, LINEARIZER, LINEARIZER2, BARD_SCHWEITZER, EXPERIMENTAL } solver_type;

bool solve( LQIO::PMIF_Document& document, solver_type solver );
static void makeopts( const struct option * longopts, std::string& opts );
static void usage() ;

#if HAVE_GETOPT_LONG
const struct option longopts[] =
    /* name */ /* has arg */ /*flag */ /* val */
{
    { "bard-schweitzer", no_argument,       0, 'b' },
#if DEBUG_MVA
    { "debug-mva",	 no_argument,	    0, 'd' },
#endif
    { "exact-mva",       no_argument,       0, 'e' },
    { "fast-linearizer", no_argument,       0, 'f' },
    { "help",            no_argument,       0, 'h' },
    { "linearizer",      no_argument,       0, 'l' },
    { "silent",          no_argument,       0, 's' },
    { "verbose",         no_argument,       0, 'v' },
    { "experimental",	 no_argument,	    0, 'x' },
    { 0, 0, 0, 0 }
};

static std::string opts;
#else
#if DEBUG_MVA
static std::string opts = "bdefhlsvx";
#else
static std::string opts = "befhlsvx";
#endif
#endif

const char * opthelp[]  = {
    /* "bard-schweitzer", */    "Test using Bard-Schweitzer solver.",
#if DEBUG_MVA
    /* "debug",           */    "Enable debug code.",
#endif
    /* "exact-mva",       */    "Test using Exact MVA solver.",
    /* "fast-linearizer", */    "Test using the Fast Linearizer solver.",
    /* "help",            */    "Show this.",
    /* "linearizer",      */    "Test using Generic Linearizer.",
    /* "silent",          */    "",
    /* "verbose",         */    "",
    /* "experimental",	  */	"",
    0
};

static bool silencio_flag = false;			/* Don't print results if 1	*/
static bool verbose_flag = true;			/* Print results		*/

std::string program_name;

class ConstructChain {
public:
    ConstructChain( const ConstructChain& src ) : _N(src._N), _Z(src._Z), _priority(src._priority), _index(src._index), _k(src._k) {}
    ConstructChain( Population& N, VectorMath<double>& Z, VectorMath<unsigned>& priority, std::map<std::string,unsigned>& index ) : _N(N), _Z(Z), _priority(priority), _index(index), _k(0) {}

    void operator()( const std::pair<std::string,LQIO::PMIF_Document::ClosedChain>& pair )
	{
	    const LQIO::PMIF_Document::ClosedChain& chain = pair.second;
	    _k += 1;
	    _index[pair.first] = _k;
	    _N[_k] = chain.getCustomers();
	    _Z[_k] = chain.getThinkTime();
	    _priority[_k] = 0;
	}

private:
    Population& _N;
    VectorMath<double>& _Z;
    VectorMath<unsigned>& _priority;
    std::map<std::string,unsigned>& _index;
    unsigned int _k;
};

class ConstructStation {
private:
    class AddDemand {
    public:
	AddDemand( const AddDemand& src ) : _server(src._server), _index(src._index) {}
	AddDemand( Server * server, const std::map<std::string,unsigned>& index ) : _server(server), _index(index) {}

	void operator()( const pair<std::string,LQIO::PMIF_Document::Station::Demand>& pair ) 
	    {
		const unsigned int k = _index.at(pair.first);
		const LQIO::PMIF_Document::Station::Demand& demand = pair.second;
		_server->setService( k, demand.getServiceTime() );
		_server->setVisits( k, demand.getVisits() );
	    }

    private:
	Server * _server;
	const std::map<std::string,unsigned>& _index;	/* Maps class name to index	*/
    };

public:
    ConstructStation( const ConstructStation& src ) : _Q(src._Q), _index(src._index), _m(src._m) {}
    ConstructStation( Vector<Server*>& M, std::map<std::string,unsigned>& index ) : _Q(M), _index(index), _m(0) {}

    void operator()( const std::pair<std::string,LQIO::PMIF_Document::Station>& pair )
	{
	    Server * server = 0;
	    const LQIO::PMIF_Document::Station& station = pair.second;
	    const unsigned int K = _index.size();
	    _m += 1;

	    switch ( station.getScheduling() ) {
	    case SCHEDULE_FIFO:
		server = new FCFS_Server(K);
		break;
	    case SCHEDULE_DELAY:
		server = new Infinite_Server(K);
		break;
	    case SCHEDULE_PS:
		server = new PS_Server(K);
		break;
	    default:
		abort();
		server = 0;		/* Need to new it. */
	    }
	    _Q[_m] = server;

	    const std::map<std::string,LQIO::PMIF_Document::Station::Demand>& demands = station.getDemands();
	    for_each ( demands.begin(), demands.end(), AddDemand( server, _index ) );
	}

private:
    Vector<Server *>& _Q;				/* Array of stations		*/
    const std::map<std::string,unsigned>& _index;	/* Maps class name to index 	*/
    unsigned int _m;					/* Current station index 	*/
};

int main (int argc, char *argv[])
{
    solver_type solver = EXACT_MVA;
    program_name = basename( argv[0] );
    
    /* Process all command line arguments.  If none specified, then     */
#if HAVE_GETOPT_LONG
    makeopts( longopts, opts );
#endif
    
    for ( ;; ) {
#if HAVE_GETOPT_LONG
	const int c = getopt_long( argc, argv, opts.c_str(), longopts, NULL );
#else
	const int c = getopt( argc, argv, opts.c_str() );
#endif
	if ( c == EOF ) break;

	switch( c ) {
	case 'b':
	    solver = BARD_SCHWEITZER;
	    break;

#if DEBUG_MVA
	case 'd':
	    MVA::debug_D = true;
	    MVA::debug_L = true;
	    MVA::debug_P = true;
	    MVA::debug_U = true;
	    MVA::debug_W = true;
	    break;
#endif

	case 'e':
	    solver = EXACT_MVA;
	    break;
			
	case 'f':
	    solver = LINEARIZER2;
	    break;

	case 'h':
	    usage();
	    return 0;

	case 'l':
	    solver = LINEARIZER;
	    break;
			
	case 's':
	    silencio_flag = true;
	    break;

	case 'v':
	    verbose_flag = true;
	    break;
			
	case 'x':
	    solver = EXPERIMENTAL;
	    break;
	    
	default:
	    usage();
	    return 1;
	}
    }

    /* input is assumed to come in from stdin.                          */

    if ( optind == argc ) {
	LQIO::PMIF_Document document( std::string( "-" ) );
	solve( document, EXACT_MVA );
    } else {
        for ( ; optind < argc; ++optind ) {
	    LQIO::PMIF_Document document( argv[optind] );
	    solve( document, EXACT_MVA );
	}
    }
    return 0;
}


bool solve( LQIO::PMIF_Document& document, solver_type solver )
{
    if ( !document.parse() ) return false;
    MVA * model = 0;

    /* Construct chains */
    
    const std::map<std::string,LQIO::PMIF_Document::ClosedChain,LQIO::PMIF_Document::ClosedChain>& closed_chains = document.getClosedChains();
    const std::map<std::string,LQIO::PMIF_Document::OpenChain,LQIO::PMIF_Document::OpenChain>& open_chains = document.getOpenChains();
    const unsigned int K = closed_chains.size();

    Population N(K);
    VectorMath<double> Z(K);
    VectorMath<unsigned> priority(K);
    std::map<std::string,unsigned> index;		/* Maps string to integer for chains. */
    
    for_each( closed_chains.begin(), closed_chains.end(), ConstructChain( N, Z, priority, index ) );

    /* Construct stations */
    
    const std::map<std::string,LQIO::PMIF_Document::Station,LQIO::PMIF_Document::Station>& stations = document.getStations();
    const unsigned M = stations.size();
    
    Vector<Server *> Q(M);
    
    for_each( stations.begin(), stations.end(), ConstructStation( Q, index ) );

    switch ( solver ) {
    case EXACT_MVA:
	model = new ExactMVA( Q, N, Z, priority );
	break;
    case LINEARIZER:
	model = new Linearizer( Q, N, Z, priority );
	break;
    case LINEARIZER2:
	model = new Linearizer2( Q, N, Z, priority );
	break;
    case BARD_SCHWEITZER:
	model = new Schweitzer( Q, N, Z, priority );
	break;
    case EXPERIMENTAL:
	model = new Linearizer3( Q, N, Z, priority );
	break;
    }

    bool ok = true;
    try {
	model->solve();
    }
    catch ( const std::runtime_error& error ) {
	cerr << "runtime error - " << error.what() << endl;
	ok = false;
    }
    catch ( const logic_error& error ) {
	cerr << "logic error - " << error.what() << endl;
	ok = false;
    }
    catch ( const floating_point_error& error ) {
	cerr << "floating point error - " << error.what() << endl;
	ok = false;
    }
    catch ( const not_implemented& error ) {
	cerr << error.what() << endl;
	ok = false;
    }
    
    if ( verbose_flag ) {
	model->print( cerr );
    }

    delete model;
    return ok;
}

static void
makeopts( const struct option * longopts, std::string& opts )
{
    for ( int i = 0; longopts[i].name != 0; ++i ) {
	if ( isgraph( longopts[i].val ) ) {
	    opts += longopts[i].val;
	    if ( longopts[i].has_arg == required_argument ) {
		opts += ':';
	    }
	}
    }
}



static void
usage() 
{
    cerr << "Usage: " << program_name;

#if HAVE_GETOPT_LONG
    cerr << " [option]" << endl << endl;
    cerr << "Options" << endl;
    const char ** p = opthelp;
    for ( const struct option *o = longopts; (o->name || o->val) && *p; ++o, ++p ) {
	string s;
	if ( o->name ) {
	    s = "--";
	    s += o->name;
	    switch ( o->val ) {
	    }
	} else {
	    s = " ";
	}
	if ( isascii(o->val) && isgraph(o->val) ) {
	    cerr << " -" << static_cast<char>(o->val) << ", ";
	} else {
	    cerr << "     ";
	}
	cerr.setf( ios::left, ios::adjustfield );
	cerr << setw(24) << s << *p << endl;
    }
#else
    const char * s;
    cerr << " [-";
    for ( s = opts.c_str(); *s; ++s ) {
	if ( *(s+1) == ':' ) {
	    ++s;
	} else {
	    cerr.put( *s );
	}
    }
    cerr << ']';

    for ( s = opts.c_str(); *s; ++s ) {
	if ( *(s+1) == ':' ) {
	    cerr << " [-" << *s;
	    switch ( *s ) {
	    }
	    cerr << ']';
	    ++s;
	}
    }
#endif
    cerr << endl;
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

#if 0
template class Vector<Probability>;
template class VectorMath<Probability>;

template std::ostream& operator<<( std::ostream& output, const Vector<unsigned int>& self );
template std::ostream& operator<<( std::ostream& output, const VectorMath<unsigned int>& self );
template std::ostream& operator<<( std::ostream& output, const VectorMath<double>& self );
#endif
#if 0
template ostream& operator<< ( ostream& output, const VectorMath<Probability>& self );
#endif
