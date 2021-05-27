/*  -*- c++ -*-
 * $Id: phase.cc 14704 2021-05-27 12:20:22Z greg $
 *
 * Everything you wanted to know about an phase, but were afraid to ask.
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
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cstdlib>
#include <sstream>
#include <lqio/input.h>
#include <lqio/error.h>
#include <lqio/dom_histogram.h>
#include <mva/fpgoop.h>
#include <mva/prob.h>
#include <mva/vector.h>
#include "call.h"
#include "entity.h"
#include "entry.h"
#include "errmsg.h"
#include "gamma.h"
#include "group.h"
#include "lqns.h"
#include "model.h"
#include "option.h"
#include "phase.h"
#include "pragma.h"
#include "processor.h"
#include "submodel.h"
#include "task.h"
#include "variance.h"



/* ---------------------- Overloaded Operators ------------------------ */

/*
 * Allocate array space for submodels
 */

NullPhase&
NullPhase::configure( const unsigned n )
{
    _wait.resize( n );
    _interlockedWait.resize( n );
    return *this;
}


NullPhase&
NullPhase::setDOM(LQIO::DOM::Phase* dom)
{
    _dom = dom;
    return *this;
}

NullPhase&
NullPhase::setServiceTime( const double t )
{
    if (getDOM() != nullptr) {
	abort();
    } else {
	_serviceTime = t;
    }
	
    return *this;
}


NullPhase&
NullPhase::addServiceTime( const double t )
{
    if (getDOM() != nullptr) {
	abort();
    } else {
	_serviceTime += t;
    }
	
    return *this;
}

double
NullPhase::serviceTime() const
{
    if ( getDOM() == nullptr ) return _serviceTime;
    try {
	return getDOM()->getServiceTimeValue();
    }
    catch ( const std::domain_error& e ) {
	solution_error( LQIO::ERR_INVALID_PARAMETER, "service time", getDOM()->getTypeName(), name().c_str(), e.what() );
	throw_bad_parameter();
    }
    return 0.0;
}

double
NullPhase::thinkTime() const
{
    try {
	return getDOM()->getThinkTimeValue();
    }
    catch ( const std::domain_error& e ) {
	solution_error( LQIO::ERR_INVALID_PARAMETER, "think time", getDOM()->getTypeName(), name().c_str(), e.what() );
	throw_bad_parameter();
    }
    return 0.;
}

double
NullPhase::CV_sqr() const
{
    if ( getDOM() ) {
	try {
	    return getDOM()->getCoeffOfVariationSquaredValue();
	}
	catch ( const std::domain_error& e ) {
	    solution_error( LQIO::ERR_INVALID_PARAMETER, "CVsqr", getDOM()->getTypeName(), name().c_str(), e.what() );
	    throw_bad_parameter();
	}
    }
    return 1.;
}


double
NullPhase::computeCV_sqr() const
{
    double w = waitExcept(0);
    if ( w ) {
	return variance() / square(w);
    } else {
	return 0.0;
    }
}


/*
 * Return per-call waiting to processor (includes service time.)
 * EXCEPT for waiting in submodel p.
 */

double
NullPhase::waitExcept( const unsigned submodel ) const
{
    const unsigned n = _wait.size();

    double sum = 0.0;
    for ( unsigned i = 1; i <= n; ++i ) {
	if ( i == submodel ) continue;
	sum += _wait[i];
    }

    // two-phase quorum semantics. Because of distribution fitting, the
    // difference in the mean before and after fitting might be
    // negative.

    return std::max( 0.0, sum );
}


void
NullPhase::setILWait( unsigned int submodel, double il_wait )
{
    _interlockedWait[submodel] = il_wait;
    if ( flags.trace_interlock ) {
	//cout<<getDOM()->getName()<<"->_interlockedWait["<<submodel<<"] :";
	//cout<< " is reset to "<<il_wait <<endl;
	
    }
}


void
NullPhase::addILWait( unsigned int submodel, double il_wait )
{
    _interlockedWait[submodel] += il_wait;
    if ( flags.trace_interlock ) {
	std::cout<<getDOM()->getName()<<"->_interlockedWait["<<submodel<<"] :";
	std::cout<< " is added by "<<il_wait <<" = "<<_interlockedWait[submodel]<<std::endl;
    }
}


double
NullPhase::getILWait( unsigned int submodel ) const
{
    return std::accumulate( _interlockedWait.begin() + submodel + 1, _interlockedWait.end(), 0.0 );
#if 0
    double sum=0.0;

    for(unsigned i=submodel+1;i<=_interlockedWait.size();i++){
	sum+= _interlockedWait[i];
    }
    return sum;
#endif
}


/*
 * Compute a histogram based on a gamma distribution with a mean m and variance v.
 */

void
NullPhase::insertDOMHistogram( LQIO::DOM::Histogram * histogram, const double m, const double v )
{
    const unsigned int n_bins = histogram->getBins();		/* +2 for under and over-flow */
    const double bin_size = histogram->getBinSize();

    if ( v > 0 ) {
	/* Compute gamma stuff. */
	const double b = v / m;
	const double k = (m*m) / v;
	Gamma_Distribution dist( k, b );

	/* Convert the Cumulative Distribution into a discrete probability distribution */
	double x = histogram->getMin();
	double prev = 0.0;
	for ( unsigned int i = 0; i <= n_bins; ++i, x += bin_size ) {	
	    const double temp = dist.getCDF(x);
	    histogram->setBinMeanVariance( i, temp - prev );
	    prev = temp;
	}
	histogram->setBinMeanVariance( n_bins + 1, 1.0 - prev );	/* overflow */
    } else {
	/* deterministic */
	histogram->setBinMeanVariance( histogram->getBinIndex( m ), 1.0 );
    }
}

/*----------------------------------------------------------------------*/
/*                                 Phase                                */
/*----------------------------------------------------------------------*/
		
/*
 * Constructor.
 */

Phase::Phase( const std::string& name )
    : NullPhase( name ),
      _entry(nullptr),
      _callList(),
      _devices(),
      _prOvertaking(0.),
      _cfs_delay(0.0),
      _cfs_delay_upperbound(0.0),
      _cfs_delay_lowerbound(0.0)
{
}



/*
 * Free up resources.
 */

Phase::~Phase()
{
    for ( std::vector<DeviceInfo *>::const_iterator device = _devices.begin(); device != _devices.end(); ++device ) {
	delete *device;
    }
    /* Release forward links */
    for ( std::set<Call *>::const_iterator call = callList().begin(); call != callList().end(); ++call ) {
	delete *call;
    }
}



/*
 * Recursively find all children and grand children from `this'.  As
 * we descend down, we bump the depth.  If our path's cross, we have a
 * loop and abort.
 */

