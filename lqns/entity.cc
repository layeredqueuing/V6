/* -*- c++ -*-
 * $Id: entity.cc 17027 2024-02-04 15:24:18Z greg $
 *
 * Everything you wanted to know about a task or processor, but were
 * afraid to ask.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 *
 * ------------------------------------------------------------------------
 */


#include "lqns.h"
#include <cmath>
#include <functional>
#include <numeric>
#include <sstream>
#include <limits>
#include <lqio/error.h>
#include <lqio/labels.h>
#include <lqio/dom_extvar.h>
#include <mva/open.h>
#include <mva/mva.h>
#include <mva/ph2serv.h>
#include <mva/vector.h>
#include "call.h"
#include "entity.h"
#include "entry.h"
#include "errmsg.h"
#include "flags.h"
#include "pragma.h"
#include "processor.h"
#include "submodel.h"
#include "task.h"
#include "variance.h"

#define DEFERRED_UTULIZATION	false

/* ---------------------- Overloaded Operators ------------------------ */

/*
 * Printing function.
 */

std::ostream&
operator<<( std::ostream& output, const Entity& self )
{
    self.print( output );
    return output;
}



/*
 * Comparison function.
 */

int
operator==( const Entity& a, const Entity& b )
{
    return a.name() == b.name();
}

/* ----------------------- Abstract Superclass. ----------------------- */

/*
 * Set me up.
 */

Entity::Entity( LQIO::DOM::Entity* dom, const std::vector<Entry *>& entries )
    : _dom(dom),
      _entries(entries),
      _tasks(),
      _thinkTime(0.0),
      _station(nullptr),		/* Reference tasks don't have server stations. */
      _attributes(),
      _interlock( *this ),
      _submodel(0),
      _maxPhase(1),
      _utilization(0),
      _lastUtilization(-1.0),		/* Force update 		*/
      _weight(0.0),
#if BUG_393
      _marginalQueueProbabilities(),
#endif
      _replica_number(1)		/* This object is not a replica	*/
{
}


/*
 * Clone an entity (Not PAN_REPLICATION).
 */

Entity::Entity( const Entity& src, unsigned int replica )
    : _dom(src._dom),
      _entries(),
      _tasks(),
      _thinkTime(src._thinkTime),
      _station(nullptr),		/* Reference tasks don't have server stations. */
      _attributes(src._attributes),
      _interlock( *this ),
      _submodel(0),
      _maxPhase(1),
      _utilization(0),
      _lastUtilization(-1.0),		/* Force update 		*/
      _weight(0.0),
#if BUG_393
      _marginalQueueProbabilities(),	/* Result, don't care.		*/
#endif
      _replica_number(replica)		/* This object is a replica	*/
{
}


/*
 * Delete all entries associated with this task.
 */

Entity::~Entity()
{
    delete _station;
}




/*
 * Set the max phase for this entity.
 */

Entity&
Entity::configure( const unsigned nSubmodels )
{
    for ( auto entry : entries() ) entry->configure( nSubmodels );
    if ( std::any_of( entries().begin(), entries().end(), std::mem_fn( &Entry::hasDeterministicPhases ) ) ) setDeterministicPhases( true );
    if ( !Pragma::variance(Pragma::Variance::NONE)
	 && ((nEntries() > 1 && Pragma::entry_variance())
	     || std::any_of( entries().begin(), entries().end(), std::mem_fn( &Entry::hasVariance ) )) ) setVarianceAttribute( true );
    _maxPhase = (*std::max_element( entries().begin(), entries().end(), Entry::max_phase ))->maxPhase();
    return *this;
}


bool
Entity::check() const
{
    if ( !schedulingIsOK() ) {
	getDOM()->runtime_error( LQIO::WRN_SCHEDULING_NOT_SUPPORTED, scheduling_label.at(scheduling()).str.c_str() );
	getDOM()->setSchedulingType(defaultScheduling());
    }
    return true;
}


