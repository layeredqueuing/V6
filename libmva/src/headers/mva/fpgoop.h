/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/libmva/src/headers/mva/fpgoop.h $
 *
 * FP error handling.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 *
 * $Id: fpgoop.h 15323 2022-01-02 16:13:23Z greg $
 *
 * ------------------------------------------------------------------------
 */

#ifndef	LIBMVA_FPGOOP_H
#define	LIBMVA_FPGOOP_H

#include <config.h>
#if HAVE_IEEEFP_H && !defined(__WINNT__) &&!defined(MSDOS)
#include <ieeefp.h>
#endif
#include <string>
#include <stdexcept>

void set_fp_abort();
bool check_fp_ok();
void set_fp_ok( bool );
int fp_status_bits();

/*
 * Compute and return factorial.
 */
double ln_gamma( const double x );
double factorial( unsigned n );
double log_factorial( const unsigned n );
double binomial_coef( const unsigned n, const unsigned k );

/*
 * Compute and return power.  We don't use pow() because we know that b is always an integer.
 */

double power( double a, int b );

/*
 * Choose
 */

double choose( unsigned i, unsigned j );
inline double square( double x ) { return x * x; }


/* -------------------------------------------------------------------- */
/* Funky Formatting functions for inline with <<.			*/
/* -------------------------------------------------------------------- */

class floating_point_error : public std::runtime_error {
public:
    floating_point_error( const std::string& f, const unsigned l ) : std::runtime_error( construct( f, l ) ) {}

    virtual ~floating_point_error() {} 

private:
    std::string construct( const std::string& f, const unsigned l );
};

typedef enum { FP_IGNORE, FP_REPORT, FP_DEFERRED_ABORT, FP_IMMEDIATE_ABORT } fp_exeception_reporting;
extern fp_exeception_reporting matherr_disposition;	/* What to do about math probs.	*/
#endif

