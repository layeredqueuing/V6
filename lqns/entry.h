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
 * $Id: entry.h 16978 2024-01-29 21:31:31Z greg $
 *
 * ------------------------------------------------------------------------
 */

#ifndef LQNS_ENTRY_H
#define LQNS_ENTRY_H

//#define BUG_425 1

#include <set>
#include <vector>
#include <lqio/dom_entry.h>
#include <mva/prob.h>
#include <mva/vector.h>
#include "call.h"
#include "activity.h"
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
class MVASubmodel;
class Model;
class Model;
class Processor;
class Server;
class Submodel;
class Task;
typedef Vector<unsigned> ChainVector;

/* -------------------- Nodes in the graph are... --------------------- */

class Entry 
{
    friend class Interlock;		/* To access interlock */
    friend class Activity;		/* To access phase[].wait */
    friend class ActivityList;		/* To access phase[].wait */
    friend class OrForkActivityList;	/* To access phase[].wait */
    friend class AndForkActivityList;	/* To access phase[].wait */
    friend class RepeatActivityList;	/* To access phase[].wait */
    friend class CallInfo;

public:
    enum class RequestType { NOT_CALLED, RENDEZVOUS, SEND_NO_REPLY, OPEN_ARRIVAL };

    
    class CallsPerform {
    public:
	CallsPerform( Call::Perform& g ) : _g(g)  {}
	void operator()( Entry * object ) const { object->callsPerform( _g ); }
	
    private:
	Call::Perform& _g;
    };

    /*
     * Used to run f over all entries (and threads).  Each thread has it's own chain.
     */
    
    class CallsPerformWithChain {
    public:
	CallsPerformWithChain( Call::Perform& g, const ChainVector& chains, unsigned int i ) : _g(g), _chains(chains), _i(i)  {}

	void operator()( Entry * object );
	unsigned int index() const { return _i; }

    private:
	Call::Perform& _g;
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
    
    class SaveServerResults {
    public:
	SaveServerResults( const MVASubmodel& submodel, const Server& station, const Entity& server ) : _submodel(submodel), _station(station), _server(server) {}
	void operator()( Entry * entry ) const;
    private:
	const MVASubmodel& _submodel;
	const Server& _station;			/* May be the base if replicated	*/
	const Entity& _server;			/* The base or replica			*/
    };

    class SaveClientResults {
    public:
	SaveClientResults( const MVASubmodel& submodel, const Server& station, unsigned int chain, const Task& client ) : _submodel(submodel), _station(station), _k(chain), _client(client) {}
	void operator()( Entry * entry ) const;
    private:
	const MVASubmodel& _submodel;
	const Server& _station;			/* May be the base if replicated	*/
	const unsigned int _k;
	const Task& _client;			/* The base or replica			*/
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

    struct max_depth
    {
	typedef unsigned int (Phase::*funcPtr)( Call::stack&, bool ) const;
	max_depth( funcPtr f, Call::stack& arg1, bool arg2 ) : _f(f), _arg1(arg1), _arg2(arg2) {}
	unsigned int operator()( unsigned int l, const Phase* r ) const { return std::max( l, (r->*_f)(_arg1,_arg2) ); }
	unsigned int operator()( unsigned int l, const Phase& r ) const { return std::max( l, (r.*_f)(_arg1,_arg2) ); }
    private:
	const funcPtr _f;
	Call::stack& _arg1;
	bool _arg2;
    };

private:
    class SRVNManip {
    public:
	SRVNManip( std::ostream& (*f)( std::ostream&, const Entry& ), const Entry& entry ) : _f(f), _entry(entry) {}
    private:
	std::ostream& (*_f)( std::ostream&, const Entry& );
	const Entry & _entry;

	friend std::ostream& operator<<( std::ostream& os, const SRVNManip& m ) { return m._f(os,m._entry); }
    };


    struct print_call {
	print_call( std::ostream& output, unsigned int submodel, const std::string& arrow ) : _output(output), _submodel(submodel), _arrow(arrow) {}
	void operator()( const CallInfo::Item& call ) const;
    private:
	std::ostream& _output;
	const unsigned int _submodel;
	const std::string& _arrow;
    };

public:
    struct get_clients {
	get_clients( std::set<Task *>& clients ) : _clients(clients) {}
	void operator()( const Entry * entry ) const;
    private:
	std::set<Task *>& _clients;
    };

private:
    /*+ interlock +*/
    struct add_PrIL_se {
	add_PrIL_se( const MVASubmodel& submodel, const Entry * serverEntry ) : _submodel(submodel), _serverEntry(serverEntry) {}
	double operator()( double sum, const Entry * client ) const;
    private:
	const MVASubmodel& _submodel;
	const Entry * _serverEntry;
    };

public:
    struct has_IL_wait {
	has_IL_wait( unsigned int submodel ) : _submodel( submodel ) {}
	bool operator()( const Entry * entry ) { return entry->_total._interlockedWait[_submodel] > 0.0; } 
    private:
	unsigned int _submodel;
    };

