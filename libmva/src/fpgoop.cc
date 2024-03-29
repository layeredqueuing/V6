/*  -*- c++ -*-
 * $Id: fpgoop.cc 16974 2024-01-29 20:12:36Z greg $
 *
 * Floating point exception handling.  It is all different on all machines.
 * See:
 *   linux...	sigaction(2)
 * 
 * if feenableexcet (fenv.h) is present, use it,
 * else if fpsetmask (ieeefp.h) is present, use it,
 *
 * A Common interface is presented to check for divide by zero, overflow,
 * and invalid operations.  Callers must either call set_fp_abort() to
 * cause immediate termination of the application at the fault location, of
 * check_fp_ok() to test afterwards at a convenient location.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 *
 * ------------------------------------------------------------------------
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <cassert>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#if HAVE_FLOAT_H
#include <float.h>
#endif
#if HAVE_IEEEFP_H
#include <ieeefp.h>
#endif
#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#if HAVE_XMMINTRIN_H
#include <xmmintrin.h>
#endif
#include "fpgoop.h"

#define EXCEPTION_EXIT 255		/* See input.h */

/*
 * IEEE exceptions.
 */

static struct {
    int bit;
    const char * str;
} fp_op_str[] = {
#if defined(FE_DENORMAL)
    { FE_DENORMAL, "Denormal" },
#endif
    { FE_DIVBYZERO, "Divide by zero" },
    { FE_INEXACT, "Inexact result" },
    { FE_INVALID, "Invalid operation" },
    { FE_OVERFLOW, "Overflow" },
    { FE_UNDERFLOW, "Underflow" },
    { 0, 0 }
};

static int fp_bits = 0;
fp_exception_reporting matherr_disposition;	/* What to do about math probs.	*/

/*
 * Print out gory details of fault.
 */

std::string
floating_point_error::construct( const std::string& file, const unsigned line ) 
{
    const int flags = fp_status_bits();
    std::ostringstream ss;

    ss << "Floating point exception" << ": " << file << " " << line << ": ";

    unsigned count = 0;
    for ( unsigned i = 0; fp_op_str[i].str; ++i ) {
	if ( flags & fp_op_str[i].bit ) {
	    if ( count > 0 ) {
		ss<< ", ";
	    }
	    ss << fp_op_str[i].str;
	    count += 1;
	}
    }
    return ss.str();
}

extern "C" {
#if SA_SIGINFO
static void my_handler( int, siginfo_t *, void * );
#else
static void my_handler( int );
#endif
}

/* ---------------- Floating point exception handling.  --------------- */

/*
 * Reset matherr signalling as per user's request.
 */

void
set_fp_abort()
{
#if HAVE_FENV_H && HAVE_FEENABLEEXCEPT
    /* Best way */
    feenableexcept( fp_bits );
#elif HAVE_IEEEFP_H && HAVE_FPSETMASK
    fpsetmask( fp_bits );
#elif !defined(__WINNT__) && HAVE_XMMINTRIN_H
    if ((fp_bits & FE_INVALID) != 0)	{ _mm_setcsr(_MM_MASK_MASK & ~_MM_MASK_INVALID); }
    if ((fp_bits & FE_DIVBYZERO) != 0)	{ _mm_setcsr(_MM_MASK_MASK & ~_MM_MASK_DIV_ZERO); }
    if ((fp_bits & FE_OVERFLOW ) != 0) 	{ _mm_setcsr(_MM_MASK_MASK & ~_MM_MASK_OVERFLOW); }
#elif defined __APPLE__ && __AARCH64EL__
    std::fenv_t env;
    fegetenv(&env);
    env.__fpcr = env.__fpcr | __fpcr_trap_invalid |  __fpcr_trap_divbyzero |  __fpcr_trap_overflow;
    fesetenv(&env);
#elif defined(__WINNT__) && HAVE__CONTROLFP_S
    _controlfp_s(NULL, ~(_EM_ZERODIVIDE | _EM_OVERFLOW), _MCW_EM);
#else
    #warning No FP abort.
#endif
#if HAVE_SIGACTION
    struct sigaction my_action;

#if defined(SA_SIGINFO)
    my_action.sa_sigaction = my_handler;
    sigemptyset( &my_action.sa_mask );
    my_action.sa_flags = SA_SIGINFO;	/* Invoke the signal catching function with */
					/*   three arguments instead of one. */
#else
    my_action.sa_handler = my_handler;
    sigemptyset( &my_action.sa_mask );
    my_action.sa_flags = 0;
#endif

#if defined __APPLE__ && __AARCH64EL__
    assert( sigaction( SIGILL, &my_action, 0 ) == 0 );
#else
    assert( sigaction( SIGFPE, &my_action, 0 ) == 0 );
#endif
#else
    signal( SIGFPE, my_handler );
#endif
}


/*
 * signal handler for fp errors.
 * See /usr/include/sys/signal.h. (MacosX)
 */

