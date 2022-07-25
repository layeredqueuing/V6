/*  -*- c++ -*-
 * $Id: call.cc 15762 2022-07-25 16:16:52Z greg $
 *
 * Everything you wanted to know about a call to an entry, but were afraid to ask.
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
#include <algorithm>
#include <cmath>
#include <sstream>
#include <mva/fpgoop.h>
#include <mva/server.h>
#include "activity.h"
#include "call.h"
#include "entry.h"
#include "errmsg.h"
#include "flags.h"
#include "pragma.h"
#include "submodel.h"
#include "task.h"

/*----------------------------------------------------------------------*/
/*      Input processing.  Called from model.cc::prepareModel()         */
/*----------------------------------------------------------------------*/

void
Call::Create::operator()( const LQIO::DOM::Call * call )
{
    _src->add_call( _p, call );
}


std::set<Task *>& Call::add_client( std::set<Task *>& clients, const Call * call )
{
    if ( !call->hasForwarding() && call->srcTask()->isUsed() ) {
	clients.insert(const_cast<Task *>(call->srcTask()));
    }
    return clients;
}

std::set<Entity *>& Call::add_server( std::set<Entity *>& servers, const Call * call )
{
    servers.insert(const_cast<Entity *>(call->dstTask()));
    return servers;
}

/*----------------------------------------------------------------------*/
/*                            Static methods                            */
/*----------------------------------------------------------------------*/

/*
 * Ignore replicas because it's handled by fanOut.
 */

/* static */ double
Call::add_rendezvous( double sum, const Call * call )
{
    if ( Pragma::pan_replication() ) {
	return sum + call->rendezvous() * call->fanOut();
    } else {
	return sum + call->rendezvous();
    }
}

/*
 * Ignore replicas because it's handled by fanOut.
 */

/* static */ double
Call::add_rendezvous_no_fwd( double sum, const Call * call )
{
    if ( call->isForwardedCall() ) {
	return sum;
    } else {
	return Call::add_rendezvous( sum, call );
    }
}

/*----------------------------------------------------------------------*/
/*                            Generic  Calls                            */
/*----------------------------------------------------------------------*/

/*
 * Initialize and zero fields.   Reverse links are set here.  Forward
 * links are done by subclass.  Processor calls are linked specially.
 */

Call::Call( const Phase * fromPhase, const Entry * toEntry )
    : _dom(nullptr),
      _source(fromPhase),
      _destination(toEntry), 
      _chainNumber(0),
      _replica_number(1),
      _wait(0.0),
      _interlockedFlow(-1.0),
      _callWeight(0.0)
#if BUG_267
    , _queueWeight(1.0)
#endif
{
    if ( toEntry != nullptr ) {
	const_cast<Entry *>(_destination)->addDstCall( this );	/* Set reverse link	*/
    }
}


Call::Call( const Call& src, unsigned int replica )
    : _dom(src._dom),
      _source(nullptr),
      _destination(nullptr),
      _chainNumber(src._chainNumber),
      _replica_number(replica),
      _wait(0.0),
      _interlockedFlow(-1.0),
      _callWeight(0.0)
#if BUG_267
    , _queueWeight(1.0)
#endif
{
}

	   
/*
 * Clean up the mess.
 */

Call::~Call()
{
}


int
Call::operator==( const Call& item ) const
{
    return dstEntry() == item.dstEntry();
}


int
Call::operator!=( const Call& item ) const
{
    return (dstEntry() != item.dstEntry());
}


/*
 * Expand replicas (Not PAN_REPLICATION) from Call(src,dst) to
 * Call(src(src_replica),dst(dst_replica)).  There is no need to copy
 * src_replica=1 to dst_replica=1.  The real guts are in the copy
 * constructors.
 */

Call&
Call::expand()
{
    const unsigned int src_replicas = srcTask()->replicas();
    for ( unsigned int src_replica = 1; src_replica <= src_replicas; ++src_replica ) {
	const unsigned int dst_replicas = fanOut();
	for ( unsigned int dst_replica = 1; dst_replica <= dst_replicas; ++dst_replica ) {
	    if ( src_replica == 1 && dst_replica == 1 ) continue;	/* This exists already */
	    Call * call = clone( src_replica, (src_replica - 1) * dst_replicas + dst_replica );	/* 2 goes to 2, etc */
#if 0
	    std::cerr << "Call::expand(): " << srcName() << "." << src_replica << " -> " << call->dstName() << "." << call->dstTask()->getReplicaNumber() << std::endl;
#endif
	    const_cast<Entity *>(call->dstTask())->addTask( call->srcTask() );
	}
    }
    return *this;
}



