/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/lqns/gamma.h $
 *
 * Gamma distribution.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 2012
 *
 * $Id: gamma.h 16800 2023-08-21 19:23:24Z greg $
 *
 * ------------------------------------------------------------------------
 */

#ifndef LQNS_GAMMA_H
#define LQNS_GAMMA_H

class Gamma_Distribution
{
public:
    Gamma_Distribution( double shape, double scale );
    double  getDensity(double x) const;
    double getCDF(double x) const;
    
private:
    void setParameters( double shape, double scale );

    static double logGamma(double x);
    static double gammaCDF(double x, double a);
    static double gammaSeries(double x, double a);
    static double gammaCF(double x, double a);
    
    double _shape;
    double _scale;
    double _c;
};
#endif /* LQNS_GAMMA_H */
