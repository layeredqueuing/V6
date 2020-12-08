/* -*- c++ -*-
 * submodel.C	-- Greg Franks Wed Dec 11 1996
 * $Id: submodel.cc 14175 2020-12-07 17:04:30Z greg $
 *
 * MVA submodel creation and solution.  This class is the interface
 * between the input model consisting of processors, tasks, and entries,
 * and the MVA model consisting of chains and stations.
 *
 * Call build() to construct the model, then solve() to solve it.
 * The submodel number (myNumber) MUST be set by layerize.
 *
 * Prior to MVA submodel solution, stations are initilized by calling
 * Entry::elapsedTime() and Call::setVisits().  After MVA solution
 * the input model is updated with Call:saveWait() and Entry::updateWait().
 * Entry::updateWait() returns the difference in waiting times between
 * iterations of this submodel which is used for the stopping criteria.
 *
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 *
 * Replication from:
 *     author =    "Pan, Amy M.",
 *     title =     "Solving Stochastic Rendezvous Networks of Large
 *                  Client-Server Systems with Symmetric Replication",
 *     school =    sce,
 *     year =      1996,
 *     month =     sep,
 *     note =      "OCIEE-96-06",
 *
 * Overlap factor from:
 *     author =    "Mak, Victor W. and Lundstrom, Stephen F. ",
 *     title =     "Predicting Performance of Parallel Computations",
 *     journal =   ieeepds,
 *     callno =    "QA76.5.A1I35",
 *     year =      1990,
 *     volume =    1,
 *     number =    3,
 *     pages =     "257--270",
 *     month =     jul,
 *
 * ----------------------------------------------------------------------
 */


#include "dim.h"
#include <algorithm>
#include <string>
#include <cstdlib>
#include <string.h>
#include <cmath>
#include "activity.h"
#include "call.h"
#include "entity.h"
#include "entry.h"
#include "errmsg.h"
#include "fpgoop.h"
#include "generate.h"
#include "group.h"
#include "interlock.h"
#include "lqns.h"
#include "model.h"
#include "mva.h"
#include "open.h"
#include "option.h"
#include "overtake.h"
#include "pragma.h"
#include "prob.h"
#include "processor.h"
#include "report.h"
#include "server.h"
#include "submodel.h"
#include "task.h"
#include "vector.h"

#define	QL_INTERLOCK

/*----------------------------------------------------------------------*/
/*                        Merged Partition Model                        */
/*----------------------------------------------------------------------*/


/*
 * Set my submodel number to n.  Reset submodel numbers of all servers.
 */

Submodel&
Submodel::number( const unsigned n )
{
    _submodel_number = n;
    for_each( _servers.begin(), _servers.end(), Exec1<Entity,const unsigned>( &Entity::setSubmodel, n ) );
    return *this;
}


/*
 * Now create client tables which are simply all callers to
 * the servers at each level.  Note that _clients is an intersection
 * of all clients to all servers.
 */

Submodel&
Submodel::initServers( const Model& )
{
    _clients = std::accumulate( _servers.begin(), _servers.end(), _clients, Entity::add_clients );
    return *this;
}


/*
 * Handy debug function
 */

void
Submodel::debug_stop( const unsigned long iterations, const double delta ) const
{
    if ( !std::cin.eof() && iterations >= flags.single_step ) {
	std::cerr << "**** Submodel " << number() << " **** Delta = " << delta << ", Continue [yna]? ";
	std::cerr.flush();

	char c;

	std::cin >> c;

	switch ( c ) {
	case '\n':
	case '\004':
	    break;

	case 'n':
	case 'N':
	    throw std::runtime_error( "Submodel::debug_stop" );
	    break;

	case 'a':
	    LQIO::internal_error( __FILE__, __LINE__, "debug abort" );
	    break;

	default:
	    std::cin.ignore( 100, '\n' );
	    break;
	}
    }
}


std::ostream&
Submodel::submodel_header_str( std::ostream& output, const Submodel& aSubmodel, const unsigned long iterations )
{
    output << "========== Iteration " << iterations << ", "
	   << aSubmodel.submodelType() << " " << aSubmodel.number() << ": "
	   << aSubmodel._clients.size() << " client" << (aSubmodel._clients.size() != 1 ? "s, " : ", ")
	   << aSubmodel._servers.size() << " server" << (aSubmodel._servers.size() != 1 ? "s."  : ".")
	   << "==========";
    return output;
}