bool
Call::check() const
{
    const int srcReplicas = srcTask()->replicas();
    const int dstReplicas = dstTask()->replicas();
    if ( srcReplicas > 1 || dstReplicas > 1 ) {
	const int fanOut = srcTask()->fanOut( dstTask() );
	const int fanIn  = dstTask()->fanIn( srcTask() );
	if ( fanIn == 0 || fanOut == 0 || srcReplicas * fanOut != dstReplicas * fanIn ) {
	    const std::string& srcName = srcTask()->name();
	    const std::string& dstName = dstTask()->name();
	    LQIO::runtime_error( LQIO::ERR_REPLICATION, 
				 fanOut, srcName.c_str(), srcReplicas,
				 fanIn,  dstName.c_str(), dstReplicas );
	    return false;
	}
    }
    return true;
}


/*
 * Common code for rendezvous, send-no-reply and forwarding.  Get and
 * check the value returned.  Additional checks for valid forwarding
 * probabilities, and a deterministic number of calls.
 */

double
Call::getDOMValue() const
{
    try {
	const double value = getDOM()->getCallMeanValue();
	if ( getDOM()->getCallType() != LQIO::DOM::Call::Type::FORWARD && getSource()->phaseTypeFlag() == LQIO::DOM::Phase::Type::DETERMINISTIC && value != std::rint( value ) ) {
	    throw std::domain_error( "invalid integer" );
	} else if ( getDOM()->getCallType() == LQIO::DOM::Call::Type::FORWARD && value > 1.0 ) {
	    throw std::domain_error( "invalid probability" );
	}
	return value;
    }
    catch ( const std::domain_error &e ) {
	getDOM()->throw_invalid_parameter( "mean value", e.what() );
    }
    return 0.;
}


unsigned
Call::fanIn() const
{
    return dstTask()->fanIn( srcTask() );
}


unsigned
Call::fanOut() const
{
    return srcTask()->fanOut( dstTask() );
}


/*
 * Return the name of the destination entry.
 */

const std::string&
Call::dstName() const
{
    return dstEntry()->name();
}



/*
 * Return the submodel number.
 */

unsigned
Call::submodel() const
{
    return dstTask()->submodel();
}


double
Call::getMaxCustomers()const
{
    if ( srcTask()->hasInfinitePopulation() ) {
	return getSource()->getMaxCustomers();
    } else {
	return std::min( srcTask()->population(), getSource()->getMaxCustomers() );
    }
}


/*
 * Return if this call matches type (for Entry::CallInfo)
 */

bool
Call::hasTypeForCallInfo( LQIO::DOM::Call::Type type ) const
{
    switch ( type ) {
    case LQIO::DOM::Call::Type::SEND_NO_REPLY: return hasSendNoReply();
    case LQIO::DOM::Call::Type::FORWARD: return isForwardedCall();
    case LQIO::DOM::Call::Type::RENDEZVOUS: return hasRendezvous() && !isForwardedCall();
    default: abort();
    }
    return false;
}
		

bool
Call::hasOvertaking() const
{
    return hasRendezvous() && dstEntry()->maxPhase() > 1;
}



/*
 * Return the total wait along this arc.  Take into account
 * replication.  This applies for both Pan and Bug 299 replication.
 */

double
Call::rendezvousDelay() const
{
    if ( !hasRendezvous() ) {
	return 0.0;
    } else if ( Pragma::pan_replication() ) {
	return rendezvous() * wait() * fanOut();
    } else {
	return rendezvous() * wait();
    }
}


#if PAN_REPLICATION
/*
 * Compute and save old rendezvous delay.		// REPL
 */

double
Call::rendezvousDelay( const unsigned k )
{
    if ( dstTask()->hasServerChain(k) ) {
	return rendezvous() * wait() * (fanOut() - 1);
    } else {
	return Call::rendezvousDelay();	// rendezvousDelay is already multiplied by fanOut.
    }
}
#endif


