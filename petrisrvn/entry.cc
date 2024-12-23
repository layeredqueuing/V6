/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* September 1991.							*/
/* August 2003.								*/
/************************************************************************/

/*
 * $Id: entry.cc 17458 2024-11-12 11:54:17Z greg $
 *
 * Generate a Petri-net from an SRVN description.
 *
 */

#include "petrisrvn.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <sstream>
#include <vector>
#include <lqio/glblerr.h>
#include <lqio/dom_entry.h>
#include "makeobj.h"
#include "errmsg.h"
#include "entry.h"
#include "task.h"
#include "phase.h"
#include "pragma.h"
#include "results.h"

using namespace std;

std::vector<Entry *> __entry;
unsigned int Entry::__next_entry_id = 1;

Entry::Entry( LQIO::DOM::Entry * dom, Task * task )
    : phase(DIMPH+1),
      forwards(),
#if defined(BUFFER_BY_ENTRY)
      ZZ(0),
#endif
      _dom(dom),
      _task(task),
      _start_activity(nullptr),
      _open_arrival_phase(nullptr),
      _entry_id(__next_entry_id++),
      _requests(Requesting_Type::NOT_CALLED),
      _replies(false),
      _random_queueing(false),
      _has_service_time(false),			/* Sevice time anywhere.	*/
      _rel_prob(0.),
      _n_phases(0),
      _fwd()
{
    unsigned n_phases = dom->getEntryType() == LQIO::DOM::Entry::Type::STANDARD ? dom->getMaximumPhase() : 2;
    for ( unsigned int p = 1; p <= n_phases; ++p ) {
	phase[p].set_dom( dom->getPhase(p), this );
    }
    clear();
}


void
Entry::clear()
{
    for ( unsigned m = 0; m < MAX_MULT; ++m ) {
	DX[m] = nullptr;
	GdX[m] = nullptr;
	_throughput[m] = 0;
    }
}

Entry *
Entry::create( LQIO::DOM::Entry * dom, Task * task )
{
    const std::string& name = dom->getName();
    if ( std::find_if( __entry.begin(), __entry.end(), [&]( const Entry * entry ){ return name == entry->name(); } ) != __entry.end() ) {
	dom->runtime_error( LQIO::ERR_DUPLICATE_SYMBOL );
	return nullptr;
    } else {
	Entry * entry = new Entry( dom, task );
	::__entry.push_back( entry );
	return entry;
    }
}


/*
 * Perform entry checks.
 */

const char * Entry::name() const
{
    return get_dom()->getName().c_str();
}


Entry&  Entry::set_start_activity( Activity * activity )
{
    _start_activity = activity;
    return *this;
}

double Entry::prob_fwd( const Entry * entry ) const
{
    std::map<const Entry *,Call>::const_iterator e = _fwd.find(entry);
    if ( e != _fwd.end() ) {
	const LQIO::DOM::Call * call = e->second.get_dom();
	try {
	    const double value = call->getCallMeanValue();
	    if ( value > 1.0 ) {
		std::stringstream ss;
		ss << value << " > " << 1;
		throw std::domain_error( ss.str() );
	    }
	    return value;
	}
	catch ( const std::domain_error& e ) {
	    call->runtime_error( LQIO::ERR_INVALID_PARAMETER, "forwarding probability", e.what() );
	    throw std::domain_error( std::string( "invalid parameter: " ) + e.what() );
	}
    }
    return 0.0;
}


double Entry::yy(const Entry* entry) const
{
    return std::accumulate( std::next(phase.begin()), phase.end(), 0.0, [=]( double sum, const Phase& phase ){ return sum + phase.y(entry); } );
}

double Entry::zz(const Entry* entry) const
{
    return std::accumulate( std::next(phase.begin()), phase.end(), 0.0, [=]( double sum, const Phase& phase ){ return sum + phase.z(entry); } );
}

bool
Entry::has_calls() const
{
    if ( !is_regular_entry() ) return false;
    return std::any_of( std::next(phase.begin()), phase.end(), std::mem_fn( &Phase::has_calls ) );
}