/*----------------------------------------------------------------------*/
/*			     MVA Submodel				*/
/*----------------------------------------------------------------------*/

/*
 * Create a new MVA submodel.
 */

MVASubmodel::MVASubmodel( const unsigned n )
    : Submodel(n),
      _hasReplication(false),
      _hasThreads(false),
      _hasSynchs(false),
      closedStation(),
      openStation(),
      closedModel(nullptr),
      openModel(nullptr),
      _overlapFactor()
{
}


/*
 * Free allocated store.  Do not delete stations -- stations are deleted by
 * tasks/processors.
 */

MVASubmodel::~MVASubmodel()
{
    if ( openModel ) {
	delete openModel;
    }
    if ( closedModel ) {
	delete closedModel;
    }
    closedStation.clear();
    openStation.clear();
    if ( _overlapFactor ) {
	delete [] _overlapFactor;
    }
}


/*----------------------------------------------------------------------*/
/*                       Initialize the model.                          */
/*----------------------------------------------------------------------*/

/*
 * Initialize server's waiting times and populations.
 */

MVASubmodel&
MVASubmodel::initServers( const Model& model )
{
    for_each ( _servers.begin(), _servers.end(), Exec1<Entity,const Vector<Submodel *>&>( &Entity::initServer, model.getSubmodels() ) );
    Submodel::initServers( model );
    return *this;
}



/*
 * Initialize server's waiting times and populations.
 */

MVASubmodel&
MVASubmodel::reinitServers( const Model& model )
{
    for_each ( _servers.begin(), _servers.end(), Exec1<Entity,const Vector<Submodel *>&>( &Entity::reinitServer, model.getSubmodels() ) );
    return *this;
}



/*
 * Go through all servers and set up path tables.
 */

MVASubmodel&
MVASubmodel::initInterlock()
{
    for_each ( _servers.begin(), _servers.end(), Exec<Entity>( &Entity::initInterlock ) );
    return *this;
}


/*
 * Build a Layer.
 */

