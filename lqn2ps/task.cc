/*  -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/lqn2ps/task.cc $
 *
 * Everything you wanted to know about a task, but were afraid to ask.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * January 2001
 *
 * ------------------------------------------------------------------------
 * $Id: task.cc 14137 2020-11-25 18:29:59Z greg $
 * ------------------------------------------------------------------------
 */

#include "lqn2ps.h"
#include <string>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <cmath>
#if HAVE_FLOAT_H
#include <float.h>
#endif
#include <limits.h>
#include <lqio/error.h>
#include <lqio/input.h>
#include <lqio/labels.h>
#include <lqio/dom_task.h>
#include <lqio/dom_entry.h>
#include <lqio/dom_activity.h>
#include <lqio/dom_actlist.h>
#include <lqio/dom_processor.h>
#include <lqio/dom_group.h>
#include <lqio/dom_document.h>
#include <lqio/srvn_output.h>
#include "errmsg.h"
#include "task.h"
#include "entry.h"
#include "activity.h"
#include "actlist.h"
#include "share.h"
#include "processor.h"
#include "call.h"
#include "label.h"
#include "model.h"

std::set<Task *,LT<Task> > Task::__tasks;

bool Task::thinkTimePresent             = false;
bool Task::holdingTimePresent           = false;
bool Task::holdingVariancePresent       = false;

const double Task::JLQNDEF_TASK_BOX_SCALING = 1.2;

/* -------------------------------------------------------------------- */
/* Funky Formatting functions for inline with <<.			*/
/* -------------------------------------------------------------------- */

class SRVNTaskManip {
public:
    SRVNTaskManip( std::ostream& (*ff)(std::ostream&, const Task & ), const Task & theTask ) : f(ff), aTask(theTask) {}
private:
    std::ostream& (*f)( std::ostream&, const Task& );
    const Task & aTask;

    friend std::ostream& operator<<(std::ostream & os, const SRVNTaskManip& m ) { return m.f(os,m.aTask); }
};


static std::ostream& entries_of_str( std::ostream& output,  const Task& aTask );
static std::ostream& task_scheduling_of_str( std::ostream& output,  const Task & aTask );

static inline SRVNTaskManip entries_of( const Task& aTask ) { return SRVNTaskManip( entries_of_str, aTask ); }
static inline SRVNTaskManip task_scheduling_of( const Task & aTask ) { return SRVNTaskManip( &task_scheduling_of_str, aTask ); }

/* -------------------------- Constructor ----------------------------- */

Task::Task( const LQIO::DOM::Task* dom, const Processor * aProc, const Share * aShare, const std::vector<Entry *>& entries )
    : Entity( dom, __tasks.size()+1 ),
      _entries(entries),
      _activities(),
      _precedences(),
      _processor(aProc),
      _share(aShare),
      _maxPhase(0),
      _entryWidthInPts(0)
{
    for_each( _entries.begin(), _entries.end(), Exec1<Entry,const Task *>( &Entry::owner, this ) );

    if ( aProc ) {
	ProcessorCall * aCall = new ProcessorCall(this,aProc);
	_calls.push_back(aCall);
	const_cast<Processor *>(aProc)->addDstCall( aCall );
	const_cast<Processor *>(aProc)->addTask( this );
    }

    myNode = Node::newNode( Flags::icon_width, Flags::graphical_output_style == TIMEBENCH_STYLE ? Flags::icon_height : Flags::entry_height );
    myLabel = Label::newLabel();
}



Task::~Task()
{
    for ( std::vector<EntityCall *>::const_iterator call = calls().begin(); call != calls().end(); ++call ) {
	delete *call;
    }
    for ( std::vector<Activity *>::const_iterator activity = activities().begin(); activity != activities().end(); ++activity ) {
	delete *activity;
    }
    for ( std::vector<ActivityList *>::const_iterator precedence = precedences().begin(); precedence != precedences().end(); ++precedence ) {
	delete *precedence;
    }
}



/*
 * Reset globals.
 */

void
Task::reset()
{
    thinkTimePresent = false;
    holdingTimePresent = false;
    holdingVariancePresent = false;
}



/* ------------------------ Instance Methods -------------------------- */

bool
Task::hasPriority() const
{
    return dynamic_cast<const LQIO::DOM::Task *>(getDOM())->hasPriority();
}



/* ------------------------------ Results ----------------------------- */

double
Task::throughput() const
{
    return dynamic_cast<const LQIO::DOM::Task *>(getDOM())->getResultThroughput();
}

/* -- */

double
Task::utilization( const unsigned p ) const
{
    assert( 0 < p && p <= LQIO::DOM::Phase::MAX_PHASE );

    return dynamic_cast<const LQIO::DOM::Task *>(getDOM())->getResultPhasePUtilization(p);
}


/* --- */

double
Task::utilization() const
{
    return dynamic_cast<const LQIO::DOM::Task *>(getDOM())->getResultUtilization();
}


/* --- */

double
Task::processorUtilization() const
{
    return dynamic_cast<const LQIO::DOM::Task *>(getDOM())->getResultProcessorUtilization();
}

/*
 * Rename tasks.
 */

Task&
Task::rename()
{
    const std::string old_name = name();
    std::ostringstream new_name;
    new_name << "t" << elementId();
    const_cast<LQIO::DOM::DocumentObject *>(getDOM())->setName( new_name.str() );
    for_each( activities().begin(), activities().end(), Exec<Element>( &Element::rename ) );

    renameFanInOut( old_name, name() );
    return *this;
}


Task&
Task::squishName()
{
    const std::string old_name = name();
    Element::squishName();
    for_each( activities().begin(), activities().end(), Exec<Element>( &Element::squishName ) );

    renameFanInOut( old_name, name() );
    return *this;
}


/* 
 * fix fan in and outs...  Have to find all references to this task the hard way.
 */
    
void
Task::renameFanInOut( const std::string& old_name, const std::string& new_name )
{
    const LQIO::DOM::Document * document = getDOM()->getDocument();		// Find root
    const std::map<std::string,LQIO::DOM::Task*>& tasks = document->getTasks();	// Get all tasks for remap
    for ( std::map<std::string,LQIO::DOM::Task*>::const_iterator t = tasks.begin(); t != tasks.end(); ++t ) {
	const LQIO::DOM::Task * task = t->second;
	renameFanInOut( const_cast<std::map<const std::string,LQIO::DOM::ExternalVariable *>&>(task->getFanIns()), old_name, new_name );
	renameFanInOut( const_cast<std::map<const std::string,LQIO::DOM::ExternalVariable *>&>(task->getFanOuts()), old_name, new_name );
    }
}


void
Task::renameFanInOut( std::map<const std::string,LQIO::DOM::ExternalVariable *>& fan_ins_or_outs, const std::string& old_name, const std::string& new_name )
{
    std::map<const std::string,LQIO::DOM::ExternalVariable *>::iterator index = fan_ins_or_outs.find( old_name );
    if ( index != fan_ins_or_outs.end() ) {
	LQIO::DOM::ExternalVariable * value = index->second;
	fan_ins_or_outs.erase( index );						// Remove old item.
	fan_ins_or_outs.insert( std::pair<const std::string,LQIO::DOM::ExternalVariable *>(new_name, value) );
    }
}



/*
 * Aggregate activities to activities and/or activities to phases.  If
 * activities are left after aggregation, we will have to recompute
 * their level because there likely is a lot less of them to draw.
 * Aggregating to a task merges everything up to an entry.  The actual
 * aggregation is done by submodel in layer.cc
 */

Task&
Task::aggregate()
{
    for_each( entries().begin(), entries().end(), Exec<Entry>( &Entry::aggregate ) );

    switch ( Flags::print[AGGREGATION].value.i ) {
    case AGGREGATE_ENTRIES:
    case AGGREGATE_PHASES:
    case AGGREGATE_ACTIVITIES:
	for ( std::vector<Activity *>::const_iterator activity = activities().begin(); activity != activities().end(); ++activity ) {
	    delete *activity;
	}
	for ( std::vector<ActivityList *>::const_iterator precedence = precedences().begin();  precedence != precedences().end(); ++precedence ) {
	    delete *precedence;
	}
	const_cast<LQIO::DOM::Task *>(dynamic_cast<const LQIO::DOM::Task *>(getDOM()))->deleteActivities();
	const_cast<LQIO::DOM::Task *>(dynamic_cast<const LQIO::DOM::Task *>(getDOM()))->deleteActivityLists();
	_activities.clear();
	_layers.clear();
	_precedences.clear();
	break;

    default:
	/* Recompute levels. */
	for_each( activities().begin(), activities().end(), Exec<Activity>( &Activity::resetLevel ) );
	generate();
	break;
    }

    return *this;
}


/*
 * Sort entries and activities based on when they were visited.
 */

Task&
Task::sort()
{
    std::sort( _calls.begin(), _calls.end(), Call::compareSrc );
    Entity::sort();
    return *this;
}


double
Task::getIndex() const
{
    double anIndex = MAXDOUBLE;

    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	anIndex = std::min( anIndex, (*entry)->getIndex() );
    }

    return anIndex;
}


int
Task::span() const
{
    if ( Flags::print[LAYERING].value.i == LAYERING_GROUP ) {
	std::vector<Entity *> myServers;
	return servers( myServers );		/* Force those making calls to lower levels right */
    }
    return 0;
}


/*
 * Set the chain of this client task to curr_k.  Chains will be set to
 * all servers of this client.  next_k will be changed iff there are
 * forks.
 */

unsigned
Task::setChain( unsigned curr_k, callPredicate aFunc  ) const
{
    unsigned next_k = curr_k;

    /*
     * I will have to find all servers to this client (including the
     * processor)...  because each server will get it's own chain.
     * Threads will complicate matters, bien sur.
     */

    if ( isReplicated() ) {
	std::vector<Entity *> servers;
	this->servers( servers );

	std::sort( servers.begin(), servers.end(), &Entity::compareCoord );

	for ( std::vector<Entity *>::const_iterator server = servers.begin(); server != servers.end(); ++server ) {
	    if ( !(*server)->isSelected() ) continue;
	    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
		next_k = (*entry)->setChain( curr_k, next_k, (*server), aFunc );
	    }
	    curr_k = next_k + 1;
	    next_k = curr_k;
	}

    } else {

	for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	    next_k = (*entry)->setChain( curr_k, next_k, 0, aFunc );
	}
    }
    return next_k;
}


/*
 * Set the server chain k.  We might have more than one processor (-Lclient).
 */

Task&
Task::setServerChain( unsigned k )
{
    for ( std::vector<EntityCall *>::const_iterator call = calls().begin(); call != calls().end(); ++call ) {
	const Processor * aProcessor = dynamic_cast<const Processor *>((*call)->dstEntity());
	if ( aProcessor ) {
	    if ( aProcessor->isInteresting() && aProcessor->isSelected() ) {
		const_cast<Processor *>(aProcessor)->setServerChain( k );
	    }
	    continue;
	}
	const Task * aTask = dynamic_cast<const Task *>((*call)->dstEntity());
	if ( aTask ) {
	    if ( aTask->isSelected() ) {
		const_cast<Task *>(aTask)->setServerChain( k ).setClientClosedChain( k );
	    }
	}
    }

    Element::setServerChain( k );
    return *this;
}



