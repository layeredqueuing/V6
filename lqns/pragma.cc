/*  -*- c++ -*-
 * $Id: pragma.cc 14185 2020-12-08 14:14:28Z greg $ *
 * Pragma processing and definitions.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 * April 2011
 * ------------------------------------------------------------------------
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include "pragma.h"
#include "lqio/glblerr.h"

Pragma * Pragma::__cache = nullptr;
const std::map<const std::string,const Pragma::fptr> Pragma::__set_pragma =
{
    { LQIO::DOM::Pragma::_cycles_,			&Pragma::setAllowCycles },
    { LQIO::DOM::Pragma::_force_multiserver_,		&Pragma::setForceMultiserver },
    { LQIO::DOM::Pragma::_interlocking_,		&Pragma::setInterlock },
    { LQIO::DOM::Pragma::_layering_,			&Pragma::setLayering },
    { LQIO::DOM::Pragma::_multiserver_,			&Pragma::setMultiserver },
    { LQIO::DOM::Pragma::_mva_,				&Pragma::setMva },
    { LQIO::DOM::Pragma::_overtaking_,			&Pragma::setOvertaking },
    { LQIO::DOM::Pragma::_processor_scheduling_,	&Pragma::setProcessorScheduling },
#if HAVE_LIBGSL && HAVE_LIBGSLCBLAS
    { LQIO::DOM::Pragma::_quorum_distribution_,		&Pragma::setQuorumDistribution },
    { LQIO::DOM::Pragma::_quorum_delayed_calls_,	&Pragma::setQuorumDelayedCalls },
    { LQIO::DOM::Pragma::_quorum_idle_time_,		&Pragma::setQuorumIdleTime },
#endif
#if RESCHEDULE
    { LQIO::DOM::Pragma::_reschedule_on_async_send_,	&Pragma::setRescheduleOnAsyncSend },
#endif
    { LQIO::DOM::Pragma::_severity_level_,		&Pragma::setSeverityLevel },
    { LQIO::DOM::Pragma::_spex_header_,			&Pragma::setSpexHeader },
    { LQIO::DOM::Pragma::_stop_on_bogus_utilization_,	&Pragma::setStopOnBogusUtilization },
    { LQIO::DOM::Pragma::_stop_on_message_loss_,	&Pragma::setStopOnMessageLoss },
    { LQIO::DOM::Pragma::_tau_,				&Pragma::setTau },
    { LQIO::DOM::Pragma::_threads_,			&Pragma::setThreads },
    { LQIO::DOM::Pragma::_variance_,			&Pragma::setVariance }
};

/*
 * Set default values in the constructor.  Defaults are used below.
 */

Pragma::Pragma() :
    _allow_cycles(false),
    _exponential_paths(false),
    _force_multiserver(Pragma::ForceMultiserver::NONE),
    _interlock(true),
    _layering(Layering::BATCHED),
    _multiserver(Multiserver::DEFAULT),
    _mva(MVA::LINEARIZER),
    _overtaking(Overtaking::MARKOV),
    _processor_scheduling(SCHEDULE_PS),
#if HAVE_LIBGSL && HAVE_LIBGSLCBLAS
    _quorumDistribution(QuorumDistribution::DEFAULT),
    _quorumDelayedCalls(QuorumDelayedCalls::DEFAULT),
    _quorumIdleTime(QuorumIdleTime::DEFAULT),
#endif
#if RESCHEDULE
    _reschedule_on_async_send(false),
#endif
    _severity_level(LQIO::NO_ERROR),
    _spex_header(true),
    _stop_on_bogus_utilization(0.),		/* Not a bool.	U > nn */
    _stop_on_message_loss(true),
    _tau(8),
    _threads(Threads::HYPER),
    _variance(Variance::DEFAULT),
    /* Bonus */
    _default_processor_scheduling(true),
    _disable_processor_cfs(false),
    _entry_variance(true),
    _init_variance_only(false)
{
}


