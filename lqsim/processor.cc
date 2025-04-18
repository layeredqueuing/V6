/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* May 1996								*/
/************************************************************************/

/*
 * Lqsim-parasol Processor interface.
 *
 * $Id: processor.cc 17464 2024-11-13 12:55:06Z greg $
 * ------------------------------------------------------------------------
 */

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdlib>
#include <iomanip>
#include <regex>
#include "lqsim.h"
#include <lqio/input.h>
#include <lqio/error.h>
#include <lqio/labels.h>
#include <lqio/dom_extvar.h>
#include "entry.h"
#include "errmsg.h"
#include "processor.h"
#include "result.h"
#include "task.h"
#include "instance.h"
#include "pragma.h"

#define	SN_PREEMPT	100			/* Message.			*/

std::set<Processor *, Processor::ltProcessor> Processor::__processors;	/* Processor table.		*/
Processor *Processor::processor_table[MAX_NODES+1];			/* NodeId to processor		*/

const std::map<const scheduling_type,const int> Processor::scheduling_types =
{
    { SCHEDULE_CUSTOMER,    PS_FIFO },
    { SCHEDULE_DELAY,	    PS_FIFO },
    { SCHEDULE_FIFO,	    PS_FIFO },
    { SCHEDULE_HOL,	    PS_HOL },
    { SCHEDULE_PPR,	    PS_PR },
    { SCHEDULE_PS,	    PS_FIFO },
    { SCHEDULE_CFS,	    PS_CFS }
};


/*
 * Find the processor and return it.
 */

Processor *
Processor::find( const std::string& processor_name  )
{
    if ( processor_name.empty() ) return nullptr;
    std::set<Processor *>::const_iterator processor = find_if( Processor::__processors.begin(), Processor::__processors.end(), [=]( const Processor * processor ){ return processor->name() == processor_name; } );
    if ( processor != Processor::__processors.end() ) {
	return *processor;
    } else {
	return nullptr;
    }
}


Processor::Processor( LQIO::DOM::Processor* dom )
    : trace_flag(false),
      r_util("Utilization",dom),
      _group(nullptr),
      _node_id(0),
      _dom( dom )
{
    trace_flag = std::regex_match( name(), processor_match_pattern );
}


/*
 * Create a processor
 */

Processor&
Processor::create()
{
    _node_id = ps_build_node( name().c_str(), multiplicity(), cpu_rate(), quantum(),
			      scheduling_types.at(discipline()),
			      SF_PER_NODE|SF_PER_HOST );

    if ( _node_id < 0 || MAX_NODES < _node_id ) {
	LQIO::input_error( ERR_CANNOT_CREATE_X, "processor", name().c_str() );
    } else {
	processor_table[_node_id] = this;
    }
    return *this;
}



/*
 * We need a way to fake out infinity... so if copies is infinite, then we change to an infinite server.
 */