/*
 * Add a task to the list of tasks for this processor and set local index
 * for MVA.
 */

Task&
Task::addEntry( Entry * anEntry )
{
    _entries.push_back(anEntry);
    return *this;
}


Task&
Task::removeEntry( Entry * anEntry )
{
    std::vector<Entry *>::iterator pos = find_if( _entries.begin(), _entries.end(), EQ<Element>(anEntry) );
    if ( pos != _entries.end() ) {
	_entries.erase(pos);
    }
    return *this;
}


/*
 * Add a task to the list of tasks for this processor and set local index
 * for MVA.
 */

Activity *
Task::findActivity( const std::string& name ) const
{
    const std::vector<Activity *>::const_iterator activity = find_if( activities().begin(), activities().end(), EQStr<Activity>( name ) );
    return activity != activities().end() ? *activity : 0;
}


Activity *
Task::findOrAddActivity( const LQIO::DOM::Activity * activity )
{
    Activity * anActivity = findActivity( activity->getName() );

    if ( !anActivity ) {
	anActivity = new Activity( this, activity );
	_activities.push_back( anActivity );
    }

    return anActivity;
}



#if defined(REP2FLAT)
/*
 * Find an existing activity which has the same name as srcActivity.
 */

Activity *
Task::findActivity( const Activity& srcActivity, const unsigned replica )
{
    std::ostringstream aName;
    aName << srcActivity.name() << "_" << replica;

    return findActivity( aName.str() );
}


/*
 * Find an existing activity which has the same name as srcActivity.  If the activity does not exist, create one and copy it from the src.
 */

Activity *
Task::addActivity( const Activity& srcActivity, const unsigned replica )
{
    std::ostringstream srcName;
    srcName << srcActivity.name() << "_" << replica;

    Activity * dstActivity = findActivity( srcName.str() );
    if ( dstActivity ) return dstActivity;	// throw error?

    LQIO::DOM::Task * dom_task = const_cast<LQIO::DOM::Task *>(dynamic_cast<const LQIO::DOM::Task *>(getDOM()));
    LQIO::DOM::Activity * dstDOM = new LQIO::DOM::Activity( *(dynamic_cast<const LQIO::DOM::Activity *>(srcActivity.getDOM())) );
    dstDOM->setName( srcName.str() );
    dom_task->addActivity( dstDOM );
    dstActivity = new Activity( this, dstDOM );
    _activities.push_back( dstActivity );

    /* Clone instance variables */
    
    dstActivity->isSpecified( srcActivity.isSpecified() );
    if ( srcActivity.reachable() ) {
	std::ostringstream aName;
	aName << srcActivity.reachedFrom()->name() << "_" << replica;
	Activity * anActivity = findActivity( aName.str() );
	dstActivity->setReachedFrom( anActivity );
    }

    if ( srcActivity.isStartActivity() ) {
	Entry *dstEntry = Entry::find_replica( srcActivity.rootEntry()->name(), replica );
	dstActivity->rootEntry( dstEntry, nullptr );
	const_cast<LQIO::DOM::Entry *>(dynamic_cast<const LQIO::DOM::Entry *>(dstEntry->getDOM()))->setStartActivity( dstDOM );
	if (dstEntry->entryTypeOk(LQIO::DOM::Entry::Type::ACTIVITY)) {
	    dstEntry->setStartActivity(dstActivity);
	}
    }

    /* Clone reply list */

    std::vector<Entry *>& dstReplyList = const_cast<std::vector<Entry *>&>(dstActivity->replies());
    for ( std::vector<Entry *>::const_iterator entry = srcActivity.replies().begin(); entry != srcActivity.replies().end(); ++entry ) {
	Entry * dstEntry = Entry::find_replica( (*entry)->name(), replica );
	dstReplyList.push_back( dstEntry );
	dstDOM->getReplyList().push_back(const_cast<LQIO::DOM::Entry *>(dynamic_cast<const LQIO::DOM::Entry *>(dstEntry->getDOM())));	/* Need dom for entry, the push that */
    }

    dstActivity->expandActivityCalls( srcActivity, replica );

    return dstActivity;
}
#endif

Task&
Task::removeActivity( Activity * anActivity )
{
    std::vector<Activity *>::iterator activity = find( _activities.begin(), _activities.end(), anActivity );
    if ( activity != _activities.end() ) {
	_activities.erase( activity );
    }
    return *this;
}


void
Task::addPrecedence( ActivityList * aPrecedence )
{
    _precedences.push_back( aPrecedence );
}


/*
 * Returns the initial depth (0 or 1) if this entity is a root of a
 * call graph.  Returns -1 otherwise.  Used by the topological sorter.
 */

int
Task::rootLevel() const
{
    int level = -1;
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	const requesting_type callType = (*entry)->isCalled();
	switch ( callType ) {

	case OPEN_ARRIVAL_REQUEST:	/* Root task, but at lower level */
	    level = Model::SERVER_LEVEL;
	    break;

	case RENDEZVOUS_REQUEST:	/* Non-root task. 		*/
	case SEND_NO_REPLY_REQUEST:	/* Non-root task. 		*/
	case NOT_CALLED:		/* No operation.		*/
	    break;
	}
    }
    return level;
}




/*
 * Return true if this task forwards to aTask.
 */

bool
Task::forwardsTo( const Task * aTask ) const
{
    return find_if( entries().begin(), entries().end(), Predicate1<Entry,const Task *>( &Entry::forwardsTo, aTask ) ) != entries().end();
}


/*
 * Return true if this task forwards to another task on this level.
 */

bool
Task::hasForwardingLevel() const
{
    return find_if( entries().begin(), entries().end(), ::Predicate<Entry>( &Entry::hasForwardingLevel ) ) != entries().end();
}


/*
 * Return true if this task forwards to another task on this level.
 */

bool
Task::isForwardingTarget() const
{
    return find_if( entries().begin(), entries().end(), ::Predicate<Entry>( &Entry::isForwardingTarget ) ) != entries().end();
}


/*
 * Return true if this task receives rendezvous requests.
 */

bool
Task::isCalled( const requesting_type callType ) const
{
    return find_if( entries().begin(), entries().end(), Predicate1<Entry,const requesting_type>( &Entry::isCalled, callType ) ) != entries().end();
}


/*
 * Return the total open arrival rate to this server.
 */

double
Task::openArrivalRate() const
{
    double sum = 0.0;
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	if ( (*entry)->hasOpenArrivalRate() ) {
	    sum += LQIO::DOM::to_double( (*entry)->openArrivalRate() );
	}
    }

    return sum;
}



/*
 * Return true if this task makes any calls to lower level tasks.
 */

bool
Task::hasCalls( const callPredicate aFunc ) const
{
    return find_if( entries().begin(), entries().end(), Predicate1<Entry,const callPredicate>( &Entry::hasCalls, aFunc ) ) != entries().end()
	|| find_if( activities().begin(), activities().end(), Predicate1<Activity,const callPredicate>( &Activity::hasCalls, aFunc ) ) != activities().end();
}



/*
 * Returns the initial depth (0 or 1) if this entity is a root of a
 * call graph.  Returns -1 otherwise.  Used by the topological sorter.
 */

bool
Task::hasOpenArrivals() const
{
    return openArrivalRate() > 0.;
}



/*
 * Return true if any entry of this task queues on the processor.
 */

bool
Task::hasQueueingTime() const
{
    return find_if( entries().begin(), entries().end(), ::Predicate<Entry>( &Entry::hasQueueingTime ) ) != entries().end()
	|| find_if( activities().begin(), activities().end(), ::Predicate<Activity>( &Activity::hasQueueingTime ) ) != activities().end();
    return false;
}


/*
 * Return true if this entity is selected.
 * See subclasses for further tests.
 */

bool
Task::isSelectedIndirectly() const
{
    return Entity::isSelectedIndirectly() || processor()->isSelected()
	|| find_if( entries().begin(), entries().end(), ::Predicate<Entry>( &Entry::isSelectedIndirectly ) ) != entries().end()
	|| find_if( activities().begin(), activities().end(), ::Predicate<Activity>( &Activity::isSelectedIndirectly ) ) != activities().end();
}


/*
 * Return true if this task can be converted into one which takes open arrivals.
 */

bool
Task::canConvertToOpenArrivals() const
{
    return partial_output() && !isSelected() && !canConvertToReferenceTask();
}


/*
 * If I don't make any calls and my processor is only for me, then return true
 */

bool
Task::isPureServer() const
{
    std::vector<Entity *> servers;

    this->servers( servers );
    return servers.size() == 0 && processor()->nTasks() == 1;
}



