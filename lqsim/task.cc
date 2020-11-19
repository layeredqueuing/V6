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
 * $Id: task.cc 14106 2020-11-18 14:33:50Z greg $
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <cstring>
#include <cmath>
#include <parasol.h>
#include "lqsim.h"
#if defined(HAVE_REGEX_H)
#include <regex.h>
#endif
#include <lqio/input.h>
#include <lqio/labels.h>
#include <lqio/error.h>
#include <lqio/dom_actlist.h>
#include <lqio/dom_extvar.h>
#include "errmsg.h"
#include "task.h"
#include "instance.h"
#include "activity.h"
#include "processor.h"
#include "group.h"

using namespace std;

#define N_SEMAPHORE_ENTRIES 2
#define N_RWLOCK_ENTRIES 4

set <Task *, ltTask> task;	/* Task table.	*/

const char * Task::type_strings[] =
{
    "Undefined",
    "client",
    "server",
    "multi",
    "infsrv",
    "sync",
    "semph",
    "open",
    "worker",
    "thread",
    "token",
    "token_r",
    "signal",
    "rw_lock",
    "rwlock",
    "token_w",
    "timeout_queue",
    "timeout_worker",
    "retry_queue",
    "retry_worker"
};


/*
 * n_tasks represents the number of task classes defined in the input
 * file.  total_tasks is the total number of tasks created.  The latter
 * will always be greater than or equal to the former because it includes
 * all "clones" of multi-server tasks.  Use n_tasks as an upper bound to
 * class_tab, and total_tasks as an upper bound to object_tab.
 */

unsigned total_tasks = 0;

Task::Task( const task_type type, LQIO::DOM::Task* dom, Processor * processor, Group * a_group )
    : _dom(dom),
      _processor(processor),
      _group_id(-1),
      _compute_func(NULL),
      _active(0),
      _max_phases(1),
      _act_list(),
      _forks(),
      _joins(),
      _pending_msgs(),
      _join_start_time(0.0),
      _free_msgs(),
      _type(type),
      _entry(),
      _activity(),
      trace_flag(false),
      _hist_data(0),
      r_cycle(),
      r_util(),
      r_group_util(),
      r_loss_prob(),
      _hold_active(0)
{
    if ( processor ) {
	processor->add_task(this);
    }
}


Task::~Task()
{
    for ( vector<Activity *>::iterator act = _activity.begin(); act != _activity.end(); ++act ) {
	delete *act;
    }
    for ( vector<ActivityList *>::iterator list = _act_list.begin(); list != _act_list.end(); ++list ) {
	delete *list;
    }

    _activity.clear();
    _act_list.clear();
    _forks.clear();
    _joins.clear();

    if ( _hist_data ) {
	delete _hist_data;
    }
    for ( std::list<Message *>::iterator msg = _free_msgs.begin(); msg != _free_msgs.end(); ++msg ) {
	delete *msg;
    }
    _free_msgs.clear();
    _pending_msgs.clear();
}


/*
 * Configure entries and activities.  Count up the number of
 * activities.  Do final list construction (i.e., tie up loose ends.)
 */

Task&
Task::configure()
{
    /* I need the instance variable "task" set from this point on. */

    double total_calls = for_each( _activity.begin(), _activity.end(), ExecSum<Activity,double>( &Activity::configure ) ).sum();
    for_each( _act_list.begin(), _act_list.end(), Exec<ActivityList>( &ActivityList::configure ) );
    total_calls += for_each( _entry.begin(), _entry.end(), ExecSum<Entry,double>( &Entry::configure ) ).sum();

    if ( total_calls == 0 && is_reference_task() ) { 
	LQIO::solution_error( LQIO::WRN_NO_SENDS_FROM_REF_TASK, name() );
    }

    for ( vector<Activity *>::const_iterator ap = _activity.begin(); ap != _activity.end(); ++ap ) {
	if ( !(*ap)->is_reachable() ) {
	    LQIO::solution_error( LQIO::WRN_NOT_USED, "Activity", (*ap)->name() );
	} else if ( !(*ap)->is_specified() ) {
	    LQIO::solution_error( LQIO::ERR_ACTIVITY_NOT_SPECIFIED, name(), (*ap)->name() );
	}
    }
    return *this;
}


Task&
Task::create()
{
    /*
     * Set compute function.  We have a special case for when no
     * processor has been allocated (eg, processor 0).  In this case,
     * tasks compute by "sleeping".
     */

    _compute_func = (!_processor || _processor->is_infinite()) ? ps_sleep : ps_compute;

    /* JOIN Stuff -- All entries are free. */

#if HAVE_REGCOMP
    trace_flag	= (bool)(task_match_pattern != 0 && regexec( task_match_pattern, (char *)name(), 0, 0, 0 ) != REG_NOMATCH );
#else
    trace_flag = false;
#endif

    if ( debug_flag ){
	(void) fprintf( stddbg, "\n-+++++---- %s task %s", type_name(), name() );
	if ( _compute_func == ps_sleep ) {
	    (void) fprintf( stddbg, " [delay]" );
	}
	(void) fprintf( stddbg, " ----+++++-\n" );
    }

    /* Compute PDF for all activities for each task. */

    create_instance();
    initialize();

    /* Create "links" where necessary. */

    build_links();

    if ( has_send_no_reply() ) {
	alloc_pool();
    }

    return *this;
}


