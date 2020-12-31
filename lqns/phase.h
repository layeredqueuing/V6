/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/lqns/phase.h $
 *
 * Everything you wanted to know about an entry, but were afraid to ask.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 *
 * $Id: phase.h 14310 2020-12-31 17:16:57Z greg $
 *
 * ------------------------------------------------------------------------
 */

#if	!defined(PHASE_H)
#define PHASE_H

#include <string>
#include <set>
#include <lqio/input.h>
#include <mva/vector.h>
#include <mva/prob.h>
#include "call.h"
#include "interlock.h"

class Activity;
class ActivityList;
class AndForkActivityList;
class DeviceEntry;
class Entity;
class Entry;
class MVASubmodel;
class ProcessorCall;
class Submodel;
class Task;
class TaskEntry;

namespace LQIO {
    namespace DOM {
	class Phase;
	class Call;
    }
}

class NullPhase {
    friend class Entry;			/* For access to myVariance */
    friend class TaskEntry;		/* For access to myVariance */
    friend class DeviceEntry;		/* For access to myWait     */
    friend class Activity;		/* For access to myVariance */
    friend class AndForkActivityList;	/* For access to myVariance */

public:
    NullPhase()
	: _name(),
	  _variance(0.0),
	  _wait(),
	  _interlockedWait(),
	  _dom(nullptr),
	  _serviceTime(0.0)
	{}

    virtual ~NullPhase() {}

    int operator==( const NullPhase& aPhase ) const { return &aPhase == this; }

    /* Initialialization */
	
    virtual NullPhase& configure( const unsigned );
    NullPhase& setDOM(LQIO::DOM::Phase* phaseInfo);
    virtual LQIO::DOM::Phase* getDOM() const { return _dom; }
    virtual const std::string& name() const { return _name; }
    NullPhase& setName( const std::string& name ) { _name = name; return *this; }

    /* Instance variable access */
    virtual bool isPresent() const { return _dom != 0 && _dom->isPresent(); }
    bool hasThinkTime() const { return _dom && _dom->hasThinkTime(); }
    virtual bool isActivity() const { return false; }
	
    NullPhase& setServiceTime( const double t );
    NullPhase& addServiceTime( const double t );
    double serviceTime() const;
    double thinkTime() const;
    double CV_sqr() const;

    virtual double variance() const { return _variance; } 		/* Computed variance.		*/
    double computeCV_sqr() const;

    virtual double waitExcept( const unsigned ) const;
    double elapsedTime() const { return waitExcept( 0 ); }
    double ILWait(unsigned int submodel) const { return _interlockedWait[submodel];} 
    void setILWait (unsigned int submodel, double il_wait);
    void addILWait (unsigned int submodel, double il_wait);
    double getILWait (unsigned submodel) const;

    virtual std::ostream& print( std::ostream& output ) const { return output; }
	
    virtual NullPhase& recalculateDynamicValues() { return *this; }

    static void insertDOMHistogram( LQIO::DOM::Histogram * histogram, const double m, const double v );

protected:
    std::string _name;			/* Name -- computed dynamically		*/
    double _variance;			/* Set if this is a processor		*/
    VectorMath<double> _wait;		/* Saved waiting time.			*/
    VectorMath<double> _interlockedWait;/* Saved interlocked waiting time.	*/
    LQIO::DOM::Phase* _dom;
	
private:
    double _serviceTime;		/* Initial service time.		*/
};


/* -------------------------------------------------------------------- */

class Phase : public NullPhase {
    friend class Entry;			/* For access to _surrogateDelay 	*/
    friend class Activity;		/* For access to _surrogateDelay 	*/
    friend class RepeatActivityList;	/* For access to _surrogateDelay 	*/
    friend class OrForkActivityList;	/* For access to _surrogateDelay 	*/

public:
    struct get_servers {
	get_servers( std::set<Entity *>& servers ) : _servers(servers) {}
	void operator()( const Phase& phase ) const;
	void operator()( const Phase* phase ) const;
    private:
	std::set<Entity *>& _servers;
    };