void
Pragma::set( const std::map<std::string,std::string>& list )
{
    if ( __cache != nullptr ) delete __cache;
    __cache = new Pragma();

    for ( std::map<std::string,std::string>::const_iterator i = list.begin(); i != list.end(); ++i ) {
	const std::string& param = i->first;
	const std::map<const std::string,const fptr>::const_iterator j = __set_pragma.find(param);
	if ( j != __set_pragma.end() ) {
	    try {
		fptr f = j->second;
		(__cache->*f)(i->second);
	    }
	    catch ( std::domain_error& e ) {
		LQIO::solution_error( LQIO::WRN_PRAGMA_ARGUMENT_INVALID, param.c_str(), e.what() );
	    }
	}
    }
}


void Pragma::setAllowCycles(const std::string& value)
{
    _allow_cycles = isTrue(value);
}


const std::map<const std::string,const Pragma::ForceMultiserver> Pragma::__force_multiserver =
{
    { LQIO::DOM::Pragma::_all_,		ForceMultiserver::ALL },
    { LQIO::DOM::Pragma::_none_,	ForceMultiserver::NONE },
    { LQIO::DOM::Pragma::_tasks_,	ForceMultiserver::TASKS },
    { LQIO::DOM::Pragma::_processors_,	ForceMultiserver::PROCESSORS }
};

void Pragma::setForceMultiserver(const std::string& value)
{
    const std::map<const std::string,const Pragma::ForceMultiserver>::const_iterator pragma = __force_multiserver.find( value );
    if ( pragma != __force_multiserver.end() ) {
	_force_multiserver = pragma->second;
    } else {
	throw std::domain_error( value.c_str() );
    }
}


void Pragma::setInterlock(const std::string& value)
{
    _interlock = isTrue(value);
}


const std::map<const std::string,const Pragma::Layering> Pragma::__layering_pragma =
{
    { LQIO::DOM::Pragma::_batched_,	Layering::BATCHED },
    { LQIO::DOM::Pragma::_batched_back_,Layering::BACKPROPOGATE_BATCHED },
    { LQIO::DOM::Pragma::_hwsw_,	Layering::HWSW },
    { LQIO::DOM::Pragma::_mol_,		Layering::METHOD_OF_LAYERS },
    { LQIO::DOM::Pragma::_mol_back_,	Layering::BACKPROPOGATE_METHOD_OF_LAYERS },
    { LQIO::DOM::Pragma::_squashed_,	Layering::SQUASHED },
    { LQIO::DOM::Pragma::_srvn_,	Layering::SRVN }
};

void Pragma::setLayering(const std::string& value)
{
    const std::map<const std::string,const Pragma::Layering>::const_iterator pragma = __layering_pragma.find( value );
    if ( pragma != __layering_pragma.end() ) {
	_layering = pragma->second;
    } else {
	throw std::domain_error( value.c_str() );
    } 
}

const std::map<const std::string,const Pragma::Multiserver> Pragma::__multiserver_pragma =
{
    { LQIO::DOM::Pragma::_bruell_,	Pragma::Multiserver::BRUELL },
    { LQIO::DOM::Pragma::_conway_,	Pragma::Multiserver::CONWAY },
    { LQIO::DOM::Pragma::_reiser_,	Pragma::Multiserver::REISER },
    { LQIO::DOM::Pragma::_reiser_ps_,	Pragma::Multiserver::REISER_PS },
    { LQIO::DOM::Pragma::_rolia_,	Pragma::Multiserver::ROLIA },
    { LQIO::DOM::Pragma::_rolia_ps_,	Pragma::Multiserver::ROLIA_PS },
    { LQIO::DOM::Pragma::_schmidt_,	Pragma::Multiserver::SCHMIDT },
    { LQIO::DOM::Pragma::_suri_,	Pragma::Multiserver::SURI }
};