    struct is_interlocked_from
    {
	is_interlocked_from( const Entry * from ) : _from(from) {}
	bool operator()( const Entry * entry ) const { return _from->isInterlocked(entry); }
    private:
	const Entry *_from;
    };

    struct exec {
	typedef void (Entry::*funcPtr)( unsigned int ) const;
	exec( const funcPtr f, unsigned int k ) : _f(f), _k(k) {}
	void operator()( const Entry * e ) { (e->*_f)( _k ); }
    private:
	const funcPtr _f;
	const unsigned int _k;
    };
    /*- interlock -*/

    struct computeCFSDelay {
	computeCFSDelay(double ratio1, double ratio2) : _ratio1(ratio1), _ratio2(ratio2) {}
	void operator()( const Entry * entry );
    private:
	double _ratio1;
	double _ratio2;
    };
    

public:
    static bool joinsPresent;
    static bool deterministicPhases;
    static unsigned max_phases;		/* maximum phase encountered.	*/
	
    int operator==( const Entry& anEntry ) const;
    static void reset();
    static Entry * find( const std::string&, unsigned int=1 );
    static Entry * create( LQIO::DOM::Entry* domEntry, unsigned int );
    static bool max_phase( const Entry * e1, const Entry * e2 ) { return e1->maxPhase() < e2->maxPhase(); }
    static double add_visit_probability( double sum, const Entry* entry ) { return sum + entry->prVisit(); }

protected:
    /* Instance creation */

    Entry( LQIO::DOM::Entry *, unsigned int, bool=true );
    Entry( const Entry&, unsigned int replica );

private:
    Entry& operator=( const Entry& ) = delete;

public:
    virtual ~Entry();
    virtual Entry * clone( unsigned int, const AndOrForkActivityList * fork=nullptr ) const = 0;

public:
    bool check() const;
    virtual Entry& configure( const unsigned );
    Entry& expand();
    Entry& expandCalls();
    unsigned findChildren( Call::stack&, const bool ) const;
    Entry& initCustomers( std::deque<const Task *>& stack, unsigned int customers );
    virtual Entry& initProcessor() = 0;
    Entry& initServiceTime();
#if PAN_REPLICATION
    Entry& setSurrogateDelaySize( size_t );
#endif
    Entry& resetInterlock();
    Entry& createInterlock();
    void initializeInterlock( Interlock::CollectTable& path );

    /* Instance Variable access */

    unsigned int index() const { return _index; }
    unsigned int entryId() const { return _entryId; }
    const Phase& getPhase( unsigned int p ) const { return _phase[p]; }
    LQIO::DOM::Phase::Type phaseTypeFlag( const unsigned p ) const { return _phase[p].phaseTypeFlag(); }
    double openArrivalRate() const;
    double computeCV_sqr() const;
    int priority() const;
    bool setIsCalledBy( const RequestType callType );
    bool isCalledUsingRendezvous() const { return _calledBy == RequestType::RENDEZVOUS; }
    bool isCalledUsingSendNoReply() const { return _calledBy == RequestType::SEND_NO_REPLY; }
    bool isCalledUsingOpenArrival() const { return _calledBy == RequestType::OPEN_ARRIVAL; }
    bool isCalled() const { return _calledBy != RequestType::NOT_CALLED; }
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
    const Activity * getStartActivity() const { return _startActivity; }
    Entry& setStartActivity( Activity * );
    virtual double processorCalls( const unsigned ) const = 0;
    double processorCalls() const; 
    bool phaseIsPresent( const unsigned p ) const { return _phase[p].isPresent(); }
    virtual double openWait() const { return 0.; }
    LQIO::DOM::Entry* getDOM() const { return _dom; }
#if PAN_REPLICATION
    Entry& clearSurrogateDelay();
#endif
    unsigned getReplicaNumber() const { return _replica_number; }
	
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
	
