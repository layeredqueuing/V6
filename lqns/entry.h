/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/lqns/entry.h $
 *
 * Everything you wanted to know about an entry, but were afraid to ask.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 *
 * $Id: entry.h 14118 2020-11-23 17:30:44Z greg $
 *
 * ------------------------------------------------------------------------
 */

#if	!defined(ENTRY_H)
#define ENTRY_H

#include "dim.h"
#include <lqio/input.h>
#include <lqio/dom_entry.h>
#include <set>
#include <vector>
#include "prob.h"
#include "call.h"
#include "vector.h"
#include "phase.h"
#include "activity.h"
#include "interlock.h"
#include "slice.h"

namespace LQIO {
    namespace DOM {
        class Call;
	class Phase;
    };
}

class Activity;
class Call;
class Entity;
class Entry;
class Exponential;
class Processor;
class Submodel;
class MVASubmodel;
class Task;
typedef Vector<unsigned> ChainVector;

typedef enum { NOT_CALLED, RENDEZVOUS_REQUEST, SEND_NO_REPLY_REQUEST, OPEN_ARRIVAL_REQUEST } requesting_type;

/* */

/*
 * Interface to parser.
 */


class CallInfoItem {
public:
    CallInfoItem( const Entry * src, const Entry * dst );
	
    bool hasRendezvous() const;
    bool hasSendNoReply() const;
    bool hasForwarding() const;
		
    bool isTaskCall() const;
    bool isProcessorCall() const;
	
    const Entry * srcEntry() const { return source; }
    const Entry * dstEntry() const { return destination; }

public:
    const Call * phase[MAX_PHASES+1];

private:
    const Entry * source; 
    const Entry * destination; 
};


class CallInfo {
    struct compare
    {
	compare( const Entry* dstEntry ) : _dstEntry(dstEntry) {}
	bool operator()( CallInfoItem& i ) { return i.dstEntry() == _dstEntry; }
    private:
	const Entry * _dstEntry;
    };
public:
    CallInfo( const Entry& anEntry, const unsigned );
    CallInfo( const CallInfo& ) { abort(); }					/* Copying is verbotten */
    CallInfo& operator=( const CallInfo& ) { abort(); return *this; }		/* Copying is verbotten */
	
    std::vector<CallInfoItem>::const_iterator begin() const { return _calls.begin(); }
    std::vector<CallInfoItem>::const_iterator end() const { return _calls.end(); }
    unsigned size() const { return _calls.size(); }

private:
    std::vector<CallInfoItem> _calls;
};

/* -------------------- Nodes in the graph are... --------------------- */

class Entry 
{
    friend class Interlock;		/* To access interlock */
    friend class Activity;		/* To access phase[].wait */
    friend class ActivityList;		/* To access phase[].wait */
    friend class OrForkActivityList;	/* To access phase[].wait */
    friend class AndForkActivityList;	/* To access phase[].wait */
    friend class RepeatActivityList;	/* To access phase[].wait */
    friend class Entity;		/* To access phase */

public:
    class CallExec {
    public:
	CallExec( callFunc f, unsigned submodel, unsigned k=0 ) : _f(f), _submodel(submodel), _k(k)  {}
	void operator()( Entry * object ) const { object->callsPerform( _f, _submodel, _k ); }
	
	const callFunc f() const { return _f; }
	unsigned submodel() const { return _submodel; }
	unsigned k() const { return _k; }

    private:
	const callFunc _f;
	const unsigned _submodel;
	const unsigned _k;
    };

    /*
     * Used to run f over all entries (and threads).  Each thread has it's own chain.
     */
    
    class CallExecWithChain {
    public:
	CallExecWithChain( callFunc f, unsigned submodel, const ChainVector& chains, unsigned int i ) : _f(f), _submodel(submodel), _chains(chains), _i(i)  {}
	void operator()( Entry * object ) const { object->callsPerform( _f, _submodel, _chains[_i] ); _i += 1; }
	
	const callFunc f() const { return _f; }
	unsigned submodel() const { return _submodel; }
	unsigned int index() const { return _i; }

    private:
	const callFunc _f;
	const unsigned _submodel;
	const ChainVector& _chains;
	mutable unsigned int _i;
    };

    struct get_servers {
	get_servers( std::set<Entity *>& servers ) : _servers(servers) {}
	void operator()( const Entry * entry ) const;
    private:
	std::set<Entity *>& _servers;
    };

