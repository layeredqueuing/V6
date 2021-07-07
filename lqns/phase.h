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
 * $Id: phase.h 14882 2021-07-07 11:09:54Z greg $
 *
 * ------------------------------------------------------------------------
 */

#if	!defined(PHASE_H)
#define PHASE_H

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

/* -------------------------------------------------------------------- */

class NullPhase {
    friend class Entry;			/* For access to myVariance */
    friend class TaskEntry;		/* For access to myVariance */
    friend class DeviceEntry;		/* For access to myWait     */
    friend class Activity;		/* For access to myVariance */
    friend class AndForkActivityList;	/* For access to myVariance */

public:
    NullPhase( const std::string& name );
    virtual ~NullPhase() = default;
    int operator==( const NullPhase& aPhase ) const { return &aPhase == this; }

protected:
    NullPhase( const NullPhase& );

public:
    /* Initialialization */
	
    /* Instance variable access */
    virtual NullPhase& configure( const unsigned );
    NullPhase& setDOM(LQIO::DOM::Phase* phaseInfo);
    virtual LQIO::DOM::Phase* getDOM() const { return _dom; }
    virtual const std::string& name() const { return _name; }
    NullPhase& setName( const std::string& name ) { _name = name; return *this; }
    unsigned int getPhaseNumber() const { return _phase_number; }
    NullPhase& setPhaseNumber( unsigned int p ) { _phase_number = p; return *this; }

    /* Queries */
    virtual bool isPresent() const { return _dom != 0 && _dom->isPresent(); }
    bool hasThinkTime() const { return _dom && _dom->hasThinkTime(); }
    virtual bool isActivity() const { return false; }
	
    NullPhase& setServiceTime( const double t );
    NullPhase& addServiceTime( const double t );
    double serviceTime() const;
    double thinkTime() const;
    double CV_sqr() const;

    NullPhase& setVariance( double variance ) { _variance = variance; return *this; }
    virtual double variance() const { return _variance; } 		/* Computed variance.		*/
    double computeCV_sqr() const;

    virtual double waitExcept( const unsigned ) const;
    double elapsedTime() const { return waitExcept( 0 ); }
    double ILWait(unsigned int submodel) const { return _interlockedWait[submodel];} 
    void setILWait (unsigned int submodel, double il_wait);
    void addILWait (unsigned int submodel, double il_wait);
    double getILWait (unsigned submodel) const;
    NullPhase& setWaitingTime( unsigned int submodel, double value ) { _wait[submodel] = value; return *this; }
    double waitingTime( unsigned int submodel ) const { return _wait[submodel];}

    virtual NullPhase& recalculateDynamicValues() { return *this; }

    static void insertDOMHistogram( LQIO::DOM::Histogram * histogram, const double m, const double v );

private:
    LQIO::DOM::Phase* _dom;
    std::string _name;			/* Name -- computed dynamically		*/
    unsigned int _phase_number;		/* phase of entry (if phase)		*/

protected:
    /* Computed in class Phase */
    double _serviceTime;		/* Initial service time.		*/
    double _variance;			/* Set if this is a processor		*/
    VectorMath<double> _wait;		/* Saved waiting time.			*/
    VectorMath<double> _interlockedWait;/* Saved interlocked waiting time.	*/
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

private:
    struct add_IL_wait {
	add_IL_wait( unsigned int submodel ) : _submodel(submodel) {}
	double operator()( double sum, const Phase& phase ) { return sum + phase.isPresent() ? phase._interlockedWait[_submodel] : 0.0; }
    private:
	const unsigned int _submodel;
    };
    
    struct add_utilization_to {
	add_utilization_to( const Entry * entry ) : _entry(entry) {}
	double operator()( double sum, const Phase& phase ) const;
    private:
	const Entry * _entry;
    };
    
    struct add_weighted_wait {
	add_weighted_wait( unsigned int submodel, double total ) : _submodel(submodel), _total(total) {}
	double operator()( double sum, const Call * call ) const { return call->submodel() == _submodel ? sum + call->wait() * call->rendezvous() / _total: sum; }
    private:
	const unsigned int _submodel;
	const double _total;
    };

    /* Bonus entries are created on devices for each phase */
    class DeviceInfo {
	friend class Phase;
	
    public:
	enum class Type { HOST, PROCESSOR, THINK_TIME, CFS_DELAY };

	DeviceInfo( const Phase&, const std::string&, Type );		/* True if this device is the phase's processor */
	~DeviceInfo();

	bool isHost() const { return _type == Type::HOST; }
	bool isProcessor() const { return _type == Type::HOST || _type == Type::PROCESSOR; }
	bool isCFSDelay() const { return _type == Type::CFS_DELAY; }
	ProcessorCall * call() const { return _call; }
	DeviceEntry * entry() const { return _entry; }
	DeviceInfo& recalculateDynamicValues();

    private:
	double service_time() const { return _phase.serviceTime(); }
	double think_time() const { return _phase.thinkTime(); }
	double n_calls() const { return _phase.numberOfSlices(); }
	double cv_sqr() const { return _phase.CV_sqr(); }

	static void initWait( DeviceInfo * device ) { device->call()->initWait(); }
	static std::set<Entity *>& add_server( std::set<Entity *>&, const DeviceInfo * );
	
	struct add_wait {
	    add_wait( unsigned int submodel ) : _submodel(submodel) {}
	    double operator()( double sum, const DeviceInfo * call ) const;
	private:
	    const unsigned int _submodel;
	};