bool
Entry::has_deterministic_calls() const
{
    if ( !is_regular_entry() ) return false;
    return std::any_of( std::next(phase.begin()), phase.end(), std::mem_fn( &Phase::has_deterministic_calls ) );
}

bool
Entry::test_and_set( LQIO::DOM::Entry::Type type )
{
    const bool rc = get_dom()->entryTypeOk( type );
    if ( !rc ) {
	get_dom()->input_error( LQIO::ERR_MIXED_ENTRY_TYPES);
    }
    return rc;
}

bool
Entry::test_and_set_recv( Requesting_Type recv )
{
    if ( task()->is_client() ) {
	task()->get_dom()->runtime_error( LQIO::ERR_REFERENCE_TASK_IS_RECEIVER, name() );
	return false;
    } else if ( _requests != Requesting_Type::NOT_CALLED && _requests != recv ) {
	get_dom()->input_error( LQIO::ERR_OPEN_AND_CLOSED_CLASSES );
	return false;
    } else {
	_requests = recv;
	return true;
    }
}


void
Entry::add_call( const unsigned int p, LQIO::DOM::Call * call )
{
    /* Make sure this is one of the supported call types */
    if (call->getCallType() != LQIO::DOM::Call::Type::SEND_NO_REPLY &&
	call->getCallType() != LQIO::DOM::Call::Type::RENDEZVOUS ) {
	abort();
    }

    if ( !test_and_set( LQIO::DOM::Entry::Type::STANDARD ) ) return;
    phase[p].add_call( call );
}


/* static */ void
Entry::add_fwd_call( LQIO::DOM::Call * call )
{
    /* Make sure this is one of the supported call types */
    if (call->getCallType() != LQIO::DOM::Call::Type::FORWARD) {
	abort();
    }

    /* Internal Entry references */
    Entry * from_entry;
    Entry * to_entry;

    if ( Entry::find( call->getSourceObject()->getName(), from_entry, call->getDestinationEntry()->getName(), to_entry ) && to_entry->test_and_set_recv( Requesting_Type::RENDEZVOUS ) ) {
	const Task * from_task = from_entry->task();
	if ( from_task->type() == Task::Type::REF_TASK ) {
	    from_task->get_dom()->runtime_error( LQIO::ERR_REFERENCE_TASK_FORWARDING, from_entry->name() );
	} else {
	    from_entry->_fwd[to_entry].set_dom( call );		/* Save dom */
	}
    }
}

/*
 * Determine the maximum number of phases over all entries,
 * and check that all phases have some service time and set
 * the release probability.  The latter is only important when
 * we are forwarding.
 */