/*
 * Return the name of the source of this call.  Sources are always tasks.
 */

const std::string&
Call::srcName() const
{
    return getSource()->name();
}


/*
 * Return the source task of this call.  Sources are always tasks.
 */

const Task *
Call::srcTask() const
{
    return dynamic_cast<const Task *>(getSource()->owner());
}


double
Call::elapsedTime() const
{
    if (flags.trace_quorum) {
	std::cout <<"\nCall::elapsedTime(): call " << this->srcName() << " to " << dstEntry()->name() << std::endl;
    }

    if ( hasRendezvous() ) {
	return dstEntry()->elapsedTimeForPhase(1);
    } else {
	return 0.0;
    }
}



/*
 * Return time spent in the queue for call to this entry.
 */

double
Call::queueingTime() const
{
    if ( hasRendezvous() ) {
	if ( std::isinf( _wait ) ) return _wait;
	const double q = _wait - elapsedTime();
	if ( q <= 0.000001 ) {
	    return 0.0;
	} else if ( q * elapsedTime() > 0. && (q/elapsedTime()) <= 0.0001 ) {
	    return 0.0;
	} else {
	    return q;
	}
    } else if ( hasSendNoReply() ) {
	return _wait;
    } else {
	return 0.0;
    }
}


/*
 * Return the probability of dropping messages.
 */

double
Call::dropProbability() const
{
    double src_tput = getSource()->throughput();
    if ( !hasSendNoReply() || !std::isfinite(src_tput) || src_tput == 0.0) return 0.0;

    src_tput *= sendNoReply();
    const double dst_tput = dstEntry()->throughput();
    if ( src_tput > dst_tput ) {
	return 1.0 - dst_tput / src_tput;
    } else {
	return 0.0;
    }
}


const Call&
Call::insertDOMResults() const
{
    if ( getSource()->getReplicaNumber() != 1 ) return *this;		/* NOP */

    const_cast<LQIO::DOM::Call *>(getDOM())->setResultWaitingTime(queueingTime());
    const double drop_probability = dropProbability();
    if ( drop_probability > 0. ) {
	const_cast<LQIO::DOM::Call *>(getDOM())->setResultDropProbability( drop_probability );
    }
    return *this;
}


#if PAN_REPLICATION
/*
 * Return the adjustment factor for this call.  //REPL
 */

double
Call::nrFactor( const Submodel& aSubmodel, const unsigned k ) const
{
    const Entity * dst_task = dstTask();
    return aSubmodel.nrFactor( dst_task->serverStation(), index(), k ) * fanOut() * rendezvous();	// 5.20
}
#endif



/*
 * Return variance of this arc.
 */

double
Call::variance() const
{
    if ( hasRendezvous() ) {
	return dstEntry()->varianceForPhase(1) + square(queueingTime());
    } else {
	return 0.0;
    }
}



/*
 * Return the coefficient of variation for this particular call.
 */

double
Call::CV_sqr() const
{
#ifdef NOTDEF
    return dstEntry()->variance(1) / square(elapsedTime());
#endif
    return variance() / square(wait());
}



/*
 * Follow the call to its destination.
 */

const Call&
Call::followInterlock( Interlock::CollectTable& path ) const
{
    /* Mark current */

    if ( rendezvous() > 0.0 && !path.prune() ) {
	Interlock::CollectTable branch( path, path.calls() * rendezvous() );
	const_cast<Entry *>(dstEntry())->initInterlock( branch );
    }
    return *this;
}



/*
 * Set the visit ratio at the destinations station.  Called from
 * Task::initClientStation only.  The calling task is mapped to the
 * base replica if necessary.
 */

void
Call::setVisits( const unsigned k, const unsigned p, const double rate )
{
    const Entity * aServer = dstTask();
    if ( aServer->hasServerChain( k ) && hasRendezvous() && !srcTask()->hasInfinitePopulation() ) {
	Server * aStation = aServer->serverStation();
	const unsigned e = dstEntry()->index();
	aStation->addVisits( e, k, p, rendezvous() * rate );
    }
}


/*
 * Set the open arrival rate to the destination's station.Called from
 * Task::initClientStation only.  The calling task is mapped to the
 * base replica if necessary.
 */