Task&
Task::initialize()
{
    for_each( _activity.begin(), _activity.end(), Exec<Activity>( &Activity::initialize ) );
    for_each( _act_list.begin(), _act_list.end(), Exec<ActivityList>( &ActivityList::initialize ) );
    for_each( _entry.begin(), _entry.end(), Exec<Entry>( &Entry::initialize ) );

    /*
     * Allocate statistics for joins.  We do it here because we
     * don't know how many joins we have when we allocate the task
     * class object.
     */
	
    for ( std::vector<AndForkActivityList *>::iterator lp = _forks.begin(); lp != _forks.end(); ++lp ) {
	(*lp)->set_visits( 0 );
	for ( unsigned j = 0; j < (*lp)->size(); ++j ) {
	    if ( (*lp)->at(j) ) {
		(*lp)->inc_visits();
	    }
	}

    }
    for ( std::vector<AndJoinActivityList *>::iterator lp = _joins.begin(); lp != _joins.end(); ++lp ) {
	const Activity * src = (*lp)->front();
	const Activity * dst = (*lp)->back();

	(*lp)->join_check();	/* check fork-join matching*/
	(*lp)->r_join.init( SAMPLE, "Join delay %-11.11s %-11.11s ", src->name(), dst->name() );
	(*lp)->r_join_sqr.init( SAMPLE, "Join delay squared %-11.11s %-11.11s ", src->name(), dst->name() );
    }

    /* statistics */
    _active = 0;		/* Reset counts */
    _hold_active = 0;

    r_cycle.init( SAMPLE,        "%s %-11.11s - Cycle Time        ", type_name(), name() );
    r_util.init( VARIABLE,       "%s %-11.11s - Utilization       ", type_name(), name() );
    r_group_util.init( VARIABLE, "%s %-11.11s - Group Utilization ", type_name(), name() );
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
    const char * entry_name = dom->getName().c_str();
    Entry * ep = Entry::find( entry_name );
	
    if ( !ep ) return;
    if ( !ep->test_and_set( LQIO::DOM::Entry::Type::ACTIVITY ) ) return;

    const LQIO::DOM::Activity * activity_dom = dom->getStartActivity();
    const char * activity_name = activity_dom->getName().c_str();
    Activity * ap = find_activity( activity_name );
    if ( !ap ) {
	LQIO::input_error2( LQIO::ERR_NOT_DEFINED, activity_name );
    } else if ( ep->get_start_activity() != NULL ) {
	LQIO::input_error2( LQIO::ERR_DUPLICATE_START_ACTIVITY, entry_name, activity_name );
    } else {
	ep->set_start_activity( ap );
	ap->set_is_start_activity(true);
    }
}

/*
 * add links between tasks to simulate communication delays.
 */

void
Task::build_links()
{
    for ( unsigned j = 0; j < n_entries(); ++j ) {
	for ( std::vector<Activity>::iterator phase = _entry[j]->_phase.begin(); phase != _entry[j]->_phase.end(); ++phase ) {
	    for ( Targets::const_iterator tp = phase->_calls.begin(); tp != phase->_calls.end(); ++tp ) {
		Processor * proc = tp->entry()->task()->processor();
		if ( proc != processor() && inter_proc_delay > 0.0 ) {
		    const int h = proc->node_id();
		    if ( static_cast<int>(link_tab[h]) == -1 ) {
			char link_name[BUFSIZ];
						
			(void) sprintf( link_name, "%s.%s", name(), proc->name() );

			/*
			 * !!!DANGER!!!!
			 * Stupid parasol insists on massaging the transmission rate,
			 * we have to fudge it here.
			 */

			link_tab[h] = ps_build_link( link_name,
						     processor()->node_id(),
						     h,
						     (double)LINKS_MESSAGE_SIZE * 2 / inter_proc_delay,
						     TRUE );
		    }
		    tar_t& target = const_cast<tar_t&>(*tp);
		    target.set_link( link_tab[h] );
		}
	    }
	}
    }
}



bool
Task::has_send_no_reply() const
{
    return find_if( _entry.begin(), _entry.end(), Predicate<Entry>( &Entry::is_send_no_reply ) ) != _entry.end();
}

/* 
 * Create a new activity assigned to a given task and set the information DOM entry for it 
 */

Activity * 
Task::add_activity( LQIO::DOM::Activity * dom_activity )
{
    Activity * activity = find_activity( dom_activity->getName().c_str() );
    if ( activity ) {
	LQIO::input_error2( LQIO::ERR_DUPLICATE_SYMBOL, "Activity", dom_activity->getName().c_str() );
    } else {
	activity = new Activity( this, dom_activity );
	_activity.push_back( activity );
    }
    return activity;
}




/*
 * Find the activity.  Return error if not found.
 */