unsigned
Phase::findChildren( Call::stack& callStack, const bool directPath ) const
{
    const unsigned depth = callStack.depth();
    unsigned max_depth = depth;

    for ( std::set<Call *>::const_iterator call = callList().begin(); call != callList().end(); ++call ) {
	if ( (*call)->isForwardedCall() ) continue;

	const Entity * dstTask = (*call)->dstTask();
	try {

	    /*
	     * Chase the call if there is no loop, and if following the path results in pushing
	     * tasks down.  Open class requests can loop up.  Always check the stack because of the
	     * short-circuit test with directPath.  Always (for forwarding)	
	     */

	    if ( (std::none_of( callStack.begin(), callStack.end(), Call::Find(*call, directPath) ) && depth >= dstTask->submodel()) || directPath ) {
		callStack.push_back( (*call) );
		if ( (*call)->hasForwarding() && directPath ) {
		    addForwardingRendezvous( callStack );
		}
		max_depth = std::max( dstTask->findChildren( callStack, directPath ), max_depth );
		callStack.pop_back();

	    }
	}
	catch ( const Call::call_cycle& error ) {
	    if ( directPath && !Pragma::allowCycles() ) {
		std::string msg = std::accumulate( callStack.rbegin(), callStack.rend(), callStack.back()->dstName(), &Call::stack::fold );
		LQIO::solution_error( LQIO::ERR_CYCLE_IN_CALL_GRAPH, msg.c_str() );
	    }
	}
    }
    for ( std::vector<DeviceInfo *>::const_iterator device = devices().begin(); device != devices().end(); ++device ) {
	Call * call = (*device)->call();
	callStack.push_back( call );
	const Entity * child = call->dstTask();
	max_depth = std::max( child->findChildren( callStack, directPath ), max_depth );
	callStack.pop_back();
    }
    return max_depth;
}




/*
 * We have to add a psuedo forwarding arc.  Search back for on the
 * call stack for a rendezvous.  On drawing, we may have to do some
 * fancy labelling.  We may have to have more than one proxy.
 */

Phase const&
Phase::addForwardingRendezvous( Call::stack& callStack ) const
{
    double rate     = 1.0;
    for ( Call::stack::reverse_iterator call = callStack.rbegin(); call != callStack.rend(); ++call ) {
	if ( (*call)->hasRendezvous() ) {
	    rate *= (*call)->rendezvous();
	    const_cast<Phase *>((*call)->getSource())->forwardedRendezvous( callStack.back(), rate );
	    break;
	} else if ( (*call)->hasSendNoReply() ) {
	    break;
	} else if ( (*call)->hasForwarding() ) {
	    rate *= (*call)->forward();
	} else {
	    abort();
	}
    }
    return *this;
}


#if PAN_REPLICATION
/*
 * Grow _surrogateDelay array as neccesary.  Initialize to zero.  Used
 * by Newton Raphson step.
 */

Phase&
Phase::initReplication( const unsigned maxSize )
{
    _surrogateDelay.resize( maxSize );
    return *this;
}
#endif



/*
 * Initialize waiting time from lower level servers.
 */

Phase&
Phase::initWait()
{
    std::for_each( callList().begin(), callList().end(), Exec<Call>( &Call::initWait ) );
    std::for_each( devices().begin(), devices().end(), &DeviceInfo::initWait );
    return *this;
}



/*
 * Initialize variance based on the CV_sqr() and service time.  This is only done
 * for device entries.
 */

Phase&
Phase::initVariance()
{
    _variance = CV_sqr() * square( serviceTime() );
    return *this;
}


#if PAN_REPLICATION
/*
 * Clear replication variables.
 */

Phase&
Phase::resetReplication()
{
    _surrogateDelay = 0.;
    return *this;
}
#endif


/*
 * Make sure deterministic phases are correct.
 */

bool
Phase::check() const
{
    if ( !isPresent() ) return true;

    /* Service time for the entry? */
    if ( serviceTime() == 0 ) {
	if ( isActivity() ) {
	    LQIO::solution_error( LQIO::WRN_NO_SERVICE_TIME_FOR, "Task", owner()->name().c_str(), getDOM()->getTypeName(),  name().c_str() );
	} else {
	    LQIO::solution_error( LQIO::WRN_NO_SERVICE_TIME_FOR, "Entry", entry()->name().c_str(), getDOM()->getTypeName(),  name().c_str() );
	}
    }

    for_each( callList().begin(), callList().end(), Predicate<Call>( &Call::check ) );

    if ( phaseTypeFlag() == LQIO::DOM::Phase::STOCHASTIC && CV_sqr() != 1.0 ) {
	if ( isActivity() ) {			/* c, phase_flag are incompatible  */
	    LQIO::solution_error( WRN_COEFFICIENT_OF_VARIATION, "Task", owner()->name().c_str(), getDOM()->getTypeName(), name().c_str() );
	} else {
	    LQIO::solution_error( WRN_COEFFICIENT_OF_VARIATION, "Entry", entry()->name().c_str(), getDOM()->getTypeName(), name().c_str() );
	}
    }
    return true;
}



const Entity *
Phase::owner() const
{
    return _entry ? _entry->owner() : nullptr;
}


double
Phase::getMaxCustomers() const
{
    return entry()->getMaxCustomers();		/* Activity will override */
}

/*
 * Initialize variance.
 */

bool
Phase::hasVariance() const
{
    return CV_sqr() != 1.0 || sumOfRendezvous() > 0;		/* Bug 234 */
}


/*
 * Aggregate whatever aFunc into the entry at the top of stack.
 * Follow the activitylist and continue.
 */

void
Phase::callsPerform( const CallExec& exec ) const
{
    std::for_each( callList().begin(), callList().end(), exec );
    for ( std::vector<DeviceInfo *>::const_iterator device = devices().begin(); device != devices().end(); ++device ) {
	exec( (*device)->call() );
    }
}




/*
 * Locate the destination anEntry in the list of destinations.
 */

Call *
Phase::findCall( const Entry * anEntry, const queryFunc aFunc ) const
{
    std::set<Call *>::const_iterator call = std::find_if( callList().begin(), callList().end(), Call::find_call( anEntry, aFunc ) );
    return call != callList().end() ? *call : nullptr;
}



/*
 * Locate the destination anEntry in the list of destinations.
 * The call must be a proxy for a forward.
 */

Call *
Phase::findFwdCall( const Entry * anEntry ) const
{
    std::set<Call *>::const_iterator call = std::find_if( callList().begin(), callList().end(), Call::find_fwd_call( anEntry ) );
    return call != callList().end() ? *call : nullptr;
}



/*
 * Return index of destination anEntry.  If it is not found in the list
 * add it.
 */

Call *
Phase::findOrAddCall( const Entry * anEntry, const queryFunc aFunc  )
{
    std::set<Call *>::const_iterator call = std::find_if( callList().begin(), callList().end(), Call::find_call( anEntry, aFunc ) );
    if ( call == callList().end() ) {
	return new PhaseCall( this, anEntry );
    } else {
	return *call;
    }
}


/*
 * Return index of destination anEntry.  If it is not found in the list
 * add it.
 */

Call *
Phase::findOrAddFwdCall( const Entry * anEntry, const Call * fwdCall )
{
    std::set<Call *>::const_iterator call = std::find_if( callList().begin(), callList().end(), Call::find_fwd_call( anEntry ) );
    if ( call == callList().end() ) {
	return new ForwardedCall( this, anEntry, fwdCall );
    } else {
	return *call;
    }
}