bool
Task::check() const
{
    bool rc = true;
    const Processor * aProcessor = processor();
    const std::string& srcName = name();

    /* Check prio/scheduling. */

    if ( aProcessor && hasPriority() && !aProcessor->hasPriorities() ) {
	LQIO::solution_error( LQIO::WRN_PRIO_TASK_ON_FIFO_PROC, srcName.c_str(), aProcessor->name().c_str() );
    }

    /* Check replication */

    if ( Flags::print[OUTPUT_FORMAT].value.i == FORMAT_PARSEABLE ) {
	LQIO::io_vars.error_messages[ERR_REPLICATION].severity = LQIO::WARNING_ONLY;
	LQIO::io_vars.error_messages[ERR_REPLICATION_PROCESSOR].severity = LQIO::WARNING_ONLY;
    }

    double srcReplicasValue = 1.0;
    if ( aProcessor->isReplicated() ) {
	bool ok = true;
	double dstReplicasValue = 1.0;
	const LQIO::DOM::ExternalVariable& dstReplicas = aProcessor->replicas();
	if ( dstReplicas.wasSet() && dstReplicas.getValue( dstReplicasValue ) && isReplicated() ) {
	    const LQIO::DOM::ExternalVariable& srcReplicas = replicas();
	    if ( srcReplicas.wasSet() && srcReplicas.getValue( srcReplicasValue ) ) {
		const double temp = static_cast<double>(srcReplicasValue) / static_cast<double>(dstReplicasValue);
		ok = trunc( temp ) == temp;		/* Integer multiple */
	    }
	} else {
	    ok = false;
	}
	if ( !ok ) {
	    LQIO::solution_error( ERR_REPLICATION_PROCESSOR,
				  static_cast<int>(srcReplicasValue), srcName.c_str(),
				  static_cast<int>(dstReplicasValue), aProcessor->name().c_str() );
	    if ( Flags::print[OUTPUT_FORMAT].value.i != FORMAT_PARSEABLE ) {
		rc = false;
	    }
	}
    }

    /* Check entries */

    if ( scheduling() == SCHEDULE_SEMAPHORE ) {
	if ( nEntries() != 2 ) {
	    LQIO::solution_error( LQIO::ERR_ENTRY_COUNT_FOR_TASK, srcName.c_str(), nEntries(), N_SEMAPHORE_ENTRIES );
	    rc = false;
	}
	if ( !((entries().at(0)->isSignalEntry() && entries().at(1)->entrySemaphoreTypeOk(LQIO::DOM::Entry::Semaphore::WAIT))
	       || (entries().at(0)->isWaitEntry() && entries().at(1)->entrySemaphoreTypeOk(LQIO::DOM::Entry::Semaphore::SIGNAL))
	       || (entries().at(1)->isSignalEntry() && entries().at(0)->entrySemaphoreTypeOk(LQIO::DOM::Entry::Semaphore::WAIT))
	       || (entries().at(1)->isWaitEntry() && entries().at(0)->entrySemaphoreTypeOk(LQIO::DOM::Entry::Semaphore::SIGNAL))) ) {
	    LQIO::solution_error( LQIO::ERR_NO_SEMAPHORE, srcName.c_str() );
	    rc = false;
	} else if ( entries().at(0)->isCalled() && !entries().at(1)->isCalled() ) {
	    LQIO::io_vars.error_messages[LQIO::WRN_NO_REQUESTS_TO_ENTRY].severity = LQIO::RUNTIME_ERROR; /* WARNING_ONLY; */
	    LQIO::solution_error( LQIO::WRN_NO_REQUESTS_TO_ENTRY, entries().at(1)->name().c_str() );
	    rc = false;
	} else if ( !entries().at(0)->isCalled() && entries().at(1)->isCalled() ) {
	    LQIO::io_vars.error_messages[LQIO::WRN_NO_REQUESTS_TO_ENTRY].severity = LQIO::RUNTIME_ERROR; /* WARNING_ONLY; */
	    LQIO::solution_error( LQIO::WRN_NO_REQUESTS_TO_ENTRY, entries().at(0)->name().c_str() );
	    rc = false;
	}
	LQIO::io_vars.error_messages[LQIO::WRN_NO_REQUESTS_TO_ENTRY].severity = LQIO::WARNING_ONLY;		/* Revert to normal */

    } else if ( scheduling() == SCHEDULE_RWLOCK ) {
	if ( nEntries() != N_RWLOCK_ENTRIES ) {
	    LQIO::solution_error( LQIO::ERR_ENTRY_COUNT_FOR_TASK, srcName.c_str(), nEntries(), N_RWLOCK_ENTRIES );
	    rc = false;
	}

	const Entry * E[N_RWLOCK_ENTRIES];

	for ( unsigned int i=0;i<N_RWLOCK_ENTRIES;i++){
	    E[i]=0;
	}

	for ( unsigned int i=0;i<N_RWLOCK_ENTRIES;i++){
	    if ( entries().at(i)->is_r_unlock_Entry() ) {
		if ( !E[0] ) {
		    E[0]=entries().at(i);
		} else { // duplicate entry TYPE error
		    LQIO::solution_error( LQIO::ERR_DUPLICATE_SYMBOL, srcName.c_str() );
		    rc = false;
		}
	    } else if (entries().at(i)->is_r_lock_Entry() ) {
		if ( !E[1] ) {
		    E[1]=entries().at(i);
		} else {
		    LQIO::solution_error( LQIO::ERR_DUPLICATE_SYMBOL, srcName.c_str() );
		    rc = false;
		}
	    } else if ( entries().at(i)->is_w_unlock_Entry() ) {
		if ( !E[2] ) {
		    E[2]=entries().at(i);
		} else {
		    LQIO::solution_error( LQIO::ERR_DUPLICATE_SYMBOL, srcName.c_str() );
		    rc = false;
		}
	    } else if ( entries().at(i)->is_w_lock_Entry() ) {
		if (!E[3]) {
		    E[3]=entries().at(i);
		} else {
		    LQIO::solution_error( LQIO::ERR_DUPLICATE_SYMBOL, srcName.c_str() );
		    rc = false;
		}
	    } else {
		LQIO::solution_error( LQIO::ERR_NO_RWLOCK, srcName.c_str() );
		rc = false;
	    }
	}

	/* Make sure both or neither entry is called */

	if ( E[0]->isCalled() && !E[1]->isCalled() ) {
	    LQIO::io_vars.error_messages[LQIO::WRN_NO_REQUESTS_TO_ENTRY].severity = LQIO::RUNTIME_ERROR; /* WARNING_ONLY; */
	    LQIO::solution_error( LQIO::WRN_NO_REQUESTS_TO_ENTRY, E[1]->name().c_str() );
	} else if ( !E[0]->isCalled() && E[1]->isCalled() ) {
	    LQIO::io_vars.error_messages[LQIO::WRN_NO_REQUESTS_TO_ENTRY].severity = LQIO::RUNTIME_ERROR; /* WARNING_ONLY; */
	    LQIO::solution_error( LQIO::WRN_NO_REQUESTS_TO_ENTRY, E[0]->name().c_str() );
	}
	if ( E[2]->isCalled() && !E[3]->isCalled() ) {
	    LQIO::io_vars.error_messages[LQIO::WRN_NO_REQUESTS_TO_ENTRY].severity = LQIO::RUNTIME_ERROR; /* WARNING_ONLY; */
	    LQIO::solution_error( LQIO::WRN_NO_REQUESTS_TO_ENTRY, E[3]->name().c_str() );
	} else if ( !E[2]->isCalled() && E[3]->isCalled() ) {
	    LQIO::io_vars.error_messages[LQIO::WRN_NO_REQUESTS_TO_ENTRY].severity = LQIO::RUNTIME_ERROR; /* WARNING_ONLY; */
	    LQIO::solution_error( LQIO::WRN_NO_REQUESTS_TO_ENTRY, E[2]->name().c_str() );
	}
	LQIO::io_vars.error_messages[LQIO::WRN_NO_REQUESTS_TO_ENTRY].severity = LQIO::WARNING_ONLY;		/* Revert to normal */
    }

    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	if ( !isReferenceTask() ) {
	    if ( !(*entry)->hasOpenArrivalRate() && !(*entry)->isCalled() ) {
		LQIO::solution_error( LQIO::WRN_NO_REQUESTS_TO_ENTRY, (*entry)->name().c_str() );
	    }
	}
	rc = (*entry)->check() && rc;
	_maxPhase = std::max( _maxPhase, (*entry)->maxPhase() );
    }

    if ( scheduling() == SCHEDULE_SEMAPHORE ) {
	LQIO::io_vars.error_messages[LQIO::WRN_NO_REQUESTS_TO_ENTRY].severity = LQIO::WARNING_ONLY;
    }

    rc = for_each( activities().begin(), activities().end(), AndPredicate<Activity>( &Activity::check ) ).result() && rc;
    return rc;
}




/*
 * Return all clients to this task.
 */

unsigned
Task::referenceTasks( std::vector<Entity *> &clients, Element * dst ) const
{
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	(*entry)->referenceTasks( clients, (dst == this) ? (*entry) : dst );	/* Map task to entry if this is the dst */
    }
    return clients.size();
}



/*
 * Return all clients to this task.
 */

unsigned
Task::clients( std::vector<Entity *> &clients, const callPredicate aFunc ) const
{
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	(*entry)->clients( clients, aFunc );
    }
    return clients.size();
}



/*
 * Locate the destination task in the list of destinations.  Processor
 * calls are lumped into the task call list.  This makes life simpler
 * when we are drawing tasks only.
 */

EntityCall *
Task::findCall( const Entity * anEntity, const callPredicate aFunc ) const
{
    for ( std::vector<EntityCall *>::const_iterator call = calls().begin(); call != calls().end(); ++call ) {
	if ( 	(!dynamic_cast<TaskCall *>(*call) || (*call)->dstEntity() == anEntity)
	     && (!(*call)->isProcessorCall() || (*call)->dstEntity() == anEntity)
	     && (!aFunc || ((*call)->*aFunc)() ) ) return *call;
    }

    return NULL;
}



EntityCall *
Task::findOrAddCall( Task * toTask, const callPredicate aFunc )
{
    EntityCall * aCall = findCall( toTask, aFunc );

    if ( !aCall ) {
	aCall = new TaskCall( this, toTask );
	_calls.push_back( aCall );
	toTask->addDstCall( aCall );
    }

    return aCall;
}



EntityCall *
Task::findOrAddPseudoCall( Entity * toEntity )
{
    EntityCall * aCall = findCall( toEntity );

    if ( !aCall ) {
	if ( dynamic_cast<Processor *>(toEntity) ) {
	    Processor * toProcessor = dynamic_cast<Processor *>(toEntity);
	    aCall = new PseudoProcessorCall( this, toProcessor );
	    toProcessor->addDstCall( aCall );
	    toProcessor->addTask( this );
	} else {
	    Task * toTask = dynamic_cast<Task *>(toEntity);
	    aCall = new PseudoTaskCall( this, toTask );
	    toTask->addDstCall( aCall );
	}
	aCall->linestyle( Graphic::DASHED_DOTTED );
	_calls.push_back( aCall );
    }

    return aCall;
}



/*
 * Return all servers to this task.
 */

unsigned
Task::servers( std::vector<Entity *> &servers ) const
{
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	for ( std::vector<Call *>::const_iterator call = (*entry)->calls().begin(); call != (*entry)->calls().end(); ++call ) {
	    if ( !(*call)->hasForwardingLevel() && (*call)->isSelected() ) {
		const Task * task = (*call)->dstTask();;
		if ( find_if( servers.begin(), servers.end(), EQ<Element>( task ) ) == servers.end() ) {
		    servers.push_back( const_cast<Task *>(task) );
		}
	    }
	}
    }

    for ( std::vector<Activity *>::const_iterator activity = activities().begin(); activity != activities().end(); ++activity ) {
	for ( std::vector<Call *>::const_iterator call = (*activity)->calls().begin(); call != (*activity)->calls().end(); ++call ) {
	    if ( !(*call)->hasForwarding() && (*call)->isSelected() ) {
		const Task * task = (*call)->dstTask();;
		if ( find_if( servers.begin(), servers.end(), EQ<Element>( task ) ) == servers.end() ) {
		    servers.push_back( const_cast<Task *>(task) );
		}
	    }
	}
    }

    for ( std::vector<EntityCall *>::const_iterator call = calls().begin(); call != calls().end(); ++call ) {
	const Processor * processor = dynamic_cast<const Processor *>((*call)->dstEntity());
	if ( processor && processor->isSelected() && processor->isInteresting() && find_if( servers.begin(), servers.end(), EQ<Element>(processor)) == servers.end() ) {
	    servers.push_back( const_cast<Processor *>(processor) );
	}
    }

    return servers.size();
}



bool
Task::isInOpenModel( const std::vector<Entity *>& servers ) const
{
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	for ( std::vector<Call *>::const_iterator call = (*entry)->calls().begin(); call != (*entry)->calls().end(); ++call ) {
	    if ( (*call)->hasSendNoReply() && find_if( servers.begin(), servers.end(), EQ<Element>((*call)->dstTask()) ) != servers.end() ) return true;
	}
    }
    for ( std::vector<Activity *>::const_iterator activity = activities().begin(); activity != activities().end(); ++activity ) {
	for ( std::vector<Call *>::const_iterator call = (*activity)->calls().begin(); call != (*activity)->calls().end(); ++call ) {
	    if ( (*call)->hasSendNoReply() && find_if( servers.begin(), servers.end(), EQ<Element>((*call)->dstTask()) ) != servers.end() ) return true;
	}
    }
    return false;
}