Activity *
Task::find_activity( const char * activity_name ) const
{
    vector<Activity *>::const_iterator ap = find_if( _activity.begin(), _activity.end(), eqActivityStr( activity_name ) );
    if ( ap != _activity.end() ) {
	return *ap;
    } else {
	return 0;
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
	    solution_error( LQIO::ERR_INVALID_PARAMETER, "pool size", "task", name(), e.what() );
	    throw_bad_parameter();
	}
    }
    for ( unsigned int i = 0; i < size; ++i ) {
	_free_msgs.push_back( new Message );
    }
}


Message *
Task::alloc_message()
{
    Message * msg = NULL;
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
    ps_record_stat( tp->r_delay.raw, delta );
    ps_record_stat( tp->r_delay_sqr.raw, square( delta ) );
    _free_msgs.push_back( msg );
}

double
Task::throughput() const
{
    switch ( type() ) {
    case Task::SEMAPHORE:  return r_cycle.mean_count() / (Model::block_period() * n_entries()); 		/* Only count for one entry.  */
    case Task::RWLOCK:	   return r_cycle.mean_count() / (Model::block_period() * n_entries()/2); 	/* Only count for two entries.  */
    case Task::SERVER:	   if ( is_sync_server() ) return  r_cycle.mean_count() / (Model::block_period() * n_entries());
	/* Fall through */
    default: 		   return r_cycle.mean_count() / Model::block_period();
    }
}



double
Task::throughput_variance() const
{
    switch ( type() ) {
    case Task::SEMAPHORE:  return r_cycle.variance_count() / (square(Model::block_period()) * n_entries());
    case Task::RWLOCK:     return r_cycle.variance_count() / (square(Model::block_period()) * n_entries()/2);
    case Task::SERVER:	   if ( is_sync_server() ) return r_cycle.variance_count() / (square(Model::block_period()) * n_entries());
	/* Fall through */
    default:		   return r_cycle.variance_count() / square(Model::block_period());
    }
}
	
Task&
Task::reset_stats()
{
    r_util.reset();
    r_cycle.reset();

    for_each( _entry.begin(), _entry.end(), Exec<Entry>( &Entry::reset_stats ) );
    for_each( _activity.begin(), _activity.end(), Exec<Activity>( &Activity::reset_stats ) );

    for ( vector<AndJoinActivityList *>::iterator lp = _joins.begin(); lp != _joins.end(); ++lp ) {
	(*lp)->r_join.reset();
	(*lp)->r_join_sqr.reset();
	if ( (*lp)->_hist_data ) {
	    (*lp)->_hist_data->reset();
	}
    }

    /* Histogram stuff */
 
    if ( _hist_data ) {
	_hist_data->reset();
    }
    return *this;
}


Task&
Task::accumulate_data()
{
    if ( type() == Task::UNDEFINED ) return *this;    /* Some tasks don't have statistics */

    r_util.accumulate();
    r_cycle.accumulate();

    for_each( _entry.begin(), _entry.end(), Exec<Entry>( &Entry::accumulate_data ) );
    for_each( _activity.begin(), _activity.end(), Exec<Activity>( &Activity::accumulate_data ) );

    for ( std::vector<AndJoinActivityList *>::iterator lp = _joins.begin(); lp != _joins.end(); ++lp ) {
	(*lp)->r_join_sqr.accumulate_variance( (*lp)->r_join.accumulate() );
	if ( (*lp)->_hist_data ) {
	    (*lp)->_hist_data->accumulate_data();
	}
    }

    /* Histogram stuff */

    if ( _hist_data ) {
	_hist_data->accumulate_data();
    }
    return *this;
}


FILE *
Task::print( FILE * output ) const
{
    r_util.print_raw( output,     "%-6.6s %-11.11s - Utilization", type_name(), name() );
    r_cycle.print_raw( output,    "%-6.6s %-11.11s - Cycle Time ", type_name(), name() );

    for ( std::vector<Entry *>::const_iterator entry = _entry.begin(); entry != _entry.end(); ++entry ) {
	(*entry)->r_cycle.print_raw( output, "Entry %-11.11s  - Cycle Time      ", (*entry)->name() );

	for_each( (*entry)->_phase.begin(), (*entry)->_phase.end(), ConstExec1<Activity,FILE *>( &Activity::print_raw_stat, output ) );
    }

    for_each( _activity.begin(), _activity.end(), ConstExec1<Activity,FILE *>( &Activity::print_raw_stat, output ) );

    for ( std::vector<AndJoinActivityList *>::const_iterator lp = _joins.begin(); lp != _joins.end(); ++lp ) {
	(*lp)->r_join.print_raw( output, "%-6.6s %-11.11s - Join Delay ", type_name(), name() );
	(*lp)->r_join_sqr.print_raw( output, "%-6.6s %-11.11s - Join DelSqr", type_name(), name() );
    }

    return output;
}