/*
 * Return the number of calls to aTask from this phase.
 */

double
Phase::rendezvous( const Entity * task ) const
{
    return std::accumulate( callList().begin(), callList().end(), 0.0, Call::add_rendezvous_to( task ) );
}



/*
 * Set the value of calls to entry `toEntry', `phase'.  Retotal
 * total.
 */

Phase&
Phase::rendezvous( Entry * toEntry, const LQIO::DOM::Call* callDOM )
{
    if ( callDOM != nullptr && toEntry->setIsCalledBy( Entry::RequestType::RENDEZVOUS ) ) {
	Task * client = const_cast<Task *>(dynamic_cast<const Task *>(owner()));
	if ( client != nullptr ) {
	    client->isPureServer( false );
	}
		
	Call * aCall = findOrAddCall( toEntry, &Call::hasRendezvousOrNone  );
	aCall->rendezvous( callDOM );

	Task * server = const_cast<Task *>(dynamic_cast<const Task *>(toEntry->owner()));
	if ( server != nullptr && client != nullptr ) {
	    server->addTask(client);
	}
    }

    return *this;
}



/*
 * Reference the value of calls to entry.  The entry must
 * already exist.  If not, returns 0.
 */

double
Phase::rendezvous( const Entry * anEntry ) const
{
    const Call * call = findCall( anEntry, &Call::hasRendezvous );
    return call ? call->rendezvous() : 0.0;
}



/*
 * Return the sum of all calls from the receiver during it's phase `p'.
 */

double
Phase::sumOfRendezvous() const
{
    return std::accumulate( callList().begin(), callList().end(), 0.0, Call::add_rendezvous );
}



/*
 * Set the value of send-no-reply calls to entry `toEntry', `phase'.
 * Retotal.
 */

Phase&
Phase::sendNoReply( Entry * toEntry, const LQIO::DOM::Call* callDOM )
{
    if ( callDOM != nullptr && toEntry->setIsCalledBy( Entry::RequestType::SEND_NO_REPLY ) ) {
	Task * client = const_cast<Task *>(dynamic_cast<const Task *>(owner()));
	if ( client != nullptr ) {
	    client->isPureServer( false );
	}

	Call * aCall = findOrAddCall( toEntry, &Call::hasSendNoReplyOrNone );
	aCall->sendNoReply( callDOM );

	Task * server = const_cast<Task *>(dynamic_cast<const Task *>(toEntry->owner()));
	if ( server != nullptr && client != nullptr ) {
	    server->addTask(client);
	}
    }
	
    return *this;
}



/*
 * Reference the value of calls to entry.  The entry must
 * already exist.  If not returns 0.
 */

double
Phase::sendNoReply( const Entry * anEntry ) const
{
    const Call * call = findCall( anEntry, &Call::hasSendNoReply  );
    return call ? call->sendNoReply() : 0.0;
}



/*
 * Create a forwarded rendezvous type arc.
 */

Phase&
Phase::forwardedRendezvous( const Call * fwdCall, const double value )
{
    const Entry * toEntry = fwdCall->dstEntry();
    if ( value > 0.0 && const_cast<Entry *>(toEntry)->setIsCalledBy( Entry::RequestType::RENDEZVOUS ) ) {
	if ( owner() ) {
	    const_cast<Entity *>(owner())->isPureServer( false );
	}
	Call * aCall = findOrAddFwdCall( toEntry, fwdCall );
	LQIO::DOM::Phase* aDOM = getDOM();
	LQIO::DOM::Call* rendezvousCall = new LQIO::DOM::Call( aDOM->getDocument(), LQIO::DOM::Call::Type::RENDEZVOUS,
							       getDOM(), toEntry->getDOM(), 
							       new LQIO::DOM::ConstantExternalVariable(value));
	aCall->rendezvous(rendezvousCall);
    }
    return *this;
}



/*
 * Store forwarding probability in call list.
 */

Phase&
Phase::forward( Entry * toEntry, const LQIO::DOM::Call* callDOM )
{
    if ( callDOM != nullptr && toEntry->setIsCalledBy( Entry::RequestType::RENDEZVOUS ) ) {
	Task * client = const_cast<Task *>(dynamic_cast<const Task *>(owner()));
	if ( client != nullptr ) {
	    client->isPureServer( false );
	}

	Call * aCall = findOrAddCall( toEntry, &Call::hasForwardingOrNone );
	aCall->forward( callDOM );

	Task * server = const_cast<Task *>(dynamic_cast<const Task *>(toEntry->owner()));
	if ( server != nullptr && client != nullptr ) {
	    server->addTask(client);
	}
    }

    return *this;
}



/*
 * Retrieve forwarding probability to entry.
 */

double
Phase::forward( const Entry * toEntry ) const
{
    const Call * call = findCall( toEntry, &Call::hasForwarding );
    return call ? call->forward() : 0.0;
}


/*
 * Return the sum of all calls from the receiver.  Forwarded calls are
 * not counted because they are pseudo calls from this entry.  Note
 * that there is ALWAYS one slice.
 */

double
Phase::numberOfSlices() const
{
    return std::accumulate( callList().begin(), callList().end(), 1.0, Call::add_rendezvous_no_fwd );
}

/* ------------------------------------------------------------------------ */


/*
 * Return throughut for phase.
 */

double
Phase::throughput() const
{
    return _entry ? _entry->throughput() : 0.0;
}


/*
 * Return utilization for phase.
 */

double
Phase::utilization() const
{
    const double f = throughput();
    if ( std::isfinite( f ) && f > 0.0 ) {
	const double t = elapsedTime();
	return f * t;
    } else {
	return 0.0;
    }
}


Phase::DeviceInfo *
Phase::getProcessor() const
{
    for ( std::vector<DeviceInfo *>::const_iterator device = devices().begin(); device != devices().end(); ++device ) {
	if ( (*device)->isHost() ) return *device;
    }
    return nullptr;
}


ProcessorCall *
Phase::processorCall() const
{
    DeviceInfo * processor = getProcessor();
    if ( processor ) return processor->call();
    return nullptr;
}



DeviceEntry *
Phase::processorEntry() const
{
    DeviceInfo * processor = getProcessor();
    if ( processor ) return processor->entry();
    return nullptr;
}



/*
 * Return number of calls to the processor.
 */

double
Phase::processorCalls() const
{
    return processorCall() ? processorCall()->rendezvous() * processorCall()->fanOut() : 0.0;
}


/*
 * Return time in queue to processor.
 */

double
Phase::queueingTime() const
{
    return processorCall() ? processorCall()->rendezvous() * processorCall()->queueingTime() : 0.0;
}


/*
 * Return per-call waiting to processor (includes service time.)
 */

double
Phase::processorWait() const
{
    return processorCall() ? processorCall()->wait() : 0.0;
}



/*
 * Return variance at processor.
 */

double
Phase::processorVariance() const
{
    return processorCall() ? processorCall()->variance() : 0.0;
}



/*
 * Return utilization for phase.
 */

