/* pragma.cc	-- Greg Franks Tue Sep  1 2009
 *
 * ------------------------------------------------------------------------
 * $Id: pragma.cc 14179 2020-12-07 22:02:52Z greg $
 * ------------------------------------------------------------------------
 */

#include "lqsim.h"
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <ctype.h>
#include <algorithm>
#if !HAVE_GETSUBOPT
#include <lqio/getsbopt.h>
#endif
#include "pragma.h"
#include <lqio/glblerr.h>

Pragma * Pragma::__pragmas = nullptr;
std::map<std::string,Pragma::fptr> Pragma::__set_pragma;

Pragma::Pragma() :
    _abort_on_dropped_message(true),  /* halt on dropped msgs.	*/
    _quorum_delayed_calls(false),	/* Quorum reply (BUG_311)	*/
    _reschedule_on_async_send(false),	/* force schedule after snr.	*/
    _scheduling_model(SCHEDULE_SLICE),
    _severity_level(LQIO::NO_ERROR),
    _spex_header(true),
    _block_period(0.0),
    _number_of_blocks(0),
    _max_blocks(0),
    _precision(0),
    _run_time(0),
    _seed_value(0),
    _initial_loops(0),
    _initial_delay(0)
{
}

void
Pragma::set( const std::map<std::string,std::string>& list )
{
    initialize();
    
    if ( __pragmas != nullptr ) delete __pragmas;
    __pragmas = new Pragma();

    for ( std::map<std::string,std::string>::const_iterator i = list.begin(); i != list.end(); ++i ) {
	const std::string& param = i->first;
	const std::map<std::string,fptr>::const_iterator j = __set_pragma.find(param);
	if ( j != __set_pragma.end() ) {
	    fptr f = j->second;
	    (__pragmas->*f)(i->second);
	}
    }
}


bool
Pragma::set_scheduling_model( const std::string& value )
{
    if ( value == "custom" ) {
	_scheduling_model = SCHEDULE_CUSTOM;
    } else if ( value == "custom-natural" ) {
	_scheduling_model = SCHEDULE_CUSTOM_NATURAL;
    } else if ( value == "default-natural" ) {
	_scheduling_model = SCHEDULE_NATURAL;
    } else {
	_scheduling_model = SCHEDULE_SLICE;
    }
    return true;
}

bool
Pragma::set_severity_level(const std::string& value)
{
    if ( value == LQIO::DOM::Pragma::_warning_ ) {
	_severity_level = LQIO::WARNING_ONLY;
    } else if ( value == LQIO::DOM::Pragma::_advisory_) {
	_severity_level = LQIO::ADVISORY_ONLY;
    } else if ( value == LQIO::DOM::Pragma::_run_time_) {
	_severity_level = LQIO::RUNTIME_ERROR;
    } else {
	_severity_level = LQIO::NO_ERROR;
    }
    return true;
}

bool
Pragma::set_reschedule_on_async_send( const std::string& value )
{
    _reschedule_on_async_send = is_true( value );
    return true;
}

bool
Pragma::set_abort_on_dropped_message( const std::string& value )
{
    _abort_on_dropped_message = is_true( value );
    return true;
}

bool
Pragma::set_nice( const std::string& value )
{
    char * endptr = nullptr;
    _nice_value = std::strtol( value.c_str(), &endptr, 10 );
    if ( _nice_value < 0 || 20 < _nice_value || *endptr != '\0' ) {
	LQIO::solution_error( LQIO::WRN_PRAGMA_ARGUMENT_INVALID, LQIO::DOM::Pragma::_nice_, value.c_str() );
	_nice_value = 20;
	return false;
    } 
    return true;
}

bool
Pragma::set_quorum_delayed_calls( const std::string& value ) 
{
    _quorum_delayed_calls = is_true( value );
    return true;
}

bool Pragma::set_block_period( const std::string& value )
{
    char * endptr = nullptr;
    _block_period = std::strtod( value.c_str(), &endptr );
    if ( _block_period < 0.001 ) {
	LQIO::solution_error( LQIO::WRN_PRAGMA_ARGUMENT_INVALID, LQIO::DOM::Pragma::_block_period_, value.c_str() );
	return false;
    }
    return true;
}

bool Pragma::set_initial_loops( const std::string& value )
{
    char * endptr = nullptr;
    _initial_loops = std::strtol( value.c_str(), &endptr, 10 );
    if ( _initial_loops < 0 ) {
	LQIO::solution_error( LQIO::WRN_PRAGMA_ARGUMENT_INVALID, LQIO::DOM::Pragma::_initial_loops_, value.c_str() );
	return false;
    }	
    return true;
}

