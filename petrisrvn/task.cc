/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* September 1991.							*/
/* August 2003.								*/
/************************************************************************/

/*
 * $Id: task.cc 17458 2024-11-12 11:54:17Z greg $
 *
 * Generate a Petri-net from an SRVN description.
 *
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <set>
#include <lqio/common_io.h>
#include <lqio/dom_activity.h>
#include <lqio/dom_extvar.h>
#include <lqio/dom_processor.h>
#include <lqio/dom_task.h>
#include <lqio/glblerr.h>
#include "activity.h"
#include "actlist.h"
#include "entry.h"
#include "errmsg.h"
#include "makeobj.h"
#include "model.h"
#include "petrisrvn.h"
#include "phase.h"
#include "pragma.h"
#include "processor.h"
#include "results.h"
#include "task.h"

using namespace std;

#define SUM_BRANCHES  true
#define FOLLOW_BRANCH false
#define FOLLOW_RNVS   true
#define IGNORE_RNVS   false

unsigned int Task::__open_model_tokens	= OPEN_MODEL_TOKENS;	/* Default val.	*/

std::vector<Task *> __task;
double Task::__server_x_offset;		/* Starting offset for next server.	*/
double Task::__client_x_offset;		/* Starting offset for next client.	*/
double Task::__server_y_offset;
double Task::__queue_y_offset;

/*
 * Add a task to the model.
 */

Task::Task( const LQIO::DOM::Task* dom, Task::Type type, Processor * processor )
    : Place( dom ),
      entries(),
#if !defined(BUFFER_BY_ENTRY)
      ZZ(0),
#endif
      _processor(processor),
      _type(type),
      _sync_server(false),
      _has_main_thread(false),
      _inservice_flag(false),
      _needs_flush(false),
      _queue_made(false),
      _n_phases(1),
      _n_threads(1),
      _max_queue_length(0),
      _max_k(0),				/* input queues. 		*/
#if !defined(BUFFER_BY_ENTRY)
      _open_tokens(Task::__open_model_tokens),
#endif
      _proc_queue_count(0),
      _requestor_no(0)
{
    clear();
    _inservice_flag = inservice_match_pattern != nullptr && std::regex_match( name(), *inservice_match_pattern );
#if !defined(BUFFER_BY_ENTRY)
    if ( dom && dom->hasQueueLength() ) {
	_open_tokens = dom->getQueueLengthValue();
    }
#endif
}


void Task::clear()
{
    for ( unsigned int m = 0; m < MAX_MULT; ++m ) {
	TX[m] = nullptr;		/* Task place.			*/
	ZX[m] = nullptr;
#if BUG_163
	SyX[m] = nullptr;		/* Sync wait place.		*/
#endif
	GdX[m] = nullptr;		/* Guard Place			*/
	gdX[m] = nullptr;		/* Guard fork transition.	*/
	LX[m] = nullptr;		/* Lock Place	(BUG_164)	*/
	_utilization[m] = 0;		/* Result for finding util.	*/
	task_tokens[m] = 0;		/* Result. 			*/
	lock_tokens[m] = 0;		/* Result.			*/
    }
#if !defined(BUFFER_BY_ENTRY)
    ZZ = 0;				/* For open requests.		*/
#endif
    _queue_made = false;		/* true if queue made for __task	*/
    _max_queue_length = 0;
    _max_k = 0;				/* input queues. 		*/
}



/*
 * Clean out old transitions after run because we check recursively for 0 on construction.
 */

void
Task::remove_netobj()
{
    for ( vector<Entry *>::const_iterator e = entries.begin(); e != entries.end(); ++e ) {
        (*e)->remove_netobj();
    }
    for ( unsigned int m = 0; m < MAX_MULT; ++m ) {
        TX[m] = 0;		/* Task place.			*/
#if BUG_163
	SyX[m] = 0;		/* Sync wait place.		*/
#endif
	GdX[m] = 0;		/* Guard Place			*/
	gdX[m] = 0;		/* Guard fork transition.	*/
	LX[m] = 0;		/* Lock Place	(BUG_164)	*/
#if !defined(BUFFER_BY_ENTRY)
#endif

    }
    ZZ = 0;			/* For open requests.		*/

    for ( vector<ActivityList *>::const_iterator l = act_lists.begin(); l != act_lists.end(); ++l ) {
	(*l)->remove_netobj( );
    }
}