double
Phase::processorUtilization() const
{
    if ( processorCall() ) {
	const Processor * aProc = dynamic_cast<const Processor *>(processorCall()->dstTask());
	return throughput() * serviceTime()  / (processorCall()->fanOut() * aProc->rate() );
    } else {
	return 0.0;		/* No processor == no utilization */
    }
}


/*
 * Compute waiting (and it we have pruned the processor, add it back.
 */

double
Phase::waitExcept( const unsigned submodel ) const
{
    double wait = NullPhase::waitExcept( submodel );
    if ( owner()->getProcessor() && !owner()->getProcessor()->isInteresting() ) {
	wait += serviceTime();		/* Add my processor's service time */
    }
    return wait;
}



#if PAN_REPLICATION
/*
 * Return waiting time.  Normally, we exclude all of chain k, but with
 * replication, we have to include replicas-1 wait for chain k too.
 */


double
Phase::waitExceptChain( const unsigned submodel, const unsigned k )
{
	
    if ( k <= _surrogateDelay.size()) {
	return _surrogateDelay[k] + waitExcept( submodel );
    } else {
	return waitExcept( submodel );
    }
}
#endif



/* DPS */
Phase::DeviceInfo *
Phase::getCFSDelayServer() const
{
    for ( std::vector<DeviceInfo *>::const_iterator device = devices().begin(); device != devices().end(); ++device ) {
	if ( (*device)->isCFSDelay() ) return *device;
    }
    return nullptr;
}



ProcessorCall *
Phase::getCFSCall() const
{
    DeviceInfo * delay = getCFSDelayServer();
    if ( delay ) return delay->call();
    return nullptr;
}



DeviceEntry *
Phase::getCFSEntry() const
{
    DeviceInfo * delay = getCFSDelayServer();
    if ( delay ) return delay->entry();
    return nullptr;
}



Phase&
Phase::computeCFSDelay( double ratio1, double ratio2 )
{
    double newCFSDelay = 0.;
    const double oldCFSDelay = getCFSCall()->wait();
    
    if ( flags.trace_cfs ) {
	std::cout << "Phase " << owner()->name() << "-" << name() << ": task ratio=" << ratio1 << ", group ratio=" << ratio2;
    }

    /* Move bounds*/
    if ( ratio1 == 0. ){
	// reset all varibles related to groups;
	_cfs_delay_upperbound = -1.;
	_cfs_delay_lowerbound = 0.;

    } else if ( ratio1 > 0. && oldCFSDelay > 0.0001 ) {
	if ( ratio2 > 0. ) {	    //it is the case of :util >share
	    _cfs_delay_lowerbound = std::max( oldCFSDelay, _cfs_delay_lowerbound );
	} else {
	    _cfs_delay_upperbound = oldCFSDelay;
	}
    }

    if ( 0. < _cfs_delay_upperbound && _cfs_delay_upperbound <= _cfs_delay_lowerbound ) {
	if ( flags.trace_cfs ) {
	    std::cout << ", No bounds!" << std::endl;
	}
	return *this;
    }

    if ( ratio1 >= 1.0 ) { //this group has only one task;
	newCFSDelay = cfsThinkTime( dynamic_cast<const Task *>(owner())->getGroup()->getRatio2() );
    } else if ( ratio1 > 0. ) {
	newCFSDelay = cfsThinkTime( ratio2 );
    } else {
	newCFSDelay = 0.0;
    }

    newCFSDelay = std::max( newCFSDelay, 0. );
    DeviceEntry * cfs_delay_entry = getCFSEntry();
    const double oldThinkTime = cfs_delay_entry->serviceTimeForPhase(1);
//    under_relax( newCFSDelay, oldThinkTime, 1. );
    if ( oldThinkTime != newCFSDelay  ) {
	cfs_delay_entry->setServiceTime(newCFSDelay);
	cfs_delay_entry->initVariance();	// Reset variance.
	cfs_delay_entry->initWait();	// Reset values in wait[].
	
	/* Recompute dynamic values. */

	getCFSCall()->setWait( newCFSDelay );
    }

    if ( flags.trace_cfs ) {
	std::cout << ", lower bound=" << _cfs_delay_lowerbound << ", CFS delay=" << newCFSDelay << ", upper bound=" << _cfs_delay_upperbound << std::endl;
    }
    _cfs_delay = newCFSDelay;
    return *this;
}


double
Phase::cfsThinkTime( double groupRatio )
{

    const double oldCFSDelay = getCFSCall()->wait();
    double newCFSDelay = oldCFSDelay + groupRatio * elapsedTime();
    if ( groupRatio < 0. && newCFSDelay < 0. ) {
	newCFSDelay = oldCFSDelay / 2.0;
    }
    if ( _cfs_delay_upperbound > 0. && ( newCFSDelay < _cfs_delay_lowerbound || _cfs_delay_upperbound < newCFSDelay ) ) {
	newCFSDelay = (_cfs_delay_upperbound + _cfs_delay_lowerbound) / 2.0;
    }
    return newCFSDelay;
}



#if 0
void
Phase::cfsRecalculateDynamicValues(double ratio1, double newthinktime)
{	
    if ( !_processorEntry ) return;
	
    const bool phase_is_present = serviceTime() > 0 || isPseudo();
    const double nCalls = phase_is_present ? numberOfSlices() : 0.0;
    const double newSliceTime = phase_is_present ? serviceTime() / nCalls : 0.0;
    const double oldSliceTime = _processorEntry->serviceTimeForPhase(1);
    if ( oldSliceTime != newSliceTime || flags.full_reinitialize ) {
	_processorEntry->setServiceTime(newSliceTime)
	    .setCV_sqr(CV_sqr())
	    .initVariance()		// Reset variance.
	    .initWait();		// Reset values in wait[].
		
	const LQIO::DOM::Call* callDOM = _processorCall->getDOM();
	const LQIO::DOM::Document * aDocument = getDOM()->getDocument();
	if ( callDOM ) delete callDOM;
	_processorCall->rendezvous( new LQIO::DOM::Call(aDocument, LQIO::DOM::Call::QUASI_RENDEZVOUS,
							 nullptr, _processorEntry->getDOM(), new LQIO::DOM::ConstantExternalVariable(nCalls)) );
	/* Recompute dynamic values. */
		
	const double newWait = oldSliceTime > 0.0 ? _processorCall->wait() * newSliceTime / oldSliceTime : newSliceTime;
	_processorCall->setWait( newWait );		/* extrapolate value */
    }

    const double newCFSDelay = newthinktime;
    const double oldThinkTime = _thinkEntry->serviceTimeForPhase(1);
    if ( oldThinkTime != newCFSDelay  ) {
	_thinkEntry->setServiceTime(newCFSDelay)
	    .initVariance()		// Reset variance.
	    .initWait();		// Reset values in wait[].
		
	/* Recompute dynamic values. */
	//print out
	
	cout<<"++++++++Phase("<<name()<<")-> think entry("<<_thinkEntry->name()<<"):, group ratio is "<<ratio1<<endl;
	cout<<"old think time: "<<oldThinkTime <<", new think time: "<<newCFSDelay<<".++++++++"<<endl;
	_thinkCall->setWait( newCFSDelay );
    }
}
#endif