void
Call::setLambda( const unsigned, const unsigned p, const double rate )
{
    Server * aStation = dstTask()->serverStation();
    const unsigned e = dstEntry()->index();
    if ( hasSendNoReply() ) {
	aStation->addVisits( e, 0, p, getSource()->throughput() * sendNoReply() );
    } else if ( hasRendezvous() && srcTask()->isOpenModelServer() && srcTask()->isInfinite() ) {
	aStation->addVisits( e, 0, p, getSource()->throughput() * rendezvous() );
    }
}



//tomari: set the chain number associated with this call.
void
Call::setChain( const unsigned k, const unsigned p, const double rate )
{
    const Entity * aServer = dstTask();
    if ( aServer->hasServerChain( k )  ){

	_chainNumber = k;

	if ( flags.trace_replication ) {
	    std::cout <<"\nCall::setChain, k=" << k<< "  " ;
	    std::cout <<",call from "<< srcName() << " To " << dstName()<< std::endl;
	}
    }
}




/*
 * Clear waiting time.
 */

void
Call::clearWait( const unsigned k, const unsigned p, const double )
{
    _wait = 0.0;
}

/*
 * Clear interlocked queueing time.
 */

void
Call::clearILWait( const unsigned k, const unsigned p, const double )
{
    const_cast<Phase *> (getSource())->setILWait (submodel(), 0.0 );
}


/*
 * Get the waiting time for this call from the mva submodel.  A call
 * can potentially orginate from multiple chains, so add them all up.
 * (Call clearWait first.)
 */

void
Call::saveOpen( const unsigned, const unsigned p, const double )
{
    const unsigned e = dstEntry()->index();
    const Server * aStation = dstTask()->serverStation();

    if ( aStation->V( e, 0, p ) > 0.0 ) {
	_wait = aStation->W[e][0][p];
    }
}



/*
 * Get the waiting time for this call from the mva submodel.  A call
 * can potentially orginate from multiple chains, so add them all up.
 * (Call clearWait first.).  This may havve to be changed if the
 * result varies by chain.  Priorities perhaps?
 */

void
Call::saveWait( const unsigned k, const unsigned p, const double )
{
    const Entity * aServer = dstTask();
    const unsigned e = dstEntry()->index();
    const Server * aStation = aServer->serverStation();

    if ( aStation->V( e, k, p ) > 0.0 ) {
	_wait = aStation->W[e][k][p];
    }
}

/************************************************************************/
/*			      Interlock					*/
/************************************************************************/

bool
Call::isRealCustomer( const MVASubmodel& submodel, const Entity * server, unsigned int k ) const
{
    return submodel.number() == this->submodel() && dstTask() == server && hasChain(k);
}



double
Call::getQueueLength( ) const
{
    return const_cast<Phase *> (getSource())->throughput() * rendezvous() * _wait;
}



double
Call::addRealCustomers( const MVASubmodel& submodel, const Entity * server, unsigned int k ) const
{
    if ( !isRealCustomer( submodel, server, k ) ) return 0.;

    const Task * client = srcTask();
    double customers = 0.;
    if ( client->isReferenceTask() ) {
	customers = client->population();
    } else {
	customers = getSource()->utilization();
    }
    customers *= server->fanIn(client);		/* Replication */

    Server * station = server->serverStation();
    station->addRealCustomers( dstEntry()->index(), k, customers );

    return customers;
}



double
Call::computeWeight( const MVASubmodel& submodel )
{
    const double n_calls = getDOM() ? getDOM()->getCallMeanValue() : 0.0;
    if ( srcTask()->isReferenceTask() ) {
	_callWeight = srcTask()->population() * n_calls;
    } else {
	_callWeight = srcTask()->throughput() * n_calls;
    }
    return _callWeight;
}



/* 
 * >0.0: interlocked flow
 * =0.0: is along the Path of an interlocked flow.
 * <0.0: is an non interlocked flow. 
 */