unsigned
Processor::multiplicity() const
{
    unsigned int value = 1;
    if ( !getDOM()->isInfinite() ) {
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
Processor::is_infinite() const
{
    return getDOM()->isInfinite();
}



/*
 * Release the processor.  Mark ready time incase we get delayed.
 */

void
Processor::reschedule( Instance * ip )
{
    ps_my_schedule_time = ps_now;
    if ( (Pragma::__pragmas->scheduling_model() & SCHEDULE_NATURAL) == 0 ) {
	Custom_Processor * pp = dynamic_cast<Custom_Processor *>(find(ps_my_node));
	if ( pp ) {
	    static char nullstr[] = "";		/* C++ 11 */
	    ps_send( pp->scheduler(), SN_PREEMPT, nullstr, ip->task_id() );
	} else {
	    ps_sleep(0);
	}
    }
}

/*----------------------------------------------------------------------*/
/*			  Output Functions.				*/
/*----------------------------------------------------------------------*/


Custom_Processor::Custom_Processor( LQIO::DOM::Processor * domProcessor )
    : Processor( domProcessor ), _active(0), _active_task(0), _scheduler(-1)

{
    if ( !is_infinite() ) {
	_active_task  = new Instance * [multiplicity()];
    }
}



Custom_Processor::~Custom_Processor()
{
    if ( _active_task ) {
	delete [] _active_task;
    }
    _active_task = nullptr;
}


/*
 * Create a processor
 */

Custom_Processor&
Custom_Processor::create()
{
    _node_id = ps_build_node2( name().c_str(), multiplicity(), cpu_rate(), cpu_scheduler_task, SF_PER_NODE|SF_PER_HOST );

    if ( _node_id < 0 || MAX_NODES < _node_id ) {
	LQIO::input_error( ERR_CANNOT_CREATE_X, "processor", name().c_str() );
    } else {
	processor_table[_node_id] = this;
    }
    return *this;
}


#define PRIORITIES

int cpu_scheduler_port;			/* Port to send CPU requests to.	*/

/*
 * The scheduler (of all evil)
 */

void
Custom_Processor::cpu_scheduler_task( void )
{
    Custom_Processor * pp = dynamic_cast<Custom_Processor *>(Processor::find(ps_my_node));
    pp->main();
}


void
Custom_Processor::main()
{
    long * rtrq = static_cast<long *>(calloc( MAX_TASKS, sizeof(long) ));

    _active_task[ps_my_host] = nullptr;
    _scheduler = ps_std_port(ps_myself);

    for ( ;; ) {
	long type;
	long task_id;
	double time_stamp;
	char * message;
	double quantum = NEVER;
	double start_time = ps_now;
	Instance * ip;

	if ( ps_receive( ps_my_std_port, quantum, &type, &time_stamp, &message, &task_id ) == 0 ) {
	    type = SN_PREEMPT;
	}
	ip = _active_task[ps_my_host];

/* 		processor_trace( PROC_GENERAL, pp, ip, type, task_id ); */

	switch ( type ) {

	case SN_PREEMPT:

	    /* Quantum expired -- round robin schedule */

	    if ( object_tab[task_id] ) {
		ps_schedule_time(task_id) = ps_now;
	    }

	    if ( ( discipline() == SCHEDULE_FIFO
		   || (discipline() == SCHEDULE_HOL
		       && ps_task_priority(rtrq[0]) >= ip->priority() ))
		 && ps_ready_queue( ps_my_node, MAX_TASKS, rtrq ) > 0 ) {
		ps_schedule_time(ip->task_id()) = ps_now;
		if ( ip->r_a_execute != nullptr ) {
		    ip->r_a_execute->record( 0.0 );
		}
		if ( ip->r_e_execute != nullptr ) {
		    ip->r_e_execute->record( 0.0 );
		}
		trace( PROC_PREEMPTING_TASK, ip, quantum );
		quantum = run_task( rtrq[0] );
	    }
	    break;

	case SN_IDLE:

	    /* Processor is free -- run a task. */

	    if ( ps_ready_queue( ps_my_node, MAX_TASKS, rtrq ) > 0 ) {

		quantum = run_task( rtrq[0] );

	    } else {

		/* No tasks.			*/

		trace( PROC_IDLE );
		quantum = NEVER;

		_active -= 1;
		_active_task[ps_my_host] = nullptr;
		r_util.record( _active );
		ps_schedule( NULL_TASK, ps_my_host );
	    }
	    break;

	case SN_READY:

	    /* New task has arrived, but not scheduled. */

	    if ( object_tab[task_id] ) {
		ps_schedule_time(task_id) = ps_now;
	    }

	    if ( ip == 0 ) {

		/* No tasks.			*/

		_active += 1;
		r_util.record( _active );
		quantum = run_task( task_id );
	    } else if ( discipline() == SCHEDULE_PPR
			&& ps_ready_queue( ps_my_node, MAX_TASKS, rtrq ) > 0
			&& ps_task_priority(rtrq[0]) > ip->priority() ) {
		ps_schedule_time(ip->task_id()) = ps_now;
		if ( ip->r_a_execute != nullptr ) {
		    ip->r_a_execute->record( 0.0 );
		}
		if ( ip->r_e_execute != nullptr ) {
		    ip->r_e_execute->record( 0.0 );
		}
		trace( PROC_PRIO_PREEMPTING_TASK, object_tab[rtrq[0]], ip );
		quantum = run_task( rtrq[0] );
	    } else {
		quantum = quantum - ps_now + start_time;
	    }
	    break;

	default:
	    abort();
	    break;
	}
    }
}



/*
 * Run the task `ip' on processor `pp'.
 */

double
Custom_Processor::run_task( long task_id )
{
    Instance * ip = object_tab[task_id];

    trace( PROC_RUNNING_TASK, ip );

    _active_task[ps_my_host] = ip;
    if ( ip ) {
	if ( ip->r_a_execute != nullptr ) {
	    ip->r_a_execute->record( 1.0 );
	}
	if ( ip->r_e_execute != nullptr ) {
	    ip->r_e_execute->record( 1.0 );
	}
    }

    ps_schedule( task_id, ps_my_host );

    return quantum();
}


/*
 * Trace processor events.
 */


void
Custom_Processor::trace( const processor_events event, ... )
{
    va_list args;

    /* Args for va-arg */

    Instance * ip;
    Instance * ip2;
    unsigned type;
    unsigned task_id;

    va_start( args, event );

    if ( trace_flag ) {
	double quantum;

	if ( trace_driver ) {
	    (void) fprintf( stddbg, "\nTime* %8g P %s: ", ps_now, name().c_str() );
	} else {
	    (void) fprintf( stddbg, "%8g P %s: ", ps_now, name().c_str() );
	}

	switch ( event ) {
	case PROC_PRIO_PREEMPTING_TASK:
	    ip  = va_arg( args, Instance * );
	    ip2 = va_arg( args, Instance *  );
	    (void) fprintf( stddbg, "%s (%ld) preempting %s (%ld).",
			    ip->name().c_str(),  ip->task_id(),
			    ip2->name().c_str(), ip2->task_id() );
	    break;

	case PROC_PREEMPTING_TASK:
	    ip      = va_arg( args, Instance * );
	    quantum = va_arg( args, double );
	    (void) fprintf( stddbg, "Preempting %s (%ld). quantum=%g",
			    ip->name().c_str(), ip->task_id(),
			    quantum );
	    break;

	case PROC_RUNNING_TASK:
	    ip = va_arg( args, Instance * );
	    quantum = va_arg( args, double );
	    (void) fprintf( stddbg, "Running %s (%ld). Delay to schedule %g.",
			    ip->name().c_str(), ip->task_id(),
			    ps_now - ps_schedule_time(ip->task_id()) );
	    break;

	case PROC_IDLE:
	    (void) fprintf( stddbg, "IDLE." );
	    break;

	case PROC_GENERAL:
	    ip      = va_arg( args, Instance * );
	    type    = va_arg( args, unsigned );
	    task_id = va_arg( args, unsigned );
	    (void) fprintf( stddbg, "Processor for task %s (%ld): ",
			    ip->name().c_str(), ip->task_id() );
	    switch ( type ) {
	    case SN_PREEMPT:
		(void) fprintf( stddbg, "PREEMPT - task %d (%s)", task_id, ps_task_name(task_id) );
		break;

	    case SN_IDLE:
		(void) fprintf( stddbg, "IDLE - task %d (%s)", task_id, ps_task_name(task_id) );
		break;

	    case SN_READY:
		(void) fprintf( stddbg, "READY - %d (%s)", task_id, ps_task_name(task_id) );
		break;

	    default:
		(void) fprintf( stddbg, "????? - %d", task_id );
		break;
	    }
	    break;


	}
	if ( !trace_driver ) {
	    fprintf( stddbg, "\n" );
	}
	fflush( stddbg );
    }

    va_end( args );
}


bool
Processor::derive_utilization() const {
    return discipline() != SCHEDULE_FIFO
	&& discipline() != SCHEDULE_DELAY
	&& (Pragma::__pragmas->scheduling_model() & SCHEDULE_CUSTOM) == 0;
}


/*
 * Used to save the tasks running on this processor.
 */

void
Processor::add_task( Task * task )
{
    _tasks.push_back( task );
}

/*----------------------------------------------------------------------*/
/*			  Output Functions.				*/
/*----------------------------------------------------------------------*/

Processor&
Processor::insertDOMResults()
{
    if ( !getDOM() ) return *this;

    double proc_util_mean = 0.0;
    double proc_util_var  = 0.0;

    for ( std::vector<Task *>::const_iterator next_task = _tasks.begin(); next_task != _tasks.end(); ++next_task ) {
	Task * cp = *next_task;

	for ( std::vector<Entry *>::const_iterator entry = cp->entries().begin(); entry != cp->entries().end(); ++entry ) {
	    for ( unsigned p = 0; p < cp->max_phases(); ++p ) {
		proc_util_mean += (*entry)->_phase[p].r_cpu_util.mean();
		proc_util_var  += (*entry)->_phase[p].r_cpu_util.variance();
	    }
	}

	for ( std::vector<Activity *>::const_iterator activity = cp->activities().begin(); activity != cp->activities().end(); ++activity ) {
//	    std::cerr << "debug: processor " << name() << ", task " << cp->name() << ", activity " << (*activity)->name() << ": utilization " << (*activity)->r_cpu_util.mean() << std::endl; 
	    proc_util_mean += (*activity)->r_cpu_util.mean();
	    proc_util_var  += (*activity)->r_cpu_util.variance();
	}
    }

    getDOM()->setResultUtilization(proc_util_mean);
    if ( number_blocks > 1 ) {
	getDOM()->setResultUtilizationVariance(proc_util_var);
    }
    return *this;
}


std::ostream&
Processor::print( std::ostream& output ) const
{
    output << r_util;
    return output;
}

/*----------------------------------------------------------------------*/
/*	 Input processing.  Called from input.y (yyparse()).		*/
/*----------------------------------------------------------------------*/

/*
 * Add a processor to the model.
 */

void
Processor::add( const std::pair<std::string,LQIO::DOM::Processor*>& p )
{
    LQIO::DOM::Processor* dom = p.second;
    const std::string& processor_name = p.first;

    /* Unroll some of the encapsulated information */

    assert( !processor_name.empty() );

    if ( Processor::find( processor_name ) ) {
	dom->runtime_error( LQIO::ERR_DUPLICATE_SYMBOL );
	return;
    }

    if ( dom->hasReplicas() ) {
	dom->runtime_error( LQIO::ERR_NOT_SUPPORTED, "replication" );
    }

    const scheduling_type scheduling_flag = dom->getSchedulingType();

    switch( scheduling_flag ) {
    case SCHEDULE_DELAY:
	if ( dom->hasCopies() ) {
	    dom->runtime_error( LQIO::WRN_INFINITE_MULTI_SERVER, dom->getCopiesValue() );
	    dom->setCopies(new LQIO::DOM::ConstantExternalVariable(1.0));
	}
	/* Fall through */
    case SCHEDULE_FIFO:
    case SCHEDULE_HOL:
    case SCHEDULE_PPR:
	if ( dom->hasQuantum() ) {
	    dom->runtime_error( LQIO::WRN_QUANTUM_SCHEDULING, scheduling_label.at(scheduling_flag).str.c_str() );
	}
	break;

    case SCHEDULE_PS:
    case SCHEDULE_CFS:
	if ( !dom->hasQuantum() ) {
	    dom->runtime_error( LQIO::ERR_NO_QUANTUM_SCHEDULING, scheduling_label.at(scheduling_flag).str.c_str() );
	}
	break;

    default:
	dom->runtime_error( LQIO::WRN_SCHEDULING_NOT_SUPPORTED, scheduling_label.at(scheduling_flag).str.c_str() );
	dom->setSchedulingType( SCHEDULE_FIFO );
	break;
    }

    Processor * processor = nullptr;
    if ( Pragma::__pragmas->scheduling_model() & SCHEDULE_CUSTOM ) {
	processor = new Custom_Processor( dom );
    } else {
	processor = new Processor( dom );
    }
    Processor::__processors.insert( processor );
}

/*
 * Define these suckers even though they are not used.
 */

extern "C" {
    void bus_failure_handler( ps_event_t *ep ) {}
    void bus_repair_handler( ps_event_t *ep ) {}
    void link_failure_handler( ps_event_t *ep ) {}
    void link_repair_handler( ps_event_t *ep ) {}
    void node_failure_handler( ps_event_t *ep ) {}
    void node_repair_handler( ps_event_t *ep ) {}
    void user_event_handler( ps_event_t *ep ) {}
}