    struct add_IL_wait {
	add_IL_wait( unsigned int submodel ) : _submodel(submodel) {}
	double operator()( double sum, const Phase& phase ) { return sum + phase.isPresent() ? phase._interlockedWait[_submodel] : 0.0; }
    private:
	const unsigned int _submodel;
    };
    
private:
    struct add_utilization_to {
	add_utilization_to( const Entry * entry ) : _entry(entry) {}
	double operator()( double sum, const Phase& phase ) const;
    private:
	const Entry * _entry;
    };
    
    struct get_interlocked_tasks {
	get_interlocked_tasks( Interlock::CollectTasks& path ) : _path(path) {}
	bool operator()( bool rc, Call * call ) const;
    private:
	Interlock::CollectTasks& _path;
    };


public:
    class CallExec {
    public:
	CallExec( const Entry * e, unsigned submodel, unsigned k, unsigned p, callFunc f, double rate ) : _e(e), _submodel(submodel), _k(k), _p(p), _f(f), _rate(rate) {}
	CallExec( const CallExec& src, double rate, unsigned p ) : _e(src._e), _submodel(src._submodel), _k(src._k), _p(p), _f(src._f), _rate(rate) {}		// Set rate and phase.
	CallExec( const CallExec& src, double scale ) : _e(src._e), _submodel(src._submodel), _k(src._k), _p(src._p), _f(src._f), _rate(src._rate * scale) {}	// Set rate.

	const Entry * entry() const { return _e; }
	double getRate() const { return _rate; }
	unsigned getPhase() const { return _p; }

	void operator()( Call * object ) const { if (object->submodel() == _submodel) (object->*_f)( _k, _p, _rate ); }

    private:
	const Entry * _e;
	const unsigned _submodel;
	const unsigned _k;
	const unsigned _p;
	const callFunc _f;
	const double _rate;
    };
    
public:
    Phase( const std::string& );
    Phase();
    virtual ~Phase();

    /* Initialialization */
	
    virtual Phase& initProcessor();
    void initCFSProcessor();
    Phase& initReplication( const unsigned );
    Phase& resetReplication();
    Phase& initWait();
    Phase& initVariance();

    unsigned findChildren( Call::stack&, const bool ) const;
    virtual const Phase& followInterlock( Interlock::CollectTable&  ) const;
    virtual void callsPerform( const CallExec& ) const;
    Phase& setInterlockedFlow( const MVASubmodel& );
    void addSrcCall( Call * aCall ) { _callList.insert(aCall); }
    void removeSrcCall( Call *aCall ) { _callList.erase(aCall); }

    /* Instance Variable access */
	
    LQIO::DOM::Phase::Type phaseTypeFlag() const { return _dom ? _dom->getPhaseTypeFlag() : LQIO::DOM::Phase::Type::STOCHASTIC; }
    Phase& setEntry( const Entry * entry ) { _entry = entry; return *this; }
    const Entry * entry() const { return _entry; }
    virtual const Entity * owner() const;
    virtual double getMaxCustomers() const;	/* Proxy */
    Phase& setPrOvertaking( const Probability& pr_ot ) { _prOvertaking = pr_ot; return *this; }
    const Probability& prOvertaking() const { return _prOvertaking; }

    bool isDeterministic() const { return _dom ? _dom->getPhaseTypeFlag() == LQIO::DOM::Phase::Type::DETERMINISTIC : false; }
    bool isNonExponential() const { return serviceTime() > 0 && CV_sqr() != 1.0; }
    
    /* Call lists to/from entries. */
	
    double rendezvous( const Entity * ) const;
    double rendezvous( const Entry * ) const;
    Phase& rendezvous( Entry *, const LQIO::DOM::Call* callDOMInfo );
    double sendNoReply( const Entry * ) const;
    Phase& sendNoReply( Entry *, const LQIO::DOM::Call* callDOMInfo );
    Phase& forward( Entry *, const LQIO::DOM::Call* callDOMInfo );
    double forward( const Entry * ) const;

