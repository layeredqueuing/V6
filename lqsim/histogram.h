/* -*- c++ -*- */
/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* May  1996.								*/
/************************************************************************/

/*
 * Global vars for simulation.
 *
 * $Id: histogram.h 14882 2021-07-07 11:09:54Z greg $
 */

#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif
#include <vector>
#include <lqio/dom_histogram.h>

class Histogram
{
private:
    Histogram( const Histogram& );
    Histogram& operator=( const Histogram& );

    class hist_bin {
    public:
	hist_bin() : bin(0), sum(0), sum_sqr(0) {}
	hist_bin& reset() { bin = 0.; return *this; }
	hist_bin& clear() { bin = 0.; sum = 0.; sum_sqr = 0.; return *this; }

	double bin;
	double sum;
	double sum_sqr;
    };

public:
    Histogram( const LQIO::DOM::Histogram * );
    void configure();
    void reset();

    void accumulate_data();
    void insert(const double value);

    void insertDOMResults();
    
private:
    unsigned int overflow_bin() const { return _n_bins + 1; }

    double mean( const unsigned i ) const;
    double variance( const unsigned i ) const;

private:
    LQIO::DOM::Histogram * _histogram;	/* */
    unsigned int _n_bins;		/* Number of bins */
    double _bin_size;	        	/* Size of each bin */
    double _min;                	/* Lower range limit on the histogram */
    double _max;                	/* Upper range limit on the histogram */
    unsigned _n;			/* Number of blocks. */
    double _count;			/* Total count in all bins */
    std::vector<hist_bin> _hist;
};
#endif