MVASubmodel&
MVASubmodel::build()
{
    /* BUG 144 */
    if ( _servers.size() == 0 ) {
	if ( !Pragma::allowCycles()  ) {
	    LQIO::solution_error( ADV_EMPTY_SUBMODEL, number() );
	}
	return *this;
    }

    const unsigned n_stations  = _clients.size() + _servers.size();
    unsigned closedStnNo       = 0;
    unsigned openStnNo	       = 0;


    /* ------------------- Count the stations. -------------------- */

    _hasReplication = std::any_of( _clients.begin(), _clients.end(), Predicate<Task>( &Task::isReplicated ) )
	|| std::any_of( _servers.begin(), _servers.end(), Predicate<Entity>( &Entity::isReplicated ) );
    _hasThreads = std::any_of( _clients.begin(), _clients.end(), Predicate<Task>( &Task::hasThreads ) );
    _hasSynchs = std::any_of( _servers.begin(), _servers.end(), Predicate<Entity>( &Entity::hasSynchs ) );

    closedStnNo = _clients.size() + std::count_if( _servers.begin(), _servers.end(), Predicate<Entity>( &Entity::isInClosedModel ) );
    openStnNo   = std::count_if( _servers.begin(), _servers.end(), Predicate<Entity>( &Entity::isInOpenModel ) );

    closedStation.resize(closedStnNo);
    openStation.resize(openStnNo);

    /* --------------------- Create Chains.  ---------------------- */

    setNChains( makeChains() );
    if ( nChains() > MAX_CLASSES ) {
	LQIO::solution_error( ADV_MANY_CLASSES, nChains(), number() );
    }

    /* ------------------- Create the clients. -------------------- */

    closedStnNo	= 0;
    for ( std::set<Task *>::const_iterator client = _clients.begin(); client != _clients.end(); ++client ) {
	closedStnNo += 1;
	closedStation[closedStnNo] = (*client)->makeClient( nChains(), number() );
    }

    /* ----------------- Create servers for model. ---------------- */

    openStnNo   = 0;
    for ( std::set<Entity *>::const_iterator server = _servers.begin(); server != _servers.end(); ++server ) {
	Server * aStation;
	if ( (*server)->nEntries() == 0 ) continue;	/* Null server. */
	aStation = (*server)->makeServer( nChains() );
	if ( (*server)->isInClosedModel() ) {
	    closedStnNo += 1;
	    closedStation[closedStnNo] = aStation;
	}
	if ( (*server)->isInOpenModel() ) {
	    openStnNo += 1;
	    openStation[openStnNo] = aStation;
	}
    }

    /* ------- Create overlap probabilities and durations. -------- */

    if ( ( hasThreads() || hasSynchs() ) && !Pragma::threads(Pragma::Threads::NONE) ) {
	_overlapFactor = new VectorMath<double> [nChains()+1];
	for ( unsigned i = 1; i <= nChains(); ++i ) {
	    _overlapFactor[i].resize( nChains(), 1.0 );
	}
    }

    /* ---------------------- Generate Model. --------------------- */

    assert ( closedStnNo <= n_stations && openStnNo <= n_stations );

    if ( n_openStns() > 0 && !flags.no_execute ) {
	openModel = new Open( openStation );
    }

    if ( nChains() > 0 && n_closedStns() > 0 ) {
	switch ( Pragma::mva() ) {
	case Pragma::MVA::EXACT:
	    closedModel = new ExactMVA(          closedStation, _customers, _thinkTime, _priority, _overlapFactor );
	    break;
	case Pragma::MVA::SCHWEITZER:
	    closedModel = new Schweitzer(        closedStation, _customers, _thinkTime, _priority, _overlapFactor );
	    break;
	case Pragma::MVA::LINEARIZER:
	    closedModel = new Linearizer(        closedStation, _customers, _thinkTime, _priority, _overlapFactor );
	    break;
	case Pragma::MVA::FAST:
	    closedModel = new Linearizer2(       closedStation, _customers, _thinkTime, _priority, _overlapFactor );
	    break;
	case Pragma::MVA::ONESTEP:
	    closedModel = new OneStepMVA(        closedStation, _customers, _thinkTime, _priority, _overlapFactor );
	    break;
	case Pragma::MVA::ONESTEP_LINEARIZER:
	    closedModel = new OneStepLinearizer( closedStation, _customers, _thinkTime, _priority, _overlapFactor );
	    break;
	}
    }

    std::for_each( _clients.begin(), _clients.end(), ConstExec1<Task,const MVASubmodel&>( &Task::setChain, *this ) );
    if ( hasReplication() ) {
	unsigned not_used = 0;
	std::for_each( _clients.begin(), _clients.end(), ExecSum2<Task,double,const Submodel&,unsigned&>( &Task::updateWaitReplication, *this, not_used ) );
    }
    std::for_each( _clients.begin(), _clients.end(), ConstExec1<Task,const MVASubmodel&>(&Task::setMaxCustomers,*this) );
    std::for_each( _servers.begin(), _servers.end(), ConstExec1<Entity,const MVASubmodel&>(&Entity::setMaxCustomers,*this) );
    if ( Pragma::interlock() ) {
	std::for_each( _servers.begin(), _servers.end(), ConstExec1<Entity,const MVASubmodel&>(&Entity::setInterlock,*this) );
	std::for_each( _servers.begin(), _servers.end(), ConstExec1<Entity,const MVASubmodel&>(&Entity::setInterlockRelation,*this) );
    }

    if (flags.trace_customers ) {
	printMaxCustomers( std::cout );
    }
    return *this;
}



/*
 * Rebuild stations and customers as needed.
 */

