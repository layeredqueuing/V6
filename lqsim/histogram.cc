/* histogram.cc	-- Greg Franks Mon Jun 15 2009
 *
 * ------------------------------------------------------------------------
 * $Id: histogram.cc 15305 2021-12-31 16:01:37Z greg $
 * ------------------------------------------------------------------------
 */


#include "lqsim.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <lqio/error.h>
#include "histogram.h"
#include "errmsg.h"
#include "result.h"

Histogram::Histogram( const LQIO::DOM::Histogram * histogram )
    : _histogram(const_cast<LQIO::DOM::Histogram *>(histogram)),
      _n_bins(histogram->getBins()),
      _bin_size(_n_bins > 0 ? static_cast<double>(histogram->getMax() - histogram->getMin())/static_cast<double>(histogram->getBins()) : 0),
      _min(histogram->getMin()),
      _max(histogram->getMax()),
      _n(0),
      _count(0),
      _hist(_n_bins+2)
{
    if ( _max < _min ) {
	input_error2( LQIO::ERR_HISTOGRAM_INVALID_MAX, _max );
    }
    for_each ( _hist.begin(), _hist.end(), Exec<hist_bin>(&hist_bin::clear) );
}


void
Histogram::configure()
{
    for_each ( _hist.begin(), _hist.end(), Exec<hist_bin>(&hist_bin::clear) );
}

void
Histogram::reset()
{
    _count = 0.0;
    for_each ( _hist.begin(), _hist.end(), Exec<hist_bin>(&hist_bin::reset) );
}



void
Histogram::insert( const double value )
{
    unsigned int i = 0;
    if ( value < _min ) {
	i = 0;
    } else if ( _max <= value ) {
	i = overflow_bin();
    } else {
	i = static_cast<unsigned int>((value - _min) / _bin_size) + 1;
    }

    _hist[i].bin++;
    _count++;
}


void
Histogram::accumulate_data()
{
    if ( _count ) {
	for ( unsigned i = 0; i < _n_bins+2; ++i ) {
	    const double mean = _hist[i].bin / _count;
	    _hist[i].sum += mean;
	    _hist[i].sum_sqr += square( mean );
	    _hist[i].bin = 0;
	}
	_count = 0;
    }
    _n += 1;
}


double
Histogram::mean( const unsigned i ) const
{
    return _hist[i].sum / (double)(_n);
}


double
Histogram::variance( const unsigned i ) const
{
    if ( _n >= 2 ) {
	const double temp = _hist[i].sum_sqr - square( _hist[i].sum ) / static_cast<double>(_n);
	if ( temp > 0.0 ) {
	    return sqrt( temp / static_cast<double>(_n - 1) );
	} else if ( temp < -0.1 ) {
	    abort();
	}
    }
    return 0.0;
}


void
Histogram::insertDOMResults()
{
    for ( unsigned int i = 0; i < _n_bins+2; ++i ) {
	_histogram->setBinMeanVariance(i,mean(i),variance(i));
    }
}