#if PAN_REPLICATION
double
Phase::nrFactor( const Call * aCall, const Submodel& aSubmodel ) const
{
    unsigned submodel = aSubmodel.number();
    double nr_factor = 0.0;
    unsigned count = 0;
	
    Task * aTask = const_cast<Task *>(dynamic_cast<const Task *>(owner()));
    const ChainVector& aChain = aTask->clientChains( submodel );
#warning should this be +=?
    for ( unsigned ix = 1; ix <= aChain.size(); ++ix, ++count ) {
	nr_factor = aCall->nrFactor( aSubmodel, aChain[ix] );
    }

    if ( count ) {
	return nr_factor / count;
    } else {
	return 0;
    }
}
#endif



/*
 * Calculate total wait for a particular submodel and save.  Return
 * the difference between this pass and the previous.
 */

Phase&
Phase::updateWait( const Submodel& aSubmodel, const double relax )
{
    const unsigned submodel = aSubmodel.number();
    const double oldWait    = _wait[submodel];

    /* Sum up waits to all other tasks and devices in this submodel */

    const double newWait = std::accumulate( devices().begin(), devices().end(),
					    std::accumulate( callList().begin(), callList().end(), 0.0, Call::add_wait( submodel ) ),
					    DeviceInfo::add_wait( submodel ) );

    /* Now update waiting values */

    under_relax( _wait[submodel], newWait, relax );

    if ( oldWait && flags.trace_delta_wait ) {
	std::cout << "Phase::updateWait(" << submodel << "," << relax << ") for " << name() << std::endl;
	std::cout << "        Sum of wait=" << newWait << ", _wait[" << submodel << "]=" << _wait[submodel] << std::endl;
    }

    return *this;
}



#if PAN_REPLICATION
double
Phase::getReplicationProcWait( unsigned int submodel, const double relax )
{
    double newWait   = 0.0;

    if ( processorCall() && processorCall()->submodel() == submodel ) {

	int k = processorCall()->getChain();
	//procWait += waitExceptChain( submodel, k );	
	if ( processorCall()->dstTask()->hasServerChain(k) ) {
	    // newWait +=  _surrogateDelay[k];

	    if (flags.trace_quorum) {
		std::cout << "\nPhase::getReplicationProcWait(): Call " << this->name() << ", Submodel=" <<  processorCall()->submodel()
		     << ", _surrogateDelay[" <<k<<"]="<<_surrogateDelay[k] << std::endl;
		fflush(stdout);
	    }
	}

	newWait += processorCall()->wait();// * processorCall()->fanOut();
    }

    /* Now update waiting values */
    // under_relax( _wait[submodel], newWait, relax );

    return newWait;
}
#endif



#if PAN_REPLICATION
/* 
 * Sum up waits to all other tasks in this submodel 
 */

double
Phase::getReplicationTaskWait( unsigned int submodel, const double relax )
{
    return std::accumulate( callList().begin(), callList().end(), 0., add_using<Call>( &Call::wait ) );
}
#endif



#if PAN_REPLICATION
double 
Phase::getReplicationRendezvous( unsigned int submodel, const double relax ) //tomari : quorum
{
    return std::accumulate( callList().begin(), callList().end(), 0.0, Call::add_replicated_rendezvous( submodel ) );
}
#endif


double
Phase::getProcWait( unsigned int submodel, const double relax ) //tomari : quorum
{
    double newWait   = 0.0;

    if ( processorCall() && processorCall()->submodel() == submodel ) {
		
	newWait += processorCall()->rendezvousDelay();

	if (flags.trace_quorum) {
	    std::cout << "\nPhase::getProcWait(): Call " << this->name() << ", Submodel=" <<  processorCall()->submodel()
		 << ", newWait="<<newWait << std::endl;
	    std::cout.flush();
	}
			
    }

    return newWait;
}

//tomari quorum: Used in a closed form formula to estimate the thread service time.
//The closed form formula was originally developed with an assumption
//that an activity calls only one server. The current code is modified to
//average the service times if an activity calls more than one server.
double
Phase::getTaskWait( unsigned int submodel, const double relax ) //tomari : quorum
{
    const double totalRendezvous = std::accumulate( callList().begin(), callList().end(), 0.0, Call::add_submodel_rendezvous( submodel ) );
    return std::accumulate( callList().begin(), callList().end(), 0.0, Call::add_weighted_wait( submodel, totalRendezvous ) );
}


double
Phase::getRendezvous( unsigned int submodel, const double relax ) //tomari : quorum
{
    return std::accumulate( callList().begin(), callList().end(), 0.0, Call::add_submodel_rendezvous( submodel ) );
}



#if PAN_REPLICATION
/* 
 * Calculate the surrogatedelay of a chain k of a
 *specific thread....tomari
 */

double
Phase::updateWaitReplication( const Submodel& aSubmodel )
{
    double delta = 0.0;

    // Over all chains k that belong to the owner thread
    // of this phase...

    if ( flags.trace_replication ) {
	std::cout <<"\nPhase::updateWaitReplication()........Current phase is " << entry()->name() << ":" << name();
	std::cout <<" ..............." << std::endl;
    }
    const Task * ownerTask = dynamic_cast<const Task *>(owner());
    const ChainVector& aChain = ownerTask->clientChains(aSubmodel.number());

    for ( unsigned ix = 1; ix <= aChain.size(); ++ix ) {
	const unsigned k = aChain[ix];

	//std::cout <<"\n***Start processing master chain k = " << k << " *****" << std::endl;

	bool proc_mod = false;
	double nr_factor = 0.0;
	double newWait = 0.0;

	unsigned chainThreadIx = ownerTask->threadIndex( aSubmodel.number(), k );

	for ( std::set<Call *>::const_iterator call = callList().begin(); call != callList().end(); ++call ) {
	    if ( (*call)->submodel() != aSubmodel.number() ) continue;

	    if (chainThreadIx != ownerTask->threadIndex( aSubmodel.number(), (*call)->getChain() )  ) {
		continue;
	    }
	
	    const double temp = (*call)->rendezvousDelay( k );
	    if ( (*call)->dstTask()->hasServerChain(k) ) {
		nr_factor = (*call)->nrFactor( aSubmodel, k );
	    }
	    if ( flags.trace_replication ) {
		std::cout << "\nCallChainNum=" << (*call)->getChain() << ",Src=" << (*call)->srcName() << ",Dst=" << (*call)->dstName() << ",Wait=" << temp << std::endl;
	    }
	    newWait += temp;

	    //Take note if there are zero visits to task.

	    if ( (*call)->rendezvousDelay() > 0 ) {
		proc_mod = true;
	    }
	}

	if ( processorCall() && proc_mod && processorCall()->submodel() == aSubmodel.number() ) {

	    if (chainThreadIx == ownerTask->threadIndex( aSubmodel.number(), processorCall()->getChain() ) ) {
		double temp=  processorCall()->rendezvousDelay( k );
		newWait += temp;
		if ( flags.trace_replication ) {
		    std::cout << "\n Processor Call: CallChainNum="<<processorCall()->getChain() <<  ",Src=" << processorCall()->srcName() << ",Dst= " << processorCall()->dstName()
			 << ",Wait=" << temp << std::endl;
		}
		if ( processorCall()->dstTask()->hasServerChain(k) ) {
		    nr_factor = processorCall()->nrFactor( aSubmodel, k );
		}
	    }
	}
	
	//REP NEWTON-RAPHSON (if NR selected)

	if ( nr_factor != 0.0 ) {
	    newWait = (newWait + _surrogateDelay[k] * nr_factor) / (1.0 + nr_factor);
	} else {
	    newWait = 0;
	}

	delta = std::max( delta, square( (_surrogateDelay[k] - newWait) * throughput() ) );
	under_relax( _surrogateDelay[k], newWait, 1.0 );
	
	if ( flags.trace_replication ) {
	    std::cout << std::endl << "SurrogateDelay of current master chain " << k << " =" << _surrogateDelay[k] << std::endl
		 << name() << ": waitExceptChain(submodel=" << aSubmodel.number() << ",k=" << k << ") = " << newWait
		 << ", nr_factor = " << nr_factor << std::endl
		 << ", waitExcept(submodel=" << aSubmodel.number() << ") = " << waitExcept( aSubmodel.number() ) << std::endl;
	}
    }
    return delta;
}
#endif