    bool hasDeterministicPhases() const { return getDOM()->hasDeterministicPhases(); }
    bool hasNonExponentialPhases() const { return getDOM()->hasNonExponentialPhases(); }
    bool hasThinkTime() const { return getDOM()->hasThinkTime(); }
    bool hasVariance() const;
    bool hasStartActivity() const { return _startActivity != nullptr; }
    bool hasOpenArrivals() const { return getDOM()->hasOpenArrivalRate(); }
    bool hasOvertaking() const { return maxPhase() > 1 /*&&( !(owner()->isReferenceTask()))*/; }
    bool hasVisitProbability() const { return getDOM()->hasVisitProbability(); }
		
    bool entryTypeOk( const LQIO::DOM::Entry::Type );
    bool entrySemaphoreTypeOk( const LQIO::DOM::Entry::Semaphore aType );
    unsigned maxPhase() const { return _phase.size(); }
    unsigned concurrentThreads() const;
    std::set<Entity *>& getServers( const std::set<Entity *>& ) const;	// Called tasks/processors

    virtual double waitExcept( const unsigned, const unsigned, const unsigned ) const;	/* For client service times */
#if PAN_REPLICATION
    virtual double waitExceptChain( const unsigned, const unsigned, const unsigned ) const; //REP N-R
#endif
//    double waitTime( unsigned int submodel ) { return _total.getWaitTimey(submodel); }
    double getProcWait( const unsigned p, int submodel )  { return _phase[p].getProcWait( submodel ); }	

    double residenceTime() const { return _total.residenceTime(); }			/* Found through deltaWait  */
    double residenceTimeForPhase( const unsigned int p ) const { return _phase[p].residenceTime(); }	/* For server service times */

    double varianceForPhase( const unsigned int p ) const { return _phase[p].variance(); }
    double variance() const { return _total.variance(); }
	
    double utilization() const;
    virtual double processorUtilization() const = 0;
    virtual double queueingTime( const unsigned ) const = 0;	// Time queued for processor.
    Probability prVisit() const;

    virtual double getStartTime() const { return 0.0; }
    virtual double getStartTimeVariance() const { return 0.0; }

    /* Computation */

    void add_call( unsigned p, const LQIO::DOM::Call * dom );
    void sliceTime( const Entry& dst, Slice_Info phase_info[], double y_xj[] ) const;
    void computeThroughputBound();
    virtual Entry& computeVariance() { return *this; }
    virtual Entry& updateWait( const Submodel&, const double ) = 0;
#if PAN_REPLICATION
    virtual double updateWaitReplication( const Submodel&, unsigned& ) = 0;
#endif
    Entry& saveClientResults( const MVASubmodel& submodel, const Server& station, unsigned int k );
    virtual Entry& saveOpenWait( const double aWait ) = 0;
    Entry& saveThroughput( double );
    Entry& setPrOvertaking( const unsigned p, const Probability& pr ) { _phase[p].setPrOvertaking(pr); return *this; }
    const Probability& getPrOvertaking( const unsigned p ) const { return _phase[p].prOvertaking(); }

    void set( const Entry * src, const Activity::Collect& );
    Entry& aggregate( const unsigned, const unsigned p, const Exponential& );
#if PAN_REPLICATION
    Entry& aggregateReplication( const Vector< VectorMath<double> >& );
#endif

    const Entry& callsPerform( Call::Perform& ) const;

    /* Dynamic Updates / Late Finalization */
    /* In order to integrate LQX's support for model changes we need to have a way  */
    /* of re-calculating what used to be static for all dynamically editable values */
	
    void recalculateDynamicValues();
	
    /* DPS */
    double getCFSDelay() const;
    Entry& reset_lowerbound();

    /* Interlock */
    
    const Entry& followInterlock( Interlock::CollectTable& ) const;
    bool getInterlockedTasks( Interlock::CollectTasks& ) const;
    bool isCalledBy( const Entry * src_entry ) const; 
    double ILWait() const;
    double rateOfUtil() const;

    Entry& updateILWait( const Submodel& aSubtasmodel, const double relax );
    Entry& setInterlockedFlow( const MVASubmodel& );
    double getInterlockPr( const MVASubmodel&, const Entity * ) const;
    double getILWait(unsigned submodel) const;
    double getILWait(unsigned submodel, const unsigned p) const;
    double getILQueueLength() const;

    Call* getCall(const unsigned k ) const;

//    double getPhase2(const Entry * serverEntry) const;

    /*+ interlock */
    void saveMaxCustomers(double, bool); 
    void setMaxCustomers(double nCusts) { _maxCusts = nCusts; }
    void setMaxCustomersForChain( unsigned int ) const;
    double getMaxCustomers() const { return _maxCusts;}
    double setInterlock( const MVASubmodel&, const Task * client, unsigned k ) const;
    /*- interlock */

