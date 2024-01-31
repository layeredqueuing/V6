/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* September 1991.							*/
/* August 2003.								*/
/************************************************************************/

/*
 * $Id: petrisrvn.cc 16986 2024-01-30 16:38:42Z greg $
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
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#if HAVE_FENV_H
#include <fenv.h>
#elif HAVE_IEEEFP_H && !defined(MSDOS)
#include <ieeefp.h>
#endif
#if HAVE_FLOAT_H
#include <float.h>
#endif
#if HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <libgen.h>
#include <lqio/commandline.h>
#include <lqio/dom_document.h>
#include <lqio/error.h>
#include <lqio/glblerr.h>
#include <lqio/srvn_spex.h>
#if !HAVE_GETSUBOPT
#include <lqio/getsbopt.h>
#endif
#include <wspnlib/global.h>
#include <wspnlib/wspn.h>
#include "errmsg.h"
#include "model.h"
#include "pragma.h"

static bool copyright_flag	= false; /* Print copyright notice	*/

bool debug_flag                 = false; /* Debugging flag set?         */
bool keep_flag                  = false; /* Keep result files?          */
bool no_execute_flag            = false; /* Don't solve model if true   */
bool json_flag  	        = false; /* JSON output desired?    	*/
bool reload_net_flag            = false; /* Reload results and print.   */
bool rtf_flag                   = false; /* Parsable output desired?    */
bool trace_flag			= false; /* Output execution of greatspn*/
bool uncondition_flag           = false; /* Uncondition in-service      */
bool verbose_flag               = false; /* Verbose text output?        */

bool customers_flag 		= true;	 /* Smash customers together.	*/
bool distinguish_join_customers = true;	 /* unique cust at join for mult*/
bool simplify_network		= false; /* Delete single place procs.  */

double	x_scaling		= 1.0;	 /* Auto-squish if val == 0.	*/

static const char * net_dir_name	= "nets";
static void my_handler (int);

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
    { "input-format",	    required_argument,  0, 'i' },
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
    { "print-comment",	    no_argument,	0, 256+'c' },
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
#else
const struct option * longopts = nullptr;
#endif

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
    /* "print-comment"	    */    "Add the model comment as the first line of output when running with SPEX input.",
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


std::regex * inservice_match_pattern	= nullptr;

FILE * stddbg;				/* debugging output goes here.	*/

static void usage( void );
 
int
main(int argc, char *argv[])
{
    std::string output_file = "";
    LQIO::DOM::Document::InputFormat input_format = LQIO::DOM::Document::InputFormat::AUTOMATIC;
    LQIO::DOM::Document::OutputFormat output_format = LQIO::DOM::Document::OutputFormat::DEFAULT;
    LQIO::CommandLine command_line( longopts );
    LQIO::DOM::Pragma pragmas;
    Model::solve_using solve_function = &Model::compute;

    extern char *optarg;
    extern int optind;

    int status  = NORMAL_TERMINATION;

    LQIO::io_vars.init( VERSION, basename( argv[0] ), LQIO::severity_action );
    std::copy( local_error_messages.begin(), local_error_messages.end(), std::inserter( LQIO::error_messages, LQIO::error_messages.begin() ) );

    command_line = LQIO::io_vars.lq_toolname;

    /* Check for non-empty non-regular file "empty" -- used by GreatSPN */
#if HAVE_SYS_STAT_H
    struct stat statbuf;
    if ( stat( "empty", &statbuf ) == 0 && ((statbuf.st_mode & S_IFMT) != S_IFREG || statbuf.st_size != 0) ) {
	fprintf( stderr, "%s: \"empty\", used by GreatSPN, is present in the working directory\n", LQIO::io_vars.toolname() );
	exit( 1 );
    }
#endif

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

    LQIO::DOM::DocumentObject::setSeverity(LQIO::ERR_NO_QUANTUM_SCHEDULING, LQIO::error_severity::WARNING );	// Don't care for petrisrvn.

    for ( ;; ) {
#if HAVE_GETOPT_LONG
	const int c = getopt_long( argc, argv, opts, longopts, NULL );
#else
	const int c = getopt( argc, argv, opts );
#endif
	if ( c == EOF) break;

	command_line.append( c, optarg );

	switch ( c ) {
	case 'c'+256:
	    LQIO::Spex::__print_comment = true;
	    pragmas.insert(LQIO::DOM::Pragma::_spex_comment_,"true");
	    break;

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
	    output_format = LQIO::DOM::Document::OutputFormat::JSON;
	    break;

	case 'I':
	    if ( strcasecmp( optarg, "xml" ) == 0 ) {
		input_format = LQIO::DOM::Document::InputFormat::XML;
	    } else if ( strcasecmp( optarg, "json" ) == 0 ) {
		input_format = LQIO::DOM::Document::InputFormat::JSON;
	    } else if ( strcasecmp( optarg, "lqn" ) == 0 ) {
		input_format = LQIO::DOM::Document::InputFormat::LQN;
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
	    output_format = LQIO::DOM::Document::OutputFormat::PARSEABLE;
	    break;

	case 'P':
	    if ( !pragmas.insert( optarg ) ) {
		Pragma::usage( std::cerr );
		exit( INVALID_ARGUMENT );
	    }
	    break;

	case 256+'p':
	    pragmas.insert(LQIO::DOM::Pragma::_processor_scheduling_,LQIO::SCHEDULE::RAND);
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
	    solve_function = &Model::reload;
	    break;

	case 256+'R':
	    solve_function = &Model::restart;
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
	    pragmas.insert(LQIO::DOM::Pragma::_severity_level_,LQIO::DOM::Pragma::_run_time_);
	    break;

	case 'x':
	    output_format = LQIO::DOM::Document::OutputFormat::XML;
	    break;

        case (256+'x'):
	    LQIO::DOM::Document::__debugXML = true;
	    break;

	case 256+'O':
	    if ( optarg == nullptr ) {
		inservice_match_pattern = new std::regex( ".*" );
	    } else {
		inservice_match_pattern = new std::regex( optarg );
	    }
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
	if ( LQIO::io_vars.severity_level == LQIO::error_severity::ERROR || LQIO::io_vars.severity_level == LQIO::error_severity::ALL ) {
	    (void) fprintf( stdout, "%s: --overtaking is incompatible with multiple customer clients\n", LQIO::io_vars.toolname() );
	    (void) fprintf( stdout, "\t--disjoint-customers assumed\n" );
	}
	customers_flag = false;
    }

    /* create net_dir_name directory */

    make_net_dir( net_dir_name );

    /* Process all command line arguments.  If none specified, then	*/
    /* input is assumed to come in from stdin.			*/

    if ( optind == argc ) {

	status |= Model::solve( solve_function, "-", input_format, output_file, output_format, pragmas );

    } else {
	unsigned int file_count = argc - optind;			/* Number of files on cmd line	*/

	if ( file_count > 1 && LQIO::Filename::isFileName( output_file ) && !LQIO::Filename::isDirectory( output_file ) ) {
	    (void) fprintf( stderr, "%s: Too many input files specified with -o <file> option.\n", LQIO::io_vars.toolname() );
	    exit( INVALID_ARGUMENT );
	}

	for ( ; optind < argc; ++optind ) {

	    if ( file_count > 1 ) {
		(void) printf( "%s:\n", argv[optind] );
	    }

	    status |= Model::solve( solve_function, argv[optind], input_format, output_file, output_format, pragmas );
	}

    }

    unlink( "empty" );		/* Clean up after GreatSPN. */

    return status;	 	/* 0 is a successful exit */
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
