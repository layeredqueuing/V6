/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* May 1996								*/
/* June 2009								*/
/************************************************************************/

/*
 * Input output processing.
 *
 * $Id: task.cc 17465 2024-11-13 13:11:32Z greg $
 */

#include "lqsim.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <lqio/input.h>
#include <numeric>
#include <sstream>
#include <lqio/labels.h>
#include <lqio/error.h>
#include <lqio/dom_actlist.h>
#include "activity.h"
#include "entry.h"
#include "errmsg.h"
#include "group.h"
#include "histogram.h"
#include "instance.h"
#include "processor.h"
#include "task.h"

#define N_SEMAPHORE_ENTRIES 2
#define N_RWLOCK_ENTRIES 4

std::set<Task *, Task::ltTask> Task::__tasks;	/* Task table.	*/

const std::map<const Task::Type,const std::string> Task::type_strings =  {
    { Task::Type::UNDEFINED,              "Undefined" },
    { Task::Type::CLIENT,                 "client" },
    { Task::Type::SERVER,                 "server" },
    { Task::Type::MULTI_SERVER,           "multi" },
    { Task::Type::INFINITE_SERVER,        "infsrv" },
    { Task::Type::SYNCHRONIZATION_SERVER, "sync" },
    { Task::Type::SEMAPHORE,              "semph" },
    { Task::Type::OPEN_ARRIVAL_SOURCE,    "open" },
    { Task::Type::WORKER,                 "worker" },
    { Task::Type::THREAD,                 "thread" },
    { Task::Type::TOKEN,                  "token" },
    { Task::Type::TOKEN_R,                "token_r" },
    { Task::Type::SIGNAL,                 "signal" },
    { Task::Type::RWLOCK,                 "rw_lock" },
    { Task::Type::RWLOCK_SERVER,          "rwlock" },
    { Task::Type::WRITER_TOKEN,           "token_w" },
    { Task::Type::TIMEOUT_QUEUE,          "timeout_queue" },
    { Task::Type::TIMEOUT_WORKER,         "timeout_worker" },
    { Task::Type::RETRY_QUEUE,            "retry_queue" },
    { Task::Type::RETRY_WORKER,           "retry_worker" }
};


/*
 * n_tasks represents the number of task classes defined in the input
 * file.  total_tasks is the total number of tasks created.  The latter
 * will always be greater than or equal to the former because it includes
 * all "clones" of multi-server tasks.  Use n_tasks as an upper bound to
 * class_tab, and total_tasks as an upper bound to object_tab.
 */

unsigned total_tasks = 0;

Task::Task( const Task::Type type, int priority, LQIO::DOM::Task* dom, Processor * processor, Group * a_group )
    : _dom(dom),
      _priority(priority),
      _processor(processor),
      _group_id(-1),
      _compute_func(nullptr),
      _active(0),
      _max_phases(1),
      _entries(),
      _activities(),
      _precedences(),
      _forks(),
      _joins(),
      _pending_msgs(),
      _join_start_time(0.0),
      _free_msgs(),
      _type(type),
      trace_flag(false),
      _hist_data(nullptr),
      r_cycle("Cycle time",dom),
      r_util("Utilization",dom),
      r_group_util("Group Utilization",dom),
      _hold_active(0)
{
    if ( processor ) {
	processor->add_task(this);
    }
}


Task::~Task()
{
    std::for_each( activities().begin(), activities().end(), []( Activity * activity ){ delete activity; } );
    std::for_each( precedences().begin(), precedences().end(), []( ActivityList * list ){ delete list; } );
    std::for_each( _free_msgs.begin(), _free_msgs.end(), []( Message * message ){ delete message; } );

    if ( _hist_data ) {
	delete _hist_data;
    }
}


/*
 * Configure entries and activities.  Count up the number of
 * activities.  Do final list construction (i.e., tie up loose ends.)
 */

Task&
Task::configure()
{
    /* I need the instance variable "task" set from this point on. */

    double total_calls = std::accumulate( activities().begin(), activities().end(), 0.0, []( double l, Activity * r ){ return l + r->configure(); } );
    std::for_each( precedences().begin(), precedences().end(), std::mem_fn( &ActivityList::configure ) );
    total_calls = std::accumulate( entries().begin(), entries().end(), total_calls, []( double l, Entry * r ) { return l + r->configure(); } );

    if ( total_calls == 0 && is_reference_task() ) {
	getDOM()->runtime_error( LQIO::WRN_NOT_USED );
    }

    for ( std::vector<Activity *>::const_iterator ap = activities().begin(); ap != activities().end(); ++ap ) {
	if ( !(*ap)->is_reachable() ) {
	    (*ap)->getDOM()->runtime_error( LQIO::WRN_NOT_USED );
	} else if ( !(*ap)->is_specified() ) {
	    (*ap)->getDOM()->runtime_error( LQIO::ERR_NOT_SPECIFIED );
	}
    }
    return *this;
}


/*
 * Construct the parasol entity.
 */

Task&
Task::create()
{
    /*
     * Set compute function.  We have a special case for when no
     * processor has been allocated (eg, processor 0).  In this case,
     * tasks compute by "sleeping".
     */

    _compute_func = (!processor() || processor()->is_infinite()) ? ps_sleep : ps_compute;

    /* JOIN Stuff -- All entries are free. */

    trace_flag	= std::regex_match( name(), task_match_pattern );

    if ( debug_flag ){
	(void) fprintf( stddbg, "\n-+++++---- %s task %s", type_name().c_str(), name().c_str() );
	if ( _compute_func == ps_sleep ) {
	    (void) fprintf( stddbg, " [delay]" );
	}
	(void) fprintf( stddbg, " ----+++++-\n" );
    }

    /* Compute PDF for all activities for each task. */

    create_instance();
    initialize();

    if ( has_send_no_reply() ) {
	alloc_pool();
    }

    return *this;
}


Task&
Task::initialize()
{
    std::for_each( activities().begin(), activities().end(), std::mem_fn( &Activity::initialize ) );
    std::for_each( entries().begin(), entries().end(), std::mem_fn( &Entry::initialize ) );
    std::for_each( _forks.begin(), _forks.end(), std::mem_fn( &AndForkActivityList::initialize ) );

    /*
     * Allocate statistics for joins.  We do it here because we
     * don't know how many joins we have when we allocate the task
     * class object.
     */

    std::for_each( _joins.begin(), _joins.end(), std::mem_fn( &AndJoinActivityList::initialize ) );

    /* statistics */
    _active = 0;		/* Reset counts */
    _hold_active = 0;

    return *this;
}