/*
 * Recursively find all children and grand children from `father'.  As
 * we descend down, we bump the depth.  If our path's cross, we have a
 * loop and abort.  Implemented by subclasses.  The superclass
 * performs common operations (setting the depth and population).   
 */

unsigned
Entity::findChildren( Call::stack& callStack, const bool ) const
{
    unsigned max_depth = std::max( submodel(), callStack.depth() );
    Entity * entity = const_cast<Entity *>(this);		// findChildren is normally const...
#if 0
    std::cerr << "Entity::findChildren: " << print_name() << "->setSubmodel(" << max_depth << ")" << std::endl;
#endif
    entity->setSubmodel( max_depth );
    return max_depth;
}



void
Entity::initializeInterlock()
{
    _interlock.initialize();
}



/*
 * Initialize server's waiting times and populations (called after recalculate dynamic variables)
 */

void
Entity::initializeServer()
{
    computeVariance();
    initThreads();
}


/*
 * Initialize server's waiting times and populations (called after recalculate dynamic variables)
 */

void
Entity::reinitializeServer()
{
    computeVariance();
}




/*
 * Copies is always an unsigned integer greater than zero.  Infinite copies are infinite (delay) servers
 * and should have exactly one instance.
 */

unsigned
Entity::copies() const
{
    unsigned int value = 1;
    try {
	value = getDOM()->getCopiesValue();
    }
    catch ( const std::domain_error& e ) {
	if ( !isInfinite() || std::strcmp( e.what(), "infinity" ) != 0 || value != 1 ) {	/* Will throw iff value == infinity */
	    getDOM()->throw_invalid_parameter( "multiplicity", e.what() );
	}
    }
    return value;
}


/*
 * Return the number of replicas (default is 1).
 */

unsigned
Entity::replicas() const
{
    unsigned int value = 1;
    try {
	value = getDOM()->getReplicasValue();
    }
    catch ( const std::domain_error &e ) {
	getDOM()->throw_invalid_parameter( "replicas", e.what() );
    }
    return value;
}


bool
Entity::isInfinite() const
{
    return getDOM()->isInfinite();
}



bool
Entity::isCalledBy( const Task* task ) const
{
    return std::find( tasks().begin(), tasks().end(), task ) != tasks().end();
}


bool
Entity::isReplica() const
{
    return Pragma::replication() == Pragma::Replication::PRUNE && getReplicaNumber() > 1;
}



/*
 * Add an entry to the list of entries for this task and set local index
 * for MVA.
 */

Entity&
Entity::addEntry( Entry * anEntry )
{
    _entries.push_back( anEntry );
    return *this;
}


/*
 * Return the aggregate throughput of this entity.
 */

double
Entity::throughput() const
{
    return std::accumulate( entries().begin(), entries().end(), 0., []( double l, Entry * r ){ return l + r->throughput(); } );
}



/*
 * Return the "saturation" (normalized utilization)
 */

double
Entity::saturation() const
{
    if ( isInfinite() ) {
	return 0.0;
    } else {
	double value = getDOM()->getCopiesValue();
	return utilization() / value;
    }
}



bool
Entity::hasServerChain( const unsigned k ) const
{
    return _serverChains.find(k) != _serverChains.end();
}


/*
 * Return the total open arrival rate to this server.
 */

bool
Entity::hasOpenArrivals() const
{
    return std::any_of( entries().begin(), entries().end(), std::mem_fn( &Entry::hasOpenArrivals ) );
}


/*
 * Find all tasks that call this task and add them to the argument.
 */

std::set<Task *>&
Entity::getClients( std::set<Task *>& clients ) const
{
    std::for_each( entries().begin(), entries().end(), Entry::get_clients( clients ) );
    return clients;
}


/*
 * Return true if phased server are used.
 */