Task *
Task::create( const LQIO::DOM::Task * dom )
{
    const string& task_name = dom->getName();
    if ( task_name.size() == 0 ) abort();

    Processor * processor = Processor::find( dom->getProcessor()->getName() );
    if ( !processor ) {
	LQIO::input_error( LQIO::ERR_NOT_DEFINED, dom->getProcessor()->getName().c_str() );
	return nullptr;
    }

    if ( !LQIO::DOM::Common_IO::is_default_value( dom->getPriority(), 0. ) && ( !processor->has_priority_scheduling() ) ) {
	dom->input_error( LQIO::WRN_PRIO_TASK_ON_FIFO_PROC, processor->name() );
    }

    /* Override scheduling */

    scheduling_type scheduling = dom->getSchedulingType();
    Task * task = nullptr;

    switch ( scheduling ) {
    case SCHEDULE_BURST:
    case SCHEDULE_UNIFORM:
	dom->runtime_error( LQIO::WRN_SCHEDULING_NOT_SUPPORTED, scheduling_label.at(scheduling).str.c_str() );
	/* Fall through */
    case SCHEDULE_CUSTOMER:
	if ( dom->hasQueueLength() ) {
	    LQIO::input_error( LQIO::WRN_TASK_QUEUE_LENGTH, task_name.c_str() );
	}
	if ( dom->isInfinite() ) {
	    dom->input_error( LQIO::ERR_REFERENCE_TASK_IS_INFINITE );
	}
	task = new Task( dom, Task::Type::REF_TASK, processor );
	break;

    default:
	dom->runtime_error( LQIO::WRN_SCHEDULING_NOT_SUPPORTED, scheduling_label.at(scheduling).str.c_str() );
	/* Fall Through */
    case SCHEDULE_FIFO:
	if ( dom->hasThinkTime() ) {
	    dom->runtime_error( LQIO::ERR_NON_REF_THINK_TIME );
	}
	if ( !Pragma::__pragmas->default_task_scheduling() ) {
	    scheduling = Pragma::__pragmas->task_scheduling();
	}
        task = new Task( dom, Task::Type::SERVER, processor );
	break;

    case SCHEDULE_DELAY:
	if ( dom->hasThinkTime() ) {
	    dom->runtime_error( LQIO::ERR_NON_REF_THINK_TIME );
	}
	if ( dom->hasQueueLength() ) {
	    LQIO::input_error( LQIO::WRN_TASK_QUEUE_LENGTH, task_name.c_str() );
	}
	if ( dom->isMultiserver() ) {
	    dom->runtime_error( LQIO::WRN_INFINITE_MULTI_SERVER, dom->getCopiesValue() );
	}
	task = new Task( dom, Task::Type::SERVER, processor );
	break;

    case SCHEDULE_SEMAPHORE:
	if ( dom->hasQueueLength() ) {
	    LQIO::input_error( LQIO::WRN_TASK_QUEUE_LENGTH, task_name.c_str() );
	}
	if ( dom->getCopiesValue() != 1 ) {
	    dom->runtime_error( LQIO::ERR_INFINITE_SERVER );
	}
#if 0
	if ( n_copies <= 0 ) {
	    dom->runtime_error( LQIO::ERR_INFINITE_SERVER );
	    dom->setCopiesValue( 1 );
	} else if ( n_copies > MAX_MULT ) {
	    LQIO::input_error( LQIO::ERR_TOO_MANY_X, "multi-server copies", MAX_MULT );
	    dom->setCopiesValue( MAX_MULT );
	}
#endif
	task = new Task( dom, Task::Type::SEMAPHORE, processor );
	break;
    }

    assert( task != nullptr );
    task->set_scheduling( scheduling );
    processor->add_task( task );
    ::__task.push_back( task );
    return task;
}


