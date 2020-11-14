/* -*- c++ -*-
 * $Id: entity.cc 14091 2020-11-13 02:59:17Z greg $
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


#include "dim.h"
#include <string>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <lqio/error.h>
#include <lqio/labels.h>
#include <lqio/dom_extvar.h>
#include "call.h"
#include "entity.h"
#include "entry.h"
#include "errmsg.h"
#include "fpgoop.h"
#include "lqns.h"
#include "mva.h"
#include "open.h"
#include "overtake.h"
#include "ph2serv.h"
#include "pragma.h"
#include "server.h"
#include "submodel.h"
#include "task.h"
#include "variance.h"
#include "vector.h"

/* ---------------------- Overloaded Operators ------------------------ */

/*
 * Printing function.
 */

ostream&
operator<<( ostream& output, const Entity& self )
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
      _population(1.0),
      _thinkTime(0.0),
      _station(nullptr),		/* Reference tasks don't have server stations. */
      _interlock( *this ),
      _submodel(0),
      _maxPhase(1),
      _utilization(0),
      _lastUtilization(-1.0),		/* Force update 		*/
      _weight(0.0)
{
    attributes.initialized      = 0;		/* entity was initialized.	*/
    attributes.closed_model	= 0;		/* Stn in in closed model.     	*/
    attributes.open_model	= 0;		/* Stn is in open model.	*/
    attributes.deterministic    = 0;		/* an entry has det. phase.	*/
    attributes.pure_delay       = 0;		/* Special task of some form.	*/
    attributes.pure_server	= 1;		/* Can use FCFS schedulging.	*/
    attributes.variance         = 0;		/* */
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
    std::for_each( entries().begin(), entries().end(), Exec1<Entry,unsigned>( &Entry::configure, nSubmodels ) );
    if ( std::any_of( entries().begin(), entries().end(), Predicate<Entry>( &Entry::hasDeterministicPhases ) ) ) attributes.deterministic = 1;
    if ( !Pragma::variance(Pragma::NO_VARIANCE)
	 && ((nEntries() > 1 && Pragma::entry_variance())
	     || std::any_of( entries().begin(), entries().end(), Predicate<Entry>( &Entry::hasVariance ) )) ) attributes.variance = 1;
    _maxPhase = (*std::max_element( entries().begin(), entries().end(), Entry::max_phase ))->maxPhase();
    return *this;
}



/*
 * Recursively find all children and grand children from `father'.  As
 * we descend down, we bump the depth.  If our path's cross, we have a
 * loop and abort.
 */

unsigned
Entity::findChildren( Call::stack& callStack, const bool ) const
{
    unsigned max_depth = max( submodel(), callStack.depth() );

    const_cast<Entity *>(this)->setSubmodel( max_depth );
    return max_depth;
}



/*
 * Initialize waiting time at my entries.
 */

Entity&
Entity::initWait()
{
    for_each( entries().begin(), entries().end(), Exec<Entry>( &Entry::initWait ) );
    return *this;
}


Entity&
Entity::initInterlock()
{
    _interlock.initialize();
    return *this;
}



/*
 * Initialize server's waiting times and populations (called after recalculate dynamic variables)
 */

Entity&
Entity::initServer( const Vector<Submodel *>& submodels )
{
    initWait();
    updateAllWaits( submodels );
    computeVariance();
    initThroughputBound();
    initPopulation();
    initThreads();
    initialized(true);
    return *this;
}


/*
 * Initialize server's waiting times and populations (called after recalculate dynamic variables)
 */

Entity&
Entity::reinitServer( const Vector<Submodel *>& submodels )
{
    updateAllWaits( submodels );
    computeVariance();
    initThroughputBound();
    initPopulation();
    return *this;
}




/*
 * Copies is always an unsigned integer greater than zero.  Infinite copies are infinite (delay) servers
 * and should have exactly one instance.
 */

