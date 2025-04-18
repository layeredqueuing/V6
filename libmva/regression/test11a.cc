/*  -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk-V6/libmva/regression/test11a.cc $
 *
 * Priority MVA Test case from:
 *
 * ------------------------------------------------------------------------
 * $Id: test11a.cc 15385 2022-01-25 11:45:59Z greg $
 * ------------------------------------------------------------------------
 */

#include <cmath>
#include "testmva.h"
#include "server.h"
#include "pop.h"
#include "mva.h"

void
test( Population& NCust, Vector<Server *>& Q, VectorMath<double>& Z, VectorMath<unsigned>& priority, const unsigned )
{
    const unsigned classes  = 2;
    const unsigned stations = 2;

    NCust.resize(classes);			/* Population vector.		*/
    Z.resize(classes);			/* Think times.			*/
    priority.resize(classes);
    Q.resize(stations);			/* Queue type.  SS/delay.	*/

    NCust[1] = 3;	NCust[2] = 3;
    Z[1] = 0.0;	Z[2] = 0.0;
    priority[1] = 0;priority[2] = 1;

    Q[1] = new Infinite_Server(classes);
    Q[2] = new PR_FCFS_Server(classes);

    Q[1]->setService(1,4.0).setVisits(1,1.0);
    Q[1]->setService(2,4.0).setVisits(2,1.0);
    Q[2]->setService(1,1.0).setVisits(1,1.0);
    Q[2]->setService(2,1.0).setVisits(2,1.0);
}


void
special_check( std::ostream&, const MVA&, const unsigned )
{
}



static double goodL[4][3][3] = {
    { { 0, 0, 0 }, { 0, 1.368, 2.197 }, { 0, 1.632, 0.8028 } },		// Exact MVA
    { { 0, 0, 0 }, { 0, 1.448, 2.197 }, { 0, 1.552, 0.8033 } },		// Linearizer
    { { 0, 0, 0 }, { 0, 1.552, 1.711 }, { 0, 1.448,  1.289 } },		// Fast Linearizer
    { { 0, 0, 0 }, { 0, 1.365, 2.158 }, { 0, 1.635, 0.8423 } },		// Bard Schweitzer
};

bool
check( const int solverId, const MVA& solver, const unsigned )
{
    bool ok = true;

    unsigned n = solver.offset(solver.NCust);
    for ( unsigned m = 1; m <= solver.M; ++m ) {
	for ( unsigned k = 1; k <= solver.K; ++k ) {
	    if ( fabs( solver.L[n][m][1][k] - goodL[solverId][m][k] ) >= 0.001 ) {
		std::cerr << "Mismatch at m=" << m <<", k=" << k;
		std::cerr << ".  Computed=" << solver.L[n][m][1][k] << ", Correct= " << goodL[solverId][m][k] << std::endl;
		ok = false;
	    }
	}
    }
    return ok;
}