void
Task::initialize()
{
    check();
    
    if ( !entries.size() ) {
	get_dom()->runtime_error( LQIO::ERR_TASK_HAS_NO_ENTRIES );
    } 
    if ( processor() != nullptr && processor()->get_scheduling() == SCHEDULE_CFS && dynamic_cast<const LQIO::DOM::Task *>(get_dom())->getGroup() == nullptr ) {
	get_dom()->runtime_error( LQIO::ERR_NO_GROUP_SPECIFIED, processor()->name() );
    }
    if ( n_activities() ) {
	if( std::all_of( entries.begin(), entries.end(), []( const Entry * entry ){ return entry->start_activity() == nullptr; } ) ) {
	    get_dom()->runtime_error( LQIO::ERR_NO_START_ACTIVITIES );
	}
	std::for_each( activities.begin(), activities.end(), std::mem_fn( &Activity::initialize ) );
    }

    if ( type() == Task::Type::SEMAPHORE ) {
	if ( n_entries() != N_SEMAPHORE_ENTRIES ) {
	    get_dom()->runtime_error( LQIO::ERR_TASK_ENTRY_COUNT, n_entries(), N_SEMAPHORE_ENTRIES );
	}
	if ( ( entries[0]->semaphore_type() == LQIO::DOM::Entry::Semaphore::SIGNAL && entries[1]->semaphore_type() !=  LQIO::DOM::Entry::Semaphore::WAIT )
	     || ( entries[0]->semaphore_type() == LQIO::DOM::Entry::Semaphore::WAIT && entries[1]->semaphore_type() != LQIO::DOM::Entry::Semaphore::SIGNAL ) ) {
	    get_dom()->runtime_error( LQIO::ERR_DUPLICATE_SEMAPHORE_ENTRY_TYPES,
				      entries[0]->get_dom()->getName().c_str(),
				      entries[1]->get_dom()->getName().c_str(),
				      entries[0]->semaphore_type() == LQIO::DOM::Entry::Semaphore::SIGNAL ? "signal" : "wait" );
	}
    } else if ( type() == Task::Type::REF_TASK ) {
	if ( std::none_of( entries.begin(), entries.end(), std::mem_fn( &Entry::has_calls ) )
	     && std::none_of( activities.begin(), activities.end(), std::mem_fn( &Phase::has_calls ) ) ) {
	    get_dom()->runtime_error( LQIO::WRN_NOT_USED );
	}
    }

    /* Check for external joins. */

    for ( vector<ActivityList *>::const_iterator l = act_lists.begin(); l != act_lists.end(); ++l ) {
	if ( (*l)->check_external_joins( ) ) {
	    _sync_server = true;
	}
	if ( (*l)->check_fork_no_join( ) || (*l)->check_quorum_join( ) ) {
	    _needs_flush = true;
	}
	if ( (*l)->check_fork_has_join( ) ) {
	    _has_main_thread = true;
	}
	if ( (*l)->type() == ActivityList::Type::AND_FORK ) {
	    if ( (*l)->n_acts() > n_threads() ) {
		_n_threads = (*l)->n_acts();
	    }
	}
    }

    if ( _sync_server && multiplicity() != 1 ) {
	LQIO::runtime_error( ERR_MULTI_SYNC_SERVER, name() );
    }

    /* Can't have in-service probabilites on single phase task! */

    if ( n_phases() < 2 ) {
	_inservice_flag = false;
    }

    for ( vector<Entry *>::const_iterator e = entries.begin(); e != entries.end(); ++e ) {
	double ysum = 0.0;
	double zsum = 0.0;

	for ( vector<Task *>::const_iterator i = ::__task.begin(); i != ::__task.end(); ++i ) {

	    /* e of i called by d of j ?*/

	    for ( vector<Entry *>::const_iterator d = (*i)->entries.begin(); d != (*i)->entries.end(); ++d ) {
		ysum += (*d)->yy(*e) + (*d)->prob_fwd(*e);
		zsum += (*d)->zz(*e);
	    }

	    for ( vector<Activity *>::const_iterator d = (*i)->activities.begin(); d != (*i)->activities.end(); ++d ) {
		ysum += (*d)->y(*e);
		zsum += (*d)->z(*e);
	    }
	}

	if ( ysum > 0.0 && zsum > 0.0 ) {
	    (*e)->get_dom()->runtime_error( LQIO::ERR_OPEN_AND_CLOSED_CLASSES );
	} else if ( ysum + zsum == 0.0 && is_server() ) {
	    (*e)->get_dom()->runtime_error( LQIO::WRN_ENTRY_HAS_NO_REQUESTS );
	}
    }

    build_forwarding_lists();
}

/* Priority for this __task	*/
int Task::priority() const
{
    try {
	return dynamic_cast<const LQIO::DOM::Task *>(get_dom())->getPriorityValue();
    }
    catch ( const std::domain_error &e ) {
	get_dom()->throw_invalid_parameter( "priority", e.what() );
    }
    return 0;
}

double Task::think_time() const
{
    if ( dynamic_cast<const LQIO::DOM::Task *>(get_dom())->hasThinkTime() ) {
	try {
	    return dynamic_cast<const LQIO::DOM::Task *>(get_dom())->getThinkTimeValue();
	}
	catch ( const std::domain_error &e ) {
	    get_dom()->throw_invalid_parameter( "think time", e.what() );
	}
    }
    return 0.;
}


bool Task::is_server() const
{
    switch ( type() ) {
    case Task::Type::SERVER:
    case Task::Type::SEMAPHORE: return true;
    default: return false;
    }
}

bool Task::is_client() const
{
    switch ( type() ) {
    case Task::Type::REF_TASK:
    case Task::Type::OPEN_SRC: return true;
    default: return false;
    }
}


bool Task::is_single_place_task() const
{
    return type() == Task::Type::REF_TASK && (!Pragma::__pragmas->disjoint_customers()
				  || (n_threads() > 1 && !processor()->is_infinite()));
}


bool Task::is_dummy_task() const
{
    return type() == Task::Type::REF_TASK && !has_service_time(); // && !has_deterministic_phases();
}


bool Task::has_service_time() const
{
    return std::any_of( entries.begin(), entries.end(), std::mem_fn( &Entry::has_service_time ) );
}


bool Task::has_deterministic_phases() const
{
    return std::any_of( entries.begin(), entries.end(), std::mem_fn( &Entry::has_deterministic_calls ) )
	|| std::any_of( activities.begin(), activities.end(), std::mem_fn( &Phase::has_deterministic_calls ) );
}