Call&
Call::setInterlockedFlow( const MVASubmodel& submodel )
{
    if ( this->submodel() != submodel.number() ) return *this;
      
    double num = dstTask()->num_sources( srcTask(), dstEntry() );
    double p = 0.0;

    if ( num > 0. ) {
	//p =num-(srcTask()->population());
	p = srcTask()->nCustomers() - srcTask()->population();
	if ( p > 0.0 ) {
	    _interlockedFlow = Probability( 1.0 / p ); 
	} else {
	    _interlockedFlow = 1.0;  // if multiserver?
	}
    }

    if ( flags.trace_interlock ) {
	if ( isProcessorCall() ) {
	    std::cout<<"aProcessorCall(srcTask="<<srcTask()->name()
		     <<", dstentry="<<dstEntry()->name()<<"),	aCall->getInterlocked="
		     << getInterlockedFlow()<<std::endl;
	} else {
	    std::cout<<"aCall(srcTask="<<srcTask()->name()
		     <<", dstentry="<<dstEntry()->name()<<"), num_source="<<num<<", p="<<p<<std::endl;
	    std::cout<<"aCall(srcTask="<<srcTask()->name()
		     <<", dstentry="<<dstEntry()->name()<<"),	aCall->getInterlocked="
		     << getInterlockedFlow()<<std::endl;
	} 
    }
    return *this;
}



#if BUG_267
void
Call::saveQueueWeight( const unsigned k, const unsigned p, const double )
{
    const Entity * server = dstTask();
    const unsigned e = dstEntry()->index();
    const Server * station = server->serverStation();

    if ( station->V( e, k, p ) > 0.0 ) {
	_queueWeight = station->getQueueWeight(e,k,p);
    
	if ( flags.trace_interlock ) {
	    std::cout << "Call::saveQueueWeight(): Call from " 
		 << srcName() << " to " << dstEntry()->name() << ": queue lenth is: " 
		 << _queueWeight << std::endl;
	}
    }
}
#endif



void
Call::saveILWait( const unsigned k, const unsigned p, const double )
{
    const Entity * aServer = dstTask();
    const unsigned e = dstEntry()->index();
    const Server * aStation = aServer->serverStation();

    double eps=elapsedTime();
    double diff;

    if ( aStation->nILRate[e][k] > 0 ) {
	if ( aServer->isProcessor() ) {
	    diff = _wait * (1- aStation->nILRate[e][k]);  //nILRate[e][k] is calculated differently in the case of FCFS or PS
	} else {
	    diff = (_wait-eps) * (1- aStation->nILRate[e][k]);
	}
    } else {
	if ( std::isinf( _wait ) && std::isinf( eps ) ) {
	    diff = 0;
	} else {
	    diff = (_wait -eps);
	}
    }

    if ( aStation->IL_Relation( e, k, 1, 0 ) == 4 ) return;

    // get rid of small numbers
    if ((diff<0.0005) && (!(eps && (diff/eps)>0.01) )) diff = 0.;
	
    diff = (diff>0.0) ? diff* getDOM()->getCallMeanValue() :0.;//?

    if ( isInterlocked() ) {
	if ( flags.trace_interlock ) {
	    std::cout << "Call::saveILWait(): Call( " << srcName() << " , " << dstEntry()->name()<<")";
	    std::cout << "has interlocked wait "<<diff << std::endl;
	    std::cout << "aStation->nILRate[e][k]="<<aStation->nILRate[e][k]
		<<", CallMeanValue="<<getDOM()->getCallMeanValue()<< std::endl;
	    std::cout << "call->elapsedTime() ="<<eps <<", _wait="<<_wait <<",diff="<<diff<< std::endl;
	}

	const Probability IL_Pr(getInterlockedFlow());
	const_cast<Phase *>(getSource())->addILWait (submodel(), diff * IL_Pr );

    } else if ( isAlongILPath() ) {
	
	// this queueing time is adjusted!
	if ( diff && dstEntry()->getILWait(submodel()) ) {
	    const_cast<Phase *>(getSource())->addILWait(submodel(), diff ) ;
	} else {
	    const_cast<Phase *>(getSource())->addILWait(submodel(), dstEntry()->getILWait(submodel() )* getDOM()->getCallMeanValue()) ;
	}
    }
}