int
Task::node_id() const
{
    return processor() ? processor()->node_id() : 0;
}

void
Task::set_start_activity( LQIO::DOM::Entry* dom )
{
    Entry * ep = Entry::find( dom->getName() );

    if ( !ep ) return;
    if ( !ep->test_and_set( LQIO::DOM::Entry::Type::ACTIVITY ) ) return;

    const LQIO::DOM::Activity * activity_dom = dom->getStartActivity();
    const char * activity_name = activity_dom->getName().c_str();
    Activity * ap = find_activity( activity_name );
    if ( !ap ) {
	LQIO::input_error( LQIO::ERR_NOT_DEFINED, activity_name );
    } else if ( ep->get_start_activity() != nullptr ) {
	ep->getDOM()->input_error( LQIO::ERR_DUPLICATE_START_ACTIVITY, activity_name );
    } else {
	ep->set_start_activity( ap );
	ap->set_is_start_activity(true);
    }
}


bool
Task::has_send_no_reply() const
{
    return std::any_of( entries().begin(), entries().end(), std::mem_fn( &Entry::is_send_no_reply ) );
}

/*
 * Create a new activity assigned to a given task and set the information DOM entry for it
 */

Activity *
Task::add_activity( LQIO::DOM::Activity * dom )
{
    Activity * activity = find_activity( dom->getName() );
    if ( activity ) {
	LQIO::input_error( LQIO::ERR_DUPLICATE_SYMBOL, "Activity", dom->getName().c_str() );
    } else {
	activity = new Activity( this, dom );
	_activities.push_back( activity );
    }
    return activity;
}




/*
 * Find the activity.  Return error if not found.
 */

Activity *
Task::find_activity( const std::string& name ) const
{
    std::vector<Activity *>::const_iterator ap = std::find_if( activities().begin(), activities().end(), [=]( const Activity * activity ){ return activity->name() == name; } );
    if ( ap != activities().end() ) {
	return *ap;
    } else {
	return nullptr;
    }
}


/*
 * allocate message pool for asynchronous messages.
 */

void
Task::alloc_pool()
{
    unsigned size = DEFAULT_QUEUE_SIZE;
    if ( getDOM()->hasQueueLength() ) {
	try {
	    size = getDOM()->getQueueLengthValue();
	}
	catch ( const std::domain_error& e ) {
	    getDOM()->throw_invalid_parameter( "pool size", e.what() );
	}
    }
    for ( unsigned int i = 0; i < size; ++i ) {
	_free_msgs.push_back( new Message );
    }
}


Message *
Task::alloc_message()
{
    Message * msg = nullptr;
    if ( _free_msgs.size() > 0 ) {
	msg = _free_msgs.front();
	_free_msgs.pop_front();
    }
    return msg;
}

void
Task::free_message( Message * msg )
{
    /* Async message -- acummulate queuing + service (M/G/m model) */

    double delta = ps_now - msg->time_stamp;
    tar_t *tp = msg->target;
    tp->r_delay.record( delta );
    tp->r_delay_sqr.record( square( delta ) );
    _free_msgs.push_back( msg );
}

double
Task::throughput() const
{
    switch ( type() ) {
    case Task::Type::SEMAPHORE: return r_cycle.mean_count() / (Model::block_period() * n_entries()); 	/* Only count for one entry.  */
    case Task::Type::RWLOCK:	return r_cycle.mean_count() / (Model::block_period() * n_entries()/2); 	/* Only count for two entries.  */
    case Task::Type::SERVER:	if ( is_sync_server() ) return  r_cycle.mean_count() / (Model::block_period() * n_entries());
	/* Fall through */
    default: 		   	return r_cycle.mean_count() / Model::block_period();
    }
}



double
Task::throughput_variance() const
{
    switch ( type() ) {
    case Task::Type::SEMAPHORE: return r_cycle.variance_count() / (square(Model::block_period()) * n_entries());
    case Task::Type::RWLOCK:    return r_cycle.variance_count() / (square(Model::block_period()) * n_entries()/2);
    case Task::Type::SERVER:	if ( is_sync_server() ) return r_cycle.variance_count() / (square(Model::block_period()) * n_entries());
	/* Fall through */
    default:		   	return r_cycle.variance_count() / square(Model::block_period());
    }
}

Task&
Task::reset_stats()
{
    r_util.reset();
    r_cycle.reset();

    std::for_each( entries().begin(), entries().end(), std::mem_fn( &Entry::reset_stats ) );
    std::for_each( activities().begin(), activities().end(), std::mem_fn( &Activity::reset_stats ) );
    std::for_each( _joins.begin(), _joins.end(), std::mem_fn( &AndJoinActivityList::reset_stats ) );

    /* Histogram stuff */

    if ( _hist_data ) {
	_hist_data->reset();
    }
    return *this;
}


Task&
Task::accumulate_data()
{
    if ( type() == Task::Type::UNDEFINED ) return *this;    /* Some tasks don't have statistics */

    r_util.accumulate();
    r_cycle.accumulate();

    std::for_each( entries().begin(), entries().end(), std::mem_fn( &Entry::accumulate_data ) );
    std::for_each( activities().begin(), activities().end(), std::mem_fn( &Activity::accumulate_data ) );
    std::for_each( _joins.begin(), _joins.end(), std::mem_fn( &AndJoinActivityList::accumulate_data ) );

    /* Histogram stuff */

    if ( _hist_data ) {
	_hist_data->accumulate_data();
    }
    return *this;
}


std::ostream&
Task::print( std::ostream& output ) const
{
    output << r_util << r_cycle;	// Change me.

    std::for_each( entries().begin(), entries().end(), [&]( const Entry * entry ){ entry->print( output ); } );
    std::for_each( activities().begin(), activities().end(), [&]( const Activity* activity ){ activity->print( output ); } );
    std::for_each( _joins.begin(), _joins.end(), [&]( const AndJoinActivityList * join ){ join->print( output ); } );

    return output;
}