bool
Task::isInClosedModel( const std::vector<Entity *>& servers ) const
{
    for ( std::vector<EntityCall *>::const_iterator call = calls().begin(); call != calls().end(); ++call ) {
	const Processor * aProcessor = dynamic_cast<const Processor *>((*call)->dstEntity());
	if ( aProcessor ) {
	    if ( aProcessor->isInteresting() && find_if( servers.begin(), servers.end(), EQ<Element>(aProcessor) ) != servers.end() ) return true;
	    continue;
	}
	const Task * task = dynamic_cast<const Task *>((*call)->dstEntity());
	if ( task ) {
	    if ( find_if( servers.begin(), servers.end(), EQ<Element>((*call)->dstEntity()) ) != servers.end() ) return true;
	}
    }

    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	for ( std::vector<Call *>::const_iterator call = (*entry)->calls().begin(); call != (*entry)->calls().end(); ++call ) {
	    if ( (*call)->hasRendezvous() && find_if( servers.begin(), servers.end(), EQ<Element>((*call)->dstTask()) ) != servers.end() ) return true;
	}
    }
    for ( std::vector<Activity *>::const_iterator activity = activities().begin(); activity != activities().end(); ++activity ) {
	for ( std::vector<Call *>::const_iterator call = (*activity)->calls().begin(); call != (*activity)->calls().end(); ++call ) {
	    if ( (*call)->hasRendezvous() && find_if( servers.begin(), servers.end(), EQ<Element>((*call)->dstTask()) ) != servers.end() ) return true;
	}
    }
    return false;
}


/*
 * Recursively find all children and grand children from `father'.  As
 * we descend down, we bump the depth.  If our path's cross, we have a
 * loop and abort.
 */

size_t
Task::findChildren( CallStack& callStack, const unsigned directPath )
{
    size_t max_depth = std::max( callStack.size(), level() );
    const Entry * dstEntry = callStack.back() ? callStack.back()->dstEntry() : 0;

    setLevel( max_depth ).addPath( directPath );

    if ( processor() ) {
	max_depth = std::max( max_depth + 1, processor()->level() );
	const_cast<Processor *>(processor())->setLevel( max_depth ).addPath( directPath );
    }

    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	max_depth = std::max( max_depth,
			      (*entry)->findChildren( callStack,
						      (((*entry) == dstEntry) || (*entry)->hasOpenArrivalRate())
						      ? directPath : 0  ) );
    }

    return max_depth;
}



/*
 * Count the number of calls that match the criteria passed
 */

unsigned
Task::countArcs( const callPredicate aFunc ) const
{
    return count_if( calls().begin(), calls().end(), GenericCall::PredicateAndSelected( aFunc ) );
}



/*
 * Count the number of calls that match the criteria passed
 */

double
Task::countCalls( const callPredicate2 aFunc ) const
{
    double count = 0.0;

    for ( std::vector<EntityCall *>::const_iterator call = calls().begin(); call != calls().end(); ++call ) {
	if ( (*call)->isSelected() ) {
	    count += ((*call)->*aFunc)() * (*call)->fanOut();
	}
    }
    return count;
}



/*
 * Try to estimate the number of threads that this task can spawn
 * (it's a gross hack)
 */

unsigned
Task::countThreads() const
{
    unsigned count = 1;
    for ( std::vector<Activity *>::const_iterator activity = activities().begin(); activity != activities().end(); ++activity ) {
	AndForkActivityList * aList = dynamic_cast<AndForkActivityList *>((*activity)->outputTo());
	if ( aList && aList->size() > 0 ) {
	    count += aList->size();
	}
    }
    return count;
}


std::vector<Activity *>
Task::repliesTo( Entry * anEntry ) const
{
    std::vector<Activity *> aCltn;
    for ( std::vector<Activity *>::const_iterator activity = activities().begin(); activity != activities().end(); ++activity ) {
	if ( (*activity)->repliesTo( anEntry ) ) {
	    aCltn.push_back( (*activity));
	}
    }
    return aCltn;
}

/*
 * Sort activities (if any).
 */

unsigned
Task::generate()
{
    unsigned int maxLevel = topologicalSort();

    _layers.resize( maxLevel + 1 );

    for ( std::vector<Activity *>::const_iterator activity = activities().begin(); activity != activities().end(); ++activity ) {
	if ( (*activity)->reachable() ) {
	    _layers.at((*activity)->level()) += (*activity);
	}
    }
    return maxLevel;
}



/*
 * Toplogical sort for activities.  It also aggregates activities.
 */

size_t
Task::topologicalSort()
{
    size_t maxLevel = 0;
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	Activity * anActivity = (*entry)->startActivity();
	if ( !anActivity ) continue;

	std::deque<const Activity *> activityStack;		// For checking for cycles.
	std::deque<const AndForkActivityList *> forkStack;	// For matching forks and joins.
	try {
	    maxLevel = std::max( maxLevel, anActivity->findActivityChildren( activityStack, forkStack, (*entry), 0, 1, 1.0  ) );
	}
	catch ( const activity_cycle& error ) {
	    maxLevel = std::max( maxLevel, error.depth()+1 );
	}
    }
    return maxLevel;
}


/*
 * Layout the activities (if we have any).
 * moveTo will patch things up.
 */

Task&
Task::format()
{
    double aWidth = 0.0;

    std::sort( _entries.begin(), _entries.end(), Entry::compare );

    /* Compute width of task.  Move entries */

    const double ty = myNode->height() + height();
    _entryWidthInPts = 0;
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	(*entry)->moveTo( _entryWidthInPts + myNode->left(), ty - (*entry)->height() );
	_entryWidthInPts += (*entry)->width() - adjustForSlope( fabs( (*entry)->height() ) );
    }
    if ( Flags::graphical_output_style == JLQNDEF_STYLE ) {
	_entryWidthInPts += Flags::entry_width * JLQNDEF_TASK_BOX_SCALING;
    }


    if ( _layers.size() ) {
	for ( std::vector<ActivityLayer>::iterator layer = _layers.begin(); layer != _layers.end(); ++layer ) {
	    layer->sort( Activity::compare ).format( 0 );
	}

	const double x = myNode->left() + adjustForSlope( fabs( height() ) ) + Flags::act_x_spacing / 4;

	/* Start from bottom and work up.  Reformat will realign the activities */

	double y = myNode->bottom();
	for ( std::vector<ActivityLayer>::reverse_iterator layer = _layers.rbegin(); layer != _layers.rend(); ++layer ) {
	    layer->moveTo( x, y );
	    y += layer->height();  /* grow down */

 	    /* Shift up layer IFF we have to draw stuff below activities */
 	    if ( layer->height() > Flags::entry_height ) {
 		layer->moveBy( 0.0, layer->height() - Flags::entry_height );
 	    }
	}

	y += Flags::icon_height;
	if ( !(queueing_output() && Flags::flatten_submodel) ) {
	    myNode->setHeight( y );
	}

	/* Calculate the space needed for the activities */

	switch( Flags::activity_justification ) {
	case ALIGN_JUSTIFY:
	case DEFAULT_JUSTIFY:
	    aWidth = justifyByEntry();
	    break;
	default:
	    aWidth = justify();
	    break;
	}
	aWidth += adjustForSlope( 2.0 * y );	/* Center icons inside task. */
    }

    /* Modify extent  */

    myNode->setWidth( std::max( std::max( aWidth, _entryWidthInPts + adjustForSlope( height() ) ), width() ) );

    return *this;
}


/*
 * Layout the entries and activities (if we have any).
 */

Task&
Task::reformat()
{
    std::sort( _entries.begin(), _entries.end(), Entry::compare );

    const double x = myNode->left() + adjustForSlope( height() );
    double y = myNode->bottom();
    const double offset = adjustForSlope( (height() - fabs(entries().front()->height())));
    const double fill = std::max( ((width() - adjustForSlope( height() )) - _entryWidthInPts) / (nEntries() + 1.0), 0.0 );

    /* Figure out which sides of the entries to draw */

    if ( fill == 0.0 ) {
	for ( std::vector<Entry *>::const_reverse_iterator entry = entries().rbegin(); entry != entries().rend(); ++entry ) {
	    (*entry)->drawLeft  = false;
	    (*entry)->drawRight = ( entry != entries().rbegin() || Flags::graphical_output_style == JLQNDEF_STYLE );
	}
    }

    /* Move entries */

    const double ty = y + height();
    double tx = myNode->left() + offset + fill;
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	(*entry)->moveTo( tx, ty - (*entry)->height() );
	tx += (*entry)->width() - adjustForSlope( fabs( (*entry)->height() ) ) + fill;
    }

    /* Move activities */

    if ( _layers.size() ) {

	for ( std::vector<ActivityLayer>::reverse_iterator layer = _layers.rbegin(); layer != _layers.rend(); ++layer ) {
	    layer->moveTo( x, y );
	    y += layer->height();  /* grow down */

 	    if ( layer->height() > Flags::entry_height ) {
 		layer->moveBy( 0.0, layer->height() - Flags::entry_height );
 	    }
	}

	for ( std::vector<ActivityLayer>::iterator layer = _layers.begin(); layer != _layers.end(); ++layer ) {
	    layer->sort( Activity::compare ).reformat( 0 );
	}

	switch( Flags::activity_justification ) {
	case ALIGN_JUSTIFY:
	case DEFAULT_JUSTIFY:
	    justifyByEntry();
	    break;
	default:
	    justify();
	    break;
	}
    }

    sort();		/* Reorder arcs */
    moveDst();
    moveSrc();		/* Move arc associated with processor */

    return *this;
}



/*
 * Move activities according to the justification type.
 */

double
Task::justify()
{
    double left  = MAXDOUBLE;
    double right = 0.0;

    for ( std::vector<ActivityLayer>::iterator layer = _layers.begin(); layer != _layers.end(); ++layer ) {
	left  = std::min( left,  layer->x() );
	right = std::max( right, layer->x() + layer->width() );
    }

    for ( std::vector<ActivityLayer>::iterator layer = _layers.begin(); layer != _layers.end(); ++layer ) {
	layer->justify( right - left, Flags::activity_justification );
    }

    return right - left;
}




/*
 * Move activities so they line up with the entries.
 */

