/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* September 1991.							*/
/* August 2003.								*/
/************************************************************************/

/*
 * $Id: petrisrvn.cc 14930 2021-07-20 12:00:33Z greg $
 *
 * Generate a Petri-net from an SRVN description.
 *
 */

#include "petrisrvn.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <errno.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#if HAVE_IEEEFP_H && !defined(MSDOS)
#include <ieeefp.h>
#elif HAVE_FENV_H
#if (__GNUC__ && __GNUC__ < 3 && defined(linux)) || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER < 800))
#define __USE_GNU
#endif
#include <fenv.h>
#endif
#if HAVE_FLOAT_H
#include <float.h>
#endif
#if HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <libgen.h>
#if !HAVE_GETSUBOPT
#include <lqio/getsbopt.h>
#endif
#include <lqio/input.h>
#include <lqio/error.h>
#include <lqio/glblerr.h>
#include <lqio/commandline.h>
#include <lqio/dom_document.h>
#include <lqio/dom_bindings.h>
#include <lqio/srvn_spex.h>
#include <lqio/filename.h>
#if !defined(HAVE_GETSUBOPT)
#include <lqio/getsbopt.h>
#endif
#include <wspnlib/global.h>
#include <wspnlib/wspn.h>
#include "errmsg.h"
#include "model.h"
#include "runlqx.h"		// Coupling here is ugly at the moment
#include "pragma.h"

static bool copyright_flag	= false; /* Print copyright notice	*/

bool debug_flag                 = false; /* Debugging flag set?         */
bool keep_flag                  = false; /* Keep result files?          */
bool no_execute_flag            = false; /* Don't solve model if true   */
bool parse_flag                 = false; /* Parsable output desired?    */
bool json_flag  	        = false; /* JSON output desired?    	*/
bool reload_net_flag            = false; /* Reload results and print.   */
Mode mode			= Mode::SOLVE;
bool rtf_flag                   = false; /* Parsable output desired?    */
bool trace_flag			= false; /* Output execution of greatspn*/
bool uncondition_flag           = false; /* Uncondition in-service      */
bool verbose_flag               = false; /* Verbose text output?        */
bool xml_flag 			= false; /* XML Output desired ?	*/

bool customers_flag 		= true;	 /* Smash customers together.	*/
bool distinguish_join_customers = true;	 /* unique cust at join for mult*/
bool simplify_network		= false; /* Delete single place procs.  */
					   
double	x_scaling		= 1.0;	 /* Auto-squish if val == 0.	*/
unsigned open_model_tokens	= OPEN_MODEL_TOKENS;	/* Default val.	*/

static const char * net_dir_name	= "nets";
static void my_handler (int);

static LQIO::DOM::Pragma pragmas;

/*
 * Command options.
 */

static const char * opts = "dHI:jkm:no:pP:rRtvVwxz:";

#if HAVE_GETOPT_LONG
static const struct option longopts[] =
    /* name */ /* has arg */ /*flag */ /* val */
{
    { "debug",              no_argument,        0, 'd' },
    { "help",               no_argument,        0, 'H' },
    { "input-format",	    required_argument,  0, 'I' },
    { "json",		    no_argument,	0, 'j' },
    { "keep-net",           no_argument,        0, 'k' },
    { "monitor-file",       required_argument,  0, 'm' },
    { "no-execute",         no_argument,        0, 'n' },
    { "output",             required_argument,  0, 'o' },
    { "parseable",          no_argument,        0, 'p' },
    { "pragma",             required_argument,  0, 'P' },
    { "rtf",	            no_argument,        0, 'r' },
    { "reload-net",	    no_argument,	0, 'R' },
    { "trace-greatspn",     no_argument,        0, 't' },
    { "verbose",            no_argument,        0, 'v' },
    { "version",            no_argument,        0, 'V' },
    { "no-warnings",        no_argument,        0, 'w' },
    { "xml",                no_argument,        0, 'x' },
    { "disjoint-customers", no_argument,	0, 256+'d' },
    { "no-header",	    no_argument,	0, 256+'h' },
    { "overtaking",	    optional_argument,	0, 256+'O' },
    { "queue-limit",        required_argument,	0, 256+'Q' },
    { "random-processors",  no_argument,	0, 256+'p' },
    { "random-queueing",    no_argument,	0, 256+'q' },
    { "reload-lqx",	    no_argument,        0, 256+'r' },
    { "restart",	    no_argument,	0, 256+'R' },
    { "simplify",	    no_argument,	0, 256+'s' },
    { "spacing",	    required_argument,	0, 256+'S' },
    { "debug-lqx",          no_argument,        0, 256+'l' },
    { "debug-xml",          no_argument,        0, 256+'x' },
    { nullptr, 0, 0, 0 }
};

