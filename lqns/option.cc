/* option.cc -- Greg Franks Wed Oct 12 2005
 *
 * $Id: option.cc 14715 2021-05-28 15:34:49Z greg $
 */

#include <config.h>
#include <fstream>
#include <sstream>
#include <errno.h>
#include <ctype.h>
#include <cstdlib>
#include "dim.h"
#include <lqio/error.h>
#include <lqio/dom_document.h>
#include <mva/mva.h>
#include "option.h"
#include "lqns.h"
#include "help.h"
#include "model.h"
#include "generate.h"
#include "pragma.h"

const std::map<const std::string, const Options::Debug> Options::Debug::__table
{
    { "all",        Debug( &Debug::all,         &Help::debugAll ) },
//    { "activities", Debug( &Debug::activities,  &Help::debugActivities ) },
//    { "calls",      Debug( &Debug::calls,       &Help::debugCalls ) },
    { "forks",      Debug( &Debug::forks,       &Help::debugForks ) },
    { "interlock",  Debug( &Debug::interlock,   &Help::debugInterlock ) },
//    { "joins",      Debug( &Debug::joins,       &Help::debugJoins ) },
    { "layers",     Debug( &Debug::layers,      &Help::debugLayers ) },
#if DEBUG_MVA
    { "mva",	    Debug( &Debug::mva,		&Help::debugMVA ) },
#endif
    { "overtaking", Debug( &Debug::overtaking,  &Help::debugOvertaking ) },
    { "variance",   Debug( &Debug::variance,    &Help::debugVariance ) },
#if HAVE_LIBGSL
    { "quorum",     Debug( &Debug::quorum,      &Help::debugQuorum ) },
#endif
    { "xml",        Debug( &Debug::xml,         &Help::debugXML ) },
    { "lqx",        Debug( &Debug::lqx,         &Help::debugLQX ) },
};

const char ** Options::Debug::__options = NULL;

std::vector<bool> Options::Debug::_bits(Options::Debug::QUORUM+1);

void
Options::Debug::all( const char * )
{
//    _bits[CALLS]      = true;
    _bits[FORKS]      = true;
    _bits[INTERLOCK]  = true;
//    _bits[JOINS]      = true;
    _bits[LAYERS]     = true;
#if HAVE_LIBGSL
    _bits[QUORUM]     = true;
#endif
}

void
Options::Debug::mva( const char * )
{
#if DEBUG_MVA
    MVA::debug_D = false;			/* Linearizer */
    MVA::debug_L = true;			/* Queue Length */
    MVA::debug_N = false;			/* Customers */
    MVA::debug_P = false;			/* Marginal Probability */
    MVA::debug_R = true;			/* Real customers */
    MVA::debug_U = false;			/* Utilization */
    MVA::debug_W = true;			/* Waiting Time */
    MVA::debug_X = false;			/* Throughput */
#endif
}

void
Options::Debug::overtaking( const char * )
{
    flags.print_overtaking = true;
}

void
Options::Debug::xml( const char * )
{
    LQIO::DOM::Document::__debugXML = true;
}

void
Options::Debug::lqx( const char * )
{
    LQIO::DOM::Document::lqx_parser_trace(stderr);
}

/* static */ void
Options::Debug::initialize()
{
    if ( __options != nullptr ) return;
    __options = new const char * [__table.size()+1];

    size_t i = 0;
    for ( std::map<const std::string, const Options::Debug>::const_iterator next_opt = __table.begin(); next_opt != __table.end(); ++next_opt ) {
	__options[i++] = next_opt->first.c_str();
    }
    __options[i] = nullptr;
}

void
Options::Debug::exec( const int ix, const char * arg )
{
    if ( ix >= 0 ) {
	(*__table.at(std::string(__options[ix])).func())( arg );
    } else {
	usage( 'd', optarg );
    }
}