    /*+ interlock +*/
    struct add_max_customers {
	add_max_customers( const Entry * server_entry ) : _server_entry(server_entry) {}
	double operator()( double sum, const Entry * entry ) { return _server_entry->isCalledBy(entry) ? sum + entry->getMaxCustomers() : sum; }
    private:
	const Entry * _server_entry;
    };
    
    struct set_real_customers {
	set_real_customers( const MVASubmodel& submodel, const Entity * server, unsigned int k ) : _submodel(submodel), _server(server),  _k(k) {}
	void operator()( const Entry * entry ) const;
    private:
	const MVASubmodel& _submodel;
	const Entity * _server;
	const unsigned int _k;
    };

    struct add_interlock {
	add_interlock( const MVASubmodel& submodel, const Task * client, unsigned int k ) : _submodel(submodel), _client(client), _k(k) {}
	double operator()( double sum, const Entry * entry ) const { return sum + entry->setInterlock( _submodel, _client, _k ); }
    private:
	const MVASubmodel& _submodel;
	const Task * _client;
	const unsigned int _k;
    };

    struct set_interlock_PrUpper {
	set_interlock_PrUpper( const MVASubmodel& submodel, Server * station, unsigned int k ) : _submodel(submodel), _station(station), _k(k) {}
	void operator()( const Entry * entry ) const;
    private:
	const MVASubmodel& _submodel;
	Server * _station;
	const unsigned int _k;
    };
    /*- interlock -*/

protected:
    struct clear_wait {
	clear_wait( unsigned int submodel ) : _submodel(submodel) {}
	void operator()( const Phase& phase ) const { phase._wait[_submodel] = 0.0; }
    private:
	const unsigned int _submodel;
    };

    struct add_wait {
	add_wait( unsigned int submodel ) : _submodel(submodel) {}
	double operator()( double sum, const Phase& phase ) const { return sum + phase._wait[_submodel]; }
    private:
	const unsigned int _submodel;
    };

private:
    struct get_clients {
	get_clients( std::set<Task *>& clients ) : _clients(clients) {}
	void operator()( const Entry * entry ) const;
    private:
	std::set<Task *>& _clients;
    };

    /*+ interlock +*/
    struct add_PrIL_se {
	add_PrIL_se( const MVASubmodel& submodel, const Entry * serverEntry ) : _submodel(submodel), _serverEntry(serverEntry) {}
	double operator()( double sum, const Entry * client ) const;
    private:
	const MVASubmodel& _submodel;
	const Entry * _serverEntry;
    };
    /*- interlock -*/

public:
    static bool joinsPresent;
    static bool deterministicPhases;
    static unsigned totalOpenArrivals;
    static unsigned max_phases;		/* maximum phase encountered.	*/
    static const char * phaseTypeFlagStr [];
	
    int operator==( const Entry& anEntry ) const;
    static void reset();
    static Entry * find( const string& entry_name );
    static Entry * create( LQIO::DOM::Entry* domEntry, unsigned int );
    static bool max_phase( const Entry * e1, const Entry * e2 ) { return e1->maxPhase() < e2->maxPhase(); }
	
protected:
    /* Instance creation */

    Entry( LQIO::DOM::Entry* aDomEntry, const unsigned, const unsigned );

public:
    virtual ~Entry();

private:
    Entry( const Entry& );
    Entry& operator=( const Entry& );

public:
    bool check() const;
    virtual Entry& configure( const unsigned );
    unsigned findChildren( Call::stack&, const bool ) const;
    virtual Entry& initProcessor() = 0;
    virtual Entry& initWait() = 0;
    Entry& initThroughputBound();
    Entry& initServiceTime();
    Entry& initReplication( const unsigned );	// REPL
    Entry& resetInterlock();
    Entry& createInterlock();
    Entry& initInterlock( Interlock::CollectTable& path );

    /* Instance Variable access */