bool
Entity::markovOvertaking() const
{
    return (bool)( hasSecondPhase()
		   && !isInfinite()
		   && Pragma::overtaking( Pragma::Overtaking::MARKOV ) );
}


/*
 * Compute the utilization based on the throughput and residence times of entries.
 */

double
Entity::computeUtilization( const MVASubmodel& submodel, const Server& )
{
    return std::accumulate( entries().begin(), entries().end(), 0., []( double l, Entry * r ){ return l + r->utilization(); } );
}


/*
 * Calculate and set variance for entire entity.
 */

Entity&
Entity::computeVariance()
{
    std::for_each( entries().begin(), entries().end(), std::mem_fn( &Entry::computeVariance ) );
    return *this;
}



/*
 * Update utilization for this entity and return
 */

double
Entity::deltaUtilization() const
{
    const double thisUtilization = utilization();
    double delta;
    if ( std::isinf( thisUtilization ) && std::isinf( _lastUtilization ) ) {
	delta = 0.0;
    } else {
	delta = thisUtilization - _lastUtilization;
    }
    _lastUtilization = thisUtilization;
    return delta;
}


Entity&
Entity::clear()
{
    serverStation()->clear();
    return *this;
}


/*
 * Check results for sanity.
 * Note -- make sure all errors are advisories, or no output will be generated.
 */

const Entity&
Entity::sanityCheck() const
{
    if ( !std::isfinite( utilization() ) ) {
	LQIO::runtime_error( ADV_INFINITE_UTILIZATION, getDOM()->getTypeName(), name().c_str() );
    } else if ( !isInfinite() && utilization() > copies() * 1.05 ) {
	LQIO::runtime_error( ADV_INVALID_UTILIZATION, getDOM()->getTypeName(), name().c_str(), copies(), utilization() );
    }
    return *this;
}


/*
 * Return true if any entry overflows in open model solution.
 */

bool
Entity::openModelInfinity() const
{
    bool rc = false;
    const Server * station = serverStation();
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	const unsigned e = (*entry)->index();
	if ( !std::isfinite( station->R(e,0) ) && station->V(e,0) != 0 && station->S(e,0) != 0 ) {
	    LQIO::runtime_error( LQIO::ERR_ARRIVAL_RATE, station->V(e,0), station->mu()/station->S(e,0), (*entry)->name().c_str() );
	    rc = true;
	}
    }
    return rc;
}


const Entity&
Entity::insertDOMResults() const
{
#if BUG_393
    if ( !_marginalQueueProbabilities.empty() ) {
	std::vector<double>& marginals = getDOM()->getResultMarginalQueueProbabilities();
	marginals = _marginalQueueProbabilities;
    }
#endif
    return *this;
}

/* -------------------------- Interlock ------------------------------- */

/*
 * Return in probability of interlocking.
 */

Probability
Entity::prInterlock( const Task& aClient ) const
{
    const Probability pr = _interlock.interlockedFlow( aClient ) / population();
    if ( flags.trace_interlock ) {
	std::cout << "Interlock: "
		  << aClient.name() << "(" << aClient.population() << ") -> "
		  << name()         << "(" << population()         << ")  = " << pr << std::endl;
    }
    return pr;
}


Probability
Entity::prInterlock( const Entry * clientEntry ) const
{
    const Probability pr = _interlock.interlockedFlow( clientEntry );
    if ( flags.trace_interlock ) {
	std::cout << "Interlock rate: ";
	const Entity * client = clientEntry->owner();
	std::cout << client->name() << "(" << client->population() << ") -> "
		  << name()         << "(" << population()         << ")  = " << pr << std::endl;
    }
    return pr;
}