void
Entry::initialize()
{
    _n_phases = 0;

    _has_service_time = false;
    for ( auto& fwd : forwards ) delete fwd;		// BUG 424
    forwards.clear();					// BUG 424

    if ( is_regular_entry() ) {
	for ( unsigned int p = 1; p <= DIMPH; ++p ) {
	    Phase * curr_phase = &phase[p];
	    if ( !curr_phase->get_dom() ) continue;

	    curr_phase->initialize();

	    if ( curr_phase->has_service_time() || curr_phase->think_time() > 0.0 ) {
		_has_service_time = true;
	    }
	    if ( ( curr_phase->has_calls() || curr_phase->has_service_time() ) && p > n_phases() ) {
		set_n_phases( p );
	    }
	}
    } else if ( is_activity_entry() ) {
	std::deque<Activity *> activity_stack;
	std::deque<ActivityList *> fork_stack;

	_has_service_time = start_activity()->find_children( activity_stack, fork_stack, this );
	double n_replies = start_activity()->count_replies( activity_stack, this, 1.0, 1, _n_phases );

	if ( requests() == Requesting_Type::RENDEZVOUS ) {
	    if ( n_replies == 0 ) {
		get_dom()->runtime_error( LQIO::ERR_REPLY_NOT_GENERATED );
	    } else if ( fabs( n_replies - 1.0 ) > EPSILON ) {
		get_dom()->runtime_error( LQIO::ERR_NON_UNITY_REPLIES, n_replies );
	    }
	}
    } else {
	get_dom()->runtime_error( LQIO::ERR_NOT_SPECIFIED, name() );
	set_n_phases( 1 );
    }

    if ( !_has_service_time ) {
	get_dom()->runtime_error( LQIO::WRN_XXXX_DEFINED_BUT_ZERO, "service time" );
    }

    const_cast<Task *>(task())->set_n_phases( n_phases() );

    switch ( semaphore_type() ) {
    case LQIO::DOM::Entry::Semaphore::WAIT:
	if ( requests() == Requesting_Type::SEND_NO_REPLY ) {
	    get_dom()->runtime_error( LQIO::ERR_ASYNC_REQUEST_TO_WAIT );
	}
	/* fall through */
    case LQIO::DOM::Entry::Semaphore::SIGNAL:
	if ( task()->type() != Task::Type::SEMAPHORE ) {
	    task()->get_dom()->runtime_error( LQIO::ERR_NOT_SEMAPHORE_TASK, (semaphore_type() == LQIO::DOM::Entry::Semaphore::SIGNAL ? "signal" : "wait"), name() );
	}
	break;
    case LQIO::DOM::Entry::Semaphore::NONE:
	break;
    }

    /* Deal with forwarding. */

    _rel_prob = 1.0;		/* Set entry "release" probability */

    for ( vector<Entry *>::const_iterator d = ::__entry.begin(); d != ::__entry.end(); ++d ) {
	_rel_prob -= prob_fwd(*d);
    }

    if ( _rel_prob < 0.0 ) {
	if ( _rel_prob > -EPSILON ) {
	    _rel_prob = 0.0;	/* Call it FP truncation.	*/
	} else {
	    get_dom()->runtime_error( LQIO::ERR_INVALID_FORWARDING_PROBABILITY, 1.0 - _rel_prob );
	}
    }
}



/*
 * Places for task state. (Ph1, Ph2, Waiting).
 */

double
Entry::transmorgrify( double base_x_pos, double base_y_pos, unsigned ix_e, struct place_object * d_place,
		      unsigned m, short enabling )
{
    unsigned ne = task()->n_entries();
    double x_pos = base_x_pos + ix_e * 0.5;
    double y_pos = base_y_pos;
    double task_y_offset = Y_OFFSET(1.0);
    struct place_object * start_place = nullptr;
    double next_pos;
    const LAYER layer_mask = ENTRY_LAYER(entry_id())|(m == 0 ? PRIMARY_LAYER : 0);

    if ( task()->inservice_flag() ) {
	task_y_offset += 1.0;
    }

    if ( is_regular_entry() ) {

	/* create phases */

	unsigned p;
	double p_pos = 0.0;

	for ( p = 1; p <= n_phases(); ++p ) {
	    p_pos = phase[p].transmorgrify( x_pos, task_y_offset, m, layer_mask, p_pos, enabling );
	}

	/* Connect phases together */

	if ( !task()->inservice_flag() || task()->is_client() ) {
	    for ( p = 1; p < n_phases(); ++p ) {
		if ( phase[p].doneX[m] != nullptr ) {
		    create_arc( layer_mask, TO_PLACE, phase[p].doneX[m], phase[p+1].ZX[m] );
		}
	    }
	    /*+ BUG_164 */
	    if ( d_place ) {
		if ( task()->type() == Task::Type::SEMAPHORE && semaphore_type() == LQIO::DOM::Entry::Semaphore::WAIT ) {
		    create_arc( layer_mask, TO_PLACE, phase[n_phases()].doneX[m], task()->LX[m] );
		} else {
		    create_arc( layer_mask, TO_PLACE, phase[n_phases()].doneX[m], d_place );
		}
	    }
	    /*- BUG_164 */
	}

	for ( p = 1; start_place == nullptr && p <= n_phases(); ++p ) {
	    start_place = phase[p].ZX[m];	/* First one, starting at 1 */
	}
	next_pos = X_OFFSET(p_pos+1,0.0);

    } else {

	/* Create activities */
	double p_pos;

	p_pos = start_activity()->transmorgrify( x_pos, task_y_offset, m, this, 0, enabling, d_place, true ) + 0.5;
	next_pos = X_OFFSET(p_pos+1,0.0);
	start_place = start_activity()->ZX[m];

    }

    /* Connect the dots. */

    if ( task()->is_client() ) {
	const double prVisit = get_dom()->hasVisitProbability() ? get_dom()->getVisitProbabilityValue() : 1.0;	// BUG_440
	struct trans_object * c_trans = create_trans( X_OFFSET(0,0), task_y_offset-0.5, layer_mask,
						      prVisit, 1, IMMEDIATE, "i%s%d", name(), m );
	create_arc( layer_mask, TO_TRANS, c_trans, task()->ZX[m] != nullptr ? task()->ZX[m] : task()->TX[m] );
	create_arc( layer_mask, TO_PLACE, c_trans, start_place );
	/* c_trans acquire processor */
	if ( !is_regular_entry() ) {
	    create_arc( MEASUREMENT_LAYER, TO_PLACE, c_trans, phase[1].XX[m] );	/* start phase 1 */
	}

    } else if ( is_regular_entry() && !task()->inservice_flag()
		&& requests() == Requesting_Type::RENDEZVOUS ) {

	create_arc( layer_mask, TO_PLACE, phase[1].doneX[m], DX[m] );

    }

    if ( !is_regular_entry() ) {
	if ( task()->gdX[m] ) {
	    /* activities need flush, so link to flush transition */
	    create_arc( MEASUREMENT_LAYER, TO_TRANS, task()->gdX[m], phase[n_phases()].XX[m] );	/* End phase n */
	}
    }
    return next_pos;
}