double
Task::justifyByEntry()
{
    const unsigned MAX_LEVEL = _layers.size();
    double width = 0;

    /*
     * Find all activities reachable from this entry...
     * and stick them into the sublayer
     */

    double x = left() + adjustForSlope( fabs( height() ) ) + Flags::act_x_spacing / 4.;
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	double right = 0.0;
	double left  = MAXDOUBLE;
	Activity * startActivity = (*entry)->startActivity();
	if ( !startActivity ) continue;

	/* Sublayer will only contain activities reachable from current entry */

	std::vector<ActivityLayer> sublayer(MAX_LEVEL);

	int i = 0;
	for ( std::vector<ActivityLayer>::iterator layer = _layers.begin(); layer != _layers.end(); ++layer, ++i ) {
	    if ( !*layer ) continue;
	    for ( std::vector<Activity *>::const_iterator activity = layer->activities().begin(); activity != layer->activities().end(); ++activity ) {
		if ( (*activity)->reachedFrom() == startActivity ) {
		    sublayer.at(i) += (*activity);
		}
	    }
	}

	/* Now, move all activities for this entry together */

	i = _layers.size();
	for ( std::vector<ActivityLayer>::reverse_iterator layer = _layers.rbegin(); layer != _layers.rend(); ++layer ) {
	    i -= 1;
	    if ( !*layer ) continue;
	    sublayer.at(i).reformat( 0 );
	    right = std::max( right, sublayer[i].x() + sublayer[i].width() );
	    left  = std::min( left,  sublayer[i].x() );
	}

	/* Justify the current "slice", then move it to its column */

	for ( unsigned i = 0; i < MAX_LEVEL; ++i ) {
	    sublayer[i].justify( right - left, Flags::activity_justification ).moveBy( x, 0 );
	}

	if ( Flags::activity_justification == ALIGN_JUSTIFY ) {
	    double shift = 0;
	    for ( unsigned i = 0; i < MAX_LEVEL; ++i ) {
		sublayer[i].alignActivities();
		shift = std::max( x - sublayer[i].x(), shift );	// If we've moved left too far, we'll have to shift everything.
	    }
	    for ( unsigned i = 0; i < MAX_LEVEL; ++i ) {
		if ( shift > 0 ) {
		    sublayer[i].moveBy( shift, 0 );
		}
		right = std::max( right, sublayer[i].x() + sublayer[i].width() - x );	// Don't forget to subtract x!
	    }
	}

	/* The next column starts here. */

	x += right;
	width = x;
	x += Flags::print[X_SPACING].value.f;
    }

    return width - (left() + adjustForSlope( fabs( height() ) ) );
//    return width - left();
}



double
Task::alignActivities()
{
    double minLeft  = MAXDOUBLE;
    double maxRight = 0;

    for ( std::vector<ActivityLayer>::iterator layer = _layers.begin(); layer != _layers.end(); ++layer ) {
	if ( !*layer ) continue;
	layer->alignActivities();
	minLeft  = std::min( minLeft, layer->x() );
	maxRight = std::max( maxRight, layer->x() + layer->width() );

    }
    return maxRight;
}


/*
 * Move the entity, it's entries and activities.  Don't recompute everything.
 */

Task&
Task::moveBy( const double dx, const double dy )
{
    myNode->moveBy( dx, dy );
    myLabel->moveBy( dx, dy );
    for_each( entries().begin(), entries().end(), ExecXY<Element>( &Entry::moveBy, dx, dy ) );    		/* Move entries */
    for_each( _layers.begin(), _layers.end(), ExecXY<ActivityLayer>( &ActivityLayer::moveBy, dx, dy ) );	/* Move activities */
    for_each( _layers.rbegin(), _layers.rend(), ExecXY<ActivityLayer>( &ActivityLayer::moveBy, 0, 0 ) );	/* clean up.*/

    sort();			/* Reorder arcs */
    moveDst();			/* Move arcs calling me.	      */
    moveSrc();			/* Move arc associated with processor */

    return *this;
}


/*
 * Move the entity, it's entries and activities.  Recompute everything.
 */

Task&
Task::moveTo( const double x, const double y )
{
    myNode->moveTo( x, y );

    reformat();

    if ( Flags::print[AGGREGATION].value.i == AGGREGATE_ENTRIES ) {
	myLabel->moveTo( bottomCenter() ).moveBy( 0, height() / 2 );
    } else if ( !queueing_output() ) {

	/* Move Label -- do after X extent recalculated */

	if ( Flags::graphical_output_style == JLQNDEF_STYLE ) {
	    myLabel->moveTo( topRight() ).moveBy( -(Flags::entry_width * JLQNDEF_TASK_BOX_SCALING * 0.5), -entries().front()->height()/2 );
	} else if ( _layers.size() ) {
	    myLabel->moveTo( topCenter() ).moveBy( 0, -entries().front()->height() - 10 );
	} else {
	    myLabel->moveTo( bottomCenter() ).moveBy( 0, height() / 5 );
	}
    } else {
	myLabel->moveTo( bottomCenter() ).moveBy( 0, height() / 5 );
    }

    return *this;
}


/*
 * Move the arc associated with the processor.  Offset the point
 * based on the location of the processor.
 * Invoked by the processor and by Task::moveTo().
 */

Task&
Task::moveSrc()
{
    if ( Flags::graphical_output_style == JLQNDEF_STYLE ) {
	Point aPoint = bottomRight();
	aPoint.moveBy( -(Flags::entry_width * JLQNDEF_TASK_BOX_SCALING)/2.0, 0 );
	calls().front()->moveSrc( aPoint );
    } else if ( calls().size() == 1 && dynamic_cast<ProcessorCall *>(calls().front()) ) {
	Point aPoint = bottomCenter();
	double diff = aPoint.x() - processor()->center().x();
	if ( diff > 0 && fabs( diff ) > width() / 4 ) {
	    aPoint.moveBy( -width() / 4, 0 );
	} else if ( diff < 0 && fabs( diff ) > width() / 4 ) {
	    aPoint.moveBy( width() / 4, 0 );
	}
	calls().front()->moveSrc( aPoint );
    } else {
	const int nFwd = countArcs( &GenericCall::hasForwardingLevel );
	Point aPoint = bottomLeft();
	const double delta = width() / static_cast<double>(countArcs() + 1 - nFwd);

	for ( std::vector<EntityCall *>::const_iterator call = calls().begin(); call != calls().end(); ++call ) {
	    if ( (*call)->isSelected()  && !(*call)->hasForwardingLevel() ) {
		aPoint.moveBy( delta, 0 );
		(*call)->moveSrc( aPoint );
	    }
	}
    }
    return *this;
}


/*
 * Move all arcs I sink.  This is only really applicable iff we're doing
 * -Ztasks-only.
 */

Task&
Task::moveDst()
{
    Point aPoint = myNode->topLeft();

    if ( Flags::print_forwarding_by_depth ) {
	const double delta = width() / static_cast<double>(countCallers() + 1);

	/* Draw other incomming arcs. */

	for ( std::vector<GenericCall *>::const_iterator call = callers().begin(); call != callers().end(); ++call ) {
	    if ( (*call)->isSelected() ) {
		aPoint.moveBy( delta, 0 );
		(*call)->moveDst( aPoint );
	    }
	}
    } else {
	/*
	 * We add the outgoing forwarding arcs to the incomming side of the entry,
	 * so adjust the counts as necessary.
	 */

	const int nFwd = countArcs( &GenericCall::hasForwardingLevel );
	const double delta = width() / static_cast<double>(countCallers() + 1 + nFwd );
	const double fy = Flags::print[Y_SPACING].value.f / 2.0 + top();

	/* Draw incomming forwarding arcs first. */

	Point fwdPoint( left(), fy );
	for ( std::vector<GenericCall *>::const_iterator call = callers().begin(); call != callers().end(); ++call ) {
	    if ( (*call)->isSelected() && (*call)->hasForwardingLevel() ) {
		aPoint.moveBy( delta, 0 );
		(*call)->moveDst( aPoint ).movePenultimate( fwdPoint );
		fwdPoint.moveBy( delta, 0 );
	    }
	}

	/* Draw other incomming arcs.  Note that secondPoint() and penultimatePoint() don't work here because   */
	/* Arc should only have two points (so the source/destination may get clobbered)			*/

	for ( std::vector<GenericCall *>::const_iterator call = callers().begin(); call != callers().end(); ++call ) {
	    if ( (*call)->isSelected() && !(*call)->hasForwardingLevel() ) {
		aPoint.moveBy( delta, 0 );
		if ( (*call)->hasForwarding() && (*call)->nPoints() == 4 ) {
		    (*call)->pointAt(1) = aPoint;
		    (*call)->pointAt(2) = aPoint;
		}
		(*call)->moveDst( aPoint );
	    }
	}

	/* Draw outgoing forwarding arcs */

	fwdPoint.moveTo( right(), fy );
	for ( std::vector<EntityCall *>::const_iterator call = calls().begin(); call != calls().end(); ++call ) {
	    if ( (*call)->isSelected() && (*call)->hasForwardingLevel() ) {
		aPoint.moveBy( delta, 0 );
		(*call)->moveSrc( aPoint ).moveSecond( fwdPoint );
		fwdPoint.moveBy( delta, 0 );
	    }
	}
    }

    return *this;
}


/*
 * Move the arc associated with the processor.  Offset the point
 * based on the location of the processor.
 * Invoked by the processor and by Task::moveTo().
 */

Task&
Task::moveSrcBy( const double dx, const double dy )
{
    for_each( calls().begin(), calls().end(), ExecXY<GenericCall>( &GenericCall::moveSrcBy, dx, dy ) );
    return *this;
}


Graphic::colour_type Task::colour() const
{
    switch ( Flags::print[COLOUR].value.i ) {
    case COLOUR_DIFFERENCES:
	return colourForDifference( throughput() );

    default:
	return Entity::colour();
    }
}


/*
 * Label the node.
 */

Entity&
Task::label()
{
    if ( queueing_output() ) {
	bool print_goop = false;
	if ( Flags::print[INPUT_PARAMETERS].value.b ) {
	    labelQueueingNetwork( &Entry::labelQueueingNetworkVisits );
	    print_goop = true;
	}
	if ( Flags::have_results && Flags::print[WAITING].value.b ) {
	    labelQueueingNetwork( &Entry::labelQueueingNetworkWaiting );
	    print_goop = true;
	}
	if ( print_goop ) {
	    myLabel->newLine();
	}
    }
    Entity::label();
    if ( !queueing_output() ) {
	myLabel->justification( Flags::label_justification );
    }
    if ( Flags::print[INPUT_PARAMETERS].value.b ) {
	if ( queueing_output() ) {
	    if ( !isSelected() ) {
		const double Z = Flags::have_results ? (copiesValue() - utilization()) / throughput() : 0.0;
		if ( Z > 0.0 ) {
		    myLabel->newLine() << " Z = " << Z;
		}
	    }
	    labelQueueingNetwork( &Entry::labelQueueingNetworkService );
	} else {
	    if ( Flags::print[AGGREGATION].value.i == AGGREGATE_ENTRIES && Flags::print[PRINT_AGGREGATE].value.b ) {
		myLabel->newLine() << " [" << print_service_time( *entries().front() ) << ']';
	    }
	    if ( hasThinkTime()  ) {
		*myLabel << " Z=" << dynamic_cast<ReferenceTask *>(this)->thinkTime();
	    }
	}

    }
    if ( Flags::have_results ) {
 	bool print_goop = false;
	if ( Flags::print[TASK_THROUGHPUT].value.b ) {
	    myLabel->newLine();
	    if ( throughput() == 0.0 && Flags::print[COLOUR].value.i != COLOUR_OFF ) {
		myLabel->colour( Graphic::RED );
	    }
	    *myLabel << begin_math( &Label::lambda ) << "=" << opt_pct(throughput());
	    print_goop = true;
	}
	if ( Flags::print[TASK_UTILIZATION].value.b ) {
	    if ( print_goop ) {
		*myLabel << ',';
	    } else {
		myLabel->newLine() << begin_math();
		print_goop = true;
	    }
	    *myLabel << _rho() << "=" << opt_pct(utilization());
	    if ( hasBogusUtilization() && Flags::print[COLOUR].value.i != COLOUR_OFF ) {
		myLabel->colour(Graphic::RED);
	    }
	}
	if ( print_goop ) {
	    *myLabel << end_math();
	}
    }

    for_each( entries().begin(), entries().end(), Exec<Element>( &Element::label ) );       	/* Now label entries. */
    for_each( activities().begin(), activities().end(), Exec<Activity>( &Activity::label ) );	/* And activities... */
    for_each( calls().begin(), calls().end(), Exec<GenericCall>( &GenericCall::label ) );   	/* And the outgoing arcs, if any */
    for_each( precedences().begin(), precedences().end(), Exec<ActivityList>( &ActivityList::label ) );
    for_each( _layers.rbegin(), _layers.rend(), Exec<ActivityLayer>( &ActivityLayer::label ) );
    return *this;
}



