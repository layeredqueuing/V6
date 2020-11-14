/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/lqns/call.h $
 *
 * Everything you wanted to know about an entry, but were afraid to ask.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 * March, 2004
 *
 * $Id: call.h 14091 2020-11-13 02:59:17Z greg $
 *
 * ------------------------------------------------------------------------
 */

#if	!defined(LQNS_CALL_H)
#define LQNS_CALL_H

#include "dim.h"
#include <lqio/input.h>
#include <lqio/dom_call.h>
#include <deque>
#include "interlock.h"


class Call;
class Entity;
class Entry;
class Entry;
class EntryPath;
class MVASubmodel;
class Path;
class PathNode;
class Phase;
class Server;
class Submodel;
class Task;
struct InterlockInfo;

typedef void (Call::*callFunc)( const unsigned, const unsigned, const double );
typedef bool (Call::*queryFunc)() const;

/* ------------------- Arcs between entries are... -------------------- */

class Call {

public:
    class Create {
    public:
	Create( Entry * src, unsigned p ) : _src(src), _p(p) {}

	void operator()( const LQIO::DOM::Call * );

    private:
	Entry * _src;
	const unsigned _p;
    };
    
    
    struct Find {
	Find( const Call * call, const bool direct_path ) : _call(call), _direct_path(direct_path), _broken(false) {}

	bool operator()( const Call * ) const;
	
    private:
	const Call * _call;
	const bool _direct_path;
	mutable bool _broken;
    };
	
    class Perform {
	Perform();
    };

    class stack : public std::deque<const Call *>
    {
    public:

	stack() : std::deque<const Call *>() {}
	unsigned depth() const;

	static std::string fold( const std::string& s, const Call * call ) { return call->getDOM() != nullptr ? s + "," + call->srcName() : s; }
    };

    class call_cycle
    {
    public:
	call_cycle() {}
	virtual ~call_cycle() throw() {}
    };

    static double add_rendezvous( double sum, const Call * call ) { return sum + call->rendezvous() * call->fanOut(); }
    static double add_rendezvous_no_fwd( double sum, const Call * call ) { return !call->isForwardedCall() ? sum + call->rendezvous() * call->fanOut() : sum; }
    static double add_forwarding( double sum, const Call * call ) { return sum + call->forward() * call->fanOut(); }
    static double add_IL_queue_length( double sum, const Call * call ) { return call->isAlongILPath() ? sum + call->getQueueLength() : sum; }

    struct find_call {
	find_call( const Entry * e, const queryFunc f ) : _e(e), _f(f) {}
	bool operator()( const Call * call ) const { return call->dstEntry() == _e && !call->isForwardedCall() && (!_f || (call->*_f)()); }
    private:
	const Entry * _e;
	const queryFunc _f;
    };
    
    struct find_any_call {
	find_any_call( const Entry * e ) : _e(e) {}
	bool operator()( const Call * call ) const { return call->dstEntry() == _e; }
    private:
	const Entry * _e;
    };
    
    struct find_fwd_call {
	find_fwd_call( const Entry * e ) : _e(e) {}
	bool operator()( const Call * call ) const { return call->dstEntry() == _e && call->isForwardedCall(); }
    private:
	const Entry * _e;
    };
    
    struct add_replicated_rendezvous {
	add_replicated_rendezvous( unsigned int submodel ) : _submodel(submodel) {}
	double operator()( double sum, const Call * call ) { return call->submodel() == _submodel ? sum + call->rendezvous() * call->fanOut() : sum; }
    private:
	const unsigned int _submodel;
    };

    struct add_rendezvous_to {
	add_rendezvous_to( const Entity * task ) : _task(task) {}
	double operator()( double sum, const Call * call ) const { return call->dstTask() == _task ? sum + call->rendezvous() * call->fanOut() : sum; }
    private:
	const Entity * _task;
    };

    struct add_submodel_rendezvous {
	add_submodel_rendezvous( unsigned int submodel ) : _submodel(submodel) {}
	double operator()( double sum, const Call * call ) const { return call->submodel() == _submodel ? sum + call->rendezvous() : sum; }
    private:
	const unsigned int _submodel;
    };

    struct add_wait {
	add_wait( unsigned int submodel ) : _submodel(submodel) {}
	double operator()( double sum, const Call * call ) const { return call->submodel() == _submodel ? sum + call->rendezvousDelay() : sum; }
    private:
	const unsigned int _submodel;
    };