/*
 * Create the forwarding places and transitions for each __entry.
 */

void
Entry::create_forwarding_gspn()
{
    double x_off = 0.;
    for ( vector<Forwarding *>::const_iterator f = forwards.begin(); f != forwards.end(); ++f, x_off += 0.5 ) {
	const Phase * root = (*f)->_root;
	double x_pos = task()->get_x_pos() - 0.5 + x_off;
	double y_pos = root->_slice[0].WX_ypos[(*f)->_m];
	unsigned ne  = task()->n_entries();
	const LAYER layer_mask = ENTRY_LAYER(entry_id())|((*f)->_m == 0 ? PRIMARY_LAYER : 0);

	(*f)->f_place = create_place( X_OFFSET(1,1.0), y_pos + 1.0, layer_mask, 0,
				      "FWD%s%d%s%d", root->name(), (*f)->_slice_no, phase[1].name(), (*f)->_m );
	if ( release_prob() > 0.0 ) {
	    (*f)->f_trans = create_trans( X_OFFSET(1,1.0), y_pos + 0.5, layer_mask,
					  release_prob(), 1, IMMEDIATE,
					  "fwd%s%d%s%d", root->name(), (*f)->_slice_no, phase[1].name(), (*f)->_m );
	    create_arc( layer_mask, TO_TRANS, (*f)->f_trans, (*f)->f_place );
	    create_arc( layer_mask, TO_PLACE, (*f)->f_trans, root->_slice[0].WX[(*f)->_m] );
	}
    }
}


void
Entry::remove_netobj()
{
    for ( unsigned int p = 1; p <= n_phases(); ++p ) {
        phase[p].remove_netobj();
    }
    for ( unsigned int m = 0; m < MAX_MULT; ++m ) {
        DX[m] = 0;		/* done				*/
	GdX[m] = 0;		/* Guard (for joins).		*/
    }
#if defined(BUFFER_BY_ENTRY)
    ZZ = 0;			/* For open requests.		*/
#endif
}

/*
 * Compute the task utilization
 */