unsigned
Entity::copies() const
{
    unsigned int value = 1;
    if ( !getDOM()->isInfinite() ) {
	try { 
	    value = getDOM()->getCopiesValue();
	}
	catch ( const std::domain_error& e ) {
	    solution_error( LQIO::ERR_INVALID_PARAMETER, "multiplicity", getDOM()->getTypeName(), name().c_str(), e.what() );
	    throw_bad_parameter();
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
	solution_error( LQIO::ERR_INVALID_PARAMETER, "replicas", getDOM()->getTypeName(), name().c_str(), e.what() );
	throw_bad_parameter();
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
    return find( tasks().begin(), tasks().end(), task ) != tasks().end();
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
    return std::accumulate( entries().begin(), entries().end(), 0., add_using<Entry>( &Entry::throughput ) );
}



/*
 * Return the total open arrival rate to this server.
 */

double
Entity::openArrivalRate() const
{
    return std::accumulate( entries().begin(), entries().end(), 0., add_using<Entry>( &Entry::openArrivalRate ) );
}



/*
 * Return utilization for this entity.  Compute from the entry utilization.
 */

double
Entity::utilization() const
{		
    const_cast<Entity *>(this)->_utilization = std::accumulate( entries().begin(), entries().end(), 0., add_using<Entry>( &Entry::utilization ) );
    if ( Pragma::stopOnBogusUtilization() > 0. && !isInfinite() && _utilization / copies() > Pragma::stopOnBogusUtilization() ) {
	std::ostringstream err;
	err << name() << " utilization=" << _utilization << " exceeds multiplicity=" << copies();
	throw std::range_error( err.str() );
    }
    return _utilization;
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


unsigned
Entity::hasServerChain( const unsigned k ) const
{
    return _serverChains.find(k);
}


/*
 * Return the total open arrival rate to this server.
 */

bool
Entity::hasOpenArrivals() const
{
    return std::any_of( entries().begin(), entries().end(), Predicate<Entry>( &Entry::hasOpenArrivals ) );
}


/*
 * Find all tasks that call this task and add them to the argument.
 * Return the number of calling tasks ADDED!
 */

const Entity&
Entity::getClients( std::set<Task *>& callingTasks ) const
{
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	const std::set<Call *>& callerList = (*entry)->callerList();
	for ( std::set<Call *>::const_iterator call = callerList.begin(); call != callerList.end(); ++call ) {
	    if ( !(*call)->hasForwarding() && (*call)->srcTask()->isUsed() ) {
		callingTasks.insert(const_cast<Task *>((*call)->srcTask()));
	    }
	}
    }

    return *this;
}

double 
Entity::nCustomers() const
{
    std::set<Task *> tasks;
    getClients( tasks );
    return std::accumulate( tasks.begin(), tasks.end(), 0., add_using<Task>( &Task::population ) );
}

/*
 * Return true if phased server are used.
 */

bool
Entity::markovOvertaking() const
{
    return (bool)( hasSecondPhase()
		   && !isInfinite()
		   && Pragma::overtaking( Pragma::MARKOV_OVERTAKING ) );
}



/*
 * Return the scheduling type allowed for this object.  Overridden by
 * subclasses if the scheduling type can be something other than FIFO.
 */

unsigned
Entity::validScheduling() const
{
    return 1 << static_cast<unsigned>(defaultScheduling());
}



/*
 * Check the scheduling type.  Return the default type if the value
 * supplied is not kosher.  Overridden by subclasses if the scheduling
 * type can be something other than FIFO.
 */

bool
Entity::schedulingIsOk( const unsigned bits ) const
{
    return ((1 << static_cast<unsigned>(scheduling())) & bits ) != 0;
}



/*
 * Upated the waiting time over all submodels.
 */

Entity&
Entity::updateAllWaits( const Vector<Submodel *>& submodels )
{
    std::for_each( submodels.begin(), submodels.end(), update_wait( *this ) );
    return *this;
}



/*
 * Calculate and set variance for entire entity.
 */

Entity&
Entity::computeVariance() 
{
    for_each( entries().begin(), entries().end(), Exec<Entry>( &Entry::computeVariance ) );
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
    if ( isinf( thisUtilization ) && isinf( _lastUtilization ) ) {
	delta = 0.0;
    } else {
	delta = thisUtilization - _lastUtilization;
    }
    _lastUtilization = thisUtilization;
    return delta;
}

/************************************************************************/
/*			      Interlock					*/
/************************************************************************/

/*
 * Return in probability of interlocking.
 */

Probability
Entity::prInterlock( const Task& aClient ) const
{
    const Probability pr = _interlock.interlockedFlow( aClient ) / population();
    if ( flags.trace_interlock ) {
	cout << "Interlock: " 
	     << aClient.name() << "(" << aClient.population() << ") -> " 
	     << name()         << "(" << population()         << ")  = " << pr << endl;
    }
    return pr;
}


Probability
Entity::prInterlock( const Entry * clientEntry ) const
{
    const Probability pr = _interlock.interlockedFlow( clientEntry );
    if ( flags.trace_interlock ) {
	cout << "Interlock rate: ";
	    const Entity * client = clientEntry->owner();
	    cout << client->name() << "(" << client->population() << ") -> " 
		 << name()         << "(" << population()         << ")  = " << pr << endl;
    }
    return pr;
}



Probability
Entity::prInterlock( const Task& aClient, const Entry * aServerEntry, double& il_rate, bool& moreThan4 ) const
{
    Probability pril = _interlock.interlockedFlow( aClient, aServerEntry, il_rate, moreThan4 );
    pril /= population();
    if ( flags.trace_interlock ) {
	cout << "Interlock probability: " 
	     << aClient.name() << "(" << aClient.population() << ") -> " 
	     << name()         << "(" << aServerEntry->name()         << ")  = " << pril << endl;
	cout << "Interlock rate: (" 
	     << aClient.name() << " -> " << name() << ")  = " << il_rate << endl;
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

void Entity::set_chain_IL_rate::operator()( unsigned int k ) { if ( _serverChains.find(k) ) { _station->setChainILRate( k, _rate ); } }





bool 
Entity::hasILWait() const 
{
    return std::any_of( entries().begin(), entries().end(), Predicate1<Entry,unsigned>( &Entry::hasILWait, submodel() ) );
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



const Entity&
Entity::setMaxCustomers( const MVASubmodel& submodel ) const
{
    Server * aStation = serverStation();
    const std::set<Task *>& clients = submodel.getClients();

    /* find out the total number of possible customer to each server entry*/
    for ( std::vector<Entry *>::const_iterator server_entry = entries().begin(); server_entry != entries().end(); ++server_entry  ) {
	const double n = std::accumulate( clients.begin(), clients.end(), 0., Task::add_max_customers( submodel, *server_entry ) );
	(*server_entry)->saveMaxCustomers( n, true );
    }

    for ( std::set<Task *>::const_iterator client = clients.begin(); client != clients.end(); ++client ) {
	const ChainVector& chain = (*client)->clientChains( submodel.number() );

	for ( unsigned ix = 2; ix <= chain.size(); ++ix ) {
	    const unsigned k = chain[ix];
	    for_each ( entries().begin(), entries().end(), ConstExec1<const Entry,unsigned int>( &Entry::setMaxCustomersForChain, k ) );
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
    return *this;
}



/*
 * Initialize the service and visit parameters for a server.  Also set
 * myChains to all chains that visit this server.
 */

const Entity&
Entity::setRealCustomers( const MVASubmodel& submodel ) const
{
    const std::set<Task *>& clients = submodel.getClients();
    std::for_each( clients.begin(), clients.end(), Task::set_real_customers( submodel, this ) );
    return *this;
}
    

const Entity&
Entity::setInterlock( const MVASubmodel& submodel ) const
{
#if THROUGHPUT_INTERLOCK
    Server * station = serverStation();
    const std::set<Task *>& clients = submodel.getClients();
    
    for ( std::set<Task *>::const_iterator client = clients.begin(); client != clients.end(); ++client ) {
	if ( (*client)->throughput() == 0.0 ) continue;

	const Probability PrIL = prInterlock( *(*client) );
	if ( PrIL == 0.0 ) continue;

	const ChainVector& chain = (*client)->clientChains( submodel.number() );

	for ( unsigned ix = 1; ix <= chain.size(); ++ix ) {
	    const unsigned k = chain[ix];
	    if ( hasServerChain(k) ) {
		for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
		    station->setInterlock( (*entry)->index(), k, PrIL );
		}
	    }
	}
    }
#else
    Server * aStation = serverStation();
    if ( !isInterlocked() ) {
	setInterlockPr_upper( submodel );
	return *this;
    }
    const std::set<Task *>& clients = submodel.getClients();
    for ( std::set<Task *>::const_iterator client = clients.begin(); client != clients.end(); ++client ) {
	if ( (*client)->throughput() == 0.0 ) continue;

	const ChainVector& chain = (*client)->clientChains( submodel.number() );

	for ( unsigned ix = 1; ix <= chain.size(); ++ix ) {
	    const unsigned k = chain[ix];
	    if ( !hasServerChain(k) ) continue;
	    const double ir_c = std::accumulate( entries().begin(), entries().end(), 0., Entry::add_interlock( submodel, *client, k ) );
	    aStation->setChainILRate(0,k,ir_c );	
	}//end for each client chain k

    }
#endif
    return *this;
}



/*
 * set interlocking relationship between client chains.
 */

const Entity&
Entity::setInterlockRelation( const MVASubmodel& submodel ) const
{
    Server * aStation = serverStation();
    std::set<Task *> clients;
    this->getClients( clients );

    for ( std::set<Task *>::const_iterator client_1 = clients.begin(); client_1 != clients.end(); ++client_1 ) {
	if ( (*client_1)->throughput() == 0.0 ) continue;
	const ChainVector& aChain = (*client_1)->clientChains( submodel.number() );

	for ( unsigned ix = 1; ix <= aChain.size(); ++ix ) {
	    const unsigned k = aChain[ix];
	    if ( hasServerChain(k) ) { // may not need;
	
		const std::vector<Entry *>& server_entries = entries();
		for ( std::vector<Entry *>::const_iterator server_entry_1 = server_entries.begin(); server_entry_1 != server_entries.end(); ++server_entry_1 ) {
		    const unsigned se1 = (*server_entry_1)->index();
		    if ( aStation->chainILRate( se1,k)>0.){
			for ( std::set<Task *>::const_iterator client_2 = clients.begin(); client_2 != clients.end(); ++client_2 ) {
			    if ( (*client_2)->throughput() == 0.0 ) continue;
			    const ChainVector& aChain2 = (*client_2)->clientChains( submodel.number() );

			    for ( unsigned ix2 = 1; ix2 <= aChain2.size(); ++ix2 ) {
				const unsigned k2 = aChain2[ix2];
				if ( hasServerChain(k2) ) { // may not need;
		
				    for ( std::vector<Entry *>::const_iterator server_entry_2 = server_entries.begin(); server_entry_2 != server_entries.end(); ++server_entry_2 ) {
					const unsigned se2 = (*server_entry_2)->index();
					if ( k == k2 && se1 == se2 ) continue;
		
					//if ( aStation->chainILRate( se2,k2)>0.){
					if (hasCommonEntry( (*server_entry_1), (*server_entry_2), (*client_1), (*client_2) )){
					    if ( aStation->chainILRate( se2,k2)> 0.){
						aStation->set_IL_Relation(se1,  k, se2, k2, 1.0);
			
						// sending interlocks;
						if ( k != k2 ) {
						    std::set<const Entry* > common_entries = getCommonEntries( (*server_entry_1), (*server_entry_2), (*client_1), (*client_2));
						    int p1=0, p2=0;
						    for ( std::set<const Entry *>::const_iterator common_entry = common_entries.begin(); common_entry != common_entries.end(); ++common_entry ) {
							if ((*common_entry)->owner() == (*client_1) && (*common_entry)->owner() != (*client_2)) {
							    p1=3;
							} else if ((*common_entry)->owner() != (*client_1) && (*common_entry)->owner() == (*client_2)) {
							    p2=3;
							}
						    }
						    if (p1==3) {
							aStation->set_IL_Relation(se1,  k, 1, 0, 3);
							aStation->set_IL_Relation(se2,  k2, 1, 0, 4);
						    }
						    if (p2==3) {
							aStation->set_IL_Relation(se2,  k2, 1, 0, 3);
							aStation->set_IL_Relation(se1,  k, 1, 0, 4);
						    }
						    if ( flags.trace_interlock && p1==3) {
							cout << "set sending Interlock relation: (server entry=" << (*server_entry_1)->name()
							     << ", client task= " << (*client_1)->name()<< "), IR_Relation( se1="
							     << se1<< ", k1= "<<k << ", se2=" << se2<< ", k2= 0) = 3"  <<endl;
						    }
						    if ( flags.trace_interlock && p2==3) {
							cout << "set sending Interlock relation: (server entry=" << (*server_entry_2)->name()
							     << ", client task= " << (*client_2)->name()<< "), IR_Relation( se2="
							     << se2<< ", k= "<<k2 << ", se1=" << se1<< ", k= 0) = 3"  <<endl;
						    }
						}
						//================
						if ( flags.trace_interlock ) {
						    cout << "set Interlock relation: (server entry="  << (*server_entry_1)->name()
							 << ", client task= " << (*client_1)->name()
							 << "), between (server entry="  << (*server_entry_2)->name()
							 << ", client task= " << (*client_2)->name()
							 << "), IR_Relation( se1=" << se1<< ", k1= "<<k
							 << ", se2=" << se2<< ", k2= "<<k2 << ") = 1"  <<endl;
	
						}
					    }
					}else{ // have no common entry!
					    // check whether is a type3 sending interlock.
					    // now:  aStation->chainILRate( se1,k)>0.
					    if ( ((*client_2)->nEntries() >1) &&  (aStation->chainILRate( se2,k2) >0.) &&
						 (isType3Sending( (*server_entry_1), (*server_entry_2), (*client_1), (*client_2))) ){
						if  ((*client_2)->population() ==1.) {//  (aStation->getMaxCustomers(se2, k2) ==1) ){
						    aStation->set_IL_Relation(se1,  k, se2, k2, 6);
						    aStation->set_IL_Relation(se2,  k2, se1, k, 6);
						}else{
						    double p= 5 + 1.0/((*client_2)->population() * (*client_2)->population()) ;
						    aStation->set_IL_Relation(se1,  k, se2, k2, p );
						    aStation->set_IL_Relation(se2,  k2, se1, k, p);
						}
						if ( flags.trace_interlock ) {
						    cout << "set Interlock relation: type3 sending (server entry="  << (*server_entry_1)->name()
							 << ", client task= " << (*client_1)->name()<< "), between (server entry="  << (*server_entry_2)->name()
							 << ", client task= " << (*client_2)->name()<< "), IR_Relation( se1="
							 << se1<< ", k1= "<<k << ", se2=" << se2<< ", k2= "<<k2
							 << ") = " << aStation->IL_Relation(se2,  k2, se1, k) <<endl;
			
						}
					    }
					}
					//} // end if ( aStation->chainILRate( se2,k2)>0.)
		
				    }//end while ((*server_entry_2)= nextEntry2())
				}
			    }
			}//end while(aclient2=nextentry())
		    }
	
		}
	    }
	}
    }
    return *this;
}


void
Entity::setInterlockPr_upper( const MVASubmodel& submodel ) const
{
    Server * aStation = serverStation();

    const std::set<Task *>& clients = submodel.getClients();
    for ( std::set<Task *>::const_iterator client = clients.begin(); client != clients.end(); ++client ) {
	if ( (*client)->throughput() == 0.0 ) continue;

	/* there is not interlock via this client task to the server in this submodel. */
		
	aStation->setMixFlow(false);

	const ChainVector& aChain = (*client)->clientChains( submodel.number() );
	for ( unsigned ix = 1; ix <= aChain.size(); ++ix ) {
	    const unsigned k = aChain[ix];
	    if ( hasServerChain(k) ) {
		for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {

		    /* modify the server service time directly */
		    aStation->setChainILRate( 0, k, 0);
		    aStation->setChainILRate( (*entry)->index(), k, 0);
					
		    Call * aCall = (*entry)->getCall(k); // is unique??
		    if (aCall) {
			double IL_wait=(*entry)->getILWait(submodel.number());
			if (IL_wait>0.005 ){
			    Probability prob= IL_wait;
			    aStation->setInterlock( (*entry)->index(), k, prob );
			    aStation->setIntermediate(true);
			    if ( flags.trace_interlock ) {
				cout<<"from Client: "<<(*client)->name() <<" to server Entry: "<< (*entry)->name() ;
				cout<<" through a call( " << aCall->srcEntry()->name() << " , " << aCall->dstEntry()->name()<<")" <<endl;
				cout << "Along path: interlock prob(e="<<(*entry)->index()<<", k= "<<k <<")= " << prob <<endl;
			    }
			}
		    }
		}
	    }
	}
    }
}

/* -------------------------------------------------------------------- */

/*
 * Calculate and set think time.  Note that population returns the
 * maximum number of customers possible at a station.  It is used,
 * rather than copies, because some multi-servers may have more
 * threads specified than can possibly be active.
 */


void
Entity::setIdleTime( const double relax ) 
{
    double z;

    if ( utilization() >= population() ) {
	z = 0.0;
    } else if ( throughput() > 0.0 ) {
	z = ( population() - utilization() ) / throughput();
    } else {
	z = get_infinity();	/* INFINITY */
    }
    if ( flags.trace_idle_time ) {
	cout << "\nEntity::setIdleTime():" << name() << "   Idle Time:  " << z << endl;
    }
    under_relax( _thinkTime, z, relax );
}



/*
 * Check results for sanity.
 * Note -- make sure all errors are advisories, or no output will be generated.
 */

const Entity&
Entity::sanityCheck() const
{
    if ( !isInfinite() && utilization() > copies() * 1.05 ) {
	LQIO::solution_error( ADV_INVALID_UTILIZATION, utilization(), 
			      getDOM()->getTypeName(),
			      name().c_str(), copies() );
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
    for ( unsigned int e = 1; e <= nEntries(); ++e ) {
	if ( !isfinite( station->R(e,0) ) && station->V(e,0) != 0 && station->S(e,0) != 0 ) {
	    LQIO::solution_error( ERR_ARRIVAL_RATE, station->V(e,0), entryAt(e-1)->name().c_str(), station->mu()/station->S(e,0) );
	    rc = true;
	}
    }
    return rc;
}



Entity&
Entity::clear()
{
    serverStation()->clear();
    return *this;
}


/*
 * Initialize the service and visit parameters for a server.  Also set myChains to
 * all chains that visit this server.
 */

Entity&
Entity::initServerStation( MVASubmodel& submodel )
{
    Server * station = serverStation();
    if ( !station ) return *this;

    if ( !Pragma::init_variance_only() ) {
	computeVariance();
    }

    const ChainVector& aChain = serverChains();
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	const unsigned e = (*entry)->index();
	const double openArrivalRate = (*entry)->openArrivalRate();

	if ( openArrivalRate > 0.0 ) {
	    station->setVisits( e, 0, 1, openArrivalRate );	// Chain 0 reserved for open class.
	}

	/* -- Set service time for entries with visits only. -- */

	for ( unsigned ix = 1; ix <= aChain.size(); ++ix ) {
	    setServiceTime( (*entry), aChain[ix] );
	}

	/*
	 * Open arrivals and other open models use chain zero which are special
	 * and won't work in the above loop anyway)
	 */

	if ( isInOpenModel() ) {
	    setServiceTime( (*entry), 0 );
	}

    }
    /* Overtaking -- compute for MARKOV overtaking only. */

    const std::set<Task *>& clients = submodel.getClients();
    if ( markovOvertaking() ) {
	for_each( clients.begin(), clients.end(), Exec1<Task,Entity*>( &Task::computeOvertaking, this ) );
	for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	    if ( (*entry)->hasOvertaking() ) {
		const unsigned e = (*entry)->index();
		for ( unsigned p = 2; p <= (*entry)->maxPhase(); p++ ) {
		    (*entry)->setPrOvertaking( p, station->PrOT_e(e, p) );
		}
	    }
	}
    }

    /* Set interlock */

    if ( isInClosedModel() && Pragma::interlock() ) {
	initWeights( submodel );
	this->setInterlock( submodel );
    }

    setRealCustomers( submodel );

    //sending interlocks need to modify client service time!!

    std::for_each( clients.begin(), clients.end(), ConstExec2<Task,const MVASubmodel&,const Entity *>( &Task::modifyParentClientServiceTime, submodel, this ) );

    if ( hasSynchs() && !Pragma::threads(Pragma::NO_THREADS) ) {
	joinOverlapFactor( submodel );
    }
    return *this;
}


/*
 * Set the service time for my station.
 */

void
Entity::setServiceTime( const Entry * entry, unsigned k ) const
{
    Server * station = serverStation();
    const unsigned e = entry->index();

    if ( station->V( e, k ) == 0 ) return;

    for ( unsigned p = 1; p <= entry->maxPhase(); ++p ) {
	station->setService( e, k, p, entry->elapsedTimeForPhase(p) );

	if ( hasVariance() ) {
	    station->setVariance( e, k, p, entry->varianceForPhase(p) );
	}
    }
}



/*
 * Save server results.  Servers only occur in one submodel.
 */

Entity&
Entity::saveServerResults( const MVASubmodel& submodel, double relax )
{
    const Server * station = serverStation();

    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	const unsigned e = (*entry)->index();
	double lambda = 0.0;

	if ( isInOpenModel() && submodel.openModel ) {
	    lambda = submodel.openModel->entryThroughput( *station, e );		/* BUG_168 */
	}

	if ( isInClosedModel() && submodel.closedModel ) {
	    const double tput = submodel.closedModel->entryThroughput( *station, e );
	    if ( isfinite( tput ) ) {
		lambda += tput;
	    } else if ( tput < 0.0 ) {
		throw domain_error( "MVASubmodel::saveServerResults" );
	    } else {
		lambda = tput;
		break;
	    }
	}
	(*entry)->saveThroughput( lambda )
	    .saveOpenWait( station->R( e, 0 ) );
    }

    setIdleTime( relax );

    return *this;
}

/* -------------------------------------------------------------------- */
/* Funky Formatting functions for inline with <<.			*/
/* -------------------------------------------------------------------- */

/*
 * Print chains for this client.
 */

/* static */ ostream&
Entity::output_server_chains( ostream& output, const Entity& aServer ) 
{
    output << "Chains:" << aServer.serverChains() << endl;
    return output;
}

/* static */  ostream&
Entity::output_entity_info( ostream& output, const Entity& aServer )
{
    if ( aServer.serverStation() ) {
	output << "(" << aServer.serverStation()->typeStr() << ")";
    }
    return output;
}
