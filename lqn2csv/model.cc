/*  -*- c++ -*-
 * $Id: model.cc 15081 2021-10-18 20:05:23Z greg $
 *
 * Command line processing.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * October, 2021
 *
 * ------------------------------------------------------------------------
 */

#include <algorithm>
#include <numeric>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <lqio/dom_document.h>
#include <lqio/dom_object.h>
#include <lqio/dom_entry.h>
#include "model.h"
#include "lqn2csv.h"

/*
 * Maps result type to the dom-object member function that gets the
 * results.  All of the getResult functions take no arguments, so they
 * must be used on the final object (phases, and not entries, for
 * phase results, etc).
 */

const std::map<Model::Result::Type,Model::Result::result_fields> Model::Result::__results =
{
    { Model::Result::Type::ACTIVITY_SERVICE,	  { Model::Object::Type::ACTIVITY,  "Service",     &LQIO::DOM::DocumentObject::getResultServiceTime          } },
    { Model::Result::Type::ACTIVITY_THROUGHPUT,   { Model::Object::Type::ACTIVITY,  "Throughput",  &LQIO::DOM::DocumentObject::getResultThroughput 	     } },
    { Model::Result::Type::ACTIVITY_VARIANCE,	  { Model::Object::Type::ACTIVITY,  "Variance",    &LQIO::DOM::DocumentObject::getResultServiceTimeVariance  } },
    { Model::Result::Type::ACTIVITY_WAITING, 	  { Model::Object::Type::CALL,      "Waiting",     &LQIO::DOM::DocumentObject::getResultWaitingTime 	     } },
    { Model::Result::Type::ENTRY_THROUGHPUT,   	  { Model::Object::Type::ENTRY,     "Throughput",  &LQIO::DOM::DocumentObject::getResultThroughput 	     } },
    { Model::Result::Type::ENTRY_UTILIZATION,     { Model::Object::Type::ENTRY,     "Utilization", &LQIO::DOM::DocumentObject::getResultUtilization 	     } },
    { Model::Result::Type::HOLD_TIMES, 		  { Model::Object::Type::JOIN,      "Hold Time",   &LQIO::DOM::DocumentObject::getResultHoldingTime 	     } },
    { Model::Result::Type::JOIN_DELAYS, 	  { Model::Object::Type::JOIN,      "Join Delay",  &LQIO::DOM::DocumentObject::getResultJoinDelay 	     } },
    { Model::Result::Type::LOSS_PROBABILITY, 	  { Model::Object::Type::CALL,      "Drop Prob",   &LQIO::DOM::DocumentObject::getResultDropProbability      } },
    { Model::Result::Type::OPEN_WAIT, 		  { Model::Object::Type::ENTRY,     "Waiting",     &LQIO::DOM::DocumentObject::getResultWaitingTime 	     } },
    { Model::Result::Type::PROCESSOR_MULTIPLICITY,{ Model::Object::Type::PROCESSOR, "Copies",      &LQIO::DOM::DocumentObject::getCopiesValueAsDouble 	     } },
    { Model::Result::Type::PROCESSOR_UTILIZATION, { Model::Object::Type::PROCESSOR, "Utilization", &LQIO::DOM::DocumentObject::getResultUtilization 	     } },
    { Model::Result::Type::PROCESSOR_WAITING,     { Model::Object::Type::PHASE,     "Waiting",     &LQIO::DOM::DocumentObject::getResultProcessorWaiting     } },
    { Model::Result::Type::REQUEST_RATE,	  { Model::Object::Type::CALL,      "Request",     &LQIO::DOM::DocumentObject::getCallMeanValue		     } },
    { Model::Result::Type::SERVICE_DEMAND,	  { Model::Object::Type::PHASE,     "Demand",      &LQIO::DOM::DocumentObject::getServiceTimeValue 	     } },
    { Model::Result::Type::SERVICE_EXCEEDED,      { Model::Object::Type::PHASE,     "Pr. Exceed",  &LQIO::DOM::DocumentObject::getResultMaxServiceTimeExceeded } },
    { Model::Result::Type::SERVICE_TIME,          { Model::Object::Type::PHASE,     "Service",     &LQIO::DOM::DocumentObject::getResultServiceTime 	     } },
    { Model::Result::Type::TASK_MULTIPLICITY,	  { Model::Object::Type::TASK,	    "Copies",      &LQIO::DOM::DocumentObject::getCopiesValueAsDouble 	     } },
    { Model::Result::Type::TASK_THROUGHPUT,       { Model::Object::Type::TASK,      "Throughput",  &LQIO::DOM::DocumentObject::getResultThroughput 	     } },
    { Model::Result::Type::TASK_UTILIZATION,      { Model::Object::Type::TASK,      "Utilization", &LQIO::DOM::DocumentObject::getResultUtilization 	     } },
    { Model::Result::Type::THROUGHPUT_BOUND,      { Model::Object::Type::ENTRY,     "Bound",       &LQIO::DOM::DocumentObject::getResultThroughputBound      } },
    { Model::Result::Type::VARIANCE, 	          { Model::Object::Type::PHASE,     "Variance",    &LQIO::DOM::DocumentObject::getResultVarianceServiceTime  } },
    { Model::Result::Type::WAITING_TIME,          { Model::Object::Type::CALL,      "Waiting",     &LQIO::DOM::DocumentObject::getResultWaitingTime 	     } }
};