Task&
Task::insertDOMResults()
{
    /* Some tasks don't have statistics */

    if ( type() == Task::Type::UNDEFINED ) return *this;

    double phaseUtil[MAX_PHASES];
    double phaseVar[MAX_PHASES];
    for ( unsigned p = 0; p < MAX_PHASES; p++ ) {
	phaseUtil[p] = 0.0;
	phaseVar[p] = 0.0;
    }

    if ( has_activities() ) {
	std::for_each( activities().begin(), activities().end(), std::mem_fn( &Activity::insertDOMResults ) );
	std::for_each( _joins.begin(), _joins.end(), std::mem_fn( &AndJoinActivityList::insertDOMResults ) );
    }

    double taskProcUtil = 0.0;		/* Total processor utilization. */
    double taskProcVar = 0.0;
    for ( std::vector<Entry *>::const_iterator nextEntry = entries().begin(); nextEntry != entries().end(); ++nextEntry ) {
	Entry * ep = *nextEntry;
	ep->insertDOMResults();

	for ( unsigned p = 0; p < max_phases(); ++p ) {
	    phaseUtil[p] += ep->_phase[p].r_util.mean();
	    phaseVar[p]  += ep->_phase[p].r_util.variance();
	    taskProcUtil += ep->_phase[p].r_cpu_util.mean();
	    taskProcVar  += ep->_phase[p].r_cpu_util.variance();
	}
    }

    for ( std::vector<Activity *>::const_iterator activity = activities().begin(); activity != activities().end(); ++activity ) {
	taskProcUtil += (*activity)->r_cpu_util.mean();
	taskProcVar  += (*activity)->r_cpu_util.variance();
    }

    /* Store totals */

    r_util.insertDOMResults( &LQIO::DOM::DocumentObject::setResultUtilization, &LQIO::DOM::DocumentObject::setResultUtilizationVariance );
    getDOM()->setResultPhaseUtilizations(max_phases(),phaseUtil)
//	.setResultUtilization(r_util.mean())
	.setResultThroughput(throughput())
	.setResultProcessorUtilization(taskProcUtil);

    if ( number_blocks > 1 ) {
	getDOM()->setResultPhaseUtilizationVariances(max_phases(),phaseVar)
//	    .setResultUtilizationVariance(r_util.variance())
	    .setResultThroughputVariance(throughput_variance())
	    .setResultProcessorUtilizationVariance(taskProcVar);
    }

    if ( _hist_data ) {
	_hist_data->insertDOMResults();
    }
    return *this;
}

/*----------------------------------------------------------------------*/
/*	 Input processing.  Called from configure() 			*/
/*----------------------------------------------------------------------*/

/*
 * Add a task to the model.
 */

Task *
Task::add( LQIO::DOM::Task* dom )
{
    /* Recover the old parameter information that used to be passed in */

    if ( Task::find( dom->getName() ) ) {
	dom->runtime_error( LQIO::ERR_DUPLICATE_SYMBOL );
    }
    if ( dom->hasReplicas() ) {
	dom->runtime_error( LQIO::ERR_NOT_SUPPORTED, "replication" );
    }

    const LQIO::DOM::Group * domGroup = dom->getGroup();
    const scheduling_type sched_type = dom->getSchedulingType();

    Task * cp = nullptr;
    const std::string& processor_name = dom->getProcessor()->getName();
    Processor * processor = Processor::find( processor_name );

    if ( !LQIO::DOM::Common_IO::is_default_value( dom->getPriority(), 0 ) && ( processor->discipline() == SCHEDULE_FIFO
										   || processor->discipline() == SCHEDULE_PS
										   || processor->discipline() == SCHEDULE_RAND ) ) {
	dom->runtime_error( LQIO::WRN_PRIO_TASK_ON_FIFO_PROC, processor_name.c_str() );
    }

    int priority = dom->hasPriority() ? dom->getPriorityValue() : 0;
    if ( priority < MIN_PRIORITY || MAX_PRIORITY < priority ) {
	priority = dom->getPriorityValue() > MAX_PRIORITY ? MAX_PRIORITY : 0;
	LQIO::runtime_error( WRN_INVALID_PRIORITY, dom->getPriorityValue(), MIN_PRIORITY, MAX_PRIORITY, priority );
    }
//    priority = MAX_PRIORITY - priority;		/* Bug 453. */

    Group * group = nullptr;
    if ( !domGroup && processor->discipline() == SCHEDULE_CFS ) {
	dom->runtime_error( LQIO::ERR_NO_GROUP_SPECIFIED, processor_name.c_str() );
    } else if ( domGroup ) {
	group = Group::find( domGroup->getName().c_str() );
	if ( !group ) {
	    LQIO::runtime_error( LQIO::ERR_NOT_DEFINED, domGroup->getName().c_str() );
	}
    }

    switch ( sched_type ) {
    case SCHEDULE_BURST:
    case SCHEDULE_UNIFORM:
    case SCHEDULE_CUSTOMER:
	if ( dom->hasQueueLength() ) {
	    dom->runtime_error( LQIO::WRN_TASK_QUEUE_LENGTH );
	}
	if ( dom->isInfinite() ) {
	    dom->runtime_error( LQIO::ERR_REFERENCE_TASK_IS_INFINITE );
	}
	if ( dom->getEntryList().size() != 1 ) {
	    LQIO::error_severity old = LQIO::DOM::DocumentObject::setSeverity( LQIO::ERR_TASK_ENTRY_COUNT, LQIO::error_severity::WARNING );
	    dom->runtime_error( LQIO::ERR_TASK_ENTRY_COUNT, dom->getEntryList().size(), 1 );
	    LQIO::DOM::DocumentObject::setSeverity( LQIO::ERR_TASK_ENTRY_COUNT, old );
	}
	cp = new Reference_Task( Task::Type::CLIENT, priority, dom, processor, group );
	break;

    case SCHEDULE_PPR:
    case SCHEDULE_HOL:
    case SCHEDULE_FIFO:
	if ( dom->hasThinkTime() ) {
	    dom->runtime_error( LQIO::ERR_NON_REF_THINK_TIME );
	}
	Task::Type a_type;

	if ( dom->isInfinite() ) {
	    a_type = Task::Type::INFINITE_SERVER;
	} else if ( dom->isMultiserver() ) {
	    a_type = Task::Type::MULTI_SERVER;
	} else {
	    a_type = Task::Type::SERVER;
	}
	cp = new Server_Task( a_type, priority, dom, processor, group );
	break;

    case SCHEDULE_DELAY:
	if ( dom->hasThinkTime() ) {
	    dom->runtime_error( LQIO::ERR_NON_REF_THINK_TIME );
	}
	if ( dom->isMultiserver() ) {
	    dom->runtime_error( LQIO::WRN_INFINITE_MULTI_SERVER, dom->getCopiesValue() );
	}
	if ( dom->hasQueueLength() ) {
	    dom->runtime_error( LQIO::WRN_TASK_QUEUE_LENGTH );
	}
	cp = new Server_Task( Task::Type::INFINITE_SERVER, priority, dom, processor, group );
	break;

/*+ BUG_164 */
    case SCHEDULE_SEMAPHORE:
	if ( dom->hasQueueLength()  ) {
	    dom->runtime_error( LQIO::WRN_TASK_QUEUE_LENGTH );
	}
	if ( dom->isInfinite() ) {
	    dom->runtime_error( LQIO::ERR_INFINITE_SERVER );
	}
 	cp = new Semaphore_Task( Task::Type::SEMAPHORE, priority, dom, processor, group );
	break;
/*- BUG_164 */

/* reader_writer lock */

    case SCHEDULE_RWLOCK:
	if ( dom->hasQueueLength() ) {
	    dom->runtime_error( LQIO::WRN_TASK_QUEUE_LENGTH );
	}
	if ( dom->isInfinite() ) {
	    dom->runtime_error( LQIO::ERR_INFINITE_SERVER );
	}
 	cp = new ReadWriteLock_Task( Task::Type::RWLOCK, priority, dom, processor, group );
	break;
/* reader_writer lock*/

/* Decision tasks */
    case SCHEDULE_ABORT:
    case SCHEDULE_TIMEOUT:
	cp = new Timeout_Task( Task::Type::TIMEOUT_QUEUE, priority, dom, processor, group );
	break;

    case SCHEDULE_INF_RETRY:
    case SCHEDULE_RETRY:

	cp = new Retry_Task( Task::Type::RETRY_QUEUE, priority, dom, processor, group );
	break;
/* Decision tasks */

    default:
	cp = new Server_Task( Task::Type::SERVER, priority, dom, processor, group );		/* Punt... */
	dom->runtime_error( LQIO::WRN_SCHEDULING_NOT_SUPPORTED, scheduling_label.at(sched_type).str.c_str() );
	break;
    }

    Task::__tasks.insert( cp );

    return cp;
}