/* -------------------------------------------------------------------- */
/*			      Interlock					*/
/* -------------------------------------------------------------------- */


/*
 * Go through the call list, looking for deterministic
 * rendezvous/async calls, to activity entries then follow them to a
 * join.
 */

const Phase&
Phase::followInterlock( Interlock::CollectTable& path ) const
{
    for_each( callList().begin(), callList().end(), follow_interlock( path ) );
    for_each( devices().begin(), devices().end(), follow_interlock( path ) );
    return *this;
}


void
Phase::follow_interlock::operator()( const Call * call ) const
{
    call->followInterlock( _path );
}

void
Phase::follow_interlock::operator()( const DeviceInfo* device ) const
{
    device->call()->followInterlock( _path );
}



/*
 * Recursively search from this entry to any entry on myServer.
 * When we pop back up the call stack we add all calling tasks
 * for each arc which calls myServer.  The task adder
 * will ignore duplicates.
 *
 * Note: we can't short circuit the search because there may be interlocking
 * on multiple branches.
 */

bool
Phase::getInterlockedTasks( Interlock::CollectTasks& path ) const
{
    return std::accumulate( callList().begin(), callList().end(),
			    std::accumulate( devices().begin(), devices().end(), false, get_interlocked_tasks( path ) ),
			    get_interlocked_tasks( path ) );
}


bool
Phase::get_interlocked_tasks::operator()( bool found, Call * call ) const
{
    const Entry * entry = call->dstEntry();
    if ( !_path.has_entry( entry ) && entry->getInterlockedTasks( _path )) {
	call->setInterlockedFlow(0.0);
	return true;
    } else {
	return found;
    }
}

bool
Phase::get_interlocked_tasks::operator()( bool found, const DeviceInfo* device ) const
{
    const Entry * entry = device->call()->dstEntry();
    return (!_path.has_entry( entry ) && entry->getInterlockedTasks( _path )) || found;			/* don't short circuit! */
}


Phase&
Phase::setInterlockedFlow( const MVASubmodel& submodel )
{
    std::for_each( callList().begin(), callList().end(), Exec1<Call,const MVASubmodel&>( &Call::setInterlockedFlow, submodel ) );
    if ( processorCall() ) {
	processorCall()->setInterlockedFlow( submodel );
    }
    return *this;
}



/*
 * Calculate total interlocked wait for a particular submodel and save.
 * Returnthe difference between this pass and the previous.
 */

Phase&
Phase::updateInterlockedWait( const Submodel& aSubmodel, const double relax )
{
    const unsigned submodel = aSubmodel.number();
    const double oldWait    = _interlockedWait[submodel];

    /* Sum up waits to all other tasks in this submodel */

    double newILWait = std::accumulate( callList().begin(), callList().end(), 0.0, Call::add_interlocked_wait( submodel ) );

    /* Tack on processor delay if necessary */

    if ( processorCall() && processorCall()->submodel() == submodel && processorCall()->isInterlocked() ) {
	newILWait += processorCall()->rendezvousDelay();
    }

    /* Now update waiting values */

    under_relax( _interlockedWait[submodel], newILWait, relax );

    if ( oldWait && flags.trace_interlock ) {
	std::cout << "Phase::updateILWait(" << submodel << "," << relax << ") for " << name() << std::endl;
	std::cout << "        Sum of interlocked wait=" << newILWait << ", _interlockedWait[" << submodel << "]=" << _interlockedWait[submodel];
    }

    return *this;
}



/*
 * Add all utilization from this phase to _entry.  See Entry::fractionUtilizationTo (then Entity::setInterlock).
 */

double
Phase::add_utilization_to::operator()( double sum, const Phase& phase ) const
{
    if ( !phase.isPresent() ) return sum;
    if ( _entry->isProcessorEntry() ) {
	if ( phase.processorCall()->dstEntry() == _entry ) return phase.utilization();
    }
    const std::set<Call *>& calls = phase.callList();
    for ( std::set<Call *>::const_iterator call = calls.begin(); call != calls.end(); ++call ) {
	if ( (*call)->dstEntry() == _entry ) {
	    sum += phase.utilization();
	}
    }
    return sum;
}

/*
 * Get my replica number
 */

unsigned int
Phase::getReplicaNumber() const
{
    return owner()->getReplicaNumber();
}



const Phase&
Phase::insertDOMResults() const
{
    if ( getReplicaNumber() != 1 ) return *this;		/* NOP */

    getDOM()->setResultServiceTime(elapsedTime())
	.setResultVarianceServiceTime(variance())
	.setResultUtilization(utilization())
	.setResultProcessorWaiting(queueingTime() + _cfs_delay);

    if ( getDOM()->hasHistogram() || getDOM()->hasMaxServiceTimeExceeded() ) {
	insertDOMHistogram( const_cast<LQIO::DOM::Histogram *>(getDOM()->getHistogram()), elapsedTime(), variance() );
    }

    for ( std::set<Call *>::const_iterator call = callList().begin(); call != callList().end(); ++call ) {
	if ( !(*call)->hasForwarding() ) {
	    (*call)->insertDOMResults();		/* Forwarded calls are done by their proxys (ForwardCall) */
	}
    }
    return *this;
}



/*
 * Recalculate the service time and visits to the processor.  Only go
 * through the hoops if the value changes.
 */

Phase&
Phase::recalculateDynamicValues()
{	
    for_each( devices().begin(), devices().end(), Exec<Phase::DeviceInfo>( &Phase::DeviceInfo::recalculateDynamicValues ) );
    return *this;
}

/*----------------------------------------------------------------------*/
/*                       Variance Calculation                           */
/*----------------------------------------------------------------------*/

/*
 * Compute variance.
 */