Probability
Entity::prInterlock( const Task& aClient, const Entry * aServerEntry, double& il_rate, bool& moreThan4 ) const
{
    Probability pril = _interlock.interlockedFlow( aClient, aServerEntry, il_rate, moreThan4 );
    pril /= population();
    if ( flags.trace_interlock ) {
	std::cout << "Interlock probability: "
		  << aClient.name() << "(" << aClient.population() << ") -> "
		  << name()         << "(" << aServerEntry->name()         << ")  = " << pril << std::endl;
	std::cout << "Interlock rate: ("
		  << aClient.name() << " -> " << name() << ")  = " << il_rate << std::endl;
    }
    return pril;
}


/*
 * Return the rate of interlocked flows.
 */
void
Entity::setChainILRate(const Task& aClient, double rate) const
{
    const ChainVector& chain = aClient.clientChains( submodel() );
    std::for_each( chain.begin(), chain.end(), set_chain_IL_rate( *this, rate ) );
}

void Entity::set_chain_IL_rate::operator()( unsigned int k ) { if ( _serverChains.find(k) ) { _station->setIR( k, _rate ); } }


bool
Entity::hasILWait() const
{
    return std::any_of( entries().begin(), entries().end(), Entry::has_IL_wait( submodel() ) );
}



/*
 * initialize total interlock weights
 */

Entity&
Entity::initWeights( MVASubmodel& submodel )
{
    double sum = 0.0;
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry  ) {
	const std::set<Call *>& callers = (*entry)->callerList();
	sum = std::accumulate( callers.begin(), callers.end(), sum, add_weight( submodel ) );
    }

    _weight = std::max( sum, 0.0 );

    return *this;
}

/* -------------------------- Interlock ------------------------------- */

void
Entity::setMaxCustomers( const MVASubmodel& submodel ) const
{
    Server * aStation = serverStation();
    const std::set<Task *>& clients = submodel.clients();

    /* find out the total number of possible customer to each server entry*/
    for ( std::vector<Entry *>::const_iterator server_entry = entries().begin(); server_entry != entries().end(); ++server_entry  ) {
	const double n = std::accumulate( clients.begin(), clients.end(), 0., Task::add_max_customers( submodel, *server_entry ) );
	(*server_entry)->saveMaxCustomers( n, true );
    }

    for ( std::set<Task *>::const_iterator client = clients.begin(); client != clients.end(); ++client ) {
	const ChainVector& chain = (*client)->clientChains( submodel.number() );

	for ( unsigned ix = 2; ix <= chain.size(); ++ix ) {
	    std::for_each( entries().begin(), entries().end(), Entry::exec( &Entry::setMaxCustomersForChain, chain[ix] ) );
	}
    }

    /*
     * find out the total number of possible customers to the server.
     * this total number is used by
     * Markov_Phased_Conway_Multi_Server::meanMinimumOvertaking() to
     * reduced the extra overtaking, in case of exsiting many availabe
     * servers
     */

    double totalCustomers=0;

    for ( std::set<Task *>::const_iterator client = clients.begin(); client != clients.end(); ++client ) {
	const ChainVector& aChain = (*client)->clientChains( submodel.number() );
	const unsigned k = aChain[1];
	double ncusts=0;
	const std::vector<Entry *>& client_entries = (*client)->entries();
	for ( std::vector<Entry *>::const_iterator client_entry = client_entries.begin(); client_entry != client_entries.end(); ++client_entry  ) {
	    for ( std::vector<Entry *>::const_iterator server_entry = entries().begin(); server_entry != entries().end(); ++server_entry ) {
		if ((*server_entry)->isCalledBy(*client_entry)){
		    ncusts+=(*client_entry)->getMaxCustomers();		// !!! std::max(???)
		    break;
		}
	    }
	    if (ncusts>(*client)->population()){
		ncusts=(*client)->population();
		break;
	    }
	}
	totalCustomers += ncusts;
	/* save the total number of possible customer from chain k to the server */
	aStation->setMaxCustomers( 0, k, ncusts );
    }

    /* save the total number of possible customer to the server */
    aStation->setMaxCustomers( 0, 0, totalCustomers );
}