/*
 * Located the task id.
 */

Task *
Task::find( const std::string& task_name )
{
    std::set<Task *>::const_iterator nextTask = std::find_if( Task::__tasks.begin(), Task::__tasks.end(), [=]( const Task * task ){ return task->name() == task_name; } );
    if ( nextTask == Task::__tasks.end() ) {
	return nullptr;
    } else {
	return *nextTask;
    }
}


/*
 * We need a way to fake out infinity... so if copies is infinite,
 * then we change to one.  Reference tasks can never be infinite.
 */

unsigned
Task::multiplicity() const
{
    unsigned int value = 1;
    if ( !getDOM()->isInfinite() || is_reference_task() ) {
	try {
	    value = getDOM()->getCopiesValue();
	}
	catch ( const std::domain_error& e ) {
	    getDOM()->throw_invalid_parameter( "multiplicity", e.what() );
	}
    }
    return value;
}


bool
Task::is_infinite() const
{
    return getDOM()->isInfinite();
}


/*
 * Return true if any entry or activity has a think time value.  If
 * so, Model::extend() will then create an entry to a thinker device.
 */

bool
Task::has_think_time() const
{
    return std::any_of( entries().begin(), entries().end(), std::mem_fn( &Entry::has_think_time ) )
	|| std::any_of( activities().begin(), activities().end(), std::mem_fn( &Activity::has_think_time ) )
	|| getDOM() != nullptr && getDOM()->hasThinkTime();
}


bool
Task::has_lost_messages() const
{
    return std::any_of( entries().begin(), entries().end(), std::mem_fn( &Entry::has_lost_messages ) );
}


bool
Task::derive_utilization() const
{
    return processor()->derive_utilization();
}

/* ------------------------------------------------------------------------ */

Reference_Task::Reference_Task( const Task::Type type, int priority, LQIO::DOM::Task* dom, Processor * aProc, Group * aGroup )
    : Task( type, priority, dom, aProc, aGroup ), _think_time(0.0), _clients()
{
}


void
Reference_Task::create_instance()
{
    if ( getDOM()->hasThinkTime() ) {
	_think_time = getDOM()->getThinkTimeValue();
    }
    _clients.clear();
    for ( unsigned i = 0; i < multiplicity(); ++i ) {
	_clients.push_back( new srn_client( this, name() ) );
    }
}


#if !BUG_289
bool
Reference_Task::start()
{
    for ( std::vector<srn_client *>::const_iterator t = _clients.begin(); t != _clients.end(); ++t ) {
	if ( ps_resume( (*t)->task_id() ) != OK ) return false;
    }
    return true;
}
#endif


Reference_Task&
Reference_Task::kill()
{
    for ( std::vector<srn_client *>::const_iterator t = _clients.begin(); t != _clients.end(); ++t ) {
	ps_kill( (*t)->task_id() );
    }
    return *this;
}

bool
Reference_Task::is_not_waiting() const
{
    for ( std::vector<srn_client *>::const_iterator t = _clients.begin(); t != _clients.end(); ++t ) {
	if ( ps_task_state( (*t)->task_id() ) != TASK_RECEIVING ) return true;
    }
    return false;
}

/* ------------------------------------------------------------------------ */

Server_Task::Server_Task( const Task::Type type, int priority, LQIO::DOM::Task* dom, Processor * aProc, Group * aGroup )
    : Task( type, priority, dom, aProc, aGroup ),
      _task(0),
      _worker_port(-1),
      _sync_server(false)
{
}

int
Server_Task::std_port() const
{
    return ps_std_port(_task->task_id());
}


bool
Server_Task::is_not_waiting() const
{
    return ps_task_state( _task->task_id() ) != TASK_RECEIVING;
}

void
Server_Task::set_synchronization_server()
{
    _sync_server = true;
}


bool
Server_Task::is_aysnc_inf_server() const
{
    return std::any_of( entries().begin(), entries().end(), std::mem_fn( &Entry::is_send_no_reply ) );
}


/*
 * Define task type at run time.  It may change because of LQX
 * execution.  Simple servers are more efficient than multi-servers
 * with 1 worker.
 */

void
Server_Task::create_instance()
{
    if ( is_infinite() ) {
	_task = new srn_multiserver( this, name(), ~0 );
	_worker_port = ps_allocate_port( name().c_str(), _task->task_id() );
	_type = Task::Type::INFINITE_SERVER;
	if ( is_aysnc_inf_server() ) {
	    getDOM()->runtime_error( LQIO::WRN_INFINITE_SERVER_OPEN_ARRIVALS );
	}
    } else if ( is_multiserver() ) {
	_task = new srn_multiserver( this, name(), multiplicity() );
	_worker_port = ps_allocate_port( name().c_str(), _task->task_id() );
	_type = Task::Type::MULTI_SERVER;
    } else {
	_task = new srn_server( this, name() );
	_worker_port = -1;
	_type = Task::Type::SERVER;
    }
}