    private:
	const Phase& _phase;
	const std::string _name;
	const Type _type;
	DeviceEntry * _entry;
	ProcessorCall * _call;
	LQIO::DOM::Entry * _entry_dom;		/* Only for ~Phase		*/
	LQIO::DOM::Call * _call_dom;		/* Only for ~Phase		*/
    };

    struct follow_interlock {
	follow_interlock( Interlock::CollectTable& path ) : _path(path) {}
	void operator()( const Call * call ) const;
	void operator()( const DeviceInfo* device ) const;
    private:
	Interlock::CollectTable& _path;
    };
    
    struct get_interlocked_tasks {
	get_interlocked_tasks( Interlock::CollectTasks& path ) : _path(path) {}
	bool operator()( bool rc, Call * call ) const;
	bool operator()( bool rc, const DeviceInfo* device ) const;
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
    Phase( const std::string& = "" );
    Phase( const Phase&, unsigned int );
    virtual ~Phase();

    /* Initialialization */
	
    Phase& expandCalls();
    virtual Phase& initProcessor();
    void initCFSProcessor();
#if PAN_REPLICATION
    Phase& initReplication( const unsigned );
    Phase& resetReplication();
#endif
    Phase& initWait();
    Phase& initVariance();
    
    unsigned findChildren( Call::stack&, const bool ) const;
    virtual const Phase& followInterlock( Interlock::CollectTable&  ) const;
    virtual void callsPerform( const CallExec& ) const;

    Phase& setInterlockedFlow( const MVASubmodel& );
    void setInterlockedCall(const unsigned submodel);
    void addSrcCall( Call * aCall ) { _callList.insert(aCall); }
    void removeSrcCall( Call *aCall ) { _callList.erase(aCall); }
    virtual unsigned int getReplicaNumber() const;

    /* Instance Variable access */
	
    LQIO::DOM::Phase::Type phaseTypeFlag() const { return getDOM() ? getDOM()->getPhaseTypeFlag() : LQIO::DOM::Phase::Type::STOCHASTIC; }
    Phase& setEntry( const Entry * entry ) { _entry = entry; return *this; }
    const Entry * entry() const { return _entry; }
    virtual const Entity * owner() const;
    virtual double getMaxCustomers() const;	/* Proxy */
    Phase& setPrOvertaking( const Probability& pr_ot ) { _prOvertaking = pr_ot; return *this; }
    const Probability& prOvertaking() const { return _prOvertaking; }

    bool isDeterministic() const { return getDOM() ? getDOM()->getPhaseTypeFlag() == LQIO::DOM::Phase::Type::DETERMINISTIC : false; }
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
	
    DeviceInfo * getProcessor() const;
    ProcessorCall * processorCall() const;
    DeviceEntry * processorEntry() const;
    double processorCalls() const;
    double queueingTime() const;
    double processorWait() const;
    const std::vector<DeviceInfo *>& devices() const { return _devices; }
	
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
#if PAN_REPLICATION
    double waitExceptChain( const unsigned, const unsigned k );
#endif
    double computeVariance();	 			/* Computed variance.		*/
    Phase& updateWait( const Submodel&, const double ); 
    double getProcWait( unsigned int submodel, const double relax ); // tomari quorum
    double getTaskWait( unsigned int submodel, const double relax );
    double getRendezvous( unsigned int submodel, const double relax );
#if PAN_REPLICATION
    double updateWaitReplication( const Submodel& );
    double getReplicationProcWait( unsigned int submodel, const double relax );
    double getReplicationTaskWait( unsigned int submodel, const double relax ); //tomari quorum
    double getReplicationRendezvous( unsigned int submodel, const double relax );
#endif
    virtual bool getInterlockedTasks( Interlock::CollectTasks& path ) const;
    Phase& updateInterlockedWait( const Submodel& aSubmodel, const double relax );

    /* recalculation of dynamic values */
	
    virtual Phase& recalculateDynamicValues();
//    void cfsRecalculateDynamicValues(double ratio1, double newthinktime);

    /*+ DPS +*/
    Phase::DeviceInfo * getCFSDelayServer() const;
    ProcessorCall * getCFSCall() const;
    DeviceEntry * getCFSEntry() const;
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
    virtual ProcessorCall * newProcessorCall( Entry * procEntry ) const;

private:
    Phase const& addForwardingRendezvous( Call::stack& callStack ) const;
    Phase& forwardedRendezvous( const Call * fwdCall, const double value );
    double sumOfRendezvous() const;
#if PAN_REPLICATION
    double nrFactor( const Call * aCall, const Submodel& aSubmodel ) const;
#endif
    double mol_phase() const;
    double stochastic_phase() const;
    double deterministic_phase() const;
    double random_phase() const;

    double cfsThinkTime( double groupRatio );

	
protected:
    const Entry * _entry;		/* Root for activity			*/
    std::set<Call *> _callList;         /* Who I call.                          */

private:
    std::vector<DeviceInfo *> _devices;	/* Will replace below			*/
#if PAN_REPLICATION
    VectorMath<double> _surrogateDelay;	/* Saved old surrogate delay. REP N-R	*/
#endif
    Probability _prOvertaking;

    double _cfs_delay;
    double _cfs_delay_upperbound;
    double _cfs_delay_lowerbound;
};
#endif
