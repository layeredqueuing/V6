/*
 *  $Id: dom_group.cpp 12219 2014-11-18 16:38:29Z greg $
 *
 *  Created by Martin Mroz on 1/07/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "dom_document.h"
#include "dom_decision.h"
#include "dom_processor.h"
#include "dom_task.h"
#include "dom_entry.h"
#include "glblerr.h"

namespace LQIO {
    namespace DOM {

	const char * DecisionPath::__typeName = "decision_path";

	DecisionPath::DecisionPath(const Document * document, const char * pathname, Decision * aDecision, const PathType pathType, const char* to_entry)
	    : DocumentObject(document, pathname),
	      _toEntryName(to_entry),
	      _pathType(pathType),
	      _owner( aDecision),
	      _name(pathname),
	      _source(NULL),
	      _destination(NULL),
	      _prob(0.)
	{
	}

	void DecisionPath::initialize(){}

	DecisionPath::~DecisionPath (){}

	void  DecisionPath::printDecisionPath()
	{
	    std::cout<< "DecisionPath name = "<<getName()<<", owned by = "<<_owner->getName()<<", path toEntry name ="<< _toEntryName;
	    std::cout<< ", Type = "<<_pathType <<".  "<< std::endl;
	
	    //if (withResults)
	}
	Decision::Decision (const Document * document, const char * decision_name,
			    const DecisionType type,
			    const char * decision_entry_name,
			    ExternalVariable* T_timeout,
			    ExternalVariable* T_abort,
			    ExternalVariable* T_sleep,
			    ExternalVariable*  max_retries)
	    : DocumentObject(document, decision_name),
	      _task(NULL),
	      _name(decision_name),
	      _entry_name( decision_entry_name),
	      _type(type),
	      _decisionEntry(NULL),
	      _decisionTask(NULL),
	      _succPath(NULL),
	      _timeoutPath(NULL),
	      _abortPath(NULL),
	      _retryPath(NULL),
	      _timeout(T_timeout),
	      _abort( T_abort),
	      _sleep( T_sleep),
	      _maxRetries(max_retries)
	{
	}

	const char * Decision::__typeName = "decision";

	Decision::Decision (const Document * document, const Task * aTask,
			    const char * decision_name,
			    const DecisionType type,
			    const char * decision_entry_name,
			    ExternalVariable* T_timeout,
			    ExternalVariable* T_abort,
			    ExternalVariable* T_sleep,
			    ExternalVariable* max_retries)
	    : DocumentObject(document, decision_name),
	      _task(aTask),
	      _name(decision_name),
	      _entry_name(decision_entry_name),
	      _type(type),
	      _decisionEntry(NULL),
	      _decisionTask(NULL),
	      _succPath(NULL),
	      _timeoutPath(NULL),
	      _abortPath(NULL),
	      _retryPath(NULL),
	      _timeout(T_timeout),
	      _abort(T_abort),
	      _sleep(T_sleep),
	      _maxRetries(max_retries)
	{
	}

	Decision::~Decision(){}

	void Decision::initialize()
	{
	}

	/*	void Decision::setProcessor( Processor * aProcessor )
		{
		if (aProcessor != NULL)
		_processor = aProcessor;
		}
	*/
	void  Decision::setTask( Task * aTask)
	{
	    if (aTask != NULL)
		_task = aTask;
	}

	double Decision::getTimeoutValue() const
	{
	    /* Returns the timeout value */
	    double value = 0.0;
	    assert(_timeout->getValue(value) == true);
	    return value;
	}

	double Decision::getAbortValue() const
	{
	    /* Returns the clean up value */
	    double value = 0.0;
	    assert(_abort->getValue(value) == true);
	    return value;
	}

	double Decision::getSleepValue() const
	{
	    /* Returns the value of retry sleep */
	    double value = 0.0;
	    assert(_sleep->getValue(value) == true);
	    return value;
	}

	double Decision::getMaxRetryValue() const
	{
	    /* Returns the number of maximum retries */
	    double value = 0.0;
	    assert(_maxRetries->getValue(value) == true);
	    return value;
	}


	void Decision::setTimeoutValue(const double value)
	{
	    if ( _timeout == NULL ) {
		_timeout = new ConstantExternalVariable(value);
	    } else {
		_timeout->set(value);
	    }
	}

	void Decision::setAbortValue(const double value)
	{
	    if ( _abort == NULL ) {
		_abort = new ConstantExternalVariable(value);
	    } else {
		_abort->set(value);
	    }
	}

	void Decision::setSleepValue(const double value)
	{
	    if ( _sleep == NULL ) {
		_sleep = new ConstantExternalVariable(value);
	    } else {
		_sleep->set(value);
	    }
	}

	void Decision::setMaxRetryValue(const double value)
	{
	    if ( _maxRetries == NULL ) {
		_maxRetries = new ConstantExternalVariable(value);
	    } else {
		_maxRetries->set(value);
	    }
	}
	void  Decision::printDecision()
	{
	    std::cout<< "Decision: name = "<<getName()<<", decision entry name ="<< _entry_name;
	    std::cout<< ", Type = "<<_type <<".  "<< std::endl;
	    std::cout<< "attributes:  T_timeout ="<<_timeout << ", T_abort ="<<_abort
		     << "T _sleep = "<< _sleep <<", M_Retry= "<< _maxRetries<<std::endl;
	    //if (withResults)
	}

	
	// void Group::addTask(Task* task)
	// {
	//     /* Link the task into the list */
	//     _taskList.insert(task);
	// }

	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- [Result Values] -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

	// Group& Group::setResultUtilizationVariance(const double resultUtilizationVariance)
	// {
	//     /* Stores the given ResultUtilization of the Group */
	//     if ( resultUtilizationVariance > 0 ) {
	// 	const_cast<Document *>(getDocument())->setResultHasConfidenceIntervals(true);
	//     }
	//     _resultUtilizationVariance = resultUtilizationVariance;
	//     return *this;
	// }
    }
}