static const char * opthelp[]  = {
    /* "debug"		    */    "Enable debug code.",
    /* "help"		    */    "Show this help.",
    /* "input-format"	    */    "Force input format to ARG.  Arg is either 'lqn' or 'xml'.",
    /* "json"		    */    "Output results in JSON format.",
    /* "keep-net"	    */    "Keep the files generated by GreatSPN.",
    /* "monitor-file"	    */    "Output results from GreatSPN to FILE.",
    /* "no-execute"	    */    "Build the Petri Net, but do not solve.",
    /* "output"		    */    "Redirect solver ouptut to FILE.",
    /* "parseable"	    */    "Generate parseable (.p) output.",
    /* "pragma"		    */    "Set solver options.",
    /* "rtf"		    */    "Output results in Rich Text Format.",
    /* "reload-net"	    */    "Reload results from GreatSPN solution.",
    /* "trace-greatspn"     */    "Show output from the execution of the GreatSPN solver.",
    /* "verbose"	    */    "Output on standard error the progress of the simulation.",
    /* "version"	    */    "Print the version of the simulator.",
    /* "no-warnings"	    */    "Do not output warning messages.",
    /* "xml"		    */    "Output results in XML format.",
    /* "disjoint-customers" */    "Create copies for reference tasks (increases state space).",
    /* "no-header"	    */	  "Do not output the variable name header on SPEX results.",
    /* "overtaking"	    */	  "Find in-service and overtaking probabilities (increases state space). ARG=condition",
    /* "queue-limit    	    */	  "Set the maximum queue size (open model).",
    /* "random-processors"  */	  "Use random queueing at all processors (reduces state space).",
    /* "random-queueing"    */    "Use random queueing at all tasks and processors (reduces state space).",
    /* "reload-lqx"	    */    "Run the LQX program, but re-use the results from a previous invocation.",
    /* "restart"	    */    "Reuse existing results where available, but solve any unsolved models.",
    /* "simplify"	    */    "Remove redundant parts of the net such as single place processors.", 	
    /* "spacing"	    */	  "Set the spacing between places and transitions to ARG.",
    /* "debug-lqx"	    */    "Output debugging information while parsing LQX input.",
    /* "debug-xml"	    */    "Output debugging information while parsing XML input.",
    nullptr
};
#endif


std::regex * inservice_match_pattern	= nullptr;

FILE * stddbg;				/* debugging output goes here.	*/

static int process ( const std::string& inputFileName, const std::string& outputFileName );
static void usage( void );
 