void
Entity::setInterlock( const MVASubmodel& submodel ) const
{
#if THROUGHPUT_INTERLOCK
    Server * station = serverStation();
    const std::set<Task *>& clients = submodel.clients();

    for ( std::set<Task *>::const_iterator client = clients.begin(); client != clients.end(); ++client ) {
	if ( (*client)->throughput() == 0.0 ) continue;

	const Probability PrIL = prInterlock( *(*client) );
	if ( PrIL == 0.0 ) continue;

	const ChainVector& chains = (*client)->clientChains( submodel.number() );
	for ( ChainVector::const_iterator k = chains.begin(); k != chains.end(); ++k ) {
	    if ( hasServerChain(*k) ) {
		for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
		    station->setInterlock( (*entry)->index(), *k, PrIL );
		}
	    }
	}
    }
#else
    const std::set<Task *>& clients = submodel.clients();
    if ( isInterlocked() ) {
	std::for_each( clients.begin(), clients.end(), Task::set_interlock( submodel, this ) );
    } else {
	std::for_each( clients.begin(), clients.end(), Task::set_interlock_PrUpper( submodel, this ) );
    }
#endif
}



/*
 * set interlocking relationship between client chains.
 */

void
Entity::setInterlockRelation( const MVASubmodel& submodel ) const
{
    Server * station = serverStation();
    std::set<Task *> clients;
    getClients( clients );

    for ( std::set<Task *>::const_iterator client_1 = clients.begin(); client_1 != clients.end(); ++client_1 ) {
	if ( (*client_1)->throughput() == 0.0 ) continue;
	const ChainVector& chain = (*client_1)->clientChains( submodel.number() );

	for ( unsigned ix = 1; ix <= chain.size(); ++ix ) {
	    const unsigned k1 = chain[ix];
	    if ( !hasServerChain(k1) ) continue;
	
	    const std::vector<Entry *>& server_entries = entries();
	    for ( std::vector<Entry *>::const_iterator server_entry_1 = server_entries.begin(); server_entry_1 != server_entries.end(); ++server_entry_1 ) {
		const unsigned se1 = (*server_entry_1)->index();
		if ( station->IR( se1, k1 ) == 0.) continue;
		for ( std::set<Task *>::const_iterator client_2 = clients.begin(); client_2 != clients.end(); ++client_2 ) {
		    if ( (*client_2)->throughput() == 0.0 ) continue;
		    const ChainVector& chain2 = (*client_2)->clientChains( submodel.number() );

		    for ( unsigned ix2 = 1; ix2 <= chain2.size(); ++ix2 ) {
			const unsigned k2 = chain2[ix2];
			if ( !hasServerChain(k2) ) continue;
		
			for ( std::vector<Entry *>::const_iterator server_entry_2 = server_entries.begin(); server_entry_2 != server_entries.end(); ++server_entry_2 ) {
			    setInterlockRelation( station, *server_entry_1, *server_entry_2, *client_1, *client_2, k1, k2 );					
			}//end while ((*server_entry_2)= nextEntry2())
		    }
		}//end while(aclient2=nextentry())
	    }
	}
    }
}


