/* -*- c++ -*-
 * runlqx.h	-- Greg Franks
 *
 * $URL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/qnsolver/runlqx.h $
 * ------------------------------------------------------------------------
 * $Id: runlqx.h 16325 2023-01-12 17:47:10Z greg $
 * ------------------------------------------------------------------------
 */

#ifndef _RUNLQX_H
#define _RUNLQX_H

#include <lqx/MethodTable.h>
#include <lqx/Environment.h>
#include "model.h"

class Model;

namespace SolverInterface {

    /* solve() simply calls up to print_symbol_table() */
    class Solve : public LQX::Method {
    public:
	/* Parameters necessary to call runSolverOnCurrentDOM() */
	Solve(Model& model ) : _model(model) {}
	virtual ~Solve() {}
		
	/* All of the glue code to make sure LQX can call solve() */
	virtual std::string getName() const { return "solve"; } 
	virtual const char* getParameterInfo() const { return "1"; } 
	virtual std::string getHelp() const { return "Solves the model."; } 
	virtual LQX::SymbolAutoRef invoke(LQX::Environment* env, std::vector<LQX::SymbolAutoRef >& args);
	static bool solveCallViaLQX;
	static bool implicitSolve;
	static std::string customSuffix;
	
    private:
	Model& _model;

	static size_t invocationCount;
    };
	
}
#endif 