MVASubmodel&
MVASubmodel::rebuild()
{
    unsigned k = 0;			/* Chain number.		*/

    /* ----- Set think times, customers and chains for this pass. ----- */

    for ( std::set<Task *>::const_iterator client = _clients.begin(); client != _clients.end(); ++client ) {
	const std::set<Entity *> clientsServers = (*client)->getServers( _servers );	/* Get all servers for this client	*/
	const unsigned threads = (*client)->nThreads();

	if ( (*client)->replicas() <= 1 ) {

	    /* ---------------- Simple case --------------- */

	    for ( unsigned i = 1; i <= threads; ++i ) {
		k += 1;				// Add one chain.

		if ( std::isfinite( (*client)->population() ) ) {
		    _customers[k] = static_cast<unsigned>((*client)->population());
		} else {
		    _customers[k] = 0;
		}
		_priority[k]  = (*client)->priority();
	    }

	} else {
	    //REPL changes
	    /* --------------- Complex case --------------- */

	    //!!! If chains are extended to entries to handle
	    //!!! fanins, modify delta_chains and Entity::fanIn()
	    //!!! for the entry-to-entry case.

	    /*
	     * Do all the chains for to a server at once.  This makes
	     * Task::threadIndex() simpler.
	     */

	    for ( std::set<Entity *>::const_iterator server = clientsServers.begin(); server != clientsServers.end(); ++server ) {
		for ( unsigned i = 1; i <= threads; ++i ) {
		    k += 1;

		    if ( std::isfinite( (*client)->population() ) ) {
			_customers[k] = static_cast<unsigned>((*client)->population()) * (*server)->fanIn(*client);
		    } else {
			_customers[k] = 0;
		    }
		}
	    }
	}
	if ( Pragma::interlock() ) {
	    (*client)->setInterlockedFlow( *this );
	}
    }

    /* ------------------ Recreate servers for model. -----------------	*/

    for ( std::set<Entity *>::const_iterator server = _servers.begin(); server != _servers.end(); ++server ) {
	if ( (*server)->nEntries() == 0 ) continue;	/* Null server. */
	Server * oldStation = (*server)->serverStation();		/* Get old station		*/
	const unsigned closedIndex = oldStation->closedIndex;		/* Copy over indicies.		*/
	const unsigned openIndex   = oldStation->openIndex;

 	Server * newStation = (*server)->makeServer( nChains() );	/* Returns NULL on no change	*/
	if ( !newStation )  continue;			/* Out with the old...		*/

	newStation->setMarginalProbabilitiesSize( _customers );

	if ( closedIndex ) {
	    newStation->closedIndex = closedIndex;
	    closedStation[closedIndex] = newStation;	/* ... and in with the new...	*/
	}
	if ( openIndex ) {
	    newStation->openIndex = openIndex;
	    openStation[openIndex] = newStation;	/* ... and in with the new...	*/
	}
    }

    std::for_each( _clients.begin(), _clients.end(), ConstExec1<Task,const MVASubmodel&>(&Task::setMaxCustomers, *this) );
    std::for_each( _servers.begin(), _servers.end(), ConstExec1<Entity,const MVASubmodel&>(&Entity::setMaxCustomers,*this) );

    if ( Pragma::interlock() ) {
	std::for_each( _servers.begin(), _servers.end(), ConstExec1<Entity,const MVASubmodel&>(&Entity::setInterlockRelation,*this) );
    }

    if (flags.trace_customers ) {
	printMaxCustomers( std::cout );
    }

    if ( closedModel ) {
	closedModel->reset();
    }
    return *this;
}


/*
 * Determine the number of chains, their populations, and their think times.
 * Customers and thinkTime are dimensioned by CHAIN (k).  The array `chains'
 * is indexed by client (i) -- it is the link from customer to chain.
 */

unsigned
MVASubmodel::makeChains()
{
    unsigned k = 0;			/* Chain number.		*/

    /* --- Set think times, customers and chains for this pass. --- */

    for ( std::set<Task *>::const_iterator client = _clients.begin(); client != _clients.end(); ++client ) {
	const std::set<Entity *> clientsServers = (*client)->getServers( _servers );	/* Get all servers for this client	*/
	const unsigned threads = (*client)->nThreads();

	if ( (*client)->replicas() <= 1 ) {

	    /* ---------------- Simple case --------------- */

	    size_t new_size = _customers.size() + threads;
	    _customers.resize( new_size ); /* N.B. -- Vector class.  Must*/
	    _thinkTime.resize( new_size ); /* grow() explicitly.	*/
	    _priority.resize( new_size );

	    for ( unsigned i = 1; i <= threads; ++i ) {
		k += 1;				// Add one chain.

		(*client)->addClientChain( number(), k );	// Set my chain number.
		if ( std::isfinite( (*client)->population() ) ) {
		    _customers[k] = static_cast<unsigned>((*client)->population());
		} else {
		    _customers[k] = 0;
		}
		_priority[k]  = (*client)->priority();

		/* add chain to all servers of this client */

		for_each( clientsServers.begin(), clientsServers.end(), Exec1<Entity,unsigned int>( &Entity::addServerChain, k ) );
	    }

	} else {
	    const unsigned sz = threads * clientsServers.size();

	    //REPL changes
	    /* --------------- Complex case --------------- */

	    //!!! If chains are extended to entries to handle
	    //!!! fanins, modify delta_chains and Entity::fanIn()
	    //!!! for the entry-to-entry case.

	    size_t new_size = _customers.size() + sz;
	    _customers.resize( new_size );   //Expand vectors to accomodate
	    _thinkTime.resize( new_size );   //new chains.
	    _priority.resize( new_size );

	    /*
	     * Do all the chains for to a server at once.  This makes
	     * Task::threadIndex() simpler.
	     */

	    for ( std::set<Entity *>::const_iterator server = clientsServers.begin(); server != clientsServers.end(); ++server ) {
		for ( unsigned i = 1; i <= threads; ++i ) {
		    k += 1;

		    (*client)->addClientChain( number(), k );
		    (*server)->addServerChain( k );
		    _priority[k]  = (*client)->priority();
		    if ( std::isfinite( (*client)->population() ) ) {
			_customers[k] = static_cast<unsigned>((*client)->population()) * (*server)->fanIn((*client));
		    } else {
			_customers[k] = 0;
		    }
		}
	    }

	}
    }
    return k;
}