double
Call::interlockPr() const
{
    if ( flags.trace_quorum ) {
	std::cout <<"\nCall::elapsedTime(): call " << this->srcName() << " to " << dstEntry()->name() << std::endl;
    }
    if ( srcTask()->hasInfinitePopulation() ) return 0.0;

    const double et = elapsedTime();
    if ( et <= 0. ) return 0.;

#warning Bugs here?  division?  Hoist common expression.
    // the intermediate server is a multiserver;
    if ( dstEntry()->owner()->population() > 1 ) {
	const double r = getMaxCustomers() / (dstEntry()->owner()->population());
	if ( r >= 1.0 ) return 0.;		/* ??? */
	else return 1. / (getMaxCustomers() * dstEntry()->owner()->population());	/* ??? */
    }
 
    const double queue = queueingTime() / et;
    const double upperbound = std::min( 1.0 / getMaxCustomers(), 1.0 );
    if ( queue < 0.1 ) {
	if ( flags.trace_interlock ) {
	    std::cout <<"queueingTime()="<<queueingTime()<<", et="<<elapsedTime()<<",queueingTime()/et= "<<queue <<", 1/N="<<1.0/getMaxCustomers()<< std::endl;
	}
	return std::max( upperbound, queue );
    }

    const int d = getMaxCustomers() - 1;
    if ( flags.trace_interlock ) {
	std::cout << "getMaxCustomers()="<<getMaxCustomers()<<",  dstEntry()->owner()->population()="<< dstEntry()->owner()->population()<<",d="<<d<< std::endl;
    }
    if ( et && d > 0 ) {
	double t =  1. - ((queueingTime() * getqueueWeight()) / (et * d));
	if ( flags.trace_interlock ) {
	    std::cout <<"queueingTime()="<<queueingTime()<<", et="<<elapsedTime()<<";getqueueWeight()="<<getqueueWeight()<<",t="<<t<< std::endl;
	}
	const double ql = srcEntry()->getILQueueLength();		// sourcing entry.
	if ( ql > 0.0 && t < ql ) {
	    t /= ql;
	}
	return std::min( t, upperbound );
    }
    return 0.0;
}



double
Call::add_interlock_pr::operator()( double sum, const Call * call ) const
{
    if ( !call->isAlongILPath() ) return sum;
    const Task * srcTask = call->srcTask();
    const Entity * dstTask = call->dstTask();
 
    if ( srcTask->population() > 1 ) {
	if ( srcTask->population() <= dstTask->population() ) {
	    sum += 1.0 / square(srcTask->population());
	} else {
	    sum += call->interlockPr();
	}
    } else if ( dynamic_cast<const Task *>(dstTask) ) { //how about multiple source??????
	sum = _server->prInterlock( *dynamic_cast<const Task*>(dstTask) );
    }
#if 1
    if ( flags.trace_interlock ) {	// will need submodel number for X
	std::cout << "getInterlockPr::in submodel " << "X" << ", Call( " << call->srcName() << " , " << call->dstEntry()->name()
	     << ") has interlock prob "<< sum <<" to client Entry "<< call->dstEntry()->name() << std::endl;
    }
#endif
    return sum;
}


double
Call::add_wait_to::operator()( double sum, const Call * call ) const
{
    const Task * dstTask = dynamic_cast<const Task *>(call->dstTask());
    if ( dstTask != nullptr && _clients.find(const_cast<Task *>(dstTask)) != _clients.end() ) {
	return sum + call->wait();
    }
    return sum;
}


/*----------------------------------------------------------------------*/
/*                              Phase Calls                             */
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*                              Phase Calls                             */
/*----------------------------------------------------------------------*/

/*
 * Call from a phase to a task.
 */

PhaseCall::PhaseCall( const Phase * fromPhase, const Entry * toEntry )
    : Call( fromPhase, toEntry ),
      FromEntry( fromPhase->entry() )
{
    const_cast<Phase *>(getSource())->addSrcCall( this );
}


/*
 * Deep copy.  Set FromEntry to the replica.
 */