std::map<const char *, Options::Trace, lt_str> Options::Trace::__table =
{
    { "activities", 	Trace( &Trace::activities        , false, &Help::traceActivities ) },
    { "cfs", 	     	Trace( &Trace::cfs		 , false, &Help::traceCFS ) },
    { "convergence",  	Trace( &Trace::convergence       , true,  &Help::traceConvergence ) },
    { "customers",    	Trace( &Trace::customers         , false, &Help::traceCustomers ) },
    { "delta-wait",   	Trace( &Trace::delta_wait        , false, &Help::traceDeltaWait ) },
    { "forks", 	     	Trace( &Trace::forks             , false, &Help::traceForks ) },
    { "idle-time",    	Trace( &Trace::idle_time         , false, &Help::traceIdleTime ) },
    { "interlock",    	Trace( &Trace::interlock         , false, &Help::traceInterlock ) },
    { "intermediate", 	Trace( &Trace::intermediate      , false, &Help::traceIntermediate ) },
    { "mva", 	     	Trace( &Trace::mva               , true,  &Help::traceMva ) },
    { "overtaking",   	Trace( &Trace::overtaking        , false, &Help::traceOvertaking ) },
//  { "processor",    	Trace( &Trace::processor         , true,  &Help::traceProcessor ) },
    { "quorum",       	Trace( &Trace::quorum            , false, &Help::traceQuorum ) },
    { "replication",  	Trace( &Trace::replication       , false, &Help::traceReplication ) },
//  { "task",         	Trace( &Trace::task              , true,  &Help::traceTask ) },
    { "throughput",   	Trace( &Trace::throughput        , false, &Help::traceThroughput ) },
    { "variance",     	Trace( &Trace::variance          , false, &Help::traceVariance ) },
    { "virtual-entry", 	Trace( &Trace::virtual_entry     , false, &Help::traceVirtualEntry ) },
    { "wait", 	     	Trace( &Trace::wait              , false, &Help::traceWait ) }
};

const char ** Options::Trace::__options = NULL;

void
Options::Trace::activities( const char * arg )
{
    flags.trace_activities = true;
}

void
Options::Trace::convergence( const char *arg )
{
    flags.trace_convergence = true;
}

void
Options::Trace::cfs( const char *arg )
{
    flags.trace_cfs = true;
}

void
Options::Trace::forks( const char *arg )
{
    flags.trace_forks = true;
}

void
Options::Trace::idle_time( const char *arg )
{
    flags.trace_idle_time = true;
}

void
Options::Trace::interlock( const char *arg )
{
    flags.trace_interlock = true;
}

void
Options::Trace::intermediate( const char *arg )
{
    flags.trace_intermediate = true;
}

void
Options::Trace::delta_wait( const char *arg )
{
    flags.trace_delta_wait = true;
}

void
Options::Trace::mva( const char *arg )
{
    unsigned temp;
    flags.trace_mva = true;
    if ( !arg ) {
	flags.trace_submodel = 0;	/* Do all submodels */
    } else if ( 0 < ( temp = (unsigned)strtol( arg, 0, 10 ) ) && temp < 100 ) {
	flags.trace_submodel = temp;
    } else {
	std::cerr << LQIO::io_vars.lq_toolname << " -tmva=" << arg << " is invalid." << std::endl;
    }
}

void
Options::Trace::quorum( const char *arg )
{
    flags.trace_quorum = true;
}

void
Options::Trace::overtaking( const char *arg )
{
    flags.trace_overtaking = true;
    flags.print_overtaking = true;
}

void
Options::Trace::replication( const char *arg )
{
    flags.trace_replication = true;
}

void
Options::Trace::throughput( const char *arg )
{
    flags.trace_throughput = true;
}

void
Options::Trace::variance( const char *arg )
{
    flags.trace_variance = true;
}

void
Options::Trace::virtual_entry( const char *arg )
{
    flags.trace_virtual_entry = true;
    flags.trace_wait = true;
}

void
Options::Trace::wait( const char *arg )
{
    flags.trace_wait = true;
}