//REPL changes
/*
 * Query the closed model to determine the factor needed for
 * Newton Raphson stuff.
 * (5.18) [Pan, pg 73].
 */

double
MVASubmodel::nrFactor( const Server * aStation, const unsigned e, const unsigned k ) const
{
    const double s = aStation->S( e, k, 1 );
    if ( std::isfinite( s ) && closedModel ) {  //tomari
	//Solution for replicated models with open arrivals.
	// See bug # 87.

	return closedModel->nrFactor( *aStation, e, k ) * s;

    } else {
	return s;
    }
}
//-- REP N-R

/*----------------------------------------------------------------------*/
/*                          Solve the model.                            */
/*----------------------------------------------------------------------*/

/*
 * Solve a layer.  Return the difference between the current and previous
 * solutions.  NOTE: Layers of tasks are passed as sequences.  Be sure to
 * always step through the entire sequence, or rewind the list.
 */
MVASubmodel&
MVASubmodel::solve( long iterations, MVACount& MVAStats, const double relax )
{
    if ( _servers.size() == 0 ) return *this;
    if ( flags.verbose ) std::cerr << '.';

    const bool trace = flags.trace_mva && (flags.trace_submodel == 0 || flags.trace_submodel == number() );

    MVAStats.start( nChains(), _servers.size() );
    double deltaRep	= 0.0;
    unsigned iter       = 0; //REP N-R

    if ( trace || Options::Debug::variance() ) {
	std::cout << print_submodel_header( *this, iterations ) << std::endl;
    }

    /* ----------------- initialize the stations ------------------ */

    for_each ( _servers.begin(), _servers.end(), Exec<Entity>( &Entity::clear ) );	/* Clear visit ratios and what have you */
    for_each ( _clients.begin(), _clients.end(), Exec1<Task,Submodel&>( &Task::initClientStation, *this ) );
    for_each ( _servers.begin(), _servers.end(), Exec1<Entity,MVASubmodel&>( &Entity::initServerStation, *this ) );

    /* ------------------- Replication Iteration ------------------- */

    do {
	//REP N-R
	iter += 1;

	/* ---- Adjust Client service times for replication ---- */

	if ( hasReplication() ) {

	    if ( flags.trace_mva  ) {
		std::cout << std::endl << "Current master iteration = " << iterations << std::endl
		     << "  Replication Iteration Number (submodel=" <<number() <<") = " << iter << std::endl
		     << "  deltaRep = " << deltaRep << ", convergence_value = " << Model::convergence_value << std::endl;

	    }

	    for_each ( _clients.begin(), _clients.end(), Exec1<Task,const MVASubmodel&>( &Task::modifyClientServiceTime, *this ) );
	}

	//REP N-R

	if ( flags.generate ) {			// Print out MVA model as C++ source file.
	    Generate::print( *this );
	}

	/* ----------------- Solve the model. ----------------- */

	if ( closedModel ) {

	    if ( openModel ) {
		if ( trace ) {
		    printOpenModel( std::cout );
		}

		/*
		 * If model has any open classes, convert for closed model.
		 */

		try {
		    openModel->convert( _customers );
		}
		catch ( const std::range_error& error ) {
		    MVAStats.faults += 1;
		    if ( Pragma::stopOnMessageLoss() && std::count_if( _servers.begin(), _servers.end(), Predicate<Entity>( &Entity::openModelInfinity ) ) > 0 ) {
			throw exception_handled( "MVA::submodel -- open model overflow" );
		    }
		}
	    }

	    if ( trace ) {
		printClosedModel( std::cout );
	    }
	    try {
		closedModel->solve();
	    }
	    catch ( const std::range_error& error ) {
		throw;
	    }

	    /* Statistics by level -- we can use this to find performance bottlenecks */

	    MVAStats.accumulate( closedModel->iterations(), closedModel->waits(), closedModel->faults() );
	}

	if ( openModel ) {
	    if ( trace && !closedModel ) {
		printOpenModel( std::cout );
	    }

	    try {
		if ( closedModel ) {
		    openModel->solve( *closedModel, _customers );	/* Calculate L[0] queue lengths. */
		} else {
		    openModel->solve();
		}
	    }
	    catch ( const std::range_error& error ) {
		if ( Pragma::stopOnMessageLoss() && std::count_if( _servers.begin(), _servers.end(), Predicate<Entity>( &Entity::openModelInfinity ) ) > 0 ) {
		    throw exception_handled( "MVA::submodel -- open model overflow" );
		}
	    }

	    if ( trace ) {
		std::cout << *openModel << std::endl << std::endl;
	    }
	}

	if ( closedModel && trace ) {
	    std::ios_base::fmtflags oldFlags = std::cout.setf( std::ios::right, std::ios::adjustfield );
	    std::cout << *closedModel << std::endl << std::endl;
	    std::cout.flags( oldFlags );
	}

	/* ---------- Set wait and think times for next pass. --------- */

	if (flags.trace_throughput || flags.trace_idle_time) {
	    std::cout <<"MVASubmodel::solve( ) .... completed solving the MVA model......." << std::endl;
	}

	for_each ( _clients.begin(), _clients.end(), Exec1<Task,const MVASubmodel&>( &Task::saveClientResults, *this ) );
	for_each ( _servers.begin(), _servers.end(), Exec2<Entity,const MVASubmodel&,double>( &Entity::saveServerResults, *this, relax ) );

	/* --- Compute and save new values for entry service times. --- */

	if ( flags.trace_delta_wait ) {
	    std::cout << "------ updateWait for submodel " << number() << ", iteration " << iterations << " ------" << std::endl;
	}

	/* Update waits for replication */

	deltaRep = 0.0;
	if ( hasReplication() ) {
	    unsigned n_deltaRep = 0;
	    deltaRep = std::for_each( _clients.begin(), _clients.end(), ExecSum2<Task,double,const Submodel&,unsigned&>( &Task::updateWaitReplication, *this, n_deltaRep ) ).sum();
	    if ( n_deltaRep ) {
		deltaRep = sqrt( deltaRep / n_deltaRep );	/* Take RMS value over all phases */
	    }
	    if ( iter >= Model::iteration_limit ) {
		LQIO::solution_error( ADV_REPLICATION_ITERATION_LIMIT, number(), iter, deltaRep, Model::convergence_value );
		deltaRep = 0;		/* Break out of loop */
	    }
	}

	/* Update waits for everyone else. */
	if ( flags.trace_interlock ) {
	    std::cout << "------ update interlocked Wait for submodel " << number() << ", iteration " << iterations << " ------" << std::endl;
	}
	for_each ( _clients.begin(), _clients.end(), Exec2<Task,const Submodel&,double>( &Task::updateWait,  *this, relax ) );

	if ( !check_fp_ok() ) {
	    throw floating_point_error( __FILE__, __LINE__ );
	}

	if ( flags.single_step ) {
	    debug_stop( iterations, 0 );
	}
	if ( flags.reset_mva ) closedModel->reset();

    } while ( deltaRep > Model::convergence_value );

    /* ----------------End of Replication Iteration --------------- */
    // REP N-R

    return *this;
}