double
Entry::task_utilization ( unsigned p ) const
{
    unsigned max_m = task()->n_customers();
    if ( p <= n_phases() ) {
	double util = 0;
	for ( unsigned m = 0; m < max_m; ++m ) {
	    if ( task()->task_tokens[m] ) {
		util += task()->utilization(m) * phase[p].task_tokens[m] / task()->task_tokens[m];
	    }
	}
	return util;
    } else {
	return 0.0;
    }
}




/*
 * Check results for open arrivals.  Throughput at entry should match
 * open arrival rate.
 */

bool
Entry::messages_lost() const
{
#if defined(BUFFER_BY_ENTRY)
    return ( openArrivalRate() > 0.0 && fabs ( openArrivalRate() - throughput[1] ) / openArrivalRate() > 0.05 )
	|| ( requests() == SEND_NO_REPLY_REQUEST && get_prob( 0, "ZZ%s", name() ) > EPSILON );
#else
    return ( openArrivalRate() > 0.0 && fabs ( openArrivalRate() - task()->multiplicity() * _throughput[0] ) / openArrivalRate() > 0.02)
	|| ( requests() == Requesting_Type::SEND_NO_REPLY && get_prob( 0, "ZZ%s", task()->name()) > EPSILON );
#endif
}

void
Entry::insert_DOM_results() const
{
    double totalPhaseUtil = 0.0;
    double proc_tokens    = 0.0;
    double tput           = 0.0;
    double phase_utils[DIMPH+1];
    const unsigned max_m = task()->n_customers();

    /* Write the results into the DOM */

    for ( unsigned m = 0; m < max_m; ++m ) {
	tput += _throughput[m];
    }
    _dom->setResultThroughput(tput);

    for ( unsigned p = 1; p <= n_phases(); ++p ) {
	const double util = task_utilization( p );
	totalPhaseUtil   += util;
	phase_utils[p-1] += util;
	for ( unsigned m = 0; m < max_m; ++m ) {
	    proc_tokens  += phase[p].proc_tokens[0];
	}
    }

    /* Store the utilization and squared coeff of variation */
    _dom->setResultUtilization(totalPhaseUtil)
	.setResultProcessorUtilization(proc_tokens);


    /* Store activity phase data */
    for ( unsigned p = 1; p <= n_phases(); ++p ) {
	if (is_activity_entry()) {
	    _dom->setResultPhasePServiceTime(p,phase[p].residence_time())
		.setResultPhasePUtilization(p,phase_utils[p-1]);
//		.setResultPhasePProcessorWaiting(p,queueingTime(p));
	} else {
	    phase[p].insert_DOM_results();
	}
    }

    /* Store forwarding data */
    for ( std::map<const Entry *,Call>::const_iterator f = _fwd.begin(); f != _fwd.end(); ++f ) {
	const Call& call = f->second;
	const Entry * entry = f->first;
	const_cast<LQIO::DOM::Call *>(call.get_dom())->setResultWaitingTime( queueing_time( entry ) );
    }
}


double Entry::queueing_time( const Entry * entry ) const
{
    std::map<const Entry *,Call>::const_iterator e = _fwd.find(entry);
    return ( e != _fwd.end() ) ? e->second._w : 0.;
}

/*
 * Find the entry and return it.
 */

/* static */ Entry * Entry::find( const std::string& name)
{
    std::vector<Entry *>::const_iterator entry = find_if( __entry.begin(), __entry.end(), [&]( const Entry * entry ){ return entry->name() == name; } );
    if ( entry == __entry.end() ) {
	return nullptr;
    } else {
	return *entry;
    }
}


/*
 * Locate both entries.  return false on error.
 */

bool
Entry::find( const std::string& from_entry_name, Entry * & from_entry, const std::string& to_entry_name, Entry * & to_entry )
{
    bool rc    = true;
    from_entry = find( from_entry_name );
    to_entry   = find( to_entry_name );

    if ( !to_entry ) {
	rc = false;
    }

    if ( !from_entry ) {
	rc = false;
    } else if ( from_entry == to_entry ) {
	LQIO::input_error( LQIO::ERR_SRC_EQUALS_DST, to_entry_name.c_str(), from_entry_name.c_str() );
	rc = false;
    }
    return rc;
}