bool
Server_Task::start()
{
    return ps_resume( _task->task_id() ) == OK;
}


Server_Task&
Server_Task::kill()
{
    ps_suspend( _task->task_id() );
    ps_kill( _task->task_id() );
    _task = 0;
    _worker_port = -1;
    return *this;
}

/* ------------------------------------------------------------------------ */

Semaphore_Task::Semaphore_Task( const Task::Type type, int priority, LQIO::DOM::Task* dom, Processor * aProc, Group * aGroup )
    : Server_Task( type, priority, dom, aProc, aGroup ),
      r_hold("Hold Time",dom),
      r_hold_sqr("Hold Time Sq",dom),
      r_hold_util("Hold Utilization",dom),
      _signal_task(0),
      _signal_port(-1)
{
}


void
Semaphore_Task::create_instance()
{
    Entry * entry_1 = entries().at(0);
    Entry * entry_2 = entries().at(1);
	
    if ( n_entries() != N_SEMAPHORE_ENTRIES ) {
	getDOM()->runtime_error( LQIO::ERR_TASK_ENTRY_COUNT, n_entries(), N_SEMAPHORE_ENTRIES );
    } else if ( (entry_1->is_signal() && !entry_2->test_and_set_semaphore( LQIO::DOM::Entry::Semaphore::WAIT ) )
	 || ( entry_1->is_wait() && !entry_2->test_and_set_semaphore( LQIO::DOM::Entry::Semaphore::SIGNAL ) ) ) {
	getDOM()->runtime_error( LQIO::ERR_DUPLICATE_SEMAPHORE_ENTRY_TYPES,
				 entry_1->getDOM()->getName().c_str(),
				 entry_2->getDOM()->getName().c_str(),
				 entry_1->is_signal() ? "signal" : "wait" );
    } else if ( entry_1->is_wait() && !entry_1->test_and_set_recv( Entry::Type::RENDEZVOUS ) ) {
	entry_1->getDOM()->runtime_error( LQIO::ERR_ASYNC_REQUEST_TO_WAIT );
    } else if ( entry_2->is_wait() && !entry_2->test_and_set_recv( Entry::Type::RENDEZVOUS ) ) {
	entry_2->getDOM()->runtime_error( LQIO::ERR_ASYNC_REQUEST_TO_WAIT );
    }
    if ( !_hist_data && getDOM()->hasHistogram() ) {
	_hist_data = new Histogram( getDOM()->getHistogram() );
    }

    std::string buf = name();
    buf += "-wait";
    /* entry for waiting request - send to token. */
    _task = new srn_semaphore( this, buf );
    _worker_port = ps_allocate_port( buf.c_str(), _task->task_id() );

    /* Entry for signal request */
    buf = name();
    buf += "-signal";
    _signal_task = new srn_signal( this,  buf );
    _signal_port = ps_allocate_port( buf.c_str(), _signal_task->task_id() );
}


bool
Semaphore_Task::start()
{
    if ( !Server_Task::start() ) return false;
    return ps_resume( _signal_task->task_id() ) == OK;
}

Semaphore_Task&
Semaphore_Task::kill()
{
    Server_Task::kill();
    ps_kill( _signal_task->task_id() );
    _signal_port  = -1;
    return *this;
}

Semaphore_Task&
Semaphore_Task::reset_stats()
{
    Task::reset_stats();

    r_hold.reset();
    r_hold_sqr.reset();
    r_hold_util.reset();
    return *this;
}

Semaphore_Task&
Semaphore_Task::accumulate_data()
{
    Task::accumulate_data();

    r_hold_sqr.accumulate_variance( r_hold.accumulate() );
    r_hold_util.accumulate();
    return *this;
}

Semaphore_Task&
Semaphore_Task::insertDOMResults()
{
    Task::insertDOMResults();

    LQIO::DOM::SemaphoreTask * dom = dynamic_cast<LQIO::DOM::SemaphoreTask *>(getDOM());

    dom->setResultHoldingTime(r_hold.mean());
    dom->setResultHoldingTimeVariance(r_hold.variance());
    dom->setResultVarianceHoldingTime(r_hold_sqr.mean());
    dom->setResultVarianceHoldingTimeVariance(r_hold_sqr.variance());
    dom->setResultHoldingUtilization(r_hold_util.mean());
    dom->setResultHoldingUtilizationVariance(r_hold_util.variance());
    return *this;
}



std::ostream&
Semaphore_Task::print( std::ostream& output ) const
{
    Task::print( output );
    output << r_hold
	   << r_hold_sqr
	   << r_hold_util;
    return output;
}

/* ------------------------------------------------------------------------ */

ReadWriteLock_Task::ReadWriteLock_Task( const Task::Type type, int priority, LQIO::DOM::Task* dom, Processor * proc, Group * group )
    : Semaphore_Task( type, priority, dom, proc, group ),
      r_reader_hold("Reader Hold Time",dom),
      r_reader_hold_sqr("Reader Hold Time sq",dom),
      r_reader_wait("Reader Blocked Time",dom),
      r_reader_wait_sqr("Reader Blocked Time sq",dom),
      r_reader_hold_util("Reader Hold Utilization",dom),
      r_writer_hold("Writer Hold Time",dom),
      r_writer_hold_sqr("Writer Hold Time sq",dom),
      r_writer_wait("Writer Blocked Time",dom),
      r_writer_wait_sqr("Writer Blocked Time sq",dom),
      r_writer_hold_util("Writer Hold Utilization",dom),
      _reader(nullptr),
      _writer(nullptr),
      _signal_port2(-1),
      _writerQ_port(-1),
      _readerQ_port(-1)
{
}