void
Entity::setInterlockRelation( Server * station, const Entry * server_entry_1, const Entry * server_entry_2, const Task * client_1, const Task * client_2, unsigned k1, unsigned k2 ) const
{
    const unsigned se1 = server_entry_1->index();
    const unsigned se2 = server_entry_2->index();
    if ( k1 == k2 && se1 == se2 ) return;
		
    //if ( station->IR( se2,k2)>0.){
    if ( hasCommonEntry( server_entry_1, server_entry_2, client_1, client_2 )){
	if ( station->IR( se2, k2 ) > 0.) {
	    station->set_IL_Relation( se1, k1, se2, k2, 1.0 );
	    if ( k1 == k2 ) return;
			
	    // sending interlocks;
	
	    std::set<const Entry* > common_entries = getCommonEntries( server_entry_1, server_entry_2, client_1, client_2);
	    int p1 = 0, p2 = 0;
	    for ( std::set<const Entry *>::const_iterator common_entry = common_entries.begin(); common_entry != common_entries.end(); ++common_entry ) {
		if ((*common_entry)->owner() == client_1 && (*common_entry)->owner() != client_2) {
		    p1 = 3;
		} else if ((*common_entry)->owner() != client_1 && (*common_entry)->owner() == client_2) {
		    p2 = 3;
		}
	    }
	    if ( p1 == 3 ) {
		station->set_IL_Relation( se1, k1, 1, 0, 3 );
		station->set_IL_Relation( se2, k2, 1, 0, 4 );
	    }
	    if ( p2 == 3 ) {
		station->set_IL_Relation( se2, k2, 1, 0, 3 );
		station->set_IL_Relation( se1, k1, 1, 0, 4 );
	    }
	    if ( flags.trace_interlock && p1 == 3 ) {
		std::cout << "set sending Interlock relation: (server entry=" << server_entry_1->name()
		     << ", client task= " << client_1->name()<< "), IR_Relation( se1="
		     << se1<< ", k1= "<<k1 << ", se2=" << se2<< ", k2= 0) = 3"  << std::endl;
	    }
	    if ( flags.trace_interlock && p2 == 3 ) {
		std::cout << "set sending Interlock relation: (server entry=" << server_entry_2->name()
		     << ", client task= " << client_2->name()<< "), IR_Relation( se2="
		     << se2<< ", k= "<<k2 << ", se1=" << se1<< ", k= 0) = 3"  << std::endl;
	    }
	}
	//================
	if ( flags.trace_interlock ) {
	    std::cout << "set Interlock relation: (server entry="  << server_entry_1->name()
		 << ", client task= " << client_1->name()
		 << "), between (server entry="  << server_entry_2->name()
		 << ", client task= " << client_2->name()
		 << "), IR_Relation( se1=" << se1<< ", k1= "<<k1
		 << ", se2=" << se2<< ", k2= "<<k2 << ") = 1"  << std::endl;
	
	}
    } else { // have no common entry!
	// check whether is a type3 sending interlock.
	// now:  station->IR( se1,k)>0.
	if ( client_2->nEntries() > 1 && station->IR( se2, k2 ) > 0. && isType3Sending( server_entry_1, server_entry_2, client_1, client_2 ) ) {
	    if ( client_2->population() == 1. ) {//  (station->getMaxCustomers(se2, k2) ==1) ){
		station->set_IL_Relation( se1, k1, se2, k2, 6 );
		station->set_IL_Relation( se2, k2, se1, k1, 6 );
	    } else {
		const double p = 5 + 1.0 / square(client_2->population());
		station->set_IL_Relation( se1, k1, se2, k2, p );
		station->set_IL_Relation( se2, k2, se1, k1, p );
	    }
	    if ( flags.trace_interlock ) {
		std::cout << "set Interlock relation: type3 sending (server entry="  << server_entry_1->name()
		     << ", client task= " << client_1->name()<< "), between (server entry="  << server_entry_2->name()
		     << ", client task= " << client_2->name()<< "), IR_Relation( se1="
		     << se1<< ", k1= "<<k1 << ", se2=" << se2<< ", k2= "<<k2
		     << ") = " << station->IL_Relation( se2, k2, se1, k1 ) << std::endl;
			
	    }
	}
    }
} // end if ( station->IR( se2,k2)>0.)

/* ----------------------------- Save Results ----------------------------- */

/*
 * Common code.
 */

