/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk-V6/lqns/variance.h $
 *
 * Variance calculations.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 *
 * $Id: variance.h 16945 2024-01-26 13:02:36Z greg $
 *
 * ------------------------------------------------------------------------
 */

#ifndef LQNS_VARIANCE_H
#define	LQNS_VARIANCE_H

class Probability;
class Entity;

class Variance {
public:
    Variance() : mean(0.0), mean_sqr(0.0) {}
    virtual ~Variance() {}

    double variance() const;
    double S() const { return mean; }

    virtual void addStage( const Probability&, const double, const Positive& ) = 0;

protected:
    double mean;
    double mean_sqr;
};

class SeriesParallel : public Variance {
public:
    SeriesParallel() : Variance() {}
	
    void addStage( const Probability&, const double, const Positive& );

    static double totalVariance( const Entity& anEntity );
};

class OrVariance : public Variance {
public:
    OrVariance() : Variance() {} 
		
    void addStage( const Probability&, const double, const Positive& );

    static double totalVariance( const Entity& anEntity );
};
#endif