    /* Sanity checks */

    bool checkDroppedCalls() const;

    /* XML output */
 
    void insertDOMQueueingTime(void) const;
    const Entry& insertDOMResults(double *phaseUtils) const;

    /* Printing */

    SRVNManip print_name() const { return SRVNManip( output_name, *this ); }
    std::ostream& printCalls( std::ostream& output, unsigned int submodel=0 ) const;
    std::ostream& printSubmodelWait( std::ostream& output, unsigned offset ) const;
    static std::string fold( const std::string& s1, const Entry * e2 );

protected:
    Entry& setMaxPhase( const unsigned phase );

private:
    Entry& sanityCheckParameters();
    void setThroughput( const double throughput ) { _throughput = throughput; }

    /* Interlock */
    bool isSendingTask( const MVASubmodel& submodel ) const;
    unsigned callingTo1(const Entry * dst_entry ) const;
    double fractionUtilizationTo(const Entry * dst_entry ) const;

    static std::ostream& output_name( std::ostream& output, const Entry& );

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
    RequestType _calledBy;			/* true if entry referenced.	*/
    double _throughput;				/* Computed throughput.		*/
    double _throughputBound;			/* Type 1 throughput bound.	*/
    Probability _visitProbability;		/* Computed visit probability	*/
	
    std::set<Call *> _callerList;		/* Who calls me.		*/

    Vector<InterlockInfo> _interlock;		/* Interlock table.		*/
    const unsigned int _replica_number;		/* > 1 if a replica		*/
};

/* --------------------------- Task Entries --------------------------- */


class TaskEntry : public Entry 
{
public:
    TaskEntry( LQIO::DOM::Entry* domEntry, unsigned int index, bool global=true );

protected:
    TaskEntry( const TaskEntry& src, unsigned int replica );
    
public:
    virtual Entry * clone( unsigned int replica, const AndOrForkActivityList * fork=nullptr ) const { return new TaskEntry( *this, replica ); }

    virtual TaskEntry& initProcessor();

    virtual TaskEntry& owner( const Entity * aTask ) { _task = aTask; return *this; }
    virtual const Entity * owner() const { return _task; }

    virtual bool isTaskEntry() const { return true; }

    virtual double processorCalls( const unsigned ) const;

    virtual double processorUtilization() const;
    virtual double queueingTime( const unsigned ) const;		// Time queued for processor.
    virtual TaskEntry& computeVariance();
    virtual TaskEntry& updateWait( const Submodel&, const double );
#if PAN_REPLICATION
    virtual double updateWaitReplication( const Submodel&, unsigned& );
#endif
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
    DeviceEntry( LQIO::DOM::Entry* domEntry, Processor * );

private:
    DeviceEntry( const DeviceEntry& src, unsigned int replica );

public:
    virtual ~DeviceEntry();
    virtual Entry * clone( unsigned int replica, const AndOrForkActivityList * fork=nullptr ) const { return new DeviceEntry( *this, replica ); }

    virtual DeviceEntry& initProcessor();
    virtual DeviceEntry& initWait();
    DeviceEntry& initVariance();

    virtual DeviceEntry& owner( const Entity * );
    virtual const Entity * owner() const { return _processor; }

    DeviceEntry& setServiceTime( const double );
    DeviceEntry& setPriority( const int );
    DeviceEntry& setCV_sqr( const double );

    virtual double processorCalls( const unsigned ) const;

    virtual bool isProcessorEntry() const { return true; }

    virtual double processorUtilization() const;
    virtual double queueingTime( const unsigned ) const;		// Time queued for processor.
    virtual DeviceEntry& updateWait( const Submodel&, const double );
    virtual DeviceEntry& saveOpenWait( const double aWait ) { return *this; }
#if PAN_REPLICATION
    virtual double updateWaitReplication( const Submodel&, unsigned& );
#endif

private:
    const Entity * _processor;
};

/* ------------------------- Virtual Entries -------------------------- */

/*
 * Used by class AndOrForkActivityList.
 */

class VirtualEntry : public TaskEntry 
{
public:
    VirtualEntry( const Activity * anActivity );

protected:
    VirtualEntry( const VirtualEntry& src, unsigned int replica ) : TaskEntry( src, replica ) {}

public:
    ~VirtualEntry();
    virtual Entry * clone( unsigned int replica, const AndOrForkActivityList * fork=nullptr ) const { return new VirtualEntry( *this, replica ); }

    virtual bool isVirtualEntry() const { return true; }
    virtual Call * processorCall( const unsigned ) const { return nullptr; }
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