/*----------------------------------------------------------------------*/
/* Printing functions.							*/
/*----------------------------------------------------------------------*/

/*
 * Print out a submodel.
 */

std::ostream&
MVASubmodel::print( std::ostream& output ) const
{
    output << "----------------------- Submodel  " << number() << " -----------------------" << std::endl
	   << "Customers: " <<  _customers << std::endl
	   << "Clients: " << std::endl;

    for ( std::set<Task *>::const_iterator client = _clients.begin(); client != _clients.end(); ++client ) {
	output << std::setw(2) << "  " << *(*client) << std::endl;
    }
    output << std::endl << "Servers: " << std::endl;

    for ( std::set<Entity *>::const_iterator server = _servers.begin(); server != _servers.end(); ++server ) {
	output << std::setw(2) << "  " << **server << std::endl;
    }
    output << std::endl;
    return output;
}



/*
 * Print stations of closed model.
 */

std::ostream&
MVASubmodel::printClosedModel( std::ostream& output ) const
{
    unsigned stnNo = 1;

    for ( std::set<Task *>::const_iterator client = _clients.begin(); client != _clients.end(); ++client ) {
	output << "[closed=" << stnNo << "] " << **client << std::endl
	       << Task::print_client_chains( **client, number() )
	       << *(*client)->clientStation( number() ) << std::endl;
	stnNo += 1;
    }

    for ( std::set<Entity *>::const_iterator server = _servers.begin(); server != _servers.end(); ++server ) {
	if ( (*server)->isInClosedModel() ) {
	    output << "[closed=" << stnNo << "] " << **server << std::endl
		   << Entity::print_server_chains( **server )
		   << *(*server)->serverStation() << std::endl;
	    (*server)->serverStation()->printOutput( output, stnNo );
	    stnNo += 1;
	}
    }
    return output;
}