void Pragma::setMultiserver(const std::string& value)
{
    const std::map<const std::string,const Pragma::Multiserver>::const_iterator pragma = __multiserver_pragma.find( value );
    if ( pragma != __multiserver_pragma.end() ) {
	_multiserver = pragma->second;
    } else if ( value == LQIO::DOM::Pragma::_default_ ) {
	_multiserver = Multiserver::DEFAULT;
    } else {
	throw std::domain_error( value.c_str() );
    }
}

const std::map<const std::string,const Pragma::MVA> Pragma::__mva_pragma =
{
    { LQIO::DOM::Pragma::_exact_, 	MVA::EXACT },
    { LQIO::DOM::Pragma::_schweitzer_, 	MVA::SCHWEITZER },
    { LQIO::DOM::Pragma::_fast_, 	MVA::FAST },
    { LQIO::DOM::Pragma::_one_step_, 	MVA::ONESTEP },
    { LQIO::DOM::Pragma::_one_step_linearizer_, MVA::ONESTEP_LINEARIZER },
};

void Pragma::setMva(const std::string& value)
{
    const std::map<const std::string,const Pragma::MVA>::const_iterator pragma = __mva_pragma.find( value );
    if ( pragma != __mva_pragma.end() ) {
	_mva = pragma->second;
    } else {
	throw std::domain_error( value.c_str() );
    }
}


const std::map<const std::string,const Pragma::Overtaking> Pragma::__overtaking_pragma =
{
    { LQIO::DOM::Pragma::_markov_,	Overtaking::MARKOV },
    { LQIO::DOM::Pragma::_none_,	Overtaking::NONE },
    { LQIO::DOM::Pragma::_rolia_,	Overtaking::ROLIA },
    { LQIO::DOM::Pragma::_simple_,	Overtaking::SIMPLE },
    { LQIO::DOM::Pragma::_special_,	Overtaking::SPECIAL }
};

void Pragma::setOvertaking(const std::string& value)
{
    const std::map<const std::string,const Pragma::Overtaking>::const_iterator pragma = __overtaking_pragma.find( value );
    if ( pragma != __overtaking_pragma.end() ) {
	_overtaking = pragma->second;
    } else {
	throw std::domain_error( value.c_str() );
    }
}


const std::map<const std::string,const scheduling_type> Pragma::__processor_scheduling_pragma =
{
    { scheduling_label[SCHEDULE_DELAY].XML,	SCHEDULE_DELAY },
    { scheduling_label[SCHEDULE_FIFO].XML,	SCHEDULE_FIFO },
    { scheduling_label[SCHEDULE_HOL].XML,	SCHEDULE_HOL },
    { scheduling_label[SCHEDULE_PPR].XML,	SCHEDULE_PPR },
    { scheduling_label[SCHEDULE_PS].XML,	SCHEDULE_PS },
    { scheduling_label[SCHEDULE_RAND].XML,	SCHEDULE_RAND }
};

void Pragma::setProcessorScheduling(const std::string& value)
{
    const std::map<const std::string,const scheduling_type>::const_iterator pragma = __processor_scheduling_pragma.find( value );
    if ( pragma != __processor_scheduling_pragma.end() ) {
	_default_processor_scheduling = false;
	_processor_scheduling = pragma->second;
    } else if ( value == LQIO::DOM::Pragma::_no_cfs_ ) {
	_disable_processor_cfs = true;
    } else if ( value == LQIO::DOM::Pragma::_default_ ) {
	_default_processor_scheduling = true;
    } else {
	throw std::domain_error( value.c_str() );
    }
}


#if RESCHEDULE
void Pragma::setRescheduleOnAsyncSend(const std::string& value)
{
    _reschedule_on_async_send = isTrue(value);
}
#endif


void Pragma::setSpexHeader(const std::string& value)
{
    _spex_header = isTrue( value );
}