    const std::set<Call *>& callList() const { return _callList; }

    /* Calls to processors */
	
    ProcessorCall * processorCall() const { return _processorCall; }
    double processorCalls() const;
    double queueingTime() const;
    double processorWait() const;
	
    /* Queries */

    virtual bool check() const;
    double numberOfSlices() const;
    double numberOfSubmodels() const { return _wait.size(); }
    virtual double throughput() const;
    double utilization() const;
    double processorUtilization() const;
    bool isUsed() const { return _callList.size() > 0.0 || serviceTime() > 0.0; }
    bool hasVariance() const;
    virtual bool isPseudo() const { return false; }		// quorum

    /* computation */
	
    virtual double waitExcept( const unsigned ) const;
    double waitExceptChain( const unsigned, const unsigned k );
    double computeVariance();	 			/* Computed variance.		*/
    Phase& updateWait( const Submodel&, const double ); 
    double getProcWait( unsigned int submodel, const double relax ); // tomari quorum
    double getTaskWait( unsigned int submodel, const double relax );
    double getRendezvous( unsigned int submodel, const double relax );
    double updateWaitReplication( const Submodel& );
    double getReplicationProcWait( unsigned int submodel, const double relax );
    double getReplicationTaskWait( unsigned int submodel, const double relax ); //tomari quorum
    double getReplicationRendezvous( unsigned int submodel, const double relax );
    virtual bool getInterlockedTasks( Interlock::CollectTasks& path ) const;
    Phase& updateInterlockedWait( const Submodel& aSubmodel, const double relax );

    /* recalculation of dynamic values */
	
    virtual Phase& recalculateDynamicValues();
//    void cfsRecalculateDynamicValues(double ratio1, double newthinktime);

    /*+ DPS +*/
    Phase& computeCFSDelay(double rate, double groupratio);
    double getCFSDelay() const { return _cfs_delay; }
    Phase& reset_lowerbound() { if ( _cfs_delay > 0. ) _cfs_delay_lowerbound = 0.; return *this; }

    virtual const Phase& insertDOMResults() const; 

protected:
    Call * findCall( const Entry * anEntry, const queryFunc = nullptr ) const;
    Call * findFwdCall( const Entry * anEntry ) const;
    virtual Call * findOrAddCall( const Entry * anEntry, const queryFunc = nullptr );
    virtual Call * findOrAddFwdCall( const Entry * anEntry, const Call * fwdCall );

    double processorVariance() const;

private:
    Phase const& addForwardingRendezvous( Call::stack& callStack ) const;
    Phase& forwardedRendezvous( const Call * fwdCall, const double value );
    double sumOfRendezvous() const;
    double nrFactor( const Call * aCall, const Submodel& aSubmodel ) const;
    double mol_phase() const;
    double stochastic_phase() const;
    double deterministic_phase() const;
    double random_phase() const;

    double cfsThinkTime( double groupRatio );

	
protected:
    const Entry * _entry;		/* Root for activity			*/
    std::set<Call *> _callList;         /* Who I call.                          */
    ProcessorCall * _processorCall;     /* Link to processor.                   */
    ProcessorCall * _thinkCall;		/* Link to processor.                   */

private:
    DeviceEntry * _processorEntry;     	/*                                      */
    LQIO::DOM::Entry * _procEntryDOM;	/* Only for ~Phase			*/
    LQIO::DOM::Call * _procCallDOM;	/* Only for ~Phase			*/
    DeviceEntry * _thinkEntry;         	/*                                      */
    LQIO::DOM::Entry * _thinkEntryDOM;	/* Only for ~Phase			*/
    LQIO::DOM::Call * _thinkCallDOM;	/* Only for ~Phase			*/

    VectorMath<double> _surrogateDelay;	/* Saved old surrogate delay. REP N-R	*/
    Probability _prOvertaking;

    double _cfs_delay;
    double _cfs_delay_upperbound;
    double _cfs_delay_lowerbound;
};

inline std::ostream& operator<<( std::ostream& output, const Phase& self ) { self.print( output ); return output; }
#endif