const std::map<Model::Object::Type, const std::string> Model::Object::__object_type =
{
    { Model::Object::Type::ACTIVITY,  "Activity"  },
    { Model::Object::Type::ENTRY,     "Entry"     },
    { Model::Object::Type::JOIN,      "Join"      },
    { Model::Object::Type::PHASE,     "Phase"     },
    { Model::Object::Type::PROCESSOR, "Processor" },
    { Model::Object::Type::TASK,      "Task"      }
};

bool Model::Result::isIndependentVariable( Model::Result::Type type )
{
    static const std::set<Result::Type> independent = {
	Type::SERVICE_DEMAND,
	Type::REQUEST_RATE,
	Type::TASK_MULTIPLICITY,
	Type::PROCESSOR_MULTIPLICITY };
    return independent.find( type ) != independent.end();
}

bool Model::Result::isDependentVariable( Model::Result::Type type )
{
    return type != Type::NONE && !isIndependentVariable( type );
}

/*
 * Load a file then extract results using Model::Result::operator().
 */

void Model::Process::operator()( const char * filename ) const
{
    unsigned int error_code = 0;
    LQIO::DOM::Document * dom = LQIO::DOM::Document::load( filename, LQIO::DOM::Document::InputFormat::AUTOMATIC, error_code, true );
    if ( !dom ) {
	throw std::runtime_error( "Input model was not loaded successfully." );
    } 

    std::ostringstream ss;
    ss << _i;
    _output << std::accumulate( _results.begin(), _results.end(), ss.str(), Model::Result( *dom ) ) << std::endl;
    _i += 1;
}

/* 
 * Get the value of the result and append it to the string "in" (fold operation for std::accumulate).
 */

std::string
Model::Result::operator()( const std::string& in, const std::pair<std::string,Model::Result::Type>& result ) const
{
    const LQIO::DOM::DocumentObject * object = findObject( result.first, result.second );
    std::ostringstream out;

    if ( !in.empty() ) out << in << ",";

    if ( object != nullptr ) {
	const fptr f = __results.at( result.second ).f;
	out << (object->*f)();
    } else {
	out << "NULL";
    }

    return out.str();
}


/*
 * Find the object.  Some may be more complicated than others (i.e., phases).
 */