bool Task::scheduling_is_ok() const
{
    switch ( get_scheduling() ) {
    case SCHEDULE_CUSTOMER: return !is_infinite();
    case SCHEDULE_DELAY: return is_infinite();
    case SCHEDULE_FIFO:
    case SCHEDULE_HOL:
    case SCHEDULE_RAND:
    case SCHEDULE_SEMAPHORE: return true;
    default: return false;
    }
}


unsigned int Task::ref_count() const
{
    if ( is_infinite() ) {
	return max_queue_length();
    } else {
	return multiplicity();
    }
}


/*
 * Return the number of "customers" to generate.  Open arrival sources
 * alway generate one copy. Reference tasks generate one copy if the
 * disjoint_customers flag is set.
 */

unsigned Task::n_customers() const
{
    if ( is_single_place_task() || type() == Task::Type::OPEN_SRC) {
	return 1;
    } else {
	return multiplicity();
    }
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
	activity = new Activity( dom, this );
	activities.push_back( activity );
    }
    return activity;
}




/*
 * Find the activity.  Return error if not found.
 */

Activity *
Task::find_activity( const std::string& name ) const
{
    vector<Activity *>::const_iterator ap = std::find_if( activities.begin(), activities.end(), [&]( Activity * activity ){ return name == activity->name(); } );
    if ( ap != activities.end() ) {
	return *ap;
    } else {
	return 0;
    }
}



void Task::set_start_activity( LQIO::DOM::Entry* dom )
{
    Activity* activity = find_activity( dom->getStartActivity()->getName() );
    Entry* realEntry = Entry::find( dom->getName() );

    realEntry->set_start_activity(activity);
}

/*----------------------------------------------------------------------*/
/* Tasks.								*/
/*----------------------------------------------------------------------*/

/*
 * Initialize stuff for fifo queueing centers.
 */

unsigned int Task::set_queue_length()
{
    unsigned length = 0;
#if !defined(BUFFER_BY_ENTRY)
    bool open_model = false;
#endif

    if ( is_client() ) return 0;  				/* Skip reference tasks. 	*/

    /*
     * Count calls to my entries.
     */

    for ( vector<Entry *>::const_iterator e = entries.begin(); e != entries.end(); ++e ) {
	set<const Task *> visit;

	/* Count rendezvous from other tasks 'i' */

	for ( vector<Task *>::const_iterator i = ::__task.begin(); i != ::__task.end(); ++i ) {
	    if ( (*i) == this || (*i)->type() == Task::Type::OPEN_SRC ) continue;

	    for ( vector<Entry *>::const_iterator d = (*i)->entries.begin(); d != (*i)->entries.end(); ++d ) {
		if ( (*d)->prob_fwd(*e) == 0.0 ) continue;

		for ( vector<Forwarding *>::const_iterator f = (*d)->forwards.begin(); f != (*d)->forwards.end(); ++f ) {
		    const Task * root = (*f)->_root->task();
		    if ( visit.find( root ) != visit.end() ) continue;
		    length += root->multiplicity();
		    visit.insert( root );
		}
	    }

	    if( visit.find( (*i) ) != visit.end() ) goto again;		/* No point.. already counted */

	    for ( vector<Entry *>::const_iterator d = (*i)->entries.begin(); d != (*i)->entries.end(); ++d ) {
		if ( !(*d)->is_regular_entry() ) continue;

		for ( unsigned p = 1; p <= (*d)->n_phases(); p++) {
		    if ( (*d)->phase[p].y(*e) == 0.0 && (*d)->phase[p].z(*e) == 0.0 ) continue;
		    if ( visit.find( *i ) == visit.end() ) {
			length += (*i)->multiplicity();
			visit.insert( *i );
			goto again;
		    }
		}
	    }

	    for ( vector<Activity *>::const_iterator a = (*i)->activities.begin(); a != (*i)->activities.end(); ++a ) {
		if ( (*a)->y(*e) == 0.0 ) continue;
		if ( visit.find( *i ) == visit.end() ) {
		    length += (*i)->multiplicity();
		    visit.insert( *i );
		    goto again;
		}
	    }
	again: ;
	}

	/* Count open arrivals at entries. */

	if ( (*e)->requests() == Requesting_Type::SEND_NO_REPLY && !is_infinite() ) {
#if defined(BUFFER_BY_ENTRY)
	    length += Task::__open_model_tokens;
#else
	    if ( !open_model ) {
		open_model = true;
		if ( _sync_server || has_random_queueing() || type() == Task::Type::SEMAPHORE || is_infinite() ) {
		    length += 1;
		} else {
		    length += _open_tokens;
		}
	    }
#endif
	}
    }

/* 	assert( length > 0 ); */
    _max_queue_length = length;
    _requestor_no     = 0;

    return length;
}