void
ReadWriteLock_Task::create_instance()
{
    if ( n_entries() != N_RWLOCK_ENTRIES ) {
	getDOM()->runtime_error( LQIO::ERR_TASK_ENTRY_COUNT, n_entries(), N_RWLOCK_ENTRIES );
    }

    int E[N_RWLOCK_ENTRIES];
    for (int i=0;i<N_RWLOCK_ENTRIES;i++){ E[i]=-1; }

    for (int i=0;i<N_RWLOCK_ENTRIES;i++){
	if ( entries()[i]->is_r_unlock() ) {
	    if (E[0]== -1) { E[0]=i; }
	    else{ // duplicate entry TYPE error
		LQIO::runtime_error( LQIO::ERR_DUPLICATE_SYMBOL, name().c_str() );
	    }
	} else if ( entries()[i]->is_r_lock() ) {
	    if (E[1]== -1) { E[1]=i; }
	    else{
		LQIO::runtime_error( LQIO::ERR_DUPLICATE_SYMBOL, name().c_str() );
	    }
	} else if ( entries()[i]->is_w_unlock() ) {
	    if (E[2]== -1) { E[2]=i; }
	    else{
		LQIO::runtime_error( LQIO::ERR_DUPLICATE_SYMBOL, name().c_str() );
	    }
	} else if ( entries()[i]->is_w_lock() ) {
	    if (E[3]== -1) { E[3]=i; }
	    else{
		LQIO::runtime_error( LQIO::ERR_DUPLICATE_SYMBOL, name().c_str() );
	    }
	} else {
	    LQIO::runtime_error( LQIO::ERR_NO_RWLOCK, name().c_str() );
	    std::cerr << "entry names: " << entries()[0]->name() << ", " << entries()[1]->name() <<", " << entries()[2]->name()<< ", " << entries()[3]->name() << std::endl;
	}
    }
    //test reader lock entry
    if ( !entries()[E[1]]->test_and_set_rwlock( LQIO::DOM::Entry::RWLock::READ_LOCK ) ) {
	entries()[E[1]]->getDOM()->runtime_error( LQIO::ERR_MIXED_RWLOCK_ENTRY_TYPES );
    }
    if ( !entries()[E[1]]->test_and_set_recv( Entry::Type::RENDEZVOUS ) ) {
	entries()[E[1]]->getDOM()->runtime_error( LQIO::ERR_ASYNC_REQUEST_TO_WAIT  );
    }

    //test reader unlock entry
    if ( !entries()[E[0]]->test_and_set_rwlock( LQIO::DOM::Entry::RWLock::READ_UNLOCK ) ) {
	entries()[E[0]]->getDOM()->runtime_error( LQIO::ERR_MIXED_RWLOCK_ENTRY_TYPES );
    }

    //test writer lock entry
    if ( !entries()[E[3]]->test_and_set_rwlock( LQIO::DOM::Entry::RWLock::WRITE_LOCK ) ) {
	entries()[E[3]]->getDOM()->runtime_error( LQIO::ERR_MIXED_RWLOCK_ENTRY_TYPES );
    }
    if ( !entries()[E[3]]->test_and_set_recv( Entry::Type::RENDEZVOUS ) ) {
	entries()[E[3]]->getDOM()->runtime_error( LQIO::ERR_ASYNC_REQUEST_TO_WAIT );
    }

    //test writer unlock entry
    if ( !entries()[E[2]]->test_and_set_rwlock( LQIO::DOM::Entry::RWLock::WRITE_UNLOCK ) ) {
	entries()[E[2]]->getDOM()->runtime_error( LQIO::ERR_MIXED_RWLOCK_ENTRY_TYPES );
    }



    std::string buf = name();
    buf += "-rwlock-server";
    /*  waiting for request - send to reader_queue or writer_token. */
    /*  srn_rwlock_server should not be blocked, even if number of concurrent */
    /*	readers is greater than the maximum number of the reader lock.*/
    _task = new srn_rwlock_server( this, buf.c_str() );
    _readerQ_port = ps_allocate_port( buf.c_str(), _task->task_id() );
    _writerQ_port = ps_allocate_port( buf.c_str(), _task->task_id() );
    _signal_port2 = ps_allocate_port( buf.c_str(), _task->task_id() );

    buf = name();
    buf += "-reader-queue";
    /* entry for waiting lock reader request - send to token. */
    _reader = new srn_semaphore( this, buf );
    _worker_port = ps_allocate_port( buf.c_str(), _reader->task_id() );

    /* Entry for signal request */
    buf = name();
    buf += "-reader-signal";
    _signal_task = new srn_signal( this,  buf );
    _signal_port = ps_allocate_port( buf.c_str(), _signal_task->task_id() );

    /*  writer token*/
    buf = name();
    buf += "-writer-token";
    _writer = new srn_writer_token( this, buf );
}



bool
ReadWriteLock_Task::start()
{
    return Semaphore_Task::start()		/* Starts _signal_task */
	&& ps_resume( _reader->task_id() )
	&& ps_resume( _writer->task_id() );
}


ReadWriteLock_Task&
ReadWriteLock_Task::kill()
{
    ps_suspend( _reader->task_id() );
    ps_suspend( _writer->task_id() );
    ps_suspend( _signal_task->task_id() );

    ps_kill( _reader->task_id() );
    ps_kill( _writer->task_id() );
    ps_kill( _signal_task->task_id() );

    Semaphore_Task::kill();
    return *this;
}


ReadWriteLock_Task&
ReadWriteLock_Task::reset_stats()
{
    Semaphore_Task::reset_stats();

    r_reader_hold.reset();
    r_reader_hold_sqr.reset();
    r_reader_hold_util.reset();
    r_reader_wait.reset();
    r_reader_wait_sqr.reset();
    r_writer_hold.reset();
    r_writer_hold_sqr.reset();
    r_writer_wait.reset();
    r_writer_wait_sqr.reset();
    r_writer_hold_util.reset();
    return *this;
}


ReadWriteLock_Task&
ReadWriteLock_Task::accumulate_data()
{
    Semaphore_Task::accumulate_data();

    r_reader_hold_sqr.accumulate_variance( r_reader_hold.accumulate() );
    r_writer_hold_sqr.accumulate_variance( r_writer_hold.accumulate() );
    r_reader_wait_sqr.accumulate_variance( r_reader_wait.accumulate() );
    r_writer_wait_sqr.accumulate_variance( r_writer_wait.accumulate() );
    r_reader_hold_util.accumulate();
    r_writer_hold_util.accumulate();
    return *this;
}