const LQIO::DOM::DocumentObject *
Model::Result::findObject( const std::string& item, Model::Result::Type type ) const
{
    const LQIO::DOM::DocumentObject * object = nullptr;

    const std::regex regex(",");
    const std::vector<std::string> tokens( std::sregex_token_iterator(item.begin(), item.end(), regex, -1), std::sregex_token_iterator() );

    if ( tokens.empty() ) return nullptr;
    const std::string& name = tokens.front();

    switch ( type ) {
    case Type::ACTIVITY_SERVICE:
    case Type::ACTIVITY_THROUGHPUT:
    case Type::ACTIVITY_VARIANCE:      
	/* task, activity */
	if ( tokens.size() == 2 ) {
	    object = findActivity( dom().getTaskByName( name ), tokens.at(1) );
	}
	break;
	
    case Type::ACTIVITY_WAITING:
	/* task, activity, entry */
	if ( tokens.size() == 3 ) {
	    object = findCall( findActivity( dom().getTaskByName( name ), tokens.at(1) ), tokens.at(2) );
	}
	break;
	
    case Type::ENTRY_THROUGHPUT:
    case Type::ENTRY_UTILIZATION:
    case Type::OPEN_WAIT:
	object = dom().getEntryByName( name );
	break;

    case Type::SERVICE_DEMAND:
    case Type::PHASE_UTILIZATION:
    case Type::PROCESSOR_WAITING:
    case Type::SERVICE_EXCEEDED:
    case Type::SERVICE_TIME:
    case Type::THROUGHPUT_BOUND:
    case Type::VARIANCE:
	/* entry,phase */
	if ( tokens.size() == 2 ) {
	    object = findPhase( dom().getEntryByName( name ), tokens.at(1));
	}
	break;
	
    case Type::LOSS_PROBABILITY:
    case Type::REQUEST_RATE:
    case Type::WAITING_TIME:			/* phase -> entry */
	/* entry,phase,entry */
	if ( tokens.size() == 3 ) {
	    object = findCall( findPhase( dom().getEntryByName( name ), tokens.at(1) ), tokens.at(2) );
	}
	break;

    case Type::TASK_MULTIPLICITY:
    case Type::TASK_THROUGHPUT:
    case Type::TASK_UTILIZATION:		/* May be by phase */
	object = dom().getTaskByName( name );
	break;

    case Type::PROCESSOR_MULTIPLICITY:
    case Type::PROCESSOR_UTILIZATION:
	object = dom().getProcessorByName( name );
	break;

    case Type::HOLD_TIMES:
    case Type::JOIN_DELAYS:
	/* Who knows! */
	break;

    case Type::NONE:
	abort();
	break;
    }

    return object;
}



/*
 * Return the phase object for the entry.
 */

const LQIO::DOM::Phase *
Model::Result::findPhase( const LQIO::DOM::Entry * entry, const std::string& phase ) const
{
    if ( entry == nullptr ) return nullptr;
    try {
	std::string::size_type i = 0;
	const unsigned int p = std::stoi( phase, &i );
	if ( 1 <= p && p <= 3 && i == phase.size() ) return entry->getPhase( p );
    }
    catch ( const std::invalid_argument& )
    {
    }
    return nullptr;	/* Error */
}


/*
 * Return the call object for the phase.
 */

const LQIO::DOM::Call *
Model::Result::findCall( const LQIO::DOM::Phase * phase, const std::string& destination ) const
{
    if ( phase == nullptr ) return nullptr;
    LQIO::DOM::Entry * target = dom().getEntryByName( destination );
    if ( target == nullptr ) return nullptr;
    return phase->getCallToTarget( target );
}


/*
 * Return the activity object for the task.
 */

const LQIO::DOM::Activity *
Model::Result::findActivity( const LQIO::DOM::Task * task, const std::string& activity ) const
{
    if ( task == nullptr ) return nullptr;
    return task->getActivity( activity );
}



std::string
Model::Result::ObjectType( const std::string& in, const std::pair<std::string,Model::Result::Type>& result )
{
    std::string out = in;
    if ( !in.empty() ) out += ",";
    return out + "\"" + Model::Object::__object_type.at(__results.at(result.second).type) + "\"";
}



std::string
Model::Result::ObjectName( const std::string& in, const std::pair<std::string,Model::Result::Type>& result )
{
    std::string out = in;
    if ( !in.empty() ) out += ",";
    return out + "\"" + result.first + "\"";
}



std::string
Model::Result::TypeName( const std::string& in, const std::pair<std::string,Model::Result::Type>& result )
{
    std::string out = in;
    if ( !in.empty() ) out += ",";
    return out + "\"" + __results.at(result.second).name + "\"";
}



bool
Model::Result::equal( Model::Result::Type type_1, Model::Result::Type type_2 )
{
    return (throughput( type_1 ) && throughput( type_2 ))
	|| (utilization( type_1 ) && utilization( type_2 ))
	|| (service_time( type_1 ) && service_time( type_2 ))
	|| (variance( type_1 ) && variance( type_2 ))
	|| (waiting( type_1 ) && waiting( type_2 ));
}