int 
main (int argc, char *argv[])
{
    std::string output_file = "";
    LQIO::CommandLine command_line( longopts );

    extern char *optarg;
    extern int optind;

    int global_error_flag  = 0; 	/* Error detected anywhere??	*/

    LQIO::io_vars.init( VERSION, basename( argv[0] ), severity_action, local_error_messages, LSTLCLERRMSG-LQIO::LSTGBLERRMSG );
    command_line = LQIO::io_vars.lq_toolname;

    stddbg   = stdout;

#if 0
    inter_proc_delay = 0.0;
    comm_delay_flag  = false;
#endif

#if defined(HAVE_FENV_H) && defined(HAVE_FEENABLEEXCEPT)
    feenableexcept( FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW );
#elif  defined(HAVE_IEEEFP_H) && defined(HAVE_FPSETMASK)
    fpsetmask( FP_X_INV | FP_X_DZ | FP_X_OFL );
#endif
    signal(SIGFPE, my_handler);

    pragmas.insert( getenv( "PETRISRVN_PRAGMAS" ) );

    for ( ;; ) {
#if HAVE_GETOPT_LONG
	const int c = getopt_long( argc, argv, opts, longopts, NULL );
#else
	const int c = getopt( argc, argv, opts );
#endif
	if ( c == EOF) break;

	command_line.append( c, optarg );

	switch ( c ) {

	case 'd':
	    debug_flag = true;
	    keep_flag  = true;
	    break;

	case 256+'d':
	  //	    distinguish_join_customers = true;			/* wrong one! */
	    customers_flag = false;
	    break;

	case 'k':
	    keep_flag = true;
	    break;

	case 256+'h':
	    pragmas.insert(LQIO::DOM::Pragma::_spex_header_,LQIO::DOM::Pragma::_false_);
	    LQIO::Spex::__no_header = true;
	    break;

	case 'H':
	    usage();
	    exit( 0 );

	case 'j':
	    json_flag = true;
	    break;

	case 'I':
	    if ( strcasecmp( optarg, "xml" ) == 0 ) {
		Model::__input_format = LQIO::DOM::Document::InputFormat::XML;
	    } else if ( strcasecmp( optarg, "lqn" ) == 0 ) {
		Model::__input_format = LQIO::DOM::Document::InputFormat::LQN;
	    } else {
		fprintf( stderr, "%s: invalid argument to -I -- %s\n", LQIO::io_vars.toolname(), optarg );
	    }
	    break;

	case 256+'l':
	    LQIO::DOM::Document::lqx_parser_trace(stderr);
	    break;

	case 'm':
	    trace_flag = true;
	    if ( strcmp( optarg, "-" ) == 0 ) {
		stddbg = stdout;
	    } else if ( !(stddbg = fopen( optarg, "w" )) ) {
		(void) fprintf( stderr, "%s: cannot open ", LQIO::io_vars.toolname() );
		perror( optarg );
		(void) exit( FILEIO_ERROR );
	    }
	    break;

	case 'n':
	    no_execute_flag = true;
	    keep_flag = true;
	    break;

	case 'o':
	    output_file = optarg;
	    break;

	case 'p':
	    parse_flag = true;
	    break;

	case 'P':
	    if ( !pragmas.insert( optarg ) ) {
		Pragma::usage( std::cerr );
		exit( INVALID_ARGUMENT );
	    }
	    break;

	case 256+'p':
	    pragmas.insert(LQIO::DOM::Pragma::_processor_scheduling_,scheduling_label[SCHEDULE_RAND].XML);
	    break;
	    
	case 256+'q':
	    pragmas.insert(LQIO::DOM::Pragma::_force_random_queueing_);
	    break;

	case 256+'s':
	    simplify_network = true;
	    break;

	case 256+'Q':
	    pragmas.insert(LQIO::DOM::Pragma::_queue_size_,optarg);
	    break;
	    
	case 'r':
	    rtf_flag = true;
	    break;

	case 'R':
	    reload_net_flag = true;
	    no_execute_flag = false;	/* Save results */
	    keep_flag = true;
	    break;

	case 256+'r':
	    mode = Mode::RELOAD;
	    break;

	case 256+'R':
	    mode = Mode::RESTART;
	    break;

	case 256+'S':
	    x_scaling = 1.0;
	    if ( optarg == nullptr || sscanf( optarg, "%lg", &x_scaling ) != 1 || x_scaling < 0 ) {
		(void) fprintf( stderr, "%s: x_scaling=%s is invalid, choose value > 0\n",
				LQIO::io_vars.toolname(), optarg != nullptr ? optarg : "" );
		(void) exit( INVALID_ARGUMENT );
	    }
	    break;

	case 't':
	    trace_flag = true;
	    keep_flag = true;
	    break;

	case 'v':
	    verbose_flag = true;
	    LQIO::Spex::__verbose = true;
	    break;

	case 'V':
	    copyright_flag = true;
	    break;

	case 'w':
	    LQIO::io_vars.severity_level = LQIO::ADVISORY_ONLY;		/* Ignore warnings. */
	    break;

	case 'x':
	    xml_flag = true;
	    break;

        case (256+'x'):
	    LQIO::DOM::Document::__debugXML = true;
	    break;

	case 256+'O':
	    inservice_match_pattern = new std::regex( optarg );
#if 0
		case UNCONDITIONAL_PROB:
		    uncondition_flag = true;
		    break;

#endif
	    break;

	default:
	    usage();
	    exit( INVALID_ARGUMENT );
	}
    }
    LQIO::io_vars.lq_command_line = command_line.c_str();

    if ( copyright_flag ) {
	(void) fprintf( stdout, "\nStochastic Rendezvous Petri Network Analyser, Version %s\n\n", VERSION );
	(void) fprintf( stdout, "  Copyright the Real-Time and Distributed Systems Group,\n" );
	(void) fprintf( stdout, "  Department of Systems and Computer Engineering,\n" );
	(void) fprintf( stdout, "  Carleton University, Ottawa, Ontario, Canada. K1S 5B6\n\n");
    }

    /* Quick check.  -zin-service requires that the customers are differentiated. */

    if ( ( inservice_match_pattern != nullptr ) && customers_flag ) {
	if ( LQIO::io_vars.severity_level <= LQIO::WARNING_ONLY ) {
	    (void) fprintf( stdout, "%s: -zin-service or -zin-queue is incompatible with multiple customer clients\n", LQIO::io_vars.toolname() );
	    (void) fprintf( stdout, "\t-zcustomers assumed\n" );
	}
	customers_flag = false;
    }

    /* create net_dir_name directory */

    make_net_dir( net_dir_name );

    /* Process all command line arguments.  If none specified, then	*/
    /* input is assumed to come in from stdin.			*/

    if ( optind == argc ) {

	/* If stdout is not a terminal and output to stdout.  		*/
	/* For pipelines.						*/

	if ( output_file.size() == 0 && LQIO::Filename::isWriteableFile( fileno( stdout ) ) > 0 ) {
	    output_file = "-";
	}

	global_error_flag |= process( "-", output_file );

    } else {
	unsigned int file_count = argc - optind;			/* Number of files on cmd line	*/

	if ( output_file.size() && file_count > 1 && !LQIO::Filename::isDirectory( output_file ) ) {
	    (void) fprintf( stderr, "%s: Too many input files specified with -o <file> option.\n", LQIO::io_vars.toolname() );
	    exit( INVALID_ARGUMENT );
	}

	for ( ; optind < argc; ++optind ) {

	    if ( file_count > 1 ) {
		(void) printf( "%s:\n", argv[optind] );
	    }

	    global_error_flag |= process( argv[optind], output_file );
	}

    }

    return global_error_flag;
}