/*
 * Print stations of open model.
 */

std::ostream&
MVASubmodel::printOpenModel( std::ostream& output ) const
{
    unsigned stnNo = 1;
    for ( std::set<Entity *>::const_iterator server = _servers.begin(); server != _servers.end(); ++server ) {
	if ( (*server)->isInOpenModel() ) {
	    output << "[open=" << stnNo << "] " << **server << std::endl
		   << *(*server)->serverStation() << std::endl;
	    stnNo += 1;
	}
    }
    return output;
}

/*
 * Print stations of open model.
 */

std::ostream&
MVASubmodel::printMaxCustomers( std::ostream& output ) const
{
    output << "Submodel " << number() << ": " << std::endl;

    for ( std::set<Task *>::const_iterator client = _clients.begin(); client != _clients.end(); ++client ) {
	const std::vector<Entry *>& entries = (*client)->entries();
	for ( std::vector<Entry *>::const_iterator entry = entries.begin(); entry != entries.end(); ++entry  ) {
	    output<<" ClientEntry: "<<(*entry)->name()<< "  has maximum "<< std::min( static_cast<const unsigned>((*client)->population()), static_cast<const unsigned>((*entry)->getMaxCustomers()))<<" customers."<< std::endl;
	}
    }

    for ( std::set<Entity *>::const_iterator server = _servers.begin(); server != _servers.end(); ++server ) {
	const std::vector<Entry *>& entries = (*server)->entries();
	for ( std::vector<Entry *>::const_iterator entry = entries.begin(); entry != entries.end(); ++entry  ) {
	    output<<" ServerEntry: "<<(*entry)->name()<< "  has maximum "<< (*entry)->getMaxCustomers()<<" customers."<< std::endl;
	}
    }

    return output;
}

/*----------------------------------------------------------------------*/
/*			  Fair Share MVA Submodel			*/
/*----------------------------------------------------------------------*/

/*
 * Initialize server's waiting times and populations.  _cfs_servers are set at the same time as _servers, so
 * we'll stick with the known processors.
 */

CFSSubmodel&
CFSSubmodel::initServers( const Model& model )
{
    MVASubmodel::initServers( model );
    for_each ( _servers.begin(), _servers.end(), Exec<Entity>( &Entity::initGroups ) );
    return *this;
}



/*
 * Initialize server's waiting times and populations.
 */

CFSSubmodel&
CFSSubmodel::reinitServers( const Model& model )
{
    MVASubmodel::reinitServers( model );
    for_each ( _servers.begin(), _servers.end(), Exec<Entity>( &Entity::initGroups ) );
    return *this;
}



CFSSubmodel&
CFSSubmodel::build()
{
    MVASubmodel::build();
    _redistributed = false;
    for_each ( _servers.begin(), _servers.end(), Exec<Entity>( &Entity::resetGroups ) );
    return *this;
}



CFSSubmodel&
CFSSubmodel::rebuild()
{
    MVASubmodel::rebuild();
    _redistributed = false;
    for_each ( _servers.begin(), _servers.end(), Exec<Entity>( &Entity::resetGroups ) );
    return *this;
}



/*
 * Solve a layer.  Return the difference between the current and previous
 * solutions.  NOTE: Layers of tasks are passed as sequences.  Be sure to
 * always step through the entire sequence, or rewind the list.
 */

CFSSubmodel&
CFSSubmodel::solve( long iterations, MVACount& MVAStats, const double relax )
{
    if ( _servers.size() == 0 ) return *this;

    if ( _model.runDPS() ) {
	for_each ( _servers.begin(), _servers.end(), redistribute( *this ) );
    }

    MVASubmodel::solve( iterations, MVAStats, relax );

    return *this;
}