void
Options::Trace::customers( const char *arg )
{
    flags.trace_customers = true;
}

/* static */ void
Options::Trace::initialize()
{
    if ( __options != nullptr ) return;

    __options = new const char * [__table.size()+1];
    std::map<const char *, Options::Trace>::const_iterator next_opt;

    unsigned int i = 0;
    for ( next_opt = __table.begin(); next_opt != __table.end(); ++next_opt ) {
	__options[i++] = next_opt->first;
    }
    __options[i] = NULL;
}

void
Options::Trace::exec( const int ix, const char * arg )
{
    if ( ix >= 0 ) {
	(*__table[__options[ix]].func())( arg );
    } else {
	usage( 't', optarg );
    }
}

std::map<const char *, Options::Special, lt_str> Options::Special::__table =
{
    { "iteration-limit", 	    Special( &Special::iteration_limit, 	   true,  &Help::specialIterationLimit ) },
    { "print-interval", 	    Special( &Special::print_interval,    	   true,  &Help::specialPrintInterval ) },
    { "overtaking", 		    Special( &Special::overtaking,        	   false, &Help::specialOvertaking ) },
    { "convergence-value", 	    Special( &Special::convergence_value,          true,  &Help::specialConvergenceValue ) },
    { "single-step", 		    Special( &Special::single_step,		   false, &Help::specialSingleStep ) },
    { "underrelaxation", 	    Special( &Special::underrelaxation,	           true,  &Help::specialUnderrelaxation ) },
    { "generate", 	            Special( &Special::generate_queueing_model,    true,  &Help::specialGenerateQueueingModel ) },
    { "mol-ms-underrelaxation",     Special( &Special::mol_ms_underrelaxation,     true,  &Help::specialMolMSUnderrelaxation ) },
    { "man",	 		    Special( &Special::make_man,		   true,  &Help::specialMakeMan ) },
    { "tex", 			    Special( &Special::make_tex,		   true,  &Help::specialMakeTex ) },
    { "min-steps", 		    Special( &Special::min_steps,                  true,  &Help::specialMinSteps ) },
#if HAVE_LIBGSL
    { "ignore-overhanging-threads", Special( &Special::ignore_overhanging_threads, false, &Help::specialIgnoreOverhangingThreads ) },
#endif
    { "full-reinitialize", 	    Special( &Special::full_reinitialize,          false, &Help::specialFullReinitialize ) }
};

const char ** Options::Special::__options = nullptr;

void
Options::Special::iteration_limit( const char * arg )
{
    if ( !arg || (Model::iteration_limit = (unsigned)strtol( arg, 0, 10 )) == 0 ) {
	std::cerr << LQIO::io_vars.lq_toolname << "iteration-limit=" << arg << " is invalid, choose non-negative integer." << std::endl;
	(void) exit( INVALID_ARGUMENT );
    } else {
	flags.override_iterations = true;
    }
}

void
Options::Special::print_interval( const char * arg )
{
    if ( !arg || (Model::print_interval = (unsigned)strtol( arg, 0, 10 )) == 0 ) {
	std::cerr << LQIO::io_vars.lq_toolname << "print-interval=" << arg << " is invalid, choose non-negative integer." << std::endl;
	(void) exit( INVALID_ARGUMENT );
    } else {
	flags.override_print_interval = true;
    }
}

void
Options::Special::overtaking( const char * )
{
    flags.print_overtaking = true;
}

void
Options::Special::convergence_value( const char * arg )
{
    if ( !arg || (Model::convergence_value = strtod( arg, 0 )) == 0 ) {
	std::cerr << LQIO::io_vars.lq_toolname << "convergence=" << arg << " is invalid, choose non-negative real." << std::endl;
	(void) exit( INVALID_ARGUMENT );
    } else {
	flags.override_convergence = true;
    }
}