void
Entity::saveServerResults( const MVASubmodel& submodel, const Server& station, double relaxation )
{
    std::for_each( entries().begin(), entries().end(), Entry::SaveServerResults( submodel, station, *this ) );

#if BUG_393
    /* Only save if needed */
    if ( Pragma::saveMarginalProbabilities() && isClosedModelServer() && station.getMarginalProbabilitiesSize() > 0 ) {
	unsigned int copies = static_cast<unsigned int>(station.mu());
	_marginalQueueProbabilities.resize(copies+1);
	for ( unsigned int i = 0; i <= copies; ++i ) {
	    _marginalQueueProbabilities[i] = submodel.closedModelMarginalQueueProbability( station, i );
	}
    }
#endif

    setUtilization( computeUtilization( submodel, station ) );
    setIdleTime( relaxation );
}


Entity&
Entity::setUtilization( double utilization )
{
    _utilization = utilization;
    if ( Pragma::stopOnBogusUtilization() > 0. && !isInfinite() && _utilization / copies() > Pragma::stopOnBogusUtilization() ) {
	std::ostringstream err;
	err << name() << " utilization=" << _utilization << " exceeds multiplicity=" << copies();
	throw std::range_error( err.str() );
    }
    return *this;
}



/*
 * Calculate and set think time.  Note that population returns the maximum
 * number of customers possible at a station.  It is used, rather than copies,
 * because some multi-servers may have more threads specified than can
 * possibly be active.
 */

void
Entity::setIdleTime( const double relax )
{
    if ( population() == std::numeric_limits<unsigned int>::max() ) {
	_thinkTime = 0.0;
    } else if ( utilization() >= population() ) {
	_thinkTime = 0.0;
    } else if ( throughput() > 0.0 ) {
	_thinkTime = under_relax( _thinkTime, (population() - utilization()) / throughput(), relax );
    } else {
	_thinkTime = std::numeric_limits<double>::infinity();
    }
    if ( flags.trace_idle_time ) {
	std::cout << "Entity(" << name() << ")::setIdleTime()   thinkTime=" << _thinkTime << std::endl;
    }
}

/* -------------------------------------------------------------------- */
/* Funky Formatting functions for inline with <<.			*/
/* -------------------------------------------------------------------- */

/* static */ std::string
Entity::fold( const std::string& s1, const Entity* e2 )
{
    std::string s = s1;
    if ( !s.empty() ) s += " ";
    s += e2->name();
    return s;
}

/*
 * Print chains for this client.
 */

/* static */ std::ostream&
Entity::output_server_chains( std::ostream& output, const Entity& entity )
{
    output << "Chains:" << entity.serverChains() << std::endl;
    return output;
}


/* static */ std::ostream&
Entity::output_entries( std::ostream& output, const Entity& entity )
{
    const std::vector<Entry *>& entries = entity.entries();
    const Entry& e1 = *entries.front();
    std::ostringstream name;
    name << e1.print_name();
    output << std::accumulate( std::next(entries.begin()), entries.end(), name.str(), &Entry::fold );
    return output;
}



/* static */ std::ostream&
Entity::output_name( std::ostream& output, const Entity& entity )
{
    output << entity.name();
    if ( entity.isReplicated() ) {
	output << "." << entity.getReplicaNumber();
    }
    return output;
}

/* static */ std::ostream&
Entity::output_info( std::ostream& output, const Entity& entity )
{
    if ( entity.serverStation() ) {
	output << entity.serverStation()->typeStr();
    } else {
	output << "--";
    }
    return output;
}


/* static */ std::ostream&
Entity::output_type( std::ostream& output, const Entity& entity )
{
    std::string buf;
    const unsigned n = entity.copies();

    if ( entity.scheduling() == SCHEDULE_CUSTOMER ) {
	buf = std::string( "ref(" ) + std::to_string( n ) + ")";
    } else if ( entity.isInfinite() ) {
	buf = "inf";
    } else if ( n > 1 ) {
	buf = std::string( "mult(" ) + std::to_string( n ) + ")";
    } else {
	buf = "serv";
    }
    output << buf;
    return output;
}