ReadWriteLock_Task&
ReadWriteLock_Task::insertDOMResults()
{
    Task::insertDOMResults();

    LQIO::DOM::RWLockTask * dom = dynamic_cast<LQIO::DOM::RWLockTask *>(getDOM());

    dom->setResultReaderHoldingTime( r_reader_hold.mean() ) ;
    dom->setResultWriterHoldingTime( r_writer_hold.mean() ) ;
    dom->setResultReaderHoldingTimeVariance( r_reader_hold.variance() ) ;
    dom->setResultWriterHoldingTimeVariance( r_writer_hold.variance() ) ;
    dom->setResultVarianceReaderHoldingTime( r_reader_hold_sqr.mean() ) ;
    dom->setResultVarianceWriterHoldingTime( r_writer_hold_sqr.mean() ) ;
    dom->setResultVarianceReaderHoldingTimeVariance( r_reader_hold_sqr.variance() ) ;
    dom->setResultVarianceWriterHoldingTimeVariance( r_writer_hold_sqr.variance() ) ;
    dom->setResultReaderBlockedTime( r_reader_wait.mean() ) ;
    dom->setResultWriterBlockedTime( r_writer_wait.mean() ) ;
    dom->setResultReaderBlockedTimeVariance( r_reader_wait.variance() ) ;
    dom->setResultWriterBlockedTimeVariance( r_writer_wait.variance() ) ;
    dom->setResultVarianceReaderBlockedTime( r_reader_wait_sqr.mean() ) ;
    dom->setResultVarianceWriterBlockedTime( r_writer_wait_sqr.mean() ) ;
    dom->setResultVarianceReaderBlockedTimeVariance( r_reader_wait_sqr.variance() ) ;
    dom->setResultVarianceWriterBlockedTimeVariance( r_writer_wait_sqr.variance() ) ;

    dom->setResultReaderHoldingUtilization( r_reader_hold_util.mean() ) ;
    dom->setResultReaderHoldingUtilizationVariance( r_reader_hold_util.variance()) ;
    dom->setResultWriterHoldingUtilization( r_writer_hold_util.mean() ) ;
    dom->setResultWriterHoldingUtilizationVariance( r_writer_hold_util.variance()) ;
    return *this;
}


std::ostream&
ReadWriteLock_Task::print( std::ostream& output ) const
{
    Semaphore_Task::print( output );

    output << r_reader_hold
	   << r_reader_hold_sqr
	   << r_reader_wait
	   << r_reader_wait_sqr
	   << r_reader_hold_util
	   << r_writer_hold
	   << r_writer_hold_sqr
	   << r_writer_wait
	   << r_writer_wait_sqr
	   << r_writer_hold_util;

    return output;
}
/* ------------------------------------------------------------------------ */

Timeout_Task::Timeout_Task( const Task::Type type, int priority, LQIO::DOM::Task* dom, Processor * processor, Group * group )
    : Server_Task( type, priority, dom, processor, group ),
      r_timeout("Timeout",dom),				/* Service time.		*/
      r_timeout_sqr("Timeout sq",dom),
      r_timeout_cycle("Timeout cycle",dom),		/* Cycle time.		        */
      r_timeout_util("Timeout Utilization",dom),	/* Utilization.		        */
      r_timeout_prob("Timeout Probability",dom),	/* Timeout prob.	        */
      r_forward("Timeout stamp",dom),
      r_calldelay("Timeout call delay",dom),
      _timeout(0),
      _cleanup(0),
      _server_task(nullptr),
      _ready_port(-1)
{
}

Timeout_Task&
Timeout_Task::create()
{
    Task::create();
		
    LQIO::DOM::TimeoutTask * domtask =  dynamic_cast<LQIO::DOM::TimeoutTask *> (getDOM()) ;
    _timeout = domtask->getTimeoutValue();
    _cleanup = domtask->getAbortValue();

    return *this;
}

void
Timeout_Task::create_instance()
{

    if ( !_hist_data && getDOM()->hasHistogram() ) {
	_hist_data = new Histogram( getDOM()->getHistogram() );
    }

    // infinite task can not be timeout task
    if ( is_infinite() ) {
	//	LQIO::solution_error( LQIO::ERR_TIMEOUT_TASK, name() );
	// error name=Timeout task can not be an infinite task.
	std::cerr << "task names: " << name() <<  std::endl;
    }

    std::string buf = name();
    buf += "-timeout-queue";

    _task = new srn_timeout_queue( this, buf.c_str());
    _ready_port = ps_allocate_port( buf.c_str(), _task->task_id() );
    _worker_port = ps_allocate_port( buf.c_str(), _task->task_id() );
    _type = Task::Type::TIMEOUT_QUEUE;

    buf = name();
    buf += "-server";

    if ( is_multiserver() ) {

	_server_task = new srn_multiserver( this, buf.c_str(), multiplicity() );
	_server_task->setParent(_ready_port);
	//_type = Task::MULTI_SERVER;
    } else {

	_server_task = new srn_server( this, buf.c_str());
	_server_task->setParent(_ready_port);
	//_type = Task::SERVER;
    }
}


bool
Timeout_Task::start()
{
    if ( !Server_Task::start() ) return false;
    return ps_resume( _server_task->task_id() ) == OK;
}

Timeout_Task&
Timeout_Task::kill()
{
    Server_Task::kill();
    ps_kill( _server_task->task_id() );
    _ready_port  = -1;
    _worker_port = -1;
    return *this;
}

Timeout_Task&
Timeout_Task::reset_stats()
{
    Task::reset_stats();
	
    r_timeout.reset();
    r_timeout_sqr.reset();
    r_timeout_cycle.reset();
    r_timeout_util.reset();
    r_timeout_prob.reset();
    r_forward.reset();
    r_calldelay.reset();

    return *this;
}

Timeout_Task&
Timeout_Task::accumulate_data()
{
    Task::accumulate_data();

    r_timeout_sqr.accumulate_variance(r_timeout.accumulate() );
    r_timeout_cycle.accumulate() ;
    r_timeout_util.accumulate() ;
    r_timeout_prob.accumulate();
    r_forward.accumulate();
    r_calldelay.accumulate();

    return *this;
}

Timeout_Task&
Timeout_Task::insertDOMResults()
{
    Task::insertDOMResults();

    printf("%s task (%s) results:\n", type_name().c_str(), name().c_str());
    printf("- Timeout  %-11.11f  \n", r_timeout.mean());

    printf("- Timeout cycle  %-11.11f  \n", r_timeout_cycle.mean());
    printf("- Timeout cycle: mean count %-11.11f  \n", r_timeout_cycle.mean_count());
    printf("- Timeout prob %-11.11f  \n", r_timeout_prob.mean());
    printf("- Timeout prob mean count %-11.11f  \n", r_timeout_prob.mean_count());
    printf("- Timeout task cycle: mean count %-11.11f  \n", r_cycle.mean_count());
    printf("- Timeout task cycle: mean  %-11.11f  \n", r_cycle.mean());
    printf("- Timeout forward prob: mean count %-11.11f  \n", r_forward.mean_count());
    printf("- Timeout call delay mean count %-11.11f  \n", r_calldelay.mean_count());
    printf("- Timeout call delay mean  %-11.11f  \n", r_calldelay.mean());

    LQIO::DOM::TimeoutTask * dom = dynamic_cast<LQIO::DOM::TimeoutTask *>(getDOM());

    dom->setResultTimeoutProbability(r_timeout_prob.mean());
    dom->setResultTimeoutProbabilityVariance(r_timeout_prob.variance());
    dom->setResultSuccessProbability( 1.0 - r_timeout_prob.mean());

    return *this;
	
}


