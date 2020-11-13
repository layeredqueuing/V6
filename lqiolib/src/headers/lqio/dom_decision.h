/* -*- c++ -*-
 *  $Id: dom_task.h 12241 2015-02-10 14:34:47Z greg $
 *
 *  Created by Martin Mroz on 24/02/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __LQIO_DOM_DECISION__
#define __LQIO_DOM_DECISION__

#include "input.h"
#include "dom_object.h"

namespace LQIO {
    namespace DOM {
	class Document;
	class ExternalVariable;
        class Decision;
	class Entry;
	class Task;

      
	/* All decision types */
	typedef enum DecisionType {
	    TYPE_TIMEOUT,
	    TYPE_ABORT,
	    TYPE_RETRY,
	    TYPE_INF_RETRY,
	} DecisionType;

	/* All decision types */
	typedef enum PathType {
	    SUCCESS_PATH,
	    ABORT_PATH,
	    TIMEOUT_PATH,
	    RETRY_PATH
	} PathType;

	class DecisionPath: public DocumentObject {
	public:
	    /* All decision types 
	       typedef enum PathType {
	       SUCCESS_PATH,
	       ABORT_PATH,
	       TIMOUT_PATH,
	       RETRY_PATH
	       } PathType;
	    */
	    DecisionPath(const Document * document, const char * pathname, Decision * aDecision, const PathType _pathType, const char* to_entry);
	    virtual ~DecisionPath ();

	    /* Accessors and Mutators */
	    const char * getTypeName() const { return __typeName; }

	    const char * getDstEntryName() const { return _toEntryName;}
	    const char * getName() const { return _name;}
	    const Decision * owner() const { return _owner; }
	    Entry * getSource() const {return _source;}
	    DecisionPath setSource(Entry * source) {_source = source; return *this;}
	    Entry * getDestination() const {return _destination;}
	    DecisionPath setDestination(Entry * dst) {_destination = dst; return *this;}
	    const PathType getPathType() const { return _pathType; }
	

	    /* initialize*/
	    void initialize();

	    /* results*/
	    double getPathProb() const { return _prob; }
	    void setPathProb(const double pathProb)  { _prob = pathProb; }
	    void printDecisionPath();

	private:
            /* instance varables for decision paths*/
	    const char* _toEntryName;
	    const PathType _pathType;
	    const Decision * _owner;
	    const char * _name;

	    Entry * _source;
	    Entry * _destination;

	    double _prob;
	    //double _prob_varience;

	public:
	    static const char * __typeName;
	};

	class Decision : public DocumentObject {
	    //public:

      
	public:
	    /* Designated initializers for the DOM Decision type */
	    Decision (const Document * document, const char * decision_name, 
		      const DecisionType type,
		      const char * decision_entry_name, 
		      ExternalVariable* T_timeout,
		      ExternalVariable* T_abort,
		      ExternalVariable* T_sleep,
		      ExternalVariable*  max_retries);

	    /* decision is defined as an element of a task*/
	    Decision (const Document * document, const Task * task, 
		      const char * decision_name, 
		      const DecisionType type,
		      const char * decision_entry_name, 
		      ExternalVariable* T_timeout,
		      ExternalVariable* T_abort,
		      ExternalVariable* T_sleep,
		      ExternalVariable*  max_retries);
	    virtual ~Decision ();


	    void initialize();

	    const char * getTypeName() const { return __typeName; }
	    const char * getName() const { return _name;}
	    const char * getEntryName() const { return _entry_name;}
	    const DecisionType getDecisionType() const { return _type; }

	    //Processor * getProcessor() const { return _processor; } 
	    //Processor * setProcessor( Processor * aProcessor ) ;
	    const Task * getTask() const { return _task; } 
	    void  setTask( Task * aTask) ;

	    Entry * getDecisionEntry() const { return _decisionEntry; }
	    Task * getDecisionTask() const { return _decisionTask; }
	    DecisionPath * getSuccPath() const { return _succPath; }
	    DecisionPath * getTimeoutPath() const { return _timeoutPath; }
	    DecisionPath * getAbortPath() const { return _abortPath; }
	    DecisionPath * getRetryPath() const { return _retryPath; }

	    void setDecisionEntry(Entry * decisionEntry ) { _decisionEntry = decisionEntry; }
	    void setDecisionTask(Task * decisionTask )  { _decisionTask = decisionTask; }
	    void setSuccPath(DecisionPath * succPath)  { _succPath = succPath; }
	    void setTimeoutPath(DecisionPath * timeoutPath)  { _timeoutPath = timeoutPath; }
	    void setAbortPath(DecisionPath * abortPath )  { _abortPath = abortPath; }
	    void setRetryPath(DecisionPath * retryPath )  { _retryPath = retryPath; }

	    ExternalVariable * getTimeout() const { return _timeout; }
	    ExternalVariable * getAbort() const { return _abort; }
	    ExternalVariable * getSleep() const { return _sleep; }
	    ExternalVariable * getMaxRetry() const { return _maxRetries; }

	    void setTimeout( ExternalVariable * timeout) {  _timeout =  timeout; }
	    void setAbort( ExternalVariable * abort ) {  _abort =  abort ; }
	    void setSleep( ExternalVariable * sleep ) {  _sleep =  sleep ; }
	    void setMaxRetry( ExternalVariable * maxRetries) {  _maxRetries =  maxRetries; }

	    double  getTimeoutValue() const ;
	    double  getAbortValue() const ;
	    double  getSleepValue() const ;
	    double  getMaxRetryValue() const ;

	    void  setTimeoutValue( const double value) ;
	    void  setAbortValue( const double value )  ;
	    void  setSleepValue( const double value )  ;
	    void  setMaxRetryValue( const double value); 
	  
	    void printDecision();
	private:
            /* instance varables for decisions*/
//	    ExternalVariable* _copies;

	    //  Processor * _processor;
	    const Task * _task;

	    const char * _name;
	    const char * _entry_name;
	    const DecisionType _type;

	    Entry * _decisionEntry;
	    Task * _decisionTask;
	    DecisionPath * _succPath;
	    DecisionPath * _timeoutPath;
	    DecisionPath * _abortPath;
	    DecisionPath * _retryPath;

	    ExternalVariable* _timeout;
	    ExternalVariable* _abort;
	    ExternalVariable* _sleep;
	    ExternalVariable* _maxRetries;

	public:
	    static const char * __typeName;
	};
    }
}
#endif /* __LQIO_DOM_DECISION__ */