/*
 * Stick labels in for queueing network.
 */

Task&
Task::labelQueueingNetwork( entryLabelFunc aFunc )
{
    for_each( _entries.begin(), _entries.end(), Exec1<Entry,Label&>( aFunc, *myLabel ) );
    return *this;
}



/*
 * Move the entity and it's entries.
 */

Task&
Task::scaleBy( const double sx, const double sy )
{
    Entity::scaleBy( sx, sy );
    for_each( entries().begin(), entries().end(), ExecXY<Element>( &Element::scaleBy, sx, sy ) );
    for_each( activities().begin(), activities().end(), ExecXY<Element>( &Element::scaleBy, sx, sy ) );
    for_each( precedences().begin(), precedences().end(), ExecXY<ActivityList>( &ActivityList::scaleBy, sx, sy ) ) ;
    for_each( calls().begin(), calls().end(), ExecXY<GenericCall>( &GenericCall::scaleBy, sx, sy ) );
    return *this;
}



/*
 * Move the entity and it's entries.
 */

Task&
Task::depth( const unsigned depth )
{
    Entity::depth( depth );
    for_each( entries().begin(), entries().end(), Exec1<Element,unsigned int>( &Element::depth, depth ) );
    for_each( activities().begin(), activities().end(), Exec1<Element,unsigned int>( &Element::depth, depth ) );
    for_each( precedences().begin(), precedences().end(), Exec1<ActivityList,unsigned int>( &ActivityList::depth, depth ) );
    for_each( calls().begin(), calls().end(), Exec1<GenericCall,unsigned int>( &GenericCall::depth, depth ) );
    return *this;
}



/*
 * Move the entity and it's entries.
 */

Task&
Task::translateY( const double dy )
{
    Entity::translateY( dy );
    for_each( entries().begin(), entries().end(), Exec1<Element,double>( &Element::translateY, dy ) );
    for_each( activities().begin(), activities().end(), Exec1<Element,double>( &Element::translateY, dy ) );
    for_each( precedences().begin(), precedences().end(), Exec1<ActivityList,double>( &ActivityList::translateY, dy ) );
    for_each( calls().begin(), calls().end(), Exec1<GenericCall,double>( &GenericCall::translateY, dy ) );
    return *this;
}

#if defined(REP2FLAT)
Task&
Task::removeReplication()
{
    Entity::removeReplication();

    LQIO::DOM::Task * dom = const_cast<LQIO::DOM::Task *>(dynamic_cast<const LQIO::DOM::Task *>(getDOM()));
    for ( std::map<const std::string, LQIO::DOM::ExternalVariable *>::const_iterator fi = dom->getFanIns().begin(); fi != dom->getFanIns().end(); ++fi ) {
	dom->setFanIn( fi->first, new LQIO::DOM::ConstantExternalVariable( 1 ) );
    }
    for ( std::map<const std::string, LQIO::DOM::ExternalVariable *>::const_iterator fo = dom->getFanOuts().begin(); fo != dom->getFanOuts().end(); ++fo ) {
	dom->setFanOut( fo->first, new LQIO::DOM::ConstantExternalVariable( 1 ) );
    }

    return *this;
}


Task&
Task::expandTask()
{
    const unsigned int numTaskReplicas = replicasValue();
    const unsigned int procFanOut = numTaskReplicas / processor()->replicasValue();

    for ( unsigned int replica = 1; replica <= numTaskReplicas; ++replica ) {

	/* Get a pointer to the replicated processor */

	const unsigned int proc_replica = static_cast<unsigned int>(static_cast<double>(replica-1) / static_cast<double>(procFanOut)) + 1;
	const Processor *aProcessor = Processor::find_replica( processor()->name(), proc_replica );

	std::ostringstream aName;
	aName << name() << "_" << replica;
	std::set<Task *>::const_iterator nextTask = find_if( __tasks.begin(), __tasks.end(), eqTaskStr( aName.str() ) );
	if ( nextTask != __tasks.end() ) {
	    std::string msg = "Task::expandTask(): cannot add symbol ";
	    msg += aName.str();
	    throw std::runtime_error( msg );
	}
	Task * aTask = clone( replica, aName.str(), aProcessor, share() );
	aTask->myPaths = myPaths;		// Bad hack?
	aTask->setLevel( level() );
	__tasks.insert( aTask );

	std::vector<LQIO::DOM::Entry *>& dom_entries = const_cast<std::vector<LQIO::DOM::Entry *>&>( dynamic_cast<const LQIO::DOM::Task *>(aTask->getDOM())->getEntryList() );
	dom_entries.clear();
	for ( std::vector<Entry *>::const_iterator entry = aTask->entries().begin(); entry != aTask->entries().end(); ++entry ) {
	    LQIO::DOM::Entry * dom_entry = const_cast<LQIO::DOM::Entry *>(dynamic_cast<const LQIO::DOM::Entry *>((*entry)->getDOM()));
	    dom_entries.push_back( dom_entry );        /* Add to task. */
	}

	/* Handle group if necessary */

	if ( hasActivities() ) {
	    aTask->expandActivities( *this, replica );
	}
    }
    return *this;
}


const std::vector<Entry *>&
Task::groupEntries( int replica, std::vector<Entry *>& newEntryList  ) const
{
    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	newEntryList.push_back( Entry::find_replica( (*entry)->name(), replica ) );
    }
    return newEntryList;
}



Task&
Task::expandActivities( const Task& src, int replica )
{
    /* clone the activities */
    LQIO::DOM::Task * task_dom = const_cast<LQIO::DOM::Task *>(dynamic_cast<const LQIO::DOM::Task *>(getDOM()));

    for ( std::vector<Activity *>::const_iterator activity = src.activities().begin(); activity != src.activities().end(); ++activity ) {
	addActivity(**activity, replica);
    }

    /* Now reconnect the precedences.  Do the join list from the task.  We have to connect the fork list. */

    for ( std::vector<ActivityList *>::const_iterator precedence = src.precedences().begin(); precedence != src.precedences().end(); ++precedence ) {
	if ( !dynamic_cast<const JoinActivityList *>((*precedence))  && !dynamic_cast<const AndOrJoinActivityList *>((*precedence)) ) continue;

	/* Ok we have the join list. */

	ActivityList * pre_list = (*precedence)->clone();
	pre_list->setOwner( this );
	addPrecedence( pre_list );
	LQIO::DOM::ActivityList * pre_list_dom = const_cast<LQIO::DOM::ActivityList *>(pre_list->getDOM());
	task_dom->addActivityList( pre_list_dom );
	pre_list_dom->setTask( task_dom );

	/* Now reconnect the activities. A little kludgey because we have a list in one case and not the other. */

	std::vector<Activity *> pre_act_list;
	if (dynamic_cast<const ForkJoinActivityList *>((*precedence))) {
	    pre_act_list = dynamic_cast<const ForkJoinActivityList *>((*precedence))->activityList();
	} else if (dynamic_cast<const SequentialActivityList *>((*precedence))) {
	    pre_act_list.push_back( dynamic_cast<const SequentialActivityList *>((*precedence))->getMyActivity());
	}

	for ( std::vector<Activity *>::const_iterator activity = pre_act_list.begin(); activity != pre_act_list.end(); ++activity ) {
	    Activity * pre_activity = findActivity(*(*activity), replica);
	    LQIO::DOM::Activity * pre_activity_dom = const_cast<LQIO::DOM::Activity *>(dynamic_cast<const LQIO::DOM::Activity *>(pre_activity->getDOM()));
	    pre_activity->outputTo( pre_list );
	    pre_activity_dom->outputTo( pre_list_dom );
	    pre_list->add( pre_activity );
	    if ( pre_list_dom != nullptr ) pre_list_dom->add( pre_activity_dom );
	}

	const ActivityList *dstPrecedence = (*precedence)->next();

	if ( dstPrecedence ) {

	    /* Here is the fork list */

	    ActivityList * post_list = dstPrecedence->clone();
	    post_list->setOwner( this );
	    addPrecedence( post_list );
	    LQIO::DOM::ActivityList * post_list_dom = const_cast<LQIO::DOM::ActivityList *>(post_list->getDOM());
	    post_list_dom->setTask( task_dom );
	    task_dom->addActivityList( post_list_dom );
	    ActivityList::act_connect( pre_list, post_list );
	    pre_list_dom->setNext( post_list_dom );
	    post_list_dom->setPrevious( pre_list_dom );

	    /* Now reconnect the activities. A little kludgey because we have a list in one case and not the other. */

	    std::vector<Activity *> post_act_list;
	    if (dynamic_cast<const ForkJoinActivityList *>(dstPrecedence)) {
		post_act_list = dynamic_cast<const ForkJoinActivityList *>(dstPrecedence)->activityList();
	    } else if (dynamic_cast<const RepeatActivityList *>(dstPrecedence)) {
		post_act_list = dynamic_cast<const RepeatActivityList *>(dstPrecedence)->activityList();
	    } else if (dynamic_cast<const SequentialActivityList *>(dstPrecedence)) {
		post_act_list.push_back(dynamic_cast<const SequentialActivityList *>(dstPrecedence)->getMyActivity());
	    }

	    for ( std::vector<Activity *>::const_iterator activity = post_act_list.begin(); activity != post_act_list.end(); ++activity ) {
		Activity * post_activity = findActivity(*(*activity), replica);
		LQIO::DOM::Activity * post_activity_dom = const_cast<LQIO::DOM::Activity *>(dynamic_cast<const LQIO::DOM::Activity *>(post_activity->getDOM()));
		post_activity->inputFrom( post_list );
		post_activity_dom->inputFrom( post_list_dom );
		post_list->add( post_activity );

		/* A little tricky here.  We need to copy over whatever parameter there was. */

		const LQIO::DOM::ActivityList * dstPrecedenceDOM = dstPrecedence->getDOM();
		LQIO::DOM::ExternalVariable * parameter = dstPrecedenceDOM->getParameter( dynamic_cast<const LQIO::DOM::Activity *>((*activity)->getDOM()) );
		post_list_dom->add( post_activity_dom, parameter );
	    }

	}
    }
    return *this;
}