/*
 * Process input and save.
 */

static int
process( const std::string& inputFileName, const std::string& outputFileName )
{
    LQIO::DOM::Document* document = Model::load( inputFileName, outputFileName );

    Model aModel( document, inputFileName, outputFileName );

    int status = 0;

    /* Make sure we got a document */
    if ( document == NULL || LQIO::io_vars.anError() ) return FILEIO_ERROR;
    document->mergePragmas( pragmas.getList() );       /* Save pragmas -- prepare will process */

    if ( !aModel.construct() ) return FILEIO_ERROR;

    if ( document->getInputFormat() != LQIO::DOM::Document::InputFormat::LQN && LQIO::Spex::__no_header ) {
        std::cerr << LQIO::io_vars.lq_toolname << ": --no-header is ignored for " << inputFileName << "." << std::endl;
    }

    /* declare Model * at this scope but don't instantiate due to problems with LQX programs and registering external symbols*/

    /* We can simply run if there's no control program */
    LQX::Program * program = document->getLQXProgram();
    if ( program ) {
	if (program == NULL) {
	    LQIO::solution_error( LQIO::ERR_LQX_COMPILATION, inputFileName.c_str() );
	    status = FILEIO_ERROR;
	} else { 
	    document->registerExternalSymbolsWithProgram(program);
	    switch ( mode ) {
	    case Mode::RESTART:
		program->getEnvironment()->getMethodTable()->registerMethod(new SolverInterface::Solve(document, &Model::restart, &aModel));
		break;
	    case Mode::RELOAD:
		program->getEnvironment()->getMethodTable()->registerMethod(new SolverInterface::Solve(document, &Model::reload, &aModel));
		break;
	    case Mode::SOLVE:
		program->getEnvironment()->getMethodTable()->registerMethod(new SolverInterface::Solve(document, &Model::solve, &aModel));
		break;
	    }

	    LQIO::RegisterBindings(program->getEnvironment(), document);

	    FILE * output = 0;
	    if ( outputFileName.size() > 0 && outputFileName != "-" && LQIO::Filename::isRegularFile(outputFileName) ) {
		output = fopen( outputFileName.c_str(), "w" );
		if ( !output ) {
		    LQIO::solution_error( LQIO::ERR_CANT_OPEN_FILE, outputFileName.c_str(), strerror( errno ) );
		    status = FILEIO_ERROR;
		} else {
		    program->getEnvironment()->setDefaultOutput( output );	/* Default is stdout */
		}
	    }

	    if ( status == 0 ) {
		/* Invoke the LQX program itself */
		if ( !program->invoke() ) {
		    LQIO::solution_error( LQIO::ERR_LQX_EXECUTION, inputFileName.c_str() );
		    status = FILEIO_ERROR;
		} else if ( !SolverInterface::Solve::solveCallViaLQX ) {
		    /* There was no call to solve the LQX */
		    LQIO::solution_error( LQIO::ADV_LQX_IMPLICIT_SOLVE, inputFileName.c_str() );
		    std::vector<LQX::SymbolAutoRef> args;
		    program->getEnvironment()->invokeGlobalMethod("solve", &args);
		}
	    }
	    if ( output ) {
		fclose( output );
	    }
	}
	delete program;

    } else {
	/* There is no control flow program, check for $-variables */
	if (document->getSymbolExternalVariableCount() != 0) {
	    LQIO::solution_error( LQIO::ERR_LQX_VARIABLE_RESOLUTION, inputFileName.c_str() );
	    status = FILEIO_ERROR;
	} else {
	    try {
		status = aModel.solve();		/* Simply invoke the solver for the current DOM state */
	    }
	    catch ( const std::runtime_error & error ) {
		std::cerr << LQIO::io_vars.lq_toolname << ": runtime error - " << error.what() << std::endl;
		LQIO::io_vars.error_count += 1;
		status = EXCEPTION_EXIT;
	    }
	    catch ( const std::logic_error& error ) {
		std::cerr << LQIO::io_vars.lq_toolname << ": logic error - " << error.what() << std::endl;
		LQIO::io_vars.error_count += 1;
		status = EXCEPTION_EXIT;
	    }
	}
    }
    return status;
}