bool Pragma::set_initial_delay( const std::string& value )
{
    char * endptr = nullptr;
    _initial_delay = std::strtol( value.c_str(), &endptr, 10 );
    if ( _initial_delay < 0 ) {
	LQIO::solution_error( LQIO::WRN_PRAGMA_ARGUMENT_INVALID, LQIO::DOM::Pragma::_initial_delay_, value.c_str() );
	return false;
    }	
    return true;
}

bool Pragma::set_max_blocks( const std::string& value )
{
    char * endptr = nullptr;
    _max_blocks = std::strtol( value.c_str(), &endptr, 10 );
    if ( _max_blocks < 0 ) {
	LQIO::solution_error( LQIO::WRN_PRAGMA_ARGUMENT_INVALID, LQIO::DOM::Pragma::_max_blocks_, value.c_str() );
	return false;
    }	
    return true;
}

bool Pragma::set_precision( const std::string& value )
{
    char * endptr = nullptr;
    _precision = std::strtod( value.c_str(), &endptr );
    if ( _precision < 0.001 ) {
	LQIO::solution_error( LQIO::WRN_PRAGMA_ARGUMENT_INVALID, LQIO::DOM::Pragma::_precision_, value.c_str() );
	return false;
    }
    return true;
}

bool Pragma::set_seed_value( const std::string& value )
{
    char * endptr = nullptr;
    _seed_value = std::strtod( value.c_str(), &endptr );
    if ( _seed_value < 0.001 ) {
	LQIO::solution_error( LQIO::WRN_PRAGMA_ARGUMENT_INVALID, LQIO::DOM::Pragma::_seed_value_, value.c_str() );
	return false;
    }
    return true;
}

bool Pragma::set_run_time( const std::string& value )
{
    char * endptr = nullptr;
    _run_time = std::strtod( value.c_str(), &endptr );
    if ( _run_time < 0.001 ) {
	LQIO::solution_error( LQIO::WRN_PRAGMA_ARGUMENT_INVALID, LQIO::DOM::Pragma::_run_time_, value.c_str() );
	return false;
    }
    return true;
}

void
Pragma::initialize()
{
    if ( __set_pragma.size() > 0 ) return;

    __set_pragma[LQIO::DOM::Pragma::_nice_] =			&Pragma::set_nice;
    __set_pragma[LQIO::DOM::Pragma::_quorum_reply_] =		&Pragma::set_quorum_delayed_calls;
    __set_pragma[LQIO::DOM::Pragma::_reschedule_on_async_send_]=&Pragma::set_reschedule_on_async_send;
    __set_pragma[LQIO::DOM::Pragma::_scheduling_model_] =	&Pragma::set_scheduling_model;
    __set_pragma[LQIO::DOM::Pragma::_severity_level_] =		&Pragma::set_severity_level;
    __set_pragma[LQIO::DOM::Pragma::_stop_on_message_loss_] =	&Pragma::set_abort_on_dropped_message;

    __set_pragma[LQIO::DOM::Pragma::_block_period_] = 	        &Pragma::set_block_period;
    __set_pragma[LQIO::DOM::Pragma::_initial_delay_] = 	        &Pragma::set_initial_delay;
    __set_pragma[LQIO::DOM::Pragma::_initial_loops_] = 	        &Pragma::set_initial_loops;
    __set_pragma[LQIO::DOM::Pragma::_max_blocks_] = 		&Pragma::set_max_blocks;
    __set_pragma[LQIO::DOM::Pragma::_precision_] = 	        &Pragma::set_precision;
    __set_pragma[LQIO::DOM::Pragma::_run_time_] = 		&Pragma::set_run_time;
    __set_pragma[LQIO::DOM::Pragma::_seed_value_] = 		&Pragma::set_seed_value;
}



/*
 * Print out available pragmas.
 */

void
Pragma::usage( std::ostream& output )
{
    Pragma::initialize();
    
    output << "Valid pragmas: " << std::endl;
    std::ios_base::fmtflags flags = output.setf( std::ios::left, std::ios::adjustfield );

    for ( std::map<std::string,Pragma::fptr>::const_iterator i = __set_pragma.begin(); i != __set_pragma.end(); ++i ) {
	output << "\t" << std::setw(20) << i->first;
	if ( i->first == LQIO::DOM::Pragma::_nice_ ) {
	    output << " = <int>" << std::endl;
	} else {
	    const std::set<const std::string>* args = LQIO::DOM::Pragma::getValues( i->first );
	    if ( args && args->size() > 1 ) {
		output << " = {";

		for ( std::set<const std::string>::const_iterator q = args->begin(); q != args->end(); ++q ) {
		    if ( q != args->begin() ) output << ",";
		    output << *q;
		}
		output << "}" << std::endl;
	    } else {
		output << " = <arg>" << std::endl;
	    }
	}
    }
    output.setf( flags );
}


bool
Pragma::is_true( const std::string& value ) const
{
    return value == "true" || value == LQIO::DOM::Pragma::_yes_;
}