void
Options::Special::single_step( const char * arg )
{
    if ( !arg ) {
	flags.single_step = true;
    } else if ( (flags.single_step = atol( arg )) <= 0 ) {
	std::cerr << LQIO::io_vars.lq_toolname << ": step=" << arg << " is invalid, choose non-negative integer." << std::endl;
	(void) exit( INVALID_ARGUMENT );
    }
}

void
Options::Special::underrelaxation( const char * arg )
{
    if ( !arg || (Model::underrelaxation = strtod( arg, 0 )) <= 0.0 || 2.0 < Model::underrelaxation ) {
	std::cerr << LQIO::io_vars.lq_toolname << "underrelaxation=" << arg << " is invalid, choose a value between 0.0 and 2.0." << std::endl;
	(void) exit( INVALID_ARGUMENT );
    } else {
	flags.override_underrelaxation = true;
    }
}

void
Options::Special::generate_queueing_model( const char * arg )
{
    if ( !arg ) {
	std::cerr << LQIO::io_vars.lq_toolname << "generate: missing filename argument.." << std::endl;
	(void) exit( INVALID_ARGUMENT );
    } else {
	flags.generate = true;
	Generate::file_name = const_cast<char *>(arg);
    }
}

void
Options::Special::mol_ms_underrelaxation( const char * arg )
{
    if ( !arg || (MVA::MOL_multiserver_underrelaxation = strtod( arg, 0 )) <= 0.0 || 1.0 < MVA::MOL_multiserver_underrelaxation ) {
	std::cerr << LQIO::io_vars.lq_toolname << "underrelaxation=" << arg << " is invalid, choose real between 0.0 and 1.0." << std::endl;
	(void) exit( INVALID_ARGUMENT );
    }
}

void
Options::Special::make_man( const char * arg )
{
    HelpTroff man;
    if ( arg ) {
	std::ofstream output;
	output.open( arg, std::ios::out );	/* NO \r's in output for windoze */
	if ( !output ) {
	    std::ostringstream msg; 
	    msg << "Cannot open output file " << arg << " - " << strerror( errno );
	    throw std::runtime_error( msg.str() );
	} else {
	    output << man;
	}
	output.close();
    } else {
	std::cout << man;
    }
    exit( 0 );
}

void
Options::Special::make_tex( const char * arg )
{
    HelpLaTeX man;
    if ( arg ) {
	std::ofstream output;
	output.open( arg, std::ios::out );	/* NO \r's in output for windoze */
	if ( !output ) {
	    std::ostringstream msg; 
	    msg << "Cannot open output file " << arg << " - " << strerror( errno );
	    throw std::runtime_error( msg.str() );
	} else {
	    output << man;
	}
	output.close();
    } else {
	std::cout << man;
    }
    exit( 0 );
}

void
Options::Special::min_steps( const char * arg )
{
    if ( !arg ) {
	std::cerr << LQIO::io_vars.lq_toolname << ": no value supplied to -zmin-steps." << std::endl;
    } else if ( (flags.min_steps = atoi( arg )) < 1 ) {
	std::cerr << LQIO::io_vars.lq_toolname << ": min-steps=" << arg << " is invalid, choose value greater than 1." << std::endl;
	(void) exit( INVALID_ARGUMENT );
    }
}

void
Options::Special::ignore_overhanging_threads( const char * )
{
    flags.ignore_overhanging_threads = true;
}

void
Options::Special::full_reinitialize( const char * )
{
    flags.full_reinitialize = true;
}

/* static */ void
Options::Special::initialize()
{
    if ( __options != nullptr ) return;

    __options = new const char * [__table.size()+1];
    std::map<const char *, Options::Special>::const_iterator next_opt;

    unsigned int i = 0;
    for ( next_opt = __table.begin(); next_opt != __table.end(); ++next_opt ) {
	__options[i++] = next_opt->first;
    }
    __options[i] = NULL;
}

void
Options::Special::exec( const int ix, const char * arg )
{
    if ( ix >= 0 ) {
	(*__table[__options[ix]].func())( arg );
    } else {
	usage( 'z', optarg );
    }
}