/*
 * Starting from the reference tasks, trace all forwarding requests.
 */

void
Task::build_forwarding_lists()
{
//    if ( type() != Task::Type::REF_TASK ) return;

    for ( vector<Entry *>::const_iterator e = entries.begin(); e != entries.end(); ++e ) {
	for ( unsigned p = 1; p <= (*e)->n_phases(); ++p ) {
	    (*e)->phase[p].build_forwarding_list();
	}
    }

    for ( vector<Activity *>::const_iterator a = activities.begin(); a != activities.end(); ++a ) {
	(*a)->build_forwarding_list();
    }
}



/*
 * Create places used to control fifo queues for entries.
 */

void
Task::make_queue_places()
{
    const double x_pos = get_x_pos() - 0.5;
    const double y_pos = get_y_pos();
    const unsigned ne  = n_entries();

    if ( _queue_made ) return;
    _queue_made = true;

    /*
     * Create places for queueing at entry.
     */

    for ( unsigned k = 1; k <= max_queue_length(); ++k ) {
        (void) move_place_tag( create_place( X_OFFSET(1,0.0), y_pos + __queue_y_offset + (double)k, FIFO_LAYER, 1, "Sh%s%d", name(), k ), PLACE_X_OFFSET, PLACE_Y_OFFSET );
    }
}



/*
 * Template logic to create a task and its associated processors.
 * Entries are created later.
 */

void
Task::transmorgrify()
{
    double x_pos;
    double y_pos;
    double next_x = 0;
    const unsigned ne = n_entries();

    if ( is_client() ) {
	x_pos = __client_x_offset;
	y_pos = CLIENT_Y_OFFSET;
    } else {
	x_pos = __server_x_offset;
	y_pos = __server_y_offset;
    }
    set_origin( x_pos, y_pos );

    /* On tasks with dedicated processors, move the processor place. */

    if ( processor() && processor()->PX && processor()->is_single_place_processor() ) {
        processor()->set_origin( X_OFFSET(4+1,0), y_pos );
	processor()->PX->center.x = IN_TO_PIX( processor()->get_x_pos() );
	processor()->PX->center.y = IN_TO_PIX( processor()->get_y_pos() );
    }

    /* task places */

    if ( is_single_place_task()
	 || type() == Task::Type::OPEN_SRC
	 || (is_infinite() && n_threads() == 1) ) {

	next_x = create_instance( x_pos, y_pos, 0, INFINITE_SERVER );

    } else if ( is_infinite() ) {
	for ( unsigned int m = 0; m < max_queue_length(); ++m ) {
	    next_x = create_instance( x_pos+m*0.25, y_pos+m*0.25, m, 1 );
	}
    } else {
	struct place_object * t_place = nullptr;
#if defined(BUG_393)
	if ( Pragma::__pragmas->save_marginal_probabilities() && !is_client() && !is_infinite() && multiplicity() > 1 ) {
	    t_place = create_place( x_pos, y_pos, make_layer_mask( MEASUREMENT_LAYER ), multiplicity(), "T%s", name() );
	}
#endif
	for ( unsigned int m = 0; m < multiplicity(); ++m ) {
	    next_x = create_instance( x_pos+m*0.25, y_pos+m*0.25, m, 1, t_place );
	}
#if defined(BUG_393)
	if ( t_place != nullptr ) {
	    set_origin( next_x, y_pos );
	}
#endif
    }

    if ( is_client() ) {
	__client_x_offset = next_x;
    } else {
	__server_x_offset = next_x;
    }
}



/*
 * Make an instance of a __task
 */

