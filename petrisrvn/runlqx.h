/* -*- C++ -*- 
 * ------------------------------------------------------------------------
 * $Id: runlqx.h 17075 2024-02-28 21:20:08Z greg $
 * ------------------------------------------------------------------------
 */

#ifndef _RUNLQX_H
#define _RUNLQX_H

#include <lqx/Program.h>
#include <lqx/MethodTable.h>
#include <lqx/Environment.h>
#include <string>

class Model;

namespace SolverInterface {

    class Solve : public LQX::Method {
    public:
	typedef bool (Model::*solve_using)();

	/* Parameters necessary to call runSolverOnCurrentDOM() */
	Solve(LQIO::DOM::Document* document, solve_using solve, Model* aModel) :
	    _aModel(aModel), _solve(solve), _document(document) {}
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
	Model * _aModel;
	solve_using _solve;
	LQIO::DOM::Document* _document;

	static unsigned int invocationCount;
    };
}
#endif