    struct add_interlocked_wait {
	add_interlocked_wait( unsigned int submodel ) : _submodel(submodel) {}
	double operator()( double sum, const Call * call ) const { return call->submodel() == _submodel && call->isInterlocked()
		? sum + call->rendezvousDelay() : sum; }
    private:
	const unsigned int _submodel;
    };

    struct add_interlock_pr {
	add_interlock_pr( const Entity * server ) : _server(server) {}
	double operator()( double sum, const Call * call ) const;
    private:
	const Entity * _server;
    };

    struct set_real_customers {
	set_real_customers( const MVASubmodel& submodel, const Entity * server, unsigned k ) : _submodel(submodel), _server(server), _k(k) {}
	double operator()( double sum, Call * call ) { return sum + call->setRealCustomers( _submodel, _server, _k ); }
    private:
	const MVASubmodel& _submodel;
	const Entity * _server;
	const unsigned _k;
    };

    typedef enum { RENDEZVOUS_CALL=0x01, SEND_NO_REPLY_CALL=0x02, FORWARDED_CALL=0x04, OVERTAKING_CALL=0x08 } call_type;

    Call( const Phase * fromPhase, const Entry * toEntry );
    virtual ~Call();

private:
    Call( const Call& ) { abort(); }		/* Copying is verbotten */
    Call& operator=( const Call& ) { abort(); return *this; }

public:
    int operator==( const Call& item ) const;
    int operator!=( const Call& item ) const;

    virtual Call& initWait() = 0;
    bool check() const;

    /* Instance variable access */

    const LQIO::DOM::Call* getDOM() const { return _dom; }
    virtual double rendezvous() const;
    virtual Call& rendezvous( const LQIO::DOM::Call* dom ) { _dom = dom; return *this; }
    Call& accumulateRendezvous( const double value )  { abort(); /*myRendezvous += value;*/ return *this; }
    double sendNoReply() const;
    Call& sendNoReply( const LQIO::DOM::Call* dom ) { _dom = dom; return *this; }
    Call& accumulateSendNoReply( const double value ) { abort(); /*mySendNoReply += value;*/ return *this; }
    double forward() const;
    Call& forward( const LQIO::DOM::Call* dom ) { _dom = dom; return *this; }
    unsigned fanOut() const;
    double sumOfCalls() const { return getDOM() ? getDOM()->getCallMeanValue() : 0.0; }
    unsigned getPhaseNum() const;

    /* Queries */

    virtual bool isForwardedCall() const { return false; }
    virtual bool isActivityCall() const { return false; }
    virtual bool isProcessorCall() const { return false; }
    bool hasRendezvous() const { return getDOM() ? (getDOM()->getCallType() == LQIO::DOM::Call::RENDEZVOUS || getDOM()->getCallType() == LQIO::DOM::Call::QUASI_RENDEZVOUS) && getDOM()->getCallMeanValue() > 0: false; }
    bool hasSendNoReply() const { return getDOM() ? getDOM()->getCallType() == LQIO::DOM::Call::SEND_NO_REPLY && getDOM()->getCallMeanValue() > 0 : false; }
    bool hasForwarding() const { return  getDOM() ? getDOM()->getCallType() == LQIO::DOM::Call::FORWARD && getDOM()->getCallMeanValue() > 0 : false; }
    bool hasOvertaking() const;
    bool hasNoCall() const { return !hasRendezvous() && !hasSendNoReply() && !hasForwarding(); }
    bool hasRendezvousOrNone() const { return !hasSendNoReply() && !hasForwarding(); }
    bool hasSendNoReplyOrNone() const { return !hasRendezvous() && !hasForwarding(); }
    bool hasForwardingOrNone() const { return !hasRendezvous() && !hasSendNoReply(); }
    bool hasNoForwarding() const { return dstEntry() == nullptr || hasRendezvous() || hasSendNoReply(); }		/* Special case for topological sort */

    virtual const Entry * srcEntry() const;
    virtual const std::string& srcName() const;
    const Phase * srcPhase() const { return source; }
    virtual const Task * srcTask() const;

    const std::string& dstName() const;
    const Entry * dstEntry() const { return destination; }
    unsigned submodel() const;	/* Proxy */

    double rendezvousDelay() const;
    double rendezvousDelay( const unsigned k );
    double wait() const { return _wait; }
    double elapsedTime() const;
    double queueingTime() const;
    double interlockPr() const ;
    int diff_population() const;
    virtual const Call& insertDOMResults() const;
    double nrFactor( const Submodel& aSubmodel, const unsigned k ) const;