double
Task::create_instance( double base_x_pos, double base_y_pos, unsigned m, short enabling, struct place_object * T_place )
{
    double x_pos	= base_x_pos;
    double y_pos	= base_y_pos;
    struct place_object * d_place = 0;
    unsigned ix_e;
    const unsigned ne 	= n_entries();

    double temp_x;
    double max_pos 	= base_x_pos;

    unsigned customers;
    const LAYER layer_mask	= make_layer_mask( m );
    
    if ( is_infinite() ) {
	customers = Task::__open_model_tokens;
    } else if ( is_single_place_task() || type() == Task::Type::OPEN_SRC ) {
	customers = multiplicity();
    } else {
	customers = 1;
    }

    if ( n_activities() > 1 ) {
	temp_x = X_OFFSET(1,n_entries()*0.5);
    } else if ( n_phases() == 1 ) {
	temp_x = X_OFFSET(1,0.5);
    } else {
	temp_x = X_OFFSET(n_phases()*3,0);
    }

    d_place = create_place( temp_x, Y_OFFSET(0.0), layer_mask, customers, "T%s%d", name(), m );
    TX[m] = d_place;

    if ( think_time() ) {
	struct place_object * z_place = create_place( X_OFFSET(1,-0.5), Y_OFFSET(0.0), layer_mask, 0, "Z%s%d", name(), m );
	ZX[m] = z_place;
	struct trans_object * c_trans = create_trans( X_OFFSET(1,0.0), Y_OFFSET(0.0), layer_mask,
						      1.0/think_time(), INFINITE_SERVER, EXPONENTIAL, "z%s%d", name(), m );
	orient_trans( c_trans, 1 );
	create_arc( layer_mask, TO_TRANS, c_trans, d_place );
	create_arc( layer_mask, TO_PLACE, c_trans, z_place );
    }


    if ( type() == Task::Type::SEMAPHORE ) {	/* BUG_164 */
	LX[m] = create_place( temp_x+0.5, Y_OFFSET(0.0), layer_mask, 0, "LX%s%d", name(), m );
    } else if ( _sync_server ) {
	d_place = create_place( temp_x+0.5, Y_OFFSET(0.0), layer_mask, 0, "Gd%s%d", name(), m );
	GdX[m] = d_place;
	gdX[m] = create_trans( temp_x+0.5, Y_OFFSET(0.0)-0.5, layer_mask,  1.0, 1, IMMEDIATE, "gd%s%d", name(), m );
	create_arc( layer_mask, TO_TRANS, gdX[m], d_place );
	create_arc( layer_mask, TO_PLACE, gdX[m], TX[m] );
#if defined(BUG_393)
	if ( T_place != nullptr ) {
	    create_arc( make_layer_mask( MEASUREMENT_LAYER ), TO_PLACE, gdX[m], T_place );	/* Instrumentation */
	}
#endif
#if BUG_163
	SyX[m] = create_place( temp_x+1.0, Y_OFFSET(0.0), layer_mask, 0, "SYNC%s%d" , name(), m );
#endif
    } else if ( _needs_flush ) {
	struct trans_object * d_trans = create_trans( temp_x+0.5, Y_OFFSET(0.0)-0.5, layer_mask, 1.0, 1, IMMEDIATE, "gd%s%d", name(), m );
	gdX[m] = d_trans;
	create_arc( layer_mask, TO_PLACE, d_trans, TX[m] );
#if defined(BUG_393)
	if ( T_place != nullptr ) {
	    create_arc( make_layer_mask( MEASUREMENT_LAYER ), TO_PLACE, d_trans, T_place );
	}
#endif
	d_place = create_place( temp_x+0.5, Y_OFFSET(0.0), layer_mask, 0, "Gd%s%d", name(), m ); 	/* We don't allow multiple copies */
	GdX[m] = d_place;
	create_arc( layer_mask, TO_TRANS, d_trans, d_place );
    }

    /*
     * Places for replies. Create here for activities because sync
     * servers can reply to entries before the entry itself is
     * created
     */

    ix_e = 0;
    for ( std::vector<Entry *>::const_iterator e = entries.begin(); e != entries.end(); ++e ) {
	if ( is_server() && (*e)->requests() == Requesting_Type::RENDEZVOUS ) {
	    const LAYER layer_mask = ENTRY_LAYER((*e)->entry_id())|(m == 0 ? PRIMARY_LAYER : 0);

	    x_pos = base_x_pos + ix_e * 0.5;
	    (*e)->DX[m] = move_place_tag( create_place( X_OFFSET(3,0), Y_OFFSET(0.0), layer_mask, 0, "D%s%d", (*e)->name(), m ), PLACE_X_OFFSET, -0.25 );
	}
	if ( !(*e)->is_regular_entry() ) {
	    unsigned p;
	    double task_y_offset = Y_OFFSET(1.0);
	    if ( inservice_flag() ) {
		task_y_offset += 1.0;
	    }
	    for ( p = 1; p <= (*e)->n_phases(); ++p ) {
		(*e)->phase[p].XX[m] = create_place( X_OFFSET(p,1.0), task_y_offset-1.0, MEASUREMENT_LAYER, 0, "X%s%d%d", (*e)->name(), p, m );
	    }
	}
	++ix_e;
    }

    /* Create the entries */

    ix_e = 0;
    for ( std::vector<Entry *>::const_iterator e = entries.begin(); e != entries.end(); ++e ) {
	if ( (*e)->n_phases() == 0 ) continue;	/* BUG 414 -- not defined */
	double next_pos = (*e)->transmorgrify( base_x_pos, base_y_pos, ix_e, d_place, m, enabling );
	if ( next_pos > max_pos ) {
	    max_pos = next_pos;
	}
	++ix_e;
    }

    return max_pos;
}



/*
 * Create an entry mask for this task
 */

LAYER
Task::make_layer_mask( const unsigned m )
{
    LAYER mask = (m == 0 ? PRIMARY_LAYER : 0);

    for ( std::vector<Entry *>::const_iterator e = entries.begin(); e != entries.end(); ++e ) {
	mask |= ENTRY_LAYER((*e)->entry_id());
    }
    return mask;
}