static void 
usage (void)
{
    (void) fprintf( stderr, "Usage: %s ", LQIO::io_vars.toolname());

#if HAVE_GETOPT_LONG
    fprintf( stderr, " [option] [file ...]\n\n" );
    fprintf( stderr, "Options:\n" );
    const char ** p = opthelp;
    for ( const struct option *o = longopts; (o->name || o->val) && *p; ++o, ++p ) {
	std::string s;
	if ( o->name ) {
	    s = "--";
	    s += o->name;
	    switch ( o->val ) {
	    case 'o':
	    case 'm': s += "=<file>"; break;
	    }
	} else {
	    s = " ";
	}
	if ( isascii(o->val) && isgraph(o->val) ) {
	    fprintf( stderr, " -%c, ", static_cast<char>(o->val) );
	} else {
	    fprintf( stderr, "     " );
	}
	fprintf( stderr, "%-24s %s\n", s.c_str(), *p );
    }
#else
    for ( char * s = opts; *s; ++s ) {
	if ( *(s+1) == ':' ) {
	    ++s;
	} else {
	    fputc( *s, stderr );
	}
    }
    std::cerr << ']';
    for ( char * s = opts; *s; ++s ) {
	if ( *(s+1) == ':' ) {
	    fprintf( stderr, " [-%c", *s );
	    switch ( *s ) {
	    default: fprintf( stderr, "file" ); break;
	    } 
	    fputf( ']', stderr );
	    ++s;
	}
    }
    fprintf( stderr, " [file ...]\n" );
#endif
}


/*
 * signal handler for fp errors.
 */

static void
my_handler (int sig )
{
    abort();
}