PhaseCall::PhaseCall( const PhaseCall& src, unsigned int src_replica, unsigned int dst_replica )
    : Call( src, dst_replica ),		/* link to DOM.	*/
      FromEntry( Entry::find( src.srcEntry()->name(), src_replica ) )
{
    /* Link to source replica */
    const Phase& src_phase = srcEntry()->getPhase(src.getSource()->getPhaseNumber());
    setSource( &src_phase );    /* Swap _source to the replica !!! */
    const_cast<Phase&>(src_phase).addSrcCall( this );

    /* Link to destination replica */
    if ( src.dstEntry() != nullptr ) {
	const unsigned int replica = static_cast<unsigned>(std::ceil( static_cast<double>(dst_replica) / static_cast<double>(src.fanIn()) ));
	Entry * dst = Entry::find( src.dstEntry()->name(), replica );
	if ( dst == nullptr ) {
	    std::ostringstream err;
	    err << "PhaseCall::PhaseCall: Can't find entry " << src.dstEntry()->name() << "." << replica;
	    throw std::runtime_error( err.str() );
	}
	setDestination( dst );
	dst->addDstCall( this );	/* Set reverse link */
    }
#if 0
    std::cerr << "PhaseCall::PhaseCall() from: " << getSource()->name() << "(" << srcEntry()->getReplicaNumber() << ")"
	      << " to: " << dstEntry()->name() << "(" << dstEntry()->getReplicaNumber() << ")" << std::endl;
#endif
}


bool
PhaseCall::isCalledBy( const Entry * entry ) const
{
    return srcEntry() == entry;
}



/*
 * Initialize waiting time.
 */

Call&
PhaseCall::initWait()
{
    setWait( elapsedTime() );			/* Initialize arc wait. 	*/
    return *this;
}

/*----------------------------------------------------------------------*/
/*                           Forwarded Calls                            */
/*----------------------------------------------------------------------*/

/*
 * call added to transform forwarding to standard model.
 */

ForwardedCall::ForwardedCall( const Phase * fromPhase, const Entry * toEntry, const Call * fwdCall )
    : Call( fromPhase, toEntry ), PhaseCall( fromPhase, toEntry ), _fwdCall( fwdCall )
{
}

bool
ForwardedCall::check() const
{
    const Task * srcTask = dynamic_cast<const Task *>(_fwdCall->getSource()->owner());
    const int srcReplicas = srcTask->replicas();
    const int dstReplicas = dstTask()->replicas();
    if ( srcReplicas > 1 || dstReplicas > 1 ) {
	const int fanOut = srcTask->fanOut( dstTask() );
	const int fanIn  = dstTask()->fanIn( srcTask );
	if ( fanIn == 0 || fanOut == 0 || srcReplicas * fanOut != dstReplicas * fanIn ) {
	    const std::string& srcName = srcTask->name();
	    const std::string& dstName = dstTask()->name();
	    LQIO::runtime_error( LQIO::ERR_REPLICATION, 
				 fanOut, srcName.c_str(), srcReplicas,
				 fanIn,  dstName.c_str(), dstReplicas );
	    return false;
	}
    }
    return true;
}

const std::string&
ForwardedCall::srcName() const
{
    return _fwdCall->srcName();
}


const ForwardedCall&
ForwardedCall::insertDOMResults() const
{
    if ( getSource()->getReplicaNumber() != 1 ) return *this;		/* NOP */

    LQIO::DOM::Call* fwdDOM = const_cast<LQIO::DOM::Call *>(_fwdCall->getDOM());		/* Proxy */
    fwdDOM->setResultWaitingTime(queueingTime());
    return *this;
}

/*----------------------------------------------------------------------*/
/*                           Processor Calls                            */
/*----------------------------------------------------------------------*/

/*
 * Call to processor entry.
 */

ProcessorCall::ProcessorCall( const Phase * fromPhase, const Entry * toEntry )
    : Call( fromPhase, toEntry )
{
}



/*
 * Set up waiting time to processors.
 */

Call&
ProcessorCall::initWait()
{
    setWait( dstEntry()->serviceTimeForPhase(1) );		/* Initialize arc wait. 	*/
    return *this;
}

/*----------------------------------------------------------------------*/
/*                            Activity Calls                            */
/*----------------------------------------------------------------------*/

/* 
 * This is a little more complicated.
 */
const Entry *
FromActivity::srcEntry() const
{
    std::deque<const Activity *> activityStack;
    std::deque<const AndOrForkActivityList *> forkStack;
    std::set<const AndOrForkActivityList *> branchSet;
    Activity::Backtrack data( activityStack, forkStack, branchSet );
    dynamic_cast<const Activity *>(getSource())->backtrack( data );		/* look for start activity */
    return data.getStartActivity()->entry();	/* return the entry */
}