LQIO::DOM::Task *
Task::cloneDOM( const std::string& aName, LQIO::DOM::Processor * dom_processor ) const
{
    LQIO::DOM::Task * dom_task = new LQIO::DOM::Task( *dynamic_cast<const LQIO::DOM::Task*>(getDOM()) );

    dom_task->setName( aName );
    dom_task->setProcessor( dom_processor );
    const_cast<LQIO::DOM::Document *>(getDOM()->getDocument())->addTaskEntity( dom_task );	    /* Reconnect all of the dom stuff. */
    dom_processor->addTask( dom_task );

    return dom_task;
}


static struct {
    set_function first;
    get_function second;
} task_mean[] = { 
// static std::pair<set_function,get_function> task_mean[] = {
    { &LQIO::DOM::DocumentObject::setResultProcessorUtilization, &LQIO::DOM::DocumentObject::getResultProcessorUtilization },
    { &LQIO::DOM::DocumentObject::setResultThroughput, &LQIO::DOM::DocumentObject::getResultThroughput },
    { &LQIO::DOM::DocumentObject::setResultUtilization, &LQIO::DOM::DocumentObject::getResultUtilization },
    { NULL, NULL }
};

static struct {
    set_function first;
    get_function second;
} task_variance[] = { 
//static std::pair<set_function,get_function> task_variance[] = {
    { &LQIO::DOM::DocumentObject::setResultProcessorUtilizationVariance, &LQIO::DOM::DocumentObject::getResultProcessorUtilizationVariance },
    { &LQIO::DOM::DocumentObject::setResultThroughputVariance, &LQIO::DOM::DocumentObject::getResultThroughputVariance },
    { &LQIO::DOM::DocumentObject::setResultUtilizationVariance, &LQIO::DOM::DocumentObject::getResultUtilizationVariance },
    { NULL, NULL }
};


/*
 * Rename XXX_1 to XXX and reinsert the task and its dom into their associated arrays.   XXX_2 and up will be discarded.
 */

Task&
Task::replicateTask( LQIO::DOM::DocumentObject ** root )
{
    unsigned int replica = 0;
    std::string root_name = baseReplicaName( replica );
    LQIO::DOM::Task * task = dynamic_cast<LQIO::DOM::Task *>(*root);
    if ( replica == 1 ) {
	*root = const_cast<LQIO::DOM::DocumentObject *>(getDOM());
	task = dynamic_cast<LQIO::DOM::Task *>(*root);
	std::pair<std::set<Task *>::iterator,bool> rc = __tasks.insert( this );
	if ( !rc.second ) throw std::runtime_error( "Duplicate task" );
	task->setName( root_name );
	const_cast<LQIO::DOM::Processor *>(task->getProcessor())->addTask( task );		/* Add back (for XML output) */
	/* Group too?? */
	const_cast<LQIO::DOM::Document *>((*root)->getDocument())->addTaskEntity( task );	/* Reconnect all of the dom stuff. */
    } else if ( task->getReplicasValue() < replica ) {
	task->setReplicasValue( replica );
	for ( unsigned int i = 0; task_mean[i].first != NULL; ++i ) {
	    update_mean( task, task_mean[i].first, getDOM(), task_mean[i].second, replica );
	    update_variance( task, task_variance[i].first, getDOM(), task_variance[i].second );
	}
    }

    for ( std::vector<Activity *>::const_iterator a = activities().begin(); a != activities().end(); ++a ) {
	const LQIO::DOM::Activity * src = dynamic_cast<const LQIO::DOM::Activity *>((*a)->getDOM());		/* The replica 1, 2,... dom */

	/* 
	 * Strip the _n from the replica name.  After pass 1 (replica==1), the "roots" activity
	 * won't have the _n either, so it can be found 
	 */
	
	std::string& name = const_cast<std::string&>(src->getName());
	size_t pos = name.rfind( '_' );
	name = name.substr( 0, pos );
	LQIO::DOM::Activity * activity = const_cast<const LQIO::DOM::Task *>(task)->getActivity(name);
	(*a)->replicateActivity( activity, replica );
    }
    return *this;
}


Task&
Task::replicateCall() 
{
    unsigned int replica = 0;
    std::string root_name = baseReplicaName( replica );
    if ( replica == 1 ) {
	for_each( activities().begin(), activities().end(), Exec<Activity>( &Activity::replicateCall ) );
    }
    return *this;
}

/*
 * Called AFTER replicas are set in all NEW model elements.
 */

/* static */ void
Task::updateFanInOut()
{
    for ( std::set<Task *>::const_iterator src = __tasks.begin(); src != __tasks.end(); ++src ) {
	const LQIO::DOM::Task * src_dom = dynamic_cast<const LQIO::DOM::Task *>((*src)->getDOM());
	const std::vector<Entry *>& entries = (*src)->entries();
	for_each( entries.begin(), entries.end(), UpdateFanInOut( *const_cast<LQIO::DOM::Task *>(src_dom)) );
	const std::vector<Activity *>& activities = (*src)->activities();
	for_each( activities.begin(), activities.end(), UpdateFanInOut( *const_cast<LQIO::DOM::Task *>(src_dom)) );
    }
}

void Task::UpdateFanInOut::operator()( Entry * entry ) const { updateFanInOut( entry->calls() ); }
void Task::UpdateFanInOut::operator()( Activity * activity ) const { updateFanInOut( activity->calls() ); }

void 
Task::UpdateFanInOut::updateFanInOut( const std::vector<Call *>& calls ) const
{
    const unsigned src_replicas = _src.getReplicasValue();
    for ( std::vector<Call *>::const_iterator call = calls.begin(); call != calls.end(); ++call ) {
	LQIO::DOM::Task& dst = *const_cast<LQIO::DOM::Task *>(dynamic_cast<const LQIO::DOM::Task *>((*call)->dstTask()->getDOM()));
	const unsigned dst_replicas = dst.getReplicasValue();
	if ( dst_replicas > src_replicas ) {
	    double fan_out = static_cast<double>(dst_replicas) / static_cast<double>(src_replicas);
	    if ( fan_out == rint( fan_out ) ) {
		_src.setFanOutValue( dst.getName(), static_cast<unsigned>(fan_out) );
	    } else {
		_src.setFanOutValue( dst.getName(), dst_replicas );
		dst.setFanInValue( _src.getName(), src_replicas );
	    }
	} else if ( src_replicas > dst_replicas ) {
	    double fan_in = static_cast<double>(src_replicas) / static_cast<double>(dst_replicas);
	    if ( fan_in == rint( fan_in ) ) {
		dst.setFanInValue( _src.getName(), static_cast<unsigned>(fan_in) );
	    } else {
		_src.setFanOutValue( dst.getName(), dst_replicas );
		dst.setFanInValue( _src.getName(), src_replicas );
	    }
	}
    }
}
#endif

/* ------------------------------------------------------------------------ */
/*                                  Output                                 */
/* ------------------------------------------------------------------------ */

/*
 * Draw the SRVN model object.
 */

const Task&
Task::draw( std::ostream& output ) const
{
    std::ostringstream aComment;
    aComment << "Task " << name()
	     << task_scheduling_of( *this )
	     << entries_of( *this );
    if ( processor() ) {
	aComment << " " << processor()->name();
    }
#if defined(BUG_375)
    aComment << " span=" << span() << ", index=" << index();
#endif
    myNode->comment( output, aComment.str() );
    myNode->fillColour( colour() );
    if ( Flags::print[COLOUR].value.i == COLOUR_OFF ) {
	myNode->penColour( Graphic::DEFAULT_COLOUR );			// No colour.
    } else if ( throughput() == 0.0 ) {
	myNode->penColour( Graphic::RED );
    } else if ( colour() == Graphic::GREY_10 ) {
	myNode->penColour( Graphic::BLACK );
    } else {
	myNode->penColour( colour() );
    }

    std::vector<Point> points(4);
    const double dx = adjustForSlope( fabs(height()) );
    points[0] = topLeft().moveBy( dx, 0 );
    points[1] = topRight();
    points[2] = bottomRight().moveBy( -dx, 0 );
    points[3] = bottomLeft();

    if ( isMultiServer() || isInfinite() || isReplicated() ) {
	std::vector<Point> copies = points;
	const double delta = -2.0 * Model::scaling() * myNode->direction();
	for_each( copies.begin(), copies.end(), ExecXY<Point>( &Point::moveBy, 2.0 * Model::scaling(), delta ) );
	const int aDepth = myNode->depth();
	myNode->depth( aDepth + 1 );
	myNode->polygon( output, copies );
	myNode->depth( aDepth );
    }
    if ( Flags::graphical_output_style == JLQNDEF_STYLE ) {
	const double shift = width() - (Flags::entry_width * JLQNDEF_TASK_BOX_SCALING * Model::scaling());
	points[0].moveBy( shift, 0 );
	points[3].moveBy( shift, 0 );
	if ( Flags::print[COLOUR].value.i == COLOUR_OFF ) {
	    myNode->fillColour( Graphic::GREY_10 );
	}
    }
    myNode->polygon( output, points );

    myLabel->backgroundColour( colour() ).comment( output, aComment.str() );
    output << *myLabel;

    if ( Flags::print[AGGREGATION].value.i != AGGREGATE_ENTRIES ) {
	for_each( entries().begin(), entries().end(), ConstExec1<Element,std::ostream&>( &Element::draw, output ) );
	for_each( activities().begin(), activities().end(), ConstExec1<Element,std::ostream&>( &Element::draw, output ) );
	for_each( precedences().begin(), precedences().end(), ConstExec1<ActivityList,std::ostream&>( &ActivityList::draw, output ) );
    }

    for_each( calls().begin(), calls().end(), ConstExec1<GenericCall,std::ostream&>( &GenericCall::draw, output ) );

    return *this;
}

/* ------------------------------------------------------------------------ */
/*                           Draw the queuieng network                      */
/* ------------------------------------------------------------------------ */

/*
 * Draw the queueing model object.
 */

std::ostream&
Task::drawClient( std::ostream& output, const bool is_in_open_model, const bool is_in_closed_model ) const
{
    std::string aComment;
    aComment += "========== ";
    aComment += name();
    aComment += " ==========";
    myNode->comment( output, aComment );
    myNode->penColour( colour() == Graphic::GREY_10 ? Graphic::BLACK : colour() ).fillColour( colour() );

    myLabel->moveTo( bottomCenter() ).justification( LEFT_JUSTIFY );
    if ( is_in_open_model && is_in_closed_model ) {
	Point aPoint = bottomCenter();
	aPoint.moveBy( radius() * -3.0, 0 );
	myNode->multi_server( output, aPoint, radius() );
	aPoint = bottomCenter().moveBy( radius() * 1.5, 0 );
	myNode->open_source( output, aPoint, radius() );
	myLabel->moveBy( radius() * 1, radius() * 3 * myNode->direction() );
    } else if ( is_in_open_model ) {
	myNode->open_source( output, bottomCenter(), radius() );
	myLabel->moveBy( radius() * 1, radius() * myNode->direction() );
    } else {
	myNode->multi_server( output, bottomCenter(), radius() );
	myLabel->moveBy( radius() * 1, radius() * 3 * myNode->direction() );
    }
    output << *myLabel;
    return output;
}