    unsigned int index() const { return _index; }
    unsigned int entryId() const { return _entryId; }
    double openArrivalRate() const;
    double computeCV_sqr() const;
    int priority() const;
    bool setIsCalledBy( const requesting_type callType );
    bool isCalledUsing( const requesting_type callType ) const { return callType == _calledBy; }
    bool isCalled() const { return _calledBy != NOT_CALLED; }
    Entry& setEntryInformation( LQIO::DOM::Entry * entryInfo );
    virtual Entry& setDOM( unsigned phase, LQIO::DOM::Phase* phaseInfo );
    Entry& setForwardingInformation( Entry* toEntry, LQIO::DOM::Call *);
    Entry& addServiceTime( const unsigned, const double );
    double serviceTimeForPhase( const unsigned int p ) const { return _phase[p].serviceTime(); }
    double serviceTime() const { return _total.serviceTime(); }
    double throughput() const { return _throughput; }
    double throughputBound() const { return _throughputBound; }
    Entry& rendezvous( Entry *, const unsigned, const LQIO::DOM::Call* callDOMInfo );
    double rendezvous( const Entry * ) const;
    const Entry& rendezvous( const Entity *, VectorMath<double>& ) const;
    Entry& sendNoReply( Entry *, const unsigned, const LQIO::DOM::Call* callDOMInfo );
    double sendNoReply( const Entry * ) const;
    double sumOfSendNoReply( const unsigned p ) const;
    Entry& forward( Entry *, const LQIO::DOM::Call* callDOMInfo  );
    double forward( const Entry * anEntry ) const { return _phase[1].forward( anEntry ); }
    virtual Entry& setStartActivity( Activity * );
    virtual double processorCalls( const unsigned ) const = 0;
    double processorCalls() const; 
    bool phaseIsPresent( const unsigned p ) const { return _phase[p].isPresent(); }
    virtual double openWait() const { return 0.; }
    LQIO::DOM::Entry* getDOM() const { return _dom; }
    Entry& resetReplication();
	
    void addDstCall( Call * aCall ) { _callerList.insert(aCall); }
    void removeDstCall( Call *aCall ) { _callerList.erase(aCall); }
    unsigned callerListSize() const { return _callerList.size(); }
    const std::set<Call *>& callerList() const { return _callerList; }
    const std::set<Call *>& callList(unsigned p) const { return _phase[p].callList(); }
    Call * processorCall(unsigned p) const { return _phase[p].processorCall(); }

    /* Queries */

    const std::string& name() const { return getDOM()->getName(); }
    virtual const Entity * owner() const = 0;
    virtual Entry& owner( const Entity * ) = 0;
	
    virtual bool isTaskEntry() const { return false; }
    virtual bool isVirtualEntry() const { return false; }
    virtual bool isProcessorEntry() const { return false; }
    bool isActivityEntry() const { return _entryType == LQIO::DOM::Entry::Type::ACTIVITY; }
    bool isStandardEntry() const { return _entryType == LQIO::DOM::Entry::Type::STANDARD; }
    bool isSignalEntry() const { return _semaphoreType == LQIO::DOM::Entry::Semaphore::SIGNAL; }
    bool isWaitEntry() const { return _semaphoreType == LQIO::DOM::Entry::Semaphore::WAIT; }
    bool isInterlocked( const Entry * dstEntry) const { return _interlock[dstEntry->entryId()].all > 0.0; }
    bool isInterlockedFrom( const Entry * srcEntry ) const { return srcEntry->isInterlocked(this); }
    bool isReferenceTaskEntry() const;
	
    bool hasDeterministicPhases() const { return getDOM()->hasDeterministicPhases(); }
    bool hasNonExponentialPhases() const { return getDOM()->hasNonExponentialPhases(); }
    bool hasThinkTime() const { return getDOM()->hasThinkTime(); }
    bool hasVariance() const;
    bool hasStartActivity() const { return _startActivity != nullptr; }
    bool hasOpenArrivals() const { return getDOM()->hasOpenArrivalRate(); }
    bool hasOvertaking() const { return maxPhase() > 1 /*&&( !(owner()->isReferenceTask()))*/; }
		
    bool entryTypeOk( const LQIO::DOM::Entry::Type );
    bool entrySemaphoreTypeOk( const LQIO::DOM::Entry::Semaphore aType );
    unsigned maxPhase() const { return _phase.size(); }
    unsigned concurrentThreads() const;
    std::set<Entity *>& getServers( const std::set<Entity *>& ) const;	// Called tasks/processors

    virtual double waitExcept( const unsigned, const unsigned, const unsigned ) const;	/* For client service times */
    virtual double waitExceptChain( const unsigned, const unsigned, const unsigned ) const; //REP N-R
    double getProcWait( const unsigned p, int submodel )  { return _phase[p].getProcWait(submodel, 0) ;}	