double
Phase::computeVariance()
{
    if ( !std::isfinite( elapsedTime() ) ) {
	_variance = elapsedTime();
    } else switch ( Pragma::variance() ) {

	case Pragma::Variance::MOL:
	    if ( phaseTypeFlag() == LQIO::DOM::Phase::Type::STOCHASTIC ) {
		_variance =  mol_phase();
		break;
	    } else {
		_variance =  deterministic_phase();
	    }
	    break;

	case Pragma::Variance::STOCHASTIC:
	    if ( phaseTypeFlag() == LQIO::DOM::Phase::Type::STOCHASTIC ) {
		_variance =  stochastic_phase();
		break;
	    } else {
		_variance =  deterministic_phase();
	    }
	    break;
		
	case Pragma::Variance::DEFAULT:
	    if ( phaseTypeFlag() == LQIO::DOM::Phase::Type::STOCHASTIC ) {
		_variance =  stochastic_phase();
	    } else {
		_variance =  deterministic_phase();
	    }
	    break;

	default:
	    _variance =  square( elapsedTime() );
	    break;
	}

    return _variance;
}



/* --------------------Random Sum Calculation-----------------------
 *
 * Classic variance calculation based on a random sum:
 *
 * A phase is a sum of a slice, plus a random sum for each call (a
 * random number of calls to that entry). Each element in one of these
 * random sums is itself the call blocking delay plus a processor
 * slice.
 */

double
Phase::stochastic_phase() const
{
    /* set up the running sums with the first slice, and save slice values*/
    double proc_wait = processorWait();
    double proc_var = processorVariance();

    Positive sumOfVariance = proc_var;

    /* over all calls made in this phase */
    for ( std::set<Call *>::const_iterator call = callList().begin(); call != callList().end(); ++call ) {
	if ( !(*call)->hasRendezvous() ) continue;

	/* Formula for a random sum: add terms due to one call */

	/* first extract the properties of the delay for one call instance*/
	const double blocking_mean = (*call)->wait(); //includes service ph 1
	// + Positive( (*call)->dstEntry()->elapsedTime(1) );
	/* mean delay for one of these calls */
	if ( !std::isfinite( blocking_mean ) ) {
	    return blocking_mean;
	}

	const double blocking_var = (*call)->variance();		/* BUG_655 */
	if ( !std::isfinite( blocking_var ) ) {
	    return blocking_var;
	}
	// this includes variance due to service
	// + square (Positive( (*call)->dstEntry()->elapsedTime(1) )) * Positive( (*call)->dstEntry()->computeCV_sqr(1) );
	/*  variance of one of these calls */

	/* then add up; the sum accounts for no of calls and fanout  */
	//accumulate mean sum
	const unsigned int fan_out = (*call)->fanOut();
	double calls_var = (*call)->rendezvous() * fan_out * ((*call)->rendezvous() * fan_out + 1 );  //var of no of calls if geometric
	sumOfVariance += (*call)->rendezvous() * fan_out * (blocking_var + proc_var)
	    + calls_var * square(proc_wait + blocking_mean ); //accumulate variance sum
    }

    return sumOfVariance;
}




/* ------------------------- Method Of Layers -------------------------
 *
 * Variance calculation from pages 124-134 of:
 *     author =  "Rolia, Jerome Alexander",
 *     title =   "Predicting the Performance of Software Systems",
 *     school =  "Univerisity of Toronto",
 *     year =    1992,
 *     address = "Toronto, Ontario, Canada.  M5S 1A1",
 *     month =   jan,
 *     note =    "Also as: CSRI Technical Report CSRI-260",
 */

/*
 * Compute the variance for a phase of an entry.  We have to chase
 * down all calls that the entry's phase makes and generate a
 * series parallel model.
 */


double
Phase::mol_phase() const
{
    const double sumOfRNV = sumOfRendezvous();
    Positive variance;
    if ( sumOfRNV == 0 ) {
	variance = processorVariance();
	return variance;
    }
	
    /*
     * Construct a "Series Parallel" model over all entries that I call...
     */
    SeriesParallel SP_Model;

    for ( std::set<Call *>::const_iterator call = callList().begin(); call != callList().end(); ++call ) {
	if ( !(*call)->hasRendezvous() ) continue;


	Probability prVisit = (*call)->rendezvous() / sumOfRNV;
	for ( unsigned i = 1; i <= (*call)->fanOut(); ++i ) {
#ifdef NOTDEF
	    SP_Model.addStage( prVisit, (*call)->wait(), Positive( (*call)->CV_sqr() ) );
#else
	    /* Use variance of phase, not of phase+arc */
	    SP_Model.addStage( prVisit, (*call)->wait(),
			       Positive( (*call)->dstEntry()->computeCV_sqr() ) );		/* !!! Phase 1 only in V5 !!! */
#endif
	}
    }

    /*
     * Now compute the variance for the phase of the entry itself.
     * Don't forget to include the visit to the processor (+ 1).
     */

    const Probability q   = 1.0 / (sumOfRNV + 1);
    const Probability v   = 1.0 - q;
    const double r_iter   = processorWait() + SP_Model.S();		// Pg 132 says: v * r_ncs.
    const double var_iter = processorVariance() + v * SP_Model.variance()
	+ v * ( 1.0 - v ) * square(SP_Model.S());
    variance = var_iter / q + ( 1.0 - q ) / square( q ) * square( r_iter );

    if ( Options::Debug::variance() ) {
	std::cout << name() << " MOL var: q=" << q << ", r_iter=" << r_iter << ", Proc Var=" << processorVariance() << ", SP Var=" << SP_Model.variance() << ", var_iter=" << var_iter << ", variance=" << variance << std::endl;
    }
    return variance;
}


/*
 * Variance calculation for deterministic phases (See eqn 12 and 13
 * from srvn6 paper.)  Code is from srvn6 solver.  There are some
 * discrepencies between the two.
 */

double
Phase::deterministic_phase() const
{
    Positive variance = 0;

    for ( std::set<Call *>::const_iterator call = callList().begin(); call != callList().end(); ++call ) {
	const double var = (*call)->variance();
	if ( std::isfinite( var ) ) {
	    variance += (*call)->fanOut() * (*call)->rendezvous() * var;
	}
    }

    /* Add processor term (from old code.) */

    variance += numberOfSlices() * processorVariance();		/* BUG 447 */

    return variance;
}



/*
 * Variance calculation for deterministic phases (See eqn 12 and 13
 * from srvn6 paper.)  Code is from srvn6 solver.  There are some
 * discrepencies between the two.
 */

double
Phase::random_phase() const
{
    Positive var_x = 0;
    Positive sum_x = 0;
    Positive mean_n = sumOfRendezvous();

    if ( mean_n == 0.0 ) {
	return processorVariance();
    }

    for ( std::set<Call *>::const_iterator call = callList().begin(); call != callList().end(); ++call ) {
	if ( !(*call)->hasRendezvous() ) continue;

#ifdef NOTDEF
	var_x += (*call)->fanOut() * (*call)->rendezvous() * ((*call)->dstEntry()->computeCV_sqr(1) * square((*call)->wait()));
#else
	var_x += (*call)->fanOut() * (*call)->rendezvous() * (*call)->variance();
#endif
	sum_x += (*call)->rendezvousDelay();
    }

    var_x  += processorCalls() * processorVariance();
    sum_x  += processorCalls() * processorWait();

    return square(mean_n) * square( sum_x / mean_n ) + var_x;
}