/*----------------------------------------------------------------------*/
/*                            Activity Calls                            */
/*----------------------------------------------------------------------*/

/*
 * Call added for activities.
 */

ActivityCall::ActivityCall( const Activity * fromActivity, const Entry * toEntry )
    : Call( fromActivity, toEntry ), FromActivity()
{
    const_cast<Phase *>(getSource())->addSrcCall( this );
}


ActivityCall::ActivityCall( const ActivityCall& src, unsigned int src_replica, unsigned int dst_replica )
    : Call( src, dst_replica ), FromActivity()
{
    const Task * task = Task::find( src.srcTask()->name(), src_replica );
    const Activity * src_activity = task->findActivity( src.getSource()->name() );
    setSource( src_activity );
    const_cast<Activity*>(src_activity)->addSrcCall( this );
    
    /* Link to destination replica */
    if ( src.dstEntry() != nullptr ) {
	Entry * dst = Entry::find( src.dstEntry()->name(), static_cast<unsigned>(std::ceil( static_cast<double>(dst_replica) / static_cast<double>(src.fanIn())) ) );
	setDestination( dst );
	dst->addDstCall( this );	/* Set reverse link */
    }
#if 0
    std::cerr << "ActivityCall::ActivityCall() from: " << getSource()->name() << "(" << srcEntry()->getReplicaNumber() << ")"
	      << " to: " << dstEntry()->name() << "(" << dstEntry()->getReplicaNumber() << ")" << std::endl;
#endif
}


bool
ActivityCall::isCalledBy( const Entry * entry ) const
{
    return entry->hasStartActivity() && srcTask() == entry->owner();
}

/*
 * Initialize waiting time.
 */

Call&
ActivityCall::initWait()
{
    setWait( elapsedTime() );			/* Initialize arc wait. 	*/
    return *this;
}

/*----------------------------------------------------------------------*/
/*                       Activity Forwarded Calls                       */
/*----------------------------------------------------------------------*/

/*
 * call added to transform forwarding to standard model.
 */

ActivityForwardedCall::ActivityForwardedCall( const Activity * fromActivity, const Entry * toEntry )
    : Call( fromActivity, toEntry ), ActivityCall( fromActivity, toEntry )
{
}

/*----------------------------------------------------------------------*/
/*                        Phase Processor Calls                         */
/*----------------------------------------------------------------------*/

PhaseProcessorCall::PhaseProcessorCall( const Phase * fromPhase, const Entry * toEntry ) 
    : Call( fromPhase, toEntry ), ProcessorCall( fromPhase, toEntry ), FromEntry( fromPhase->entry() )
{
}


bool
PhaseProcessorCall::isCalledBy( const Entry * entry ) const
{
    return srcEntry() == entry;
}

/*----------------------------------------------------------------------*/
/*                      Activity Processor Calls                        */
/*----------------------------------------------------------------------*/

ActivityProcessorCall::ActivityProcessorCall( const Activity * fromActivity, const Entry * toEntry )
    : Call( fromActivity, toEntry ), ProcessorCall( fromActivity, toEntry ), FromActivity()
{
}



bool
ActivityProcessorCall::isCalledBy( const Entry * entry ) const
{
    return entry->hasStartActivity() && srcTask() == entry->owner();
}

/*----------------------------------------------------------------------*/
/*                          CallStack Functions                         */
/*----------------------------------------------------------------------*/

/*
 * We are looking for matching tasks for calls.
 */

bool 
Call::Find::operator()( const Call * call ) const
{
    if ( call == nullptr || call->getDOM() == nullptr ) return false;

    if ( call->hasSendNoReply() ) _broken = true;		/* Cycle broken - async call */
    if ( call->dstTask() == _call->dstTask() ) {
	if ( call->hasRendezvous() && _call->hasRendezvous() && !_broken ) {
	    throw call_cycle();		/* Dead lock */
	} else if ( call->dstEntry() == _call->dstEntry() && _direct_path ) {
	    throw call_cycle();		/* Livelock */
	} else {
	    return true;
	}
    }
    return false;
}

/*
 * We may skip back over forwarded calls when computing the size.
 */

unsigned
Call::stack::depth() const	
{
    return std::count_if( begin(), end(), Predicate<Call>( &Call::hasNoForwarding ) );
}