#if HAVE_SIGACTION && defined(SA_SIGINFO)
static void
my_handler (int sig, siginfo_t * info, void * )
{
    if ( sig == SIGFPE ) {
	std::cerr << "Floating point exception: ";

	switch ( info->si_code ) {
#if defined(FPE_INTDIV)
	case FPE_INTDIV: std::cerr << "integer divide by zero"; break;
#endif
#if defined(FPE_INTOVF)
	case FPE_INTOVF: std::cerr << "integer overflow"; break;
#endif
	case FPE_FLTDIV: std::cerr << "floating point divide by zero"; break;
	case FPE_FLTOVF: std::cerr << "floating point overflow"; break;
	case FPE_FLTUND: std::cerr << "floating point underflow"; break;
	case FPE_FLTRES: std::cerr << "floating point inexact result"; break;
	case FPE_FLTINV: std::cerr << "floating point invalid operation"; break;
#if defined(FPE_FLTSUB)
	case FPE_FLTSUB: std::cerr << "subscript out of range"; break;
#endif
	default: std::cerr << "unknown!"; break;
	}

	std::cerr.fill('0');
	std::cerr.setf ( std::ios::hex|std::ios::showbase, std::ios::basefield|std::ios::showbase );  // set hex as the basefield
	std::cerr << " at " << std::setw(10) << reinterpret_cast<unsigned long>(info->si_addr);
	std::cerr << std::endl;
    } else {
	std::cerr << "Caught signal " << sig << std::endl;
    }
    exit( 255 );
}

#else
void
my_handler ( int )
{
    throw floating_point_error( __FILE__, __LINE__ );
    exit( 255 );
}

#endif

/*
 * Return 1 if FP is o.k. to date, and 0 otherwise.
 */
bool
check_fp_ok()
{
    if ( matherr_disposition == fp_exception_reporting::IGNORE ) return true;
    
#if HAVE_FENV_H && HAVE_FETESTEXCEPT

    return fetestexcept( fp_bits ) == 0;

#elif HAVE_IEEEFP_H && HAVE_FPGETSTICKY

    return (fpgetsticky() & fp_bits) == 0;

#elif defined(__WINNT__) && HAVE__STATUSFP

    return (_statusfp() & (EM_ZERODIVIDE|EM_OVERFLOW)) == 0;

#else

    return true;

#endif
}


/*
 * Reset the floating point unit to a happy state.
 */

void
set_fp_ok( bool overflow )
{
#if HAVE_FENV_H && HAVE_FECLEAREXCEPT

    if ( matherr_disposition == fp_exception_reporting::IGNORE ) {
	fp_bits = 0;
    } else {
#if defined __APPLE__ && __AARCH64EL__
	fp_bits = FE_DIVBYZERO;		/* Invalid is set for any infinity, so ignore */
#else
	fp_bits = FE_DIVBYZERO|FE_INVALID;
#endif
	if ( overflow ) {
	    fp_bits |= FE_OVERFLOW;
	}
    }
    feclearexcept( FE_ALL_EXCEPT );

#elif HAVE_IEEEFP_H && HAVE_FPSETSTICKY

    if ( matherr_disposition == fp_exception_reporting::IGNORE ) {
	fp_bits = 0;
    } else {
	fp_bits = FP_X_INV | FP_X_DZ;
	if ( overflow ) {
	    fp_bits |= FP_X_OFL;
	}
    }

    fpsetsticky( FP_CLEAR );

#elif defined(__WINNT__) && HAVE__CLEARFP
    _clearfp();

#endif
}




/*
 * Return the floating point status bits.
 */

int
fp_status_bits()
{
#if  HAVE_FENV_H && HAVE_FETESTEXCEPT

    return fetestexcept( FE_ALL_EXCEPT );

#elif HAVE_IEEEFP_H && HAVE_FPGETSTICKY

    return fpgetsticky();

#else
    return 0;

#endif
}

/*
 * return factorial.  Cache results.
 */

double
factorial( unsigned n )
{
    static double a[101];
    if ( n == 1 || n == 0 ) return 1.0;
    if ( n <= 100 ) {
	if ( a[n] == 0 ) {
	    a[n] = static_cast<double>(n) * factorial(n-1);
	}
	return a[n];
    } else {
	double product;
	for ( product = 1.0; n > 1; --n ) {
	    product *= n;
	}
	return product;
    }
}


/*
 * return ln of factorial of n.  Cache results.
 */

double
log_factorial( const unsigned n )
{
    static double a[101];	/* Automatically initialized to zero! */
	
    if ( n == 0 ) throw std::domain_error( "log_factorial(0)" );
    if ( n == 1 ) return 0.0;
    if ( n <= 100 ) {
	if ( a[n] == 0 ) {
	    a[n] = std::lgamma( n + 1.0 );
	}
	return a[n];
    } else {
	return std::lgamma( n + 1.0 );
    }
}


/*
 * Returns the binomial coefficient
 */

double
binomial_coef( const unsigned n, const unsigned k )
{
    return floor( 0.5 + exp( log_factorial( n ) - log_factorial( k ) - log_factorial( n - k ) ) );
}


/*
 * Exponentiation.  Handles negative exponents.
 */

double
power( double a, int b )
{
    if ( b > 5 ) {
	return pow( a, (double)b );
    } else if ( b >= 0 ) {
	double product = 1.0;
	for ( ; b > 0; --b ) product *= a;
	return product;
    } else {
	return 1.0 / power( a, -b );
    }
}


/*
 * Choose.
 * i! / ( j! * (i-j)! )
 */

double
choose( unsigned i, unsigned j )
{
    assert ( i >= j );
    if ( i == 0 ) return 0;
    if ( j == 0 || i == j ) return 1;
	
    double product;
    const unsigned int a = std::max( j, i - j );
    const unsigned int b = std::min( j, i - j );
	
    for ( product = 1.0; i > a; --i ) {
	product *= i;
    }
	
    return product / factorial( b );
}