/*
 * DPS for group.  Redistibute share.
 */

void
CFSSubmodel::redistribute::operator()( Entity * server )
{
    CFS_Processor * processor = dynamic_cast<CFS_Processor *>(server);
    if ( processor == nullptr ) return;

    const std::set<Group *>& groups = processor->groups();
    const bool trace = flags.trace_cfs && (flags.trace_submodel == 0 || flags.trace_submodel == _submodel.number() );

    std::for_each( groups.begin(), groups.end(), Exec<Group>( &Group::setSpareStatus ) );

    /*
     * This block is only run once, and after the model has stabilized
     * to the point where the processor uilization is known (set
     * Model::setRunDPS in model.cc).  Check whether need to
     * redistributed? If all cap shares: No need check all groups
     * whether group-util<share check: if there are extra resources
     */
	
    if ( !redistributed()
	 && !processor->allCaps()			/* All shares are caps */
	 && std::all_of( groups.begin(), groups.end(), Predicate<Group>( &Group::isUtilLessThanShare ) )
	 && std::accumulate( groups.begin(), groups.end(), 0., add_using<Group>( &Group::utilization ) ) <= processor->copies() * 0.99 ) {

	/* share redistritbution */

	double available_share = 0.;
	double denominator = 0.;

	for ( std::set<Group *>::const_iterator group = groups.begin(); group != groups.end(); ++group ) {
	    double contribution = (*group)->couldContribute();
	    if ( trace ) std::cout << "group: "<< (*group)->name()<<",  contribute "<< contribution<<std::endl;

	    if ( contribution > 0. ) {
		available_share += contribution;
	    } else if ( (*group)->wouldReceive() ) {  // check if this group wants more share.
		denominator += (*group)->getShare();
		if ( trace ) std::cout << "  Group " <<  (*group)->name() << " is able to receive: denominator=" << denominator << std::endl;
	    }
	}

	if ( available_share > 0. &&  denominator > 0. ) {
	    setRedistributed( true );
	    double ratio = available_share / denominator;	// Fraction available.

	    /* redistribute surplus share to groups wanting more */

	    for ( std::set<Group *>::const_iterator group = groups.begin(); group != groups.end(); ++group ) {
		if ( !(*group)->isReceiving() ) continue;	// Don't want extra share.

		(*group)->changeShare( ratio );

		// need to change group task share as well;
		const std::set<Task *>& tasks = (*group)->tasks();
		for_each ( tasks.begin(), tasks.end(), Exec1<Task,double>( &Task::changeShare, ratio ) );
		for_each ( tasks.begin(), tasks.end(), Exec<Task>( &Task::reset_lowerbound ) );

		//delta += (*group)->adjustGroupRatio();
		if ( trace ) {
		    std::cout << "  Group " << (*group)->name() << " received " << (*group)->getShare() - (*group)->getOriginalShare() << std::endl;
		}
	    }

	    Server * station = processor->serverStation();
	    for ( std::set<Task *>::const_iterator client = clients().begin(); client != clients().end(); ++client ) {
		const Group * group = (*client)->group();
		if ( (*client)->getProcessor() != processor || !group->isContributing() ) continue;

		if ( trace ) {
		    std::cout << "  Task " << (*client)->name() <<" in group " << group->name() << " is contributing." << std::endl;
		}

		const std::vector<Entry *>& client_entries = (*client)->entries();
		for ( std::vector<Entry *>::const_iterator entry = client_entries.begin(); entry != client_entries.end(); ++entry  ) {
		    for ( unsigned p = 1; p <= (*entry)->maxPhase(); p++ ) {
			Call * procCall = (*entry)->processorCall( p );
			const double maxwait = procCall->queueingTime();
			station->setMaxWait( procCall->dstEntry()->index(), maxwait );
			if ( trace ) {
			    std::cout << "station->setMaxWait(e="<<procCall->dstEntry()->index()<<", maxwait= "<<maxwait<<");"<<std::endl;
			}
		    }
		}
	    }//end while client task
	}//end if ( available_share > 0. )
    } //end if (need to redistibute)

    /*
     * adjusting group ratio : reduce the extra delay for the receiving groups.
     */

    for_each ( groups.begin(), groups.end(), Exec<Group>( &Group::computeCFSDelay ) );
}