std::ostream&
Timeout_Task::print( std::ostream& output ) const
{
    Task::print( output );

    output << r_timeout
	   << r_timeout_sqr
	   << r_timeout_cycle
	   << r_timeout_util
	   << r_timeout_prob;
	return output;
}

/* ------------------------------------------------------------------------ */

Retry_Task::Retry_Task( const Task::Type type, int priority, LQIO::DOM::Task* dom, Processor * processor, Group * group )
    : Server_Task( type, priority, dom, processor, group ),
      r_nretry("Retry",dom),				/* Mean number of retries.	*/
      r_Yretry("total requests to Retry",dom),
      r_tretry("Retry",dom),
      r_abort_prob("Retry Probability",dom),		/* Abort prob.	                */
      _server_task(nullptr),
      _ready_port(-1)
{
}


Retry_Task&
Retry_Task::create()
{
    Task::create();		

    LQIO::DOM::RetryTask * domtask =  dynamic_cast<LQIO::DOM::RetryTask *> (getDOM()) ;
    _sleep = domtask->getSleepTimeValue();
    _cleanup = domtask->getAbortValue();
    _maxRetries = domtask->getMaxRetriesValue();
    return *this;
}

void
Retry_Task::create_instance()
{

    if ( !_hist_data && getDOM()->hasHistogram() ) {
	_hist_data = new Histogram( getDOM()->getHistogram() );
    }

    // infinite task can not be a retry task
    if ( is_infinite() ) {
	getDOM()->input_error( LQIO::ERR_INFINITE_SERVER );
    }

    std::string buf = name();
    buf += "-retry-queue";

    _task = new srn_retry_queue( this, buf.c_str());
    _ready_port = ps_allocate_port( buf.c_str(), _task->task_id() );
    _worker_port = ps_allocate_port( buf.c_str(), _task->task_id() );
    _type = Task::Type::RETRY_QUEUE;

    buf = name();
    buf += "-server";

    if ( is_multiserver() ) {

	_server_task = new srn_multiserver( this, buf.c_str(), multiplicity() );
	_server_task->setParent(_ready_port);
	//_type = Task::MULTI_SERVER;
    } else {

        _server_task = new srn_server( this, buf.c_str());
	_server_task->setParent(_ready_port);
	//_type = Task::SERVER;
    }
}


bool
Retry_Task::start()
{
    if ( !Server_Task::start() ) return false;
    return ps_resume( _server_task->task_id() ) == OK;
}

Retry_Task&
Retry_Task::kill()
{
    Server_Task::kill();
    ps_kill( _server_task->task_id() );
    _ready_port  = -1;
    _worker_port = -1;
    return *this;
}

Retry_Task&
Retry_Task::reset_stats()
{
    Task::reset_stats();
	
    r_nretry.reset();
    r_tretry.reset();
    r_Yretry.reset();
    r_abort_prob.reset();

    return *this;
}

Retry_Task&
Retry_Task::accumulate_data()
{
    Task::accumulate_data();

    r_nretry.accumulate() ;
    r_tretry.accumulate() ;
    r_Yretry.accumulate() ;
    r_abort_prob.accumulate();

    return *this;
}

Retry_Task&
Retry_Task::insertDOMResults()
{
    Task::insertDOMResults();
    printf("%s task (%s) results:\n", type_name().c_str(), name().c_str());
    printf("- nRetries  %-11.11f  \n", r_nretry.mean());
    printf("- nRetries  mean count %-11.11f  \n", r_nretry.mean_count());
    printf("- totalRequest  %-11.11f  \n", r_tretry.mean());
    printf("- total Requests  mean count %-11.11f  \n", r_tretry.mean_count());
    printf("- Y_Retry  %-11.11f  \n", r_Yretry.mean());
    printf("- Y_Retry  mean count %-11.11f  \n", r_Yretry.mean_count());
    printf("- Abort prob %-11.11f  \n", r_abort_prob.mean());
    printf("- Abort prob mean count %-11.11f  \n", r_abort_prob.mean_count());

    LQIO::DOM::RetryTask * dom = dynamic_cast<LQIO::DOM::RetryTask *>(getDOM());
    /*
      dom->setResultRetry(r_nretry.mean());
      dom->setResultRetryVariance(r_nretry.variance());
      dom->setResultVarianceRetry(r_nretry_sqr.mean());
      dom->setResultVarianceRetryVariance(r_nretry_sqr.variance());
      dom->setResultRetryCycle(r_nretry_cycle.mean());
      dom->setResultRetryCycleVariance(r_nretry_cycle.variance());
      dom->setResultRetryUtilization(r_nretry_util.mean());
      dom->setResultRetryUtilizationVariance(r_nretry_util.variance());
      dom->setResultRetryProbability(r_abort_porb.mean());
      dom->setResultRetryProbabilityVariance(r_abort_porb.variance());
    */
    dom->setResultSuccessProbability( 1.0 - r_abort_prob.mean());
    dom->setResultAbortProbability( r_abort_prob.mean());
    dom->setResultNumberOfRetries( r_nretry.mean());
    dom->setResultAbortProbabilityVariance( r_abort_prob.variance() );
    dom->setResultNumberOfRetriesVariance( r_nretry.variance() );

    return *this;
}



std::ostream&
Retry_Task::print( std::ostream& output ) const
{
    Task::print( output );

    output << r_nretry
	   << r_tretry
	   << r_Yretry
	   << r_abort_prob;
    return output;
}


/* ------------------------------------------------------------------------ */

/*
 * Pseudo Tasks are used to source open arrivals.  The DOM's of the
 * pseudo entries of this pseudo task are the DOM's of the
 * corresponding entries with open arrivals.
 */

Pseudo_Task&
Pseudo_Task::insertDOMResults()
{
    if ( type() != Task::Type::OPEN_ARRIVAL_SOURCE ) return *this;

    /* Waiting times for open arrivals */

    std::for_each( entries().begin(), entries().end(), std::mem_fn( &Entry::insertDOMResults ) );
    return *this;
}


void
Pseudo_Task::create_instance()
{
    if ( type() != Task::Type::OPEN_ARRIVAL_SOURCE ) return;

    _task = new srn_open_arrivals( this, name() );	/* Create a fake task.			*/
}


bool
Pseudo_Task::start()
{
    return ps_resume( _task->task_id() ) == OK;
}


Pseudo_Task&
Pseudo_Task::kill()
{
    ps_kill( _task->task_id() );
    _task = 0;
    return *this;
}