const std::map<const std::string,const LQIO::severity_t> Pragma::__serverity_level_pragma =
{
    { LQIO::DOM::Pragma::_advisory_,	LQIO::ADVISORY_ONLY },
    { LQIO::DOM::Pragma::_run_time_,	LQIO::RUNTIME_ERROR },
    { LQIO::DOM::Pragma::_warning_,	LQIO::WARNING_ONLY }
};

void Pragma::setSeverityLevel(const std::string& value)
{
    const std::map<const std::string,const LQIO::severity_t>::const_iterator pragma = __serverity_level_pragma.find( value );
    if ( pragma != __serverity_level_pragma.end() ) {
	_severity_level = pragma->second;
    } else {
	_severity_level = LQIO::NO_ERROR;
    }
}


void Pragma::setStopOnBogusUtilization(const std::string& value)
{
    char * endptr = nullptr;
    _stop_on_bogus_utilization = std::strtod( value.c_str(), &endptr );
    if ( (_stop_on_bogus_utilization < 1 && _stop_on_bogus_utilization != 0) || *endptr != '\0' ) {
	throw std::domain_error( value.c_str() );
    }
}


void Pragma::setStopOnMessageLoss(const std::string& value)
{
    _stop_on_message_loss = isTrue(value);
}


void Pragma::setTau(const std::string& value)
{
    char * endptr = nullptr;
    _tau = std::strtol( value.c_str(), &endptr, 10 );
    if ( _tau > 20 || *endptr != '\0' ) {
	throw std::domain_error( value.c_str() );
    }
}


const std::map<const std::string,const Pragma::Threads> Pragma::__threads_pragma =
{
    { LQIO::DOM::Pragma::_hyper_,	Threads::HYPER },
    { LQIO::DOM::Pragma::_mak_,		Threads::MAK_LUNDSTROM },
    { LQIO::DOM::Pragma::_none_,	Threads::NONE }
};

void Pragma::setThreads(const std::string& value)
{
    const std::map<const std::string,const Pragma::Threads>::const_iterator pragma = __threads_pragma.find( value );
    if ( pragma != __threads_pragma.end() ) {
	_threads = pragma->second;
    } else if ( value == LQIO::DOM::Pragma::_exponential_ ) {
	_exponential_paths = true;
    } else {
	throw std::domain_error( value.c_str() );
    }
}


const std::map<const std::string,const Pragma::Variance> Pragma::__variance_pragma =
{
    { LQIO::DOM::Pragma::_default_,	Variance::DEFAULT },
    { LQIO::DOM::Pragma::_mol_,		Variance::MOL },
    { LQIO::DOM::Pragma::_none_,	Variance::NONE },
    { LQIO::DOM::Pragma::_stochastic_,	Variance::STOCHASTIC }
};
	
void Pragma::setVariance(const std::string& value)
{
    const std::map<const std::string,const Pragma::Variance>::const_iterator pragma = __variance_pragma.find( value );
    if ( pragma != __variance_pragma.end() ) {
	_variance = pragma->second;
    } else if ( value == LQIO::DOM::Pragma::_no_entry_ ) {
	_entry_variance = false;
    } else if ( value == LQIO::DOM::Pragma::_init_only_ ) {
	_init_variance_only = true;
    } else {
	throw std::domain_error( value.c_str() );
    }
}

bool Pragma::isTrue(const std::string& value )
{
    return value == "true" || value == LQIO::DOM::Pragma::_yes_;
}

/*
 * Print out available pragmas.
 */

std::ostream&
Pragma::usage( std::ostream& output )
{
    output << "Valid pragmas: " << std::endl;
    std::ios_base::fmtflags flags = output.setf( std::ios::left, std::ios::adjustfield );

    for ( std::map<const std::string,const fptr>::const_iterator i = __set_pragma.begin(); i != __set_pragma.end(); ++i ) {
	output << "\t" << std::setw(20) << i->first;
	if ( i->first == LQIO::DOM::Pragma::_tau_ ) {
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
    return output;
}