#if defined(PMIF_OUTPUT)
/*
 *
 */

const Task&
Task::setClientDemand( LQIO::PMIF_Document::Station& station, const std::vector<Entity *>& entities ) const
{
    for ( std::vector<EntityCall *>::const_iterator call = calls().begin(); call != calls().end(); ++call ) {
	const TaskCall * task_call = dynamic_cast<const TaskCall *>(*call);
	if ( !task_call ) continue;
    }

    return *this;
}
#endif

/* ------------------------- Reference Tasks -------------------------- */

ReferenceTask::ReferenceTask( const LQIO::DOM::Task* dom, const Processor * aProc, const Share * aShare, const std::vector<Entry *>& entries )
    : Task( dom, aProc, aShare, entries )
{
}

int ReferenceTask::rootLevel() const
{
    return Model::CLIENT_LEVEL;
}

ReferenceTask *
ReferenceTask::clone( unsigned int replica, const std::string& aName, const Processor * aProcessor, const Share * aShare ) const
{
    LQIO::DOM::Task * dom_task = cloneDOM( aName, const_cast<LQIO::DOM::Processor *>(dynamic_cast<const LQIO::DOM::Processor *>(aProcessor->getDOM()) ) );

//	.setGroup( aShare->getDOM() );

    std::vector<Entry *> entries;
    return new ReferenceTask( dom_task, aProcessor, aShare, groupEntries( replica, entries ) );
}


bool
ReferenceTask::hasThinkTime() const
{
    return dynamic_cast<const LQIO::DOM::Task *>(getDOM())->hasThinkTime();
}

LQIO::DOM::ExternalVariable&
ReferenceTask::thinkTime() const
{
    return *dynamic_cast<const LQIO::DOM::Task *>(getDOM())->getThinkTime();
}


/*
 * Reference tasks are always fully utilized, but never a performance
 * problem, so always draw them black.
 */

Graphic::colour_type
ReferenceTask::colour() const
{
    switch ( Flags::print[COLOUR].value.i ) {
    case COLOUR_SERVER_TYPE:
	return Graphic::RED;

    case COLOUR_RESULTS:
	return processor()->colour();
	break;

    default:
	return Task::colour();
    }
}


/*
 * Recursively find all children and grand children from `father'.  As
 * we descend down, we bump the depth.  If our path's cross, we have a
 * loop and abort.
 */

size_t
ReferenceTask::findChildren( CallStack& callStack, const unsigned directPath )
{
    size_t max_depth = std::max( callStack.size(), level() );

    setLevel( max_depth ).addPath( directPath );

    if ( processor() ) {
	max_depth = std::max( max_depth + 1, processor()->level() );
	const_cast<Processor *>(processor())->setLevel( max_depth ).addPath( directPath );
    }

    for ( std::vector<Entry *>::const_iterator entry = entries().begin(); entry != entries().end(); ++entry ) {
	max_depth = std::max( max_depth, (*entry)->findChildren( callStack, directPath ) );
    }

    return max_depth;
}


bool
Task::canConvertToReferenceTask() const
{
    return Flags::convert_to_reference_task
      && (submodel_output()
#if HAVE_REGEX_T
	  || Flags::print[INCLUDE_ONLY].value.r
#endif
      )
      && !isSelected()
      && !hasOpenArrivals()
      && !isInfinite()
      && nEntries() == 1
      && processor();
}

/* --------------------------- Server Tasks --------------------------- */

ServerTask::ServerTask( const LQIO::DOM::Task* dom, const Processor * aProc, const Share * aShare, const std::vector<Entry *>& entries )
    : Task( dom, aProc, aShare, entries )
{
}


ServerTask*
ServerTask::clone( unsigned int replica, const std::string& aName, const Processor * aProcessor, const Share * aShare ) const
{
    LQIO::DOM::Task * dom_task = cloneDOM( aName, const_cast<LQIO::DOM::Processor *>(dynamic_cast<const LQIO::DOM::Processor *>(aProcessor->getDOM()) ) );

//	.setGroup( aShare->getDOM() );

    std::vector<Entry *> entries;
    return new ServerTask( dom_task, aProcessor, aShare, groupEntries( replica, entries ) );
}


/*
 * If we are converting a model and simplifying, in some instances we
 * can convert a server to a reference task.
 */

bool
ServerTask::canConvertToReferenceTask() const
{
    return Flags::convert_to_reference_task
      && (submodel_output()
#if HAVE_REGEX_T
	  || Flags::print[INCLUDE_ONLY].value.r
#endif
	  )
      && !isSelected()
      && !hasOpenArrivals()
      && !isInfinite()
      && nEntries() == 1
      && processor();
}

/*+ BUG_164 */
/* -------------------------- Semaphore Tasks ------------------------- */

SemaphoreTask::SemaphoreTask( const LQIO::DOM::Task* dom, const Processor * aProc, const Share * aShare, const std::vector<Entry *>& entries )
    : Task( dom, aProc, aShare, entries )
{
}


SemaphoreTask*
SemaphoreTask::clone( unsigned int replica, const std::string& aName, const Processor * aProcessor, const Share * aShare ) const
{
    LQIO::DOM::Task * dom_task = cloneDOM( aName, const_cast<LQIO::DOM::Processor *>(dynamic_cast<const LQIO::DOM::Processor *>(aProcessor->getDOM()) ) );

    std::vector<Entry *> entries;
    return new SemaphoreTask( dom_task, aProcessor, aShare, groupEntries( replica, entries ) );
}


/* -------------------------- RWLOCK Tasks ------------------------- */

RWLockTask::RWLockTask( const LQIO::DOM::Task* dom, const Processor * aProc, const Share * aShare, const std::vector<Entry *>& entries )
    : Task( dom, aProc, aShare, entries )
{
}


RWLockTask*
RWLockTask::clone( unsigned int replica, const std::string& aName, const Processor * aProcessor, const Share * aShare ) const
{
    LQIO::DOM::Task * dom_task = cloneDOM( aName, const_cast<LQIO::DOM::Processor *>(dynamic_cast<const LQIO::DOM::Processor *>(aProcessor->getDOM()) ) );

    std::vector<Entry *> entries;
    return new RWLockTask( dom_task, aProcessor, aShare, groupEntries( replica, entries ) );
}


/*----------------------------------------------------------------------*/
/*		 	   Called from yyarse.  			*/
/*----------------------------------------------------------------------*/

/*
 * Add a task to the model.  Called by the parser.
 */

Task *
Task::create( const LQIO::DOM::Task* domTask, std::vector<Entry *>& entries )
{
    /* Recover the old parameter information that used to be passed in */
    const char* task_name = domTask->getName().c_str();
    const LQIO::DOM::Group * domGroup = domTask->getGroup();
    const scheduling_type sched_type = domTask->getSchedulingType();

    if ( !task_name || strlen( task_name ) == 0 ) abort();

    if ( entries.size() == 0 ) {
	LQIO::input_error2( LQIO::ERR_NO_ENTRIES_DEFINED_FOR_TASK, task_name );
	return nullptr;
    }
    if ( find_if( __tasks.begin(), __tasks.end(), eqTaskStr( task_name ) ) != __tasks.end() ) {
	LQIO::input_error2( LQIO::ERR_DUPLICATE_SYMBOL, "Task", task_name );
	return nullptr;
    }

    const std::string& processor_name = domTask->getProcessor()->getName();
    Processor * aProcessor = Processor::find( processor_name );
    if ( !aProcessor ) {
	LQIO::input_error2( LQIO::ERR_NOT_DEFINED, processor_name.c_str() );
    }

    const Share * aShare = 0;
    if ( !domGroup && aProcessor->scheduling() == SCHEDULE_CFS ) {
	LQIO::input_error2( LQIO::ERR_NO_GROUP_SPECIFIED, task_name, processor_name.c_str() );
    } else if ( domGroup ) {
	std::set<Share *>::const_iterator nextShare = find_if( aProcessor->shares().begin(), aProcessor->shares().end(), EQStr<Share>( domGroup->getName() ) );
	if ( nextShare == aProcessor->shares().end() ) {
	    LQIO::input_error2( LQIO::ERR_NOT_DEFINED, domGroup->getName().c_str() );
	} else {
	    aShare = *nextShare;
	}
    }

    /* Pick-a-task */

    Task * aTask;
    switch ( sched_type ) {
    case SCHEDULE_UNIFORM:
    case SCHEDULE_BURST:
    case SCHEDULE_CUSTOMER:
	aTask = new ReferenceTask( domTask, aProcessor, aShare, entries );
	break;

    case SCHEDULE_FIFO:
    case SCHEDULE_PPR:
    case SCHEDULE_HOL:
    case SCHEDULE_DELAY:
	aTask = new ServerTask( domTask, aProcessor, aShare, entries );
	break;

    case SCHEDULE_SEMAPHORE:
	if ( entries.size() != 2 ) {
	    ::LQIO::input_error2( LQIO::ERR_ENTRY_COUNT_FOR_TASK, task_name, entries.size(), 2 );
	}
	aTask = new SemaphoreTask( domTask, aProcessor, aShare, entries );
	break;

	case SCHEDULE_RWLOCK:
	if ( entries.size() != N_RWLOCK_ENTRIES ) {
	    ::LQIO::input_error2( LQIO::ERR_ENTRY_COUNT_FOR_TASK, task_name, entries.size(), N_RWLOCK_ENTRIES );
	}
	aTask = new RWLockTask( domTask, aProcessor, aShare, entries );
	break;

    default:
	LQIO::input_error2( LQIO::WRN_SCHEDULING_NOT_SUPPORTED, scheduling_label[(unsigned)sched_type].str, "task", task_name );
	aTask = new ServerTask( domTask, aProcessor, aShare, entries );
	break;
    }

    __tasks.insert( aTask );
    return aTask;
}

/* ---------------------------------------------------------------------- */

static std::ostream&
entries_of_str( std::ostream& output, const Task& aTask )
{
    for ( std::vector<Entry *>::const_reverse_iterator entry = aTask.entries().rbegin(); entry != aTask.entries().rend(); ++entry ) {
	if ( (*entry)->pathTest() ) {
	    output << " " << (*entry)->name();
	}
    }
    output << " -1";
    return output;
}

/*
 * Print out of scheduling flag field.  See ../lqio input.h
 */

static std::ostream&
task_scheduling_of_str( std::ostream& output, const Task & aTask )
{
    output << ' ';
    switch ( aTask.scheduling() ) {
    case SCHEDULE_BURST:     output << 'b'; break;
    case SCHEDULE_CUSTOMER:  output << 'r'; break;
    case SCHEDULE_DELAY:     output << 'n'; break;
    case SCHEDULE_FIFO:	     output << 'n'; break;
    case SCHEDULE_HOL:	     output << 'h'; break;
    case SCHEDULE_POLL:      output << 'P'; break;
    case SCHEDULE_PPR:	     output << 'p'; break;
    case SCHEDULE_RWLOCK:    output << 'W'; break;
    case SCHEDULE_UNIFORM:   output << 'u'; break;
    default:	   	     output << '?'; break;
    }
    return output;
}