/*
 * Expand replicas of all of the calls from this phase (Not PAN_REPLICATION).
 */

Phase&
Phase::expandCalls()
{
//    std::for_each( callList().begin(), callList().end(), Exec<Call>( &Call::expand ) );
    const std::set<Call *> calls = callList();	/* Create a copy as the list is changed */
    for ( auto& call : calls ) {
	call->expand();
    }
    return *this;
}



/*
 * If there is a processor associated with this phase initialize it.
 * Do this during initialization, rather than construction as we
 * don't know how many slices exist until the entire model is loaded.
 */

Phase&
Phase::initProcessor()
{	
    if ( getProcessor() != nullptr || getDOM() == nullptr ) return *this;
    const Processor * processor = owner()->getProcessor();
    if ( !processor ) return *this;

    /* 
     * If I don't have an entry on the processor, create one provided
     * that the processor is interesting 
     */
	
    if ( getDOM()->hasServiceTime() ) {
	const std::string entry_name = owner()->name() + ':' + name();
	_devices.push_back( new DeviceInfo( *this, entry_name, DeviceInfo::Type::HOST ) );
	if ( owner()->getProcessor()->isCFSserver() ) {
	    initCFSProcessor();
	}
    }

    /*
     * If the entry has think time, connect it to the delay server.
     */

    if ( hasThinkTime() ) {
	const std::string entry_name = Model::__think_server->name() + ":" + name();
	_devices.push_back( new DeviceInfo( *this, entry_name, DeviceInfo::Type::THINK_TIME ) );
    }

    return *this;
}


/*
 * Overridden by activity for activities in DeviceInfo.
 */

ProcessorCall *
Phase::newProcessorCall( Entry * procEntry ) const
{
    return new PhaseProcessorCall( this, procEntry );
}

Phase::DeviceInfo::DeviceInfo( const Phase& phase, const std::string& name, Type type )
    : _phase(phase), _name(name), _type(type),
      _entry(nullptr), _call(nullptr), _entry_dom(nullptr), _call_dom(nullptr)
{
    const LQIO::DOM::Document * document = _phase.getDOM()->getDocument();
    const Processor * processor = _phase.owner()->getProcessor();

    _entry_dom = new LQIO::DOM::Entry( document, name );
    switch ( type ) {
    case Type::THINK_TIME:
	_entry = new DeviceEntry( _entry_dom, Model::__think_server );
	_entry->setServiceTime( think_time() )
	    .setCV_sqr( 1.0 )
	    .initVariance();
	break;
    case Type::CFS_DELAY:
	_entry = new DeviceEntry( _entry_dom, Model::__cfs_server );
	_entry->setServiceTime( 0.00001 )
	    .setCV_sqr( 1.0 )
	    .initVariance();
	break;
    default:
	_entry = new DeviceEntry( _entry_dom, const_cast<Processor *>( processor ) );
	_entry->setServiceTime( service_time() / n_calls() )
	    .setCV_sqr( cv_sqr() )
	    .initVariance()
	    .setPriority( phase.owner()->priority() );
	break;
    }

    Model::__entry.insert( _entry );
		
    /* 
     * We may have to change this at some point.  However, we can't do
     * priority by class in the analytic solver anyway - only by
     * chain.  Note - _call_dom is NOT stored in the DOM, so we delete it.
     */	

    LQIO::DOM::ConstantExternalVariable * visits;
    if ( isProcessor() ) {
	visits = new LQIO::DOM::ConstantExternalVariable(n_calls());
    } else {
	visits = new LQIO::DOM::ConstantExternalVariable(1.0);
    }
    _call = _phase.newProcessorCall( _entry );
    _call_dom = new LQIO::DOM::Call( document, LQIO::DOM::Call::Type::QUASI_RENDEZVOUS,
				     _phase.getDOM(), _entry->getDOM(), visits );
    _call->rendezvous( _call_dom );
}


Phase::DeviceInfo::~DeviceInfo()
{
    if ( _call != nullptr ) delete _call;
    if ( _call_dom != nullptr ) delete _call_dom;
    if ( _entry_dom != nullptr ) delete _entry_dom;
    /* Model will delete _entry */
}


Phase::DeviceInfo&
Phase::DeviceInfo::recalculateDynamicValues()
{
    const double old_time = entry()->serviceTimeForPhase(1);
    if ( isProcessor() ) {
	const double new_time = n_calls() > 0. ? service_time() / n_calls() : 0.0;
	if ( old_time == new_time && !flags.full_reinitialize ) return *this;

	_entry->setServiceTime(new_time)
	    .setCV_sqr(cv_sqr())
	    .initVariance()
	    .initWait();

	_call_dom->setCallMeanValue( n_calls() );
	_call->setWait( old_time > 0.0 ? _call->wait() * new_time / old_time : new_time );

    } else {
	const double new_time = think_time();
	if ( old_time == new_time && !flags.full_reinitialize ) return *this;

	_entry->setServiceTime(new_time)
	    .initVariance()
	    .initWait();

	_call->setWait( new_time );
    }
    return *this;
}


/* init CFS processor delay entry */
void
Phase::initCFSProcessor()
{	
    if ( !owner()->getProcessor()->isCFSserver() ) return;

    if ( getCFSEntry() == nullptr ) {

	std::string cfs_entry_name;
	cfs_entry_name = Model::__cfs_server->name();
	cfs_entry_name += "-";
	cfs_entry_name += name();
	_devices.push_back( new DeviceInfo( *this, cfs_entry_name, DeviceInfo::Type::CFS_DELAY ) );
    }
    _cfs_delay=0.00001;
}


std::set<Entity *>& Phase::DeviceInfo::add_server( std::set<Entity *>& servers, const DeviceInfo * device )
{
    servers.insert(const_cast<Entity *>(device->call()->dstTask()));
    return servers;
}

double
Phase::DeviceInfo::add_wait::operator()( double sum, const DeviceInfo * device ) const
{
    const Call * call = device->call();
    if ( call->submodel() == _submodel ) sum += call->rendezvousDelay();
    return sum;
}

void
Phase::get_servers::operator()( const Phase& phase ) const		// For phases
{
    _servers = std::accumulate( phase.callList().begin(), phase.callList().end(), _servers, Call::add_server );
    _servers = std::accumulate( phase.devices().begin(),  phase.devices().end(),  _servers, DeviceInfo::add_server );
}

void
Phase::get_servers::operator()( const Phase* phase ) const		// For activities
{
    _servers = std::accumulate( phase->callList().begin(), phase->callList().end(), _servers, Call::add_server );
    _servers = std::accumulate( phase->devices().begin(),  phase->devices().end(),  _servers, DeviceInfo::add_server );
}