/* ------------------------------------------------------------------------ */
/* Results								    */
/* ------------------------------------------------------------------------ */

void
Task::get_results()
{
    const unsigned int max_m = n_customers();
    for ( unsigned int m = 0; m < max_m; ++m ) {
	get_results_for( m );
    }
    std::for_each( entries.begin(), entries.end(), []( const Entry * entry ){ if ( entry->messages_lost() ) { entry->get_dom()->runtime_error( LQIO::ADV_MESSAGES_DROPPED ); } } );
}



void
Task::get_results_for( unsigned m )
{
    if ( is_single_place_task() ) {
	_utilization[m] = multiplicity() - get_pmmean( "T%s%d", name(), m );
    } else if ( !is_infinite() ) {
	_utilization[m] = 1.0 - get_pmmean( "T%s%d", name(), m );
    } else {
	_utilization[m] = Task::__open_model_tokens - get_pmmean( "T%s%d", name(), m );
    }
    assert( _utilization[m] >= 0. );
    task_tokens[m] = 0.0;

    if ( type() == Task::Type::SEMAPHORE ) {
	lock_tokens[m] = get_pmmean( "LX%s%d", name(), m );
    } else {
	lock_tokens[m] = 0.0;
    }

    /* for each entry of i	    */

    for ( std::vector<Entry *>::const_iterator e = entries.begin(); e != entries.end(); ++e ) {
	if ( (*e)->is_regular_entry() ) {

	    for ( unsigned p = 1; p <= (*e)->n_phases(); ++p ) {
	        Phase& phase = (*e)->phase[p];
		if ( !phase.get_dom()->isPresent() ) continue;
		(*e)->_throughput[m] = get_throughput( (*e), &phase, m );

		/* Task utilization (includes queue) */

		task_tokens[m] += phase.get_utilization( m );

		/* Procesor utilization (ignores queue) */

		if ( processor() ) {
		    processor()->proc_tokens[m] += phase.get_processor_utilization( m );
		}
	    }

	} else {

	    /* Task utilization */

	    double tokens[DIMPH+1];
	    (*e)->_throughput[m] = get_throughput( *e, (*e)->start_activity(), m );

	    for ( unsigned p = 0; p <= DIMPH; ++p ) {
		tokens[p] = 0;
	    }
	    (*e)->start_activity()->follow_activity_for_tokens( (*e), 1, m, FOLLOW_BRANCH, 1.0, &Phase::get_utilization, tokens );

	    /* Use tokens found from instrumentation */
	    for ( unsigned int p = 1; p <= (*e)->n_phases(); ++p ) {
		(*e)->phase[p].task_tokens[m] = get_pmmean( "X%s%d%d", (*e)->name(), p, m );	/* Phase service time.	*/
		task_tokens[m] += (*e)->phase[p].task_tokens[m];
	    }

	    /* Procesor utilization */

	    if ( processor() ) {
		for ( unsigned p = 0; p <= DIMPH; ++p ) {
		    tokens[p] = 0;
		}
		(*e)->start_activity()->follow_activity_for_tokens( (*e), 1, m, SUM_BRANCHES, 1.0, &Phase::get_processor_utilization, tokens );
		for ( unsigned p = 1; p <= (*e)->n_phases(); ++p ) {
		    (*e)->phase[p].proc_tokens[m]   = tokens[p];
		    processor()->proc_tokens[m]    += tokens[p];
		    tokens[p] = 0;
		}
	    }
	}
    }
}



/*
 * Find entry throughput.
 */

double
Task::get_throughput( const Entry * d, const Phase * phase_d, unsigned m  )
{
    double throughput = 0.0;

    if ( !inservice_flag() || !is_server() || d == 0 ) {
      //	throughput = get_tput( IMMEDIATE, "done%s%d", phase_d->name(), m );	/* done_P transition  */
        throughput = phase_d->doneX[m]->f_time;		/* Access directly */
    } else {
	unsigned p_d = d->n_phases();
	for ( vector<Entry *>::const_iterator e = ::__entry.begin(); e != ::__entry.end(); ++e ) {
	    unsigned max_m = n_customers();
	    unsigned p_e;

	    for ( p_e = 1; p_e <= (*e)->n_phases(); ++p_e ) {
		const Phase * phase_e = &(*e)->phase[p_e];
		if ( phase_e->y(d) + phase_e->z(d) == 0.0 ) continue;

		for ( unsigned int n = 0; n < max_m; ++n ) {
		    throughput += get_tput( IMMEDIATE,"ph%d%s%d%s%d", p_d, phase_e->name(), n, d->name(), m  );
		}
	    }
	}
    }

    if ( debug_flag && d ) {
	(void) fprintf( stddbg, "%-20.20s tput=%15.10g ", d->name(), throughput );
    }
    return throughput;
}