    /* Computation */

    double variance() const;
    double CV_sqr() const;
    const Call& followInterlock( Interlock::CollectTable& ) const;

    /* MVA interface */

    void setChain( const unsigned k, const unsigned p, const double rate );
    unsigned getChain() const { return _chainNumber; } //tomari
    bool hasChain( unsigned int k ) const { return k == _chainNumber; }

    void setVisits( const unsigned k, const unsigned p, const double rate );
    virtual void setLambda( const unsigned k, const unsigned p, const double rate );
    void clearWait( const unsigned k, const unsigned p, const double );
    void saveWait( const unsigned k, const unsigned p, const double );
    void saveOpen( const unsigned k, const unsigned p, const double );

    /*  Interlocked flow */
    virtual double getMaxCustomers() const;
    double setRealCustomers( const MVASubmodel&, const Entity * aServer, unsigned int k ) const;

    void saveILWait( const unsigned k, const unsigned p, const double );
    void clearILWait( const unsigned k, const unsigned p, const double );

    bool isInterlocked () const { return _interlockedFlow > 0.0; }
    bool isAlongILPath() const { return _interlockedFlow >= 0.0; }

    Call& setInterlockedFlow( const MVASubmodel& submodel );
    void saveQueueWeight( const unsigned k, const unsigned p, const double );
    double getQueueLength() const;

    double computeWeight( const MVASubmodel& );
    double getWeight() const { return _callWeight; }

    void setqueueWeight( double weight ) { _queueWeight = weight; }
    double getqueueWeight() const { return _queueWeight; }

    /* Proxies */

    const Entity * dstTask() const;
    short index() const;
    double serviceTime() const;

private:
    /* Interlock */
    bool isRealCustomer( const MVASubmodel&, const Entity *, unsigned ) const;
    double getInterlockedFlow() const { return std::max( _interlockedFlow, 0.0 ); }

protected:
    const Phase* source;		/* Calling entry.		*/
    double _wait;			/* Waiting time.		*/
    double _interlockedFlow;   		/* >0.0: interlocked flow 	*/
    					/* =0.0: is along the Path of an interlocked flow. */
					/* <0.0: is an non interlocked flow ; */
    double _callWeight;
    double _queueWeight;		/* the ratio of queueing time from its own chain */

private:
    const Entry* destination;		/* to whom I am referring to	*/

    /* Input */
    const LQIO::DOM::Call* _dom;

    unsigned _chainNumber;
};


/* -------------------------------------------------------------------- */

class NullCall : public Call {
public:
    NullCall() : Call(nullptr,nullptr) {}

    virtual NullCall& initWait() { return *this; }
};

/* -------------------------------------------------------------------- */

class TaskCall : public Call {
public:
    TaskCall( const Phase *, const Entry * toEntry );

    virtual TaskCall& initWait();
};


class ForwardedCall : public TaskCall {
public:
    ForwardedCall( const Phase *, const Entry * toEntry, const Call * fwdCall );

    virtual bool isForwardedCall() const { return true; }
    virtual const std::string& srcName() const;
    virtual const ForwardedCall& insertDOMResults() const;

private:
    const Call * myFwdCall;		// Original forwarded call
};

class ProcessorCall : public Call {
public:
    ProcessorCall( const Phase *, const Entry * toEntry );

    virtual double getMaxCustomers() const;
    virtual bool isProcessorCall() const { return true; }
    virtual ProcessorCall& initWait();
    virtual void setWait(double newWait);
};


class ActivityCall : public TaskCall {
public:
    ActivityCall( const Phase * fromActivity, const Entry * toEntry );

    virtual bool isActivityCall() const { return true; }
    virtual double getMaxCustomers()const ;
    virtual const Entry * srcEntry() const;
    virtual const std::string& srcName() const;
    virtual const Task * srcTask() const;
};


class ActivityForwardedCall : public ActivityCall {
public:
    ActivityForwardedCall( const Phase *, const Entry * toEntry );

    virtual bool isForwardedCall() const { return true; }
};


class ActProcCall : public ProcessorCall {
public:
    ActProcCall( const Phase *, const Entry * toEntry );

    virtual bool isActivityCall() const { return true; }
    virtual const Entry * srcEntry() const;
    virtual const std::string& srcName() const;
    virtual const Task * srcTask() const;
    virtual double getMaxCustomers() const;
};
#endif