    double elapsedTime() const { return _total.elapsedTime(); }			/* Found through deltaWait  */
    double elapsedTimeForPhase( const unsigned int p) const { return _phase[p].elapsedTime(); }	/* For server service times */

    double varianceForPhase( const unsigned int p ) const { return _phase[p].variance(); }
    double variance() const { return _total.variance(); }
	
    double utilization() const;
    virtual double processorUtilization() const = 0;
    virtual double queueingTime( const unsigned ) const = 0;	// Time queued for processor.
    Probability prVisit() const;

    virtual double getStartTime() const { return 0.0; }
    virtual double getStartTimeVariance() const { return 0.0; }

    /* Computation */

    void add_call( const unsigned p, const LQIO::DOM::Call* call );
    void sliceTime( const Entry& dst, Slice_Info phase_info[], double y_xj[] ) const;
    virtual Entry& computeVariance() { return *this; }
    virtual Entry& updateWait( const Submodel&, const double ) = 0;
    virtual double updateWaitReplication( const Submodel&, unsigned& ) = 0;
    virtual Entry& saveOpenWait( const double aWait ) = 0;
    Entry& saveThroughput( double );
    Entry& setPrOvertaking( const unsigned p, const Probability& pr ) { _phase[p].setPrOvertaking(pr); return *this; }
    const Probability& getPrOvertaking( const unsigned p ) const { return _phase[p].prOvertaking(); }

    void set( const Entry * src, const Activity::Collect& );
    Entry& aggregate( const unsigned, const unsigned p, const Exponential& );
    Entry& aggregateReplication( const Vector< VectorMath<double> >& );

    const Entry& callsPerform( callFunc aFunc, const unsigned submodel, const unsigned k ) const;

    /* Dynamic Updates / Late Finalization */
    /* In order to integrate LQX's support for model changes we need to have a way  */
    /* of re-calculating what used to be static for all dynamically editable values */
	
    Entry& recalculateDynamicValues();
	
    /* DPS */
    Entry& computeCFSDelay(double ratio1, double ratio2);
    double getCFSDelay() const;
    Entry& reset_lowerbound();

    /* Interlock */
    
    const Entry& followInterlock( Interlock::CollectTable& ) const;
    bool getInterlockedTasks( Interlock::CollectTasks& ) const;
    bool isCalledBy( const Entry * src_entry ) const; 
    double ILWait() const;
    double rateOfUtil() const;

    Entry& updateILWait( const Submodel& aSubmodel, const double relax );
    Entry& setInterlockedFlow( const MVASubmodel& );
    double getInterlockPr( const MVASubmodel&, const Entity * ) const;
    bool hasILWait(const unsigned submodel) const { return _total._interlockedWait[submodel] > 0.0; } 
    double getILWait(unsigned submodel) const;
    double getILWait(unsigned submodel, const unsigned p) const;
    double getILQueueLength() const;

    Call* getCall(const unsigned k ) const;

//    double getPhase2(const Entry * serverEntry) const;
    bool hasPath(const Entry * dstEntry) const { return _interlock[dstEntry->entryId()]>0.0;}

    void saveMaxCustomers(double, bool); 
    void setMaxCustomers(double nCusts) { _maxCusts = nCusts; }
    const Entry& setMaxCustomersForChain( unsigned int ) const;
    double getMaxCustomers() const { return _maxCusts;}

    unsigned getPhaseNum(const Phase * aPhase) const;

    double setInterlock( const MVASubmodel&, const Task * client, unsigned k ) const;

    /* Sanity checks */

    bool checkDroppedCalls() const;

    /* XML output */
 
    void insertDOMQueueingTime(void) const;
    const Entry& insertDOMResults(double *phaseUtils) const;

    /* Printing */

    std::ostream& printSubmodelWait( std::ostream& output, unsigned offset ) const;

protected:
    Entry& setMaxPhase( const unsigned phase );

private:
    Entry& sanityCheckParameters();
    void setThroughput( const double throughput ) { _throughput = throughput; }