/*
 * Return the total throughput between i and j.
 */

void
Task::get_total_throughput( Task * dst, double tot_tput[] )
{
    unsigned p;			/* phase index.			*/

    for ( p = 0; p <= n_phases(); ++p ) {
	tot_tput[p] = 0.0;
    }

    for ( vector<Entry *>::const_iterator d = entries.begin(); d != entries.end(); ++d ) {
	for ( vector<Entry *>::const_iterator e = dst->entries.begin(); e != dst->entries.end(); ++e ) {
	    for ( p = 1; p <= (*d)->n_phases(); ++p ) {
		tot_tput[p] += (*d)->phase[p].lambda( 0, *e, &(*d)->phase[p] );
	    }
	}
    }

    for ( p = 1; p <= n_phases(); ++p ) {
	tot_tput[0] += tot_tput[p];
    }
}




/*
 * Print throughputs and utilizations
 */

void
Task::insert_DOM_results() const
{
    double tput_sum     = 0.0;
    double util_sum     = 0.0;
    unsigned max_m      = n_customers();
    double col_tot[DIMPH];

    for ( unsigned p = 0; p < DIMPH; p++ ) {
	col_tot[p] = 0;
    }

    /* for each entry of i	    */

    for ( std::vector<Entry *>::const_iterator e = entries.begin(); e != entries.end(); ++e ) {
	double tput = 0.0;

	(*e)->insert_DOM_results();

	for ( unsigned m = 0; m < max_m; ++m ) {
	    tput += (*e)->_throughput[m];
	}
	if ( type() != Task::Type::SEMAPHORE || e == entries.begin() ) {
	    tput_sum += tput;		// For semaphore tasks, only count throughput once.
	}

	for ( unsigned p = 1; p <= n_phases(); p++ ) {
	    double util = (*e)->task_utilization( p );
	    col_tot[p-1] += util;
	    util_sum     += util;
	}
    }

    if ( is_sync_server() ) {
	tput_sum = get_tput( IMMEDIATE, "gd%s%d", name(), 0 );
    }

    LQIO::DOM::Entity * dom = const_cast<LQIO::DOM::Entity *>(get_dom());
    dom->setResultPhaseUtilizations(n_phases(),col_tot)
	.setResultUtilization(util_sum)
	.setResultThroughput(tput_sum);

#if defined(BUG_393)
    const unsigned int m = multiplicity();
    if ( Pragma::__pragmas->save_marginal_probabilities() && !is_client() && m > 1 ) {
	dom->setResultMarginalQueueProbabilitiesSize( m + 1 );

	/* Can we get it from the processor? Only if it's a single place with enough tokens */
	if ( processor()->n_tasks() == 1 && (processor()->is_infinite() || processor()->multiplicity() >= m )) {
	    for ( unsigned int i = 0; i <= m; ++i ) {
		dom->setResultMarginalQueueProbability( m - i, get_prob( i, "P%s", processor()->name() ) );	/* Token distribution is backwards */
	    }
	} else {
	    for ( unsigned int i = 0; i <= m; ++i ) {
		dom->setResultMarginalQueueProbability( m - i, get_prob( i, "T%s", name() ) );			/* Grab from special place	*/
	    }
	}
    }
#endif

    for ( vector<Activity *>::const_iterator a = activities.begin(); a != activities.end(); ++a ) {
	(*a)->insert_DOM_results();
    }


    /* Forks-Join lists.		*/
    for ( std::vector<ActivityList *>::const_iterator l = act_lists.begin(); l != act_lists.end(); ++l ) {
	if ( (*l)->type() != ActivityList::Type::AND_JOIN ) continue;
	(*l)->insert_DOM_results();
    }


}

/* -------------------------------------------------------------------- */
/* Open Arrival Tasks							*/
/* -------------------------------------------------------------------- */

OpenTask::OpenTask( LQIO::DOM::Document * document, const std::string& name, const Entry * dst )
    : Task( 0, Type::OPEN_SRC, 0 ), _name(name), _dst(dst)
{
}


OpenTask::~OpenTask()
{
}


void
OpenTask::get_results_for( unsigned m )
{
    Phase& phase = entries[0]->phase[1];
    assert( phase._call.size() == 1 );
    Call& call = phase._call.begin()->second;
    phase.compute_queueing_delay( call, m, _dst, multiplicity(), &phase );
}


/*
 * Print estimated waiting time at servers with open arrival.
 */

void
OpenTask::insert_DOM_results() const
{
    LQIO::DOM::Entry * entry = const_cast<LQIO::DOM::Entry *>(_dst->get_dom());
    Phase& phase = entries[0]->phase[1];
    entry->setResultWaitingTime( phase.response_time( _dst ) );
    Call& call = phase._call.begin()->second;
    if ( call._dp > 0. ) {
	entry->setResultDropProbability( call._dp );
    }
}