Task&
Task::insertDOMResults()
{
    /* Some tasks don't have statistics */

    if ( type() == Task::UNDEFINED ) return *this;

    double phaseUtil[MAX_PHASES];
    double phaseVar[MAX_PHASES];
    for ( unsigned p = 0; p < MAX_PHASES; p++ ) {
	phaseUtil[p] = 0.0;
	phaseVar[p] = 0.0;
    }

    if ( has_activities() ) {
	for_each( _activity.begin(), _activity.end(), Exec<Activity>( &Activity::insertDOMResults ) );
	for_each( _joins.begin(), _joins.end(), Exec<AndJoinActivityList>( &AndJoinActivityList::insertDOMResults ) );
    }

    double taskProcUtil = 0.0;		/* Total processor utilization. */
    double taskProcVar = 0.0;
    for ( std::vector<Entry *>::const_iterator nextEntry = _entry.begin(); nextEntry != _entry.end(); ++nextEntry ) {
	Entry * ep = *nextEntry;
	ep->insertDOMResults();

	for ( unsigned p = 0; p < max_phases(); ++p ) {
	    phaseUtil[p] += ep->_phase[p].r_util.mean();
	    phaseVar[p]  += ep->_phase[p].r_util.variance();
	    taskProcUtil += ep->_phase[p].r_cpu_util.mean();
	    taskProcVar  += ep->_phase[p].r_cpu_util.variance();
	}
    }

    /* Store totals */
	
    getDOM()->setResultPhaseUtilizations(max_phases(),phaseUtil)
	.setResultUtilization(r_util.mean())
	.setResultThroughput(throughput())
	.setResultProcessorUtilization(taskProcUtil);

    if ( number_blocks > 1 ) {
	getDOM()->setResultPhaseUtilizationVariances(max_phases(),phaseVar)
	    .setResultUtilizationVariance(r_util.variance())
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
 *
 * NB:	The parser returns a mallocated string for task_name which
 * 	should be freed but once!
 */

Task * 
Task::add( LQIO::DOM::Task* domTask )
{
    /* Recover the old parameter information that used to be passed in */
    const char* task_name = domTask->getName().c_str();
    const LQIO::DOM::Group * domGroup = domTask->getGroup();
    const scheduling_type sched_type = domTask->getSchedulingType();
    
    if ( !task_name || strlen( task_name ) == 0 ) abort();

    if ( Task::find( task_name ) ) {
	LQIO::input_error2( LQIO::ERR_DUPLICATE_SYMBOL, "Task", task_name );
    }
    if ( domTask->hasReplicas() ) {
	LQIO::input_error2( ERR_REPLICATION, "task", task_name );
    }

    Task * cp = 0;
    const char* processor_name = domTask->getProcessor()->getName().c_str();
    Processor * processor = Processor::find( processor_name );

    if ( !LQIO::DOM::Common_IO::is_default_value( domTask->getPriority(), 0 ) && ( processor->discipline() == SCHEDULE_FIFO
								     || processor->discipline() == SCHEDULE_PS
								     || processor->discipline() == SCHEDULE_RAND ) ) {
	LQIO::input_error2( LQIO::WRN_PRIO_TASK_ON_FIFO_PROC, task_name, processor_name );
    }

    Group * group = 0;
    if ( !domGroup && processor->discipline() == SCHEDULE_CFS ) {
	LQIO::input_error2( LQIO::ERR_NO_GROUP_SPECIFIED, task_name, processor_name );
    } else if ( domGroup ) {
	group = Group::find( domGroup->getName().c_str() );
	if ( !group ) {
	    LQIO::input_error2( LQIO::ERR_NOT_DEFINED, domGroup->getName().c_str() );
	}
    }

    switch ( sched_type ) {
    case SCHEDULE_BURST:
    case SCHEDULE_UNIFORM:
    case SCHEDULE_CUSTOMER:
	if ( domTask->hasQueueLength() ) {
	    input_error2( LQIO::WRN_QUEUE_LENGTH, task_name );
	}
	if ( domTask->isInfinite() ) {
	    input_error2( LQIO::ERR_REFERENCE_TASK_IS_INFINITE, task_name );
	}
	cp = new Reference_Task( Task::CLIENT, domTask, processor, group );
	break;

    case SCHEDULE_PPR:
    case SCHEDULE_HOL:
    case SCHEDULE_FIFO:
	if ( domTask->hasThinkTime() ) {
	    input_error2( LQIO::ERR_NON_REF_THINK_TIME, task_name );
	}
	Task::task_type a_type;
	
	if ( domTask->isInfinite() ) {
	    a_type = Task::INFINITE_SERVER;
	} else if ( domTask->isMultiserver() ) {
	    a_type = Task::MULTI_SERVER;
	} else {
	    a_type = Task::SERVER;
	}
	cp = new Server_Task( a_type, domTask, processor, group );
	//}
	break;

    case SCHEDULE_DELAY:
	if ( domTask->hasThinkTime() ) {
	    input_error2( LQIO::ERR_NON_REF_THINK_TIME, task_name );
	}
	if ( domTask->isMultiserver() ) {
	    LQIO::input_error2( LQIO::WRN_INFINITE_MULTI_SERVER, "Task", task_name, domTask->getCopiesValue() );
	}	
	if ( domTask->hasQueueLength() ) {
	    LQIO::input_error2( LQIO::WRN_QUEUE_LENGTH, task_name );
	}
	cp = new Server_Task( Task::INFINITE_SERVER, domTask, processor, group );
	break;

/*+ BUG_164 */
    case SCHEDULE_SEMAPHORE:
	if ( domTask->hasQueueLength()  ) {
	    input_error2( LQIO::WRN_QUEUE_LENGTH, task_name );
	}
	if ( domTask->isInfinite() ) {
	    input_error2( LQIO::ERR_INFINITE_TASK, task_name );
	}
 	cp = new Semaphore_Task( Task::SEMAPHORE, domTask, processor, group );
	break;
/*- BUG_164 */
	
/* reader_writer lock */ 

    case SCHEDULE_RWLOCK:
	if ( domTask->hasQueueLength() ) {
	    input_error2( LQIO::WRN_QUEUE_LENGTH, task_name );
	}
	if ( domTask->isInfinite() ) {
	    input_error2( LQIO::ERR_INFINITE_TASK, task_name );
	}
 	cp = new ReadWriteLock_Task( Task::RWLOCK, domTask, processor, group );
	break;
/* reader_writer lock*/

/* Decision tasks */
    case SCHEDULE_ABORT:
    case SCHEDULE_TIMEOUT:
	cp = new Timeout_Task( Task::TIMEOUT_QUEUE, domTask, processor, group );
	break;

    case SCHEDULE_INF_RETRY:      
    case SCHEDULE_RETRY:

	cp = new Retry_Task( Task::RETRY_QUEUE, domTask, processor, group );
	break;
/* Decision tasks */

    default:
	cp = new Server_Task( Task::SERVER, domTask, processor, group );		/* Punt... */
	input_error2( LQIO::WRN_SCHEDULING_NOT_SUPPORTED, scheduling_label[sched_type].str, "task", task_name );
	break;
    }

    ::task.insert( cp );

    return cp;
}


/*
 * Located the task id.
 */

Task *
Task::find( const char * task_name )
{
    set<Task *,ltTask>::const_iterator nextTask = find_if( ::task.begin(), ::task.end(), eqTaskStr( task_name ) );
    if ( nextTask == ::task.end() ) {
	return 0;
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
	    solution_error( LQIO::ERR_INVALID_PARAMETER, "multiplicity", "task", name(), e.what() );
	    throw_bad_parameter();
	}
    }
    return value;
}


int
Task::priority() const
{
    try {
	return getDOM()->getPriorityValue();
    }
    catch ( const std::domain_error &e ) {
	LQIO::solution_error( LQIO::ERR_INVALID_PARAMETER, "priority", "task", name(), e.what() );
	throw_bad_parameter();
    }
    return 0;
}

bool 
Task::is_infinite() const
{
    return getDOM()->isInfinite();
}


bool
Task::derive_utilization() const
{ 
    return processor()->derive_utilization(); 
}


bool
Task::has_lost_messages() const
{
    return find_if( _entry.begin(), _entry.end(), Predicate<Entry>( &Entry::has_lost_messages ) ) != _entry.end();
}

/* ------------------------------------------------------------------------ */

Reference_Task::Reference_Task( const task_type type, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup )
    : Task( type, domTask, aProc, aGroup ), _think_time(0.0), _task_list()
{
}


void
Reference_Task::create_instance()
{
    if ( n_entries() != 1 ) {
	solution_error( LQIO::WRN_TOO_MANY_ENTRIES_FOR_REF_TASK, name() );
    } 
    if ( getDOM()->hasThinkTime() ) {
	_think_time = getDOM()->getThinkTimeValue();
    }
    _task_list.clear();
    for ( unsigned i = 0; i < multiplicity(); ++i ) {
	_task_list.push_back( new srn_client( this, name() ) );
    }
}


bool
Reference_Task::start()
{
    for ( std::vector<srn_client *>::const_iterator t = _task_list.begin(); t != _task_list.end(); ++t ) {
	if ( ps_resume( (*t)->task_id() ) != OK ) return false;
    }
    return true;
}


Reference_Task&
Reference_Task::kill()
{
    for ( std::vector<srn_client *>::const_iterator t = _task_list.begin(); t != _task_list.end(); ++t ) {
	ps_kill( (*t)->task_id() );
    }
    return *this;
}

bool
Reference_Task::is_not_waiting() const
{
    for ( std::vector<srn_client *>::const_iterator t = _task_list.begin(); t != _task_list.end(); ++t ) {
	if ( ps_task_state( (*t)->task_id() ) != TASK_RECEIVING ) return true;
    }
    return false;
}

/* ------------------------------------------------------------------------ */

Server_Task::Server_Task( const task_type type, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup )
    : Task( type, domTask, aProc, aGroup ),
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
	_worker_port = ps_allocate_port( name(), _task->task_id() );
	_type = Task::INFINITE_SERVER;
    } else if ( is_multiserver() ) {
	_task = new srn_multiserver( this, name(), multiplicity() );
	_worker_port = ps_allocate_port( name(), _task->task_id() );
	_type = Task::MULTI_SERVER;
    } else {
	_task = new srn_server( this, name() );
	_worker_port = -1;
	_type = Task::SERVER;
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

Semaphore_Task::Semaphore_Task( const task_type type, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup )
    : Server_Task( type, domTask, aProc, aGroup ),
      r_hold(),
      r_hold_sqr(),
      r_hold_util(),
      _signal_task(0),
      _signal_port(-1)
{
}


Semaphore_Task&
Semaphore_Task::create()
{
    Task::create();

    r_hold.init( SAMPLE,         "%s %-11.11s - Hold Time         ", type_name(), name() );
    r_hold_sqr.init( SAMPLE,     "%s %-11.11s - Hold Time Sq      ", type_name(), name() );
    r_hold_util.init( VARIABLE,  "%s %-11.11s - Hold Utilization  ", type_name(), name() );
    return *this;
}

void
Semaphore_Task::create_instance()
{
    if ( n_entries() != N_SEMAPHORE_ENTRIES ) {
	LQIO::solution_error( LQIO::ERR_ENTRY_COUNT_FOR_TASK, name(), n_entries(), N_SEMAPHORE_ENTRIES );
    } else if ( _entry[0]->is_signal() ) {
	if ( !_entry[1]->test_and_set_semaphore( LQIO::DOM::Entry::Semaphore::WAIT ) ) {
	    LQIO::solution_error( LQIO::ERR_MIXED_SEMAPHORE_ENTRY_TYPES, name() );
	}
	if ( !_entry[1]->test_and_set_recv( Entry::RECEIVE_RENDEZVOUS ) ) {
	    LQIO::solution_error( LQIO::ERR_ASYNC_REQUEST_TO_WAIT, _entry[1]->name() );
	}
    } else if ( _entry[0]->is_wait() ) {
	if ( !_entry[1]->test_and_set_semaphore( LQIO::DOM::Entry::Semaphore::SIGNAL ) ) {
	    LQIO::solution_error( LQIO::ERR_MIXED_SEMAPHORE_ENTRY_TYPES, name() );
	}
	if ( !_entry[0]->test_and_set_recv( Entry::RECEIVE_RENDEZVOUS ) ) {
	    LQIO::solution_error( LQIO::ERR_ASYNC_REQUEST_TO_WAIT, _entry[0]->name() );
	}
    } else {
	LQIO::solution_error( LQIO::ERR_NO_SEMAPHORE, name() );
	cerr << "entry names: " << _entry[0]->name() << ", " << _entry[1]->name() << endl;
    }
    if ( !_hist_data && getDOM()->hasHistogram() ) {
	_hist_data = new Histogram( getDOM()->getHistogram() );
    }

    string buf = name();
    buf += "-wait";
    /* entry for waiting request - send to token. */
    _task = new srn_semaphore( this, buf.c_str() );
    _worker_port = ps_allocate_port( buf.c_str(), _task->task_id() );

    /* Entry for signal request */
    buf = name();
    buf += "-signal";
    _signal_task = new srn_signal( this,  buf.c_str() );
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



FILE *
Semaphore_Task::print( FILE * output ) const
{
    Task::print( output );
    r_hold.print_raw( output,      "%-6.6s %-11.11s - Hold Time  ", type_name(), name() );
    r_hold_sqr.print_raw( output,  "%-6.6s %-11.11s - Hold Sqr   ", type_name(), name() );
    r_hold_util.print_raw( output, "%-6.6s %-11.11s - Hold Util  ", type_name(), name() );

    return output;
}

/* ------------------------------------------------------------------------ */

ReadWriteLock_Task::ReadWriteLock_Task( const task_type type, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup )
    : Semaphore_Task( type, domTask, aProc, aGroup ),
      _reader(0),
      _writer(0),
      _signal_port2(-1),
      _writerQ_port(-1),
      _readerQ_port(-1)
{
}

void
ReadWriteLock_Task::create_instance()
{
    if ( n_entries() != N_RWLOCK_ENTRIES ) {
	LQIO::solution_error( LQIO::ERR_ENTRY_COUNT_FOR_TASK, name(), n_entries(), N_RWLOCK_ENTRIES );
    }
	
    int E[N_RWLOCK_ENTRIES];
    for (int i=0;i<N_RWLOCK_ENTRIES;i++){ E[i]=-1; }
	
    for (int i=0;i<N_RWLOCK_ENTRIES;i++){
	if ( _entry[i]->is_r_unlock() ) {
	    if (E[0]== -1) { E[0]=i; }
	    else{ // duplicate entry TYPE error
		LQIO::solution_error( LQIO::ERR_DUPLICATE_SYMBOL, name() );
	    }
	} else if ( _entry[i]->is_r_lock() ) {
	    if (E[1]== -1) { E[1]=i; }
	    else{ 
		LQIO::solution_error( LQIO::ERR_DUPLICATE_SYMBOL, name() );
	    }
	} else if ( _entry[i]->is_w_unlock() ) {
	    if (E[2]== -1) { E[2]=i; }
	    else{ 
		LQIO::solution_error( LQIO::ERR_DUPLICATE_SYMBOL, name() );
	    }
	} else if ( _entry[i]->is_w_lock() ) {
	    if (E[3]== -1) { E[3]=i; }
	    else{ 
		LQIO::solution_error( LQIO::ERR_DUPLICATE_SYMBOL, name() );
	    }
	} else { 
	    LQIO::solution_error( LQIO::ERR_NO_RWLOCK, name() );
	    cerr << "entry names: " << _entry[0]->name() << ", " << _entry[1]->name() <<", " << _entry[2]->name()<< ", " << _entry[3]->name()<< endl;
	}
    }
    //test reader lock entry
    if ( !_entry[E[1]]->test_and_set_rwlock( LQIO::DOM::Entry::RWLock::READ_LOCK ) ) {
	LQIO::solution_error( LQIO::ERR_MIXED_RWLOCK_ENTRY_TYPES, name() );
    }
    if ( !_entry[E[1]]->test_and_set_recv( Entry::RECEIVE_RENDEZVOUS ) ) {
	LQIO::solution_error( LQIO::ERR_ASYNC_REQUEST_TO_WAIT, _entry[E[1]]->name() );
    }
	 
    //test reader unlock entry
    if ( !_entry[E[0]]->test_and_set_rwlock( LQIO::DOM::Entry::RWLock::READ_UNLOCK ) ) {
	LQIO::solution_error( LQIO::ERR_MIXED_RWLOCK_ENTRY_TYPES, name() );
    }

    //test writer lock entry
    if ( !_entry[E[3]]->test_and_set_rwlock( LQIO::DOM::Entry::RWLock::WRITE_LOCK ) ) {
	LQIO::solution_error( LQIO::ERR_MIXED_RWLOCK_ENTRY_TYPES, name() );
    }
    if ( !_entry[E[3]]->test_and_set_recv( Entry::RECEIVE_RENDEZVOUS ) ) {
	LQIO::solution_error( LQIO::ERR_ASYNC_REQUEST_TO_WAIT, _entry[E[3]]->name() );
    }

    //test writer unlock entry
    if ( !_entry[E[2]]->test_and_set_rwlock( LQIO::DOM::Entry::RWLock::WRITE_UNLOCK ) ) {
	LQIO::solution_error( LQIO::ERR_MIXED_RWLOCK_ENTRY_TYPES, name() );
    }

 

    string buf = name();
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
    _reader = new srn_semaphore( this, buf.c_str() );
    _worker_port = ps_allocate_port( buf.c_str(), _reader->task_id() );

    /* Entry for signal request */
    buf = name();
    buf += "-reader-signal";
    _signal_task = new srn_signal( this,  buf.c_str() );
    _signal_port = ps_allocate_port( buf.c_str(), _signal_task->task_id() );

    /*  writer token*/
    buf = name();
    buf += "-writer-token";
    _writer = new srn_writer_token( this, buf.c_str() );
}



ReadWriteLock_Task&
ReadWriteLock_Task::create()
{
    Semaphore_Task::create();

    r_reader_hold.init( SAMPLE,         "%s %-11.11s - Reader Hold Time         ", type_name(), name() );
    r_reader_hold_sqr.init( SAMPLE,     "%s %-11.11s - Reader Hold Time sq      ", type_name(), name() );
    r_reader_wait.init( SAMPLE,         "%s %-11.11s - Reader Blocked Time      ", type_name(), name() );
    r_reader_wait_sqr.init( SAMPLE,     "%s %-11.11s - Reader Blocked Time sq   ", type_name(), name() );
    r_reader_hold_util.init( VARIABLE,  "%s %-11.11s - Reader Hold Utilization  ", type_name(), name() );
    r_writer_hold.init( SAMPLE,         "%s %-11.11s - Writer Hold Time         ", type_name(), name() );
    r_writer_hold_sqr.init( SAMPLE,     "%s %-11.11s - Writer Hold Time sq      ", type_name(), name() );
    r_writer_wait.init( SAMPLE,         "%s %-11.11s - Writer Blocked Time      ", type_name(), name() );
    r_writer_wait_sqr.init( SAMPLE,     "%s %-11.11s - Writer Blocked Time sq   ", type_name(), name() );
    r_writer_hold_util.init( VARIABLE,  "%s %-11.11s - Writer Hold Utilization  ", type_name(), name() );
    return *this;
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


FILE *
ReadWriteLock_Task::print( FILE * output ) const
{
    Semaphore_Task::print( output );

    r_reader_hold.print_raw( output,      "%-6.6s %-11.11s - Reader Hold Time    ", type_name(), name() );
    r_reader_hold_sqr.print_raw( output,  "%-6.6s %-11.11s - Reader Hold Sqr     ", type_name(), name() );
    r_reader_wait.print_raw( output,      "%-6.6s %-11.11s - Reader Blocked Time ", type_name(), name() );
    r_reader_wait_sqr.print_raw( output,  "%-6.6s %-11.11s - Reader Blocked Sqr  ", type_name(), name() );
    r_reader_hold_util.print_raw( output, "%-6.6s %-11.11s - Reader Hold Util    ", type_name(), name() );

    r_writer_hold.print_raw( output,      "%-6.6s %-11.11s - Writer Hold Time    ", type_name(), name() );
    r_writer_hold_sqr.print_raw( output,  "%-6.6s %-11.11s - Writer Hold Sqr     ", type_name(), name() );
    r_writer_wait.print_raw( output,      "%-6.6s %-11.11s - Writer Blocked Time ", type_name(), name() );
    r_writer_wait_sqr.print_raw( output,  "%-6.6s %-11.11s - Writer Blocked Sqr  ", type_name(), name() );
    r_writer_hold_util.print_raw( output, "%-6.6s %-11.11s - Writer Hold Util    ", type_name(), name() );

    return output;
}
/* ------------------------------------------------------------------------ */

Timeout_Task::Timeout_Task( const task_type type, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup )
    : Server_Task( type, domTask, aProc, aGroup ),
      _server_task(0),
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

    r_timeout.init( SAMPLE,         "%s %-11.11s - Timeout         ", type_name(), name() );
    r_timeout_sqr.init( SAMPLE,         "%s %-11.11s - Timeout sq         ", type_name(), name() );
    r_timeout_cycle.init( SAMPLE,         "%s %-11.11s - Timeout cycle         ", type_name(), name() );
    r_timeout_util.init( VARIABLE,  "%s %-11.11s - Timeout Utilization  ", type_name(), name() );
    r_timeout_prob.init( SAMPLE,  "%s %-11.11s - Timeout Probability  ", type_name(), name() );
    r_forward.init( SAMPLE,  "%s %-11.11s - Timeout stamp  ", type_name(), name() );
    r_calldelay.init( SAMPLE,  "%s %-11.11s - Timeout call delay  ", type_name(), name() );

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
	cerr << "task names: " << name() <<  endl;
    }
       
    string buf = name();
    buf += "-timeout-queue";

    _task = new srn_timeout_queue( this, buf.c_str());
    _ready_port = ps_allocate_port( buf.c_str(), _task->task_id() );
    _worker_port = ps_allocate_port( buf.c_str(), _task->task_id() );
    _type = Task::TIMEOUT_QUEUE;
       
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

    printf("%s task (%s) results:\n",type_name(), name());
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


FILE *
Timeout_Task::print( FILE * output ) const
{
    Task::print( output );

    r_timeout.print_raw( output,      "%-6.6s %-11.11s - Timeout  ", type_name(), name() );
    r_timeout_sqr.print_raw( output,  "%-6.6s %-11.11s - Timeout sq ", type_name(), name() );
    r_timeout_cycle.print_raw( output,"%-6.6s %-11.11s - Timeout Cycle ", type_name(), name() );
    r_timeout_util.print_raw( output, "%-6.6s %-11.11s - Timeout Utilization  ", type_name(), name() );
    r_timeout_prob.print_raw( output, "%-6.6s %-11.11s - Timeout probability  ", type_name(), name() );
    return output;
}

/* ------------------------------------------------------------------------ */

Retry_Task::Retry_Task( const task_type type, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup )
    : Server_Task( type, domTask, aProc, aGroup ),
      _server_task(0),
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
    

    r_nretry.init( SAMPLE,         "%s %-11.11s - Retry         ", type_name(), name() );
    r_tretry.init( SAMPLE,         "%s %-11.11s - total requests to Retry         ", type_name(), name() );
    r_Yretry.init( SAMPLE,         "%s %-11.11s - Retry         ", type_name(), name() );
    r_abort_prob.init( SAMPLE,  "%s %-11.11s - Retry Probability  ", type_name(), name() );

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
	input_error2( LQIO::ERR_INFINITE_TASK, name() );
    }

    string buf = name();
    buf += "-retry-queue";

    _task = new srn_retry_queue( this, buf.c_str());
    _ready_port = ps_allocate_port( buf.c_str(), _task->task_id() );
    _worker_port = ps_allocate_port( buf.c_str(), _task->task_id() );
    _type = Task::RETRY_QUEUE;
    
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
    printf("%s task (%s) results:\n",type_name(), name());
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



FILE *
Retry_Task::print( FILE * output ) const
{
    Task::print( output );

    r_nretry.print_raw( output, "%-6.6s %-11.11s - nRetries  ", type_name(), name() );
    r_tretry.print_raw( output, "%-6.6s %-11.11s - totalRequests  ", type_name(), name() );
    r_Yretry.print_raw( output, "%-6.6s %-11.11s - Y_Retry  ", type_name(), name() );
    r_abort_prob.print_raw( output, "%-6.6s %-11.11s - Abort probability  ", type_name(), name() );

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
    if ( type() != Task::OPEN_ARRIVAL_SOURCE ) return *this;

    /* Waiting times for open arrivals */

    for_each( _entry.begin(), _entry.end(), Exec<Entry>( &Entry::insertDOMResults ) );
    return *this;
}


void
Pseudo_Task::create_instance()
{
    if ( type() != Task::OPEN_ARRIVAL_SOURCE ) return;

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