    /* Interlock */
    bool isSendingTask( const MVASubmodel& submodel ) const;
    unsigned callingTo1(const Entry * dst_entry ) const;
    double fractionUtilizationTo(const Entry * dst_entry ) const;

protected:
    LQIO::DOM::Entry* _dom;	
    Vector<Phase> _phase;
    NullPhase _total;
    double _nextOpenWait;			/* copy for delta computation	*/
    double _maxCusts;				/* maximum possible number of customers*/
    /* Activity Entries */
	
    Activity * _startActivity;			/* Starting activity.		*/

private:
    const unsigned _entryId;			/* Gobal entry id. (for chain)	*/
    const unsigned short _index;		/* My index (for mva)		*/
    LQIO::DOM::Entry::Type _entryType;
    LQIO::DOM::Entry::Semaphore _semaphoreType;	/* Extra type information	*/
    requesting_type _calledBy;			/* true if entry referenced.	*/
    double _throughput;				/* Computed throughput.		*/
    double _throughputBound;			/* Type 1 throughput bound.	*/
	
    std::set<Call *> _callerList;		/* Who calls me.		*/

    Vector<InterlockInfo> _interlock;		/* Interlock table.		*/
};

/* --------------------------- Task Entries --------------------------- */


class TaskEntry : public Entry 
{
public:
    TaskEntry( LQIO::DOM::Entry* domEntry, const unsigned id, const unsigned int index ) : Entry(domEntry,id,index), _task(0), _openWait(0.), _nextOpenWait(0.) {}

    virtual TaskEntry& initProcessor();
    virtual TaskEntry& initWait();

    virtual TaskEntry& owner( const Entity * aTask ) { _task = aTask; return *this; }
    virtual const Entity * owner() const { return _task; }

    virtual bool isTaskEntry() const { return true; }

    virtual double processorCalls( const unsigned ) const;

    virtual double processorUtilization() const;
    virtual double queueingTime( const unsigned ) const;		// Time queued for processor.
    virtual TaskEntry& computeVariance();
    virtual TaskEntry& updateWait( const Submodel&, const double );
    virtual double updateWaitReplication( const Submodel&, unsigned& );
    virtual TaskEntry& saveOpenWait( const double aWait ) { _nextOpenWait = aWait; return *this; }
    virtual double openWait() const { return _openWait; }
    
private:
    const Entity * _task;			/* My task.			*/
    double _openWait;				/* Computed open response time.	*/
    double _nextOpenWait;			/* copy for delta computation	*/
};

/* -------------------------- Device Entries -------------------------- */

class DeviceEntry : public Entry 
{
public:
    DeviceEntry( LQIO::DOM::Entry* domEntry, const unsigned, Processor * );
    virtual ~DeviceEntry();

    virtual DeviceEntry& initProcessor();
    virtual DeviceEntry& initWait();
    DeviceEntry& initVariance();

    virtual DeviceEntry& owner( const Entity * aProcessor );
    virtual const Entity * owner() const { return myProcessor; }

    DeviceEntry& setServiceTime( const double );
    DeviceEntry& setPriority( const int );
    DeviceEntry& setCV_sqr( const double );

    virtual double processorCalls( const unsigned ) const;

    virtual bool isProcessorEntry() const { return true; }

    virtual double processorUtilization() const;
    virtual double queueingTime( const unsigned ) const;		// Time queued for processor.
    virtual DeviceEntry& updateWait( const Submodel&, const double );
    virtual DeviceEntry& saveOpenWait( const double aWait ) { return *this; }
    virtual double updateWaitReplication( const Submodel&, unsigned& );

private:
    const Entity * myProcessor;
};

/* ------------------------- Virtual Entries -------------------------- */

class VirtualEntry : public TaskEntry 
{
public:
    VirtualEntry( const Activity * anActivity );
    ~VirtualEntry();

    virtual bool isVirtualEntry() const { return true; }
    virtual Entry& setStartActivity( Activity * );

    virtual Call * processorCall( const unsigned ) const { return 0; }
};

void set_start_activity (Task* newTask, LQIO::DOM::Entry* targetEntry);

/* ------------------ Proxy messages for class call. ------------------ */


/*
 * Forward request to associated entry.  Defined here rather than in
 * class body due to foward reference problems.  Inlined.
 */

inline const Entity * Call::dstTask() const { return _destination->owner(); }
inline short Call::index() const { return _destination->index(); }
inline double Call::serviceTime() const { return _destination->serviceTimeForPhase(1); }
#endif
