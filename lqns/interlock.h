/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk-V6/lqns/interlock.h $
 *
 * Layer-ization of model.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 *
 * $Id: interlock.h 16945 2024-01-26 13:02:36Z greg $
 *
 * ------------------------------------------------------------------------
 */

#ifndef LQNS_INTERLOCK_H
#define	LQNS_INTERLOCK_H

#include <deque>
#include <set>
#include <vector>
#include <lqio/dom_call.h>
#include <mva/vector.h>

class Call;
class Entry;
class Entity;
class Phase;
class Task;
class Probability;

struct InterlockInfo 
{
    InterlockInfo( float a = 0.0, float p = 0.0 ) : all(a), ph1(p) {}
    InterlockInfo& operator=( const InterlockInfo& );
    bool operator==( const InterlockInfo& ) const;
    InterlockInfo& reset() { all = 0.; ph1 = 0.; return *this; }

    float all;	/* Calls from all phases at root.	*/	
    float ph1;	/* Calls from phase 1 only at root.	*/
};

class CallInfo {
public:
    class Item {
    public:
	class collect_calls {
	    struct compare {
		compare( const Entry* dstEntry ) : _dstEntry(dstEntry) {}
		bool operator()( CallInfo::Item& ) const;
	    private:
		const Entry * _dstEntry;
	    };

	public:
	    collect_calls( std::vector<CallInfo::Item>& calls, const Entry& entry, LQIO::DOM::Call::Type type, unsigned int p=0 ) : _calls(calls), _srcEntry(entry), _type(type), _p(p) {}
	    collect_calls( const collect_calls& ) = default;
	    
	private:
	    collect_calls& operator=( const collect_calls& ) = delete;

	public:
	    void operator()( const Phase& );

	    unsigned int phase() const { return _p; }
	    void setPhase( unsigned int p ) { _p = p; }
	    const Entry& source() const { return _srcEntry; }
	private:
	    std::vector<CallInfo::Item>& _calls;
	    const Entry& _srcEntry;
	    const LQIO::DOM::Call::Type _type;
	    unsigned int _p;		/* set if chasing activities */
	};
    
    private:
	struct Predicate {
	    typedef bool (Call::*predicate)() const;
	    Predicate( const predicate p ) : _p(p) {}
	    bool operator()( const Call * call ) const { return call != nullptr && (call->*_p)(); }
	private:
	    const predicate _p;
	};
    
    public:
	Item( const Entry * src, const Entry * dst );
	
	bool hasRendezvous() const;
	bool hasSendNoReply() const;
	bool hasForwarding() const;
		
	bool isTaskCall() const;
	bool isProcessorCall() const;
    
	const Entry * srcEntry() const { return _source; }
	const Entry * dstEntry() const { return _destination; }

    public:
	Vector<const Call *> _phase;
    private:
	const Entry * _source; 
	const Entry * _destination; 
    };
public:
    CallInfo( const Entry& anEntry, LQIO::DOM::Call::Type );
    CallInfo( const CallInfo& ) { abort(); }					/* Copying is verbotten */
    CallInfo& operator=( const CallInfo& ) { abort(); return *this; }		/* Copying is verbotten */
	
    std::vector<CallInfo::Item>::const_iterator begin() const { return _calls.begin(); }
    std::vector<CallInfo::Item>::const_iterator end() const { return _calls.end(); }
    unsigned size() const { return _calls.size(); }

private:
    std::vector<CallInfo::Item> _calls;
};

/* --------------------------- Interlocker. --------------------------- */

class Interlock {

public:
    class CollectTasks {
    public:
	CollectTasks( const Entity& server, std::set<const Entity *>& interlockedTasks )
	    : _server(server),
	      _entryStack(),
	      _interlockedTasks( interlockedTasks )
	    {}

    private:
	CollectTasks( const CollectTasks& ) = delete;
	CollectTasks& operator=( const CollectTasks& ) = delete;

    public:
	const Entity * server() const { return &_server; }
	bool headOfPath() const { return _entryStack.size() == 0; }
	bool prune() const { return _entryStack.size() > 1; }		/* Allow from the top-of-path entry only */

	void push_back( const Entry * entry ) { _entryStack.push_back( entry ); }
	void pop_back() { _entryStack.pop_back(); }
	const Entry * back() const { return _entryStack.back(); }
	std::pair<std::set<const Entity *>::const_iterator,bool> insert( const Entity * entity ) { return _interlockedTasks.insert( entity ); }
	bool has_entry( const Entry * entry ) const;
	
    private:
	const Entity& _server;				/* In */
	std::deque<const Entry *> _entryStack;		/* local */
	std::set<const Entity *>& _interlockedTasks;	/* Out */
    };

    class CollectTable {
    public:
	CollectTable() : _entryStack(), _phase2(false), _calls(1.0,1.0) {}
	CollectTable( const CollectTable& src, bool phase2 ) : _entryStack(src._entryStack), _phase2(phase2), _calls(src._calls) {}
	CollectTable( const CollectTable& src, const InterlockInfo& calls ) : _entryStack(src._entryStack), _phase2(src._phase2), _calls(calls)
	    {
		if ( _phase2 ) _calls.ph1 = 0.0;
	    }

    private:
	CollectTable( const CollectTable& ) = delete;
	CollectTable& operator=( const CollectTable& ) = delete;

    public:
	bool prune() const { return _entryStack.size() > 1 && _phase2; }
	InterlockInfo& calls() { return _calls; }
	const InterlockInfo& calls() const { return _calls; }
	
	void push_back( const Entry * entry ) { _entryStack.push_back( entry ); }
	void pop_back() { _entryStack.pop_back(); }
	const Entry * front() const { return _entryStack.front(); }
	const Entry * back() const { return _entryStack.back(); }
	bool has_entry( const Entry * entry ) const;

    private:
	std::deque<const Entry *> _entryStack;		/* local */
	bool _phase2;					/* local */
	InterlockInfo _calls;				/* out */
    };

private:
    struct insert_if_via1 {
	insert_if_via1( const Interlock& interlock, const Entry * entry1, const Task * task1 ) : _interlock(interlock), _entry1(entry1), _task1(task1) {}
	std::set<const Entry *>& operator()( std::set<const Entry *>& set, const Entry * entry ) { if ( _interlock.via( entry, _entry1, _task1  ) ) set.insert(entry); return set; }
    private:
	const Interlock& _interlock;
	const Entry * _entry1;
	const Task * _task1;
    };

    struct is_via2 {
	is_via2( const Interlock& interlock, const Entry * entry2, const Task * task2 ) : _interlock(interlock), _entry2(entry2), _task2(task2) {}
	bool operator()( const Entry * entry );
    private:
	const Interlock& _interlock;
	const Entry * _entry2;
	const Task * _task2;
    };

    class flow_common {
    public:
	flow_common( const Interlock& interlock ) : _allSourceTasks(interlock._allSourceTasks), _ph2SourceTasks(interlock._ph2SourceTasks), _ph2(0.) {}
	double flow( const Entry * srcEntry, const Entry * dstEntry, bool any_phase=false ) const;
    private:
	const std::set<const Entity *>& _allSourceTasks;	/* Phase 1+ sources.		*/
	const std::set<const Entity *>& _ph2SourceTasks;	/* Phase 2+ sources.		*/
    public:
	mutable double _ph2;					/* For trace only */
    };
    
    class flow_src_dst : private flow_common {
    public:
	flow_src_dst( const Interlock& interlock, const Entry * srcEntry ) : flow_common(interlock), _srcEntry(srcEntry) {}
	double operator()( double, const Entry * dstEntry ) const;
    private:
	const Entry * _srcEntry;
    };

    class flow_dst_src : private flow_common {
    public:
	flow_dst_src( const Interlock& interlock, const Entry * dstEntry ) : flow_common(interlock), _dstEntry(dstEntry) {}
	double operator()( double, const Entry * srcEntry ) const;
    private:
	const Entry * _dstEntry;
    };

#if IL_USED
    class ilrate_flow : private flow_common {
    public:
	ilrate_flow( const Interlock& interlock, const Entry * dstEntry, bool is_processor_entry ) : flow_common(interlock), _dstEntry(dstEntry), _is_processor_entry(is_processor_entry) {}
	std::pair<double,double>& operator()( std::pair<double,double>&, const Entry * srcEntry ) const;
    private:
	const Entry * _dstEntry;
	const bool _is_processor_entry;
    };
#endif

    class ilrate_pril_flow {
    public:
	ilrate_pril_flow( const Interlock& interlock, const Entry * dstEntry, double& n_cust, bool is_processor_entry ) :
	    _dstEntry(dstEntry), _is_processor_entry(is_processor_entry), _n_cust(n_cust), _ph2(0.0) {}
	std::pair<double,double>& operator()( std::pair<double,double>&, const Entry * srcEntry ) const;
    private:
	double flow( const Entry * srcEntry, const Entry * dstEntry ) const;
	const Entry * _dstEntry;
	const bool _is_processor_entry;
	double& _n_cust;
	mutable double _ph2;
    };

public:
    Interlock( const Entity& aServer );
    virtual ~Interlock();

    void initialize();

    const std::set<const Entry *>& commonEntries() const { return _commonEntries; }
    unsigned getNsources() const { return _sources;} 

    double numberOfSources( const Task& viaTask, const Entry * aServerEntry) const;
    bool isCommonEntry( const Entry * ) const;
    bool hasCommonEntry( const Entry*, const Task* ) const;
    bool hasCommonEntry( const Entry*, const Entry*, const Task*, const Task* ) const;
    std::set<const Entry*> getCommonEntries( const Entry*, const Entry*, const Task*, const Task*) const;
    bool isType3Sending( const Entry*, const Entry*, const Task*, const Task*) const;
    bool hasCommonEntry( const Entry&, const Entry& ) const;
    bool hasSingleSource() const;

    double interlockedFlow( const Task& viaTask ) const;
    double interlockedFlow( const Entry * aClientEntry ) const;
    Probability interlockedFlow( const Task& viaTask, const Entry * aServerEntry,  double& il_rate, bool& moreThan4 ) const;

    std::ostream& print( std::ostream& output ) const;
    static std::ostream& printPathTable( std::ostream& output );
private:
    Interlock( const Interlock& );
    Interlock& operator=( const Interlock& );

private:
    bool via( const Entry*, const Entry*, const Task* ) const;
    void findInterlock();
    void pruneInterlock();
    void findSources();
    void findParentEntries( const Entry&, const Entry& );

    bool isBranchPoint( const Entry& srcX, const Entry& entryA, const Entry& srcY, const Entry& entryB ) const;
    bool getInterlockedTasks( const int headOfPath, const Entry *, std::set<const Entity *>& interlockedTasks ) const; 
    unsigned countSources( const std::set<const Entity *>& );
	
private:
    std::set<const Entry *> _commonEntries;	/* common source entries	*/
    std::set<const Entity *> _allSourceTasks;	/* Phase 1+ sources.		*/
    std::set<const Entity *> _ph2SourceTasks;	/* Phase 2+ sources.		*/
    const Entity& _server;			/* My server.			*/
    unsigned _sources;
};

inline std::ostream& operator<<( std::ostream& output, const Interlock& self) { return self.print( output ); }

InterlockInfo& operator+=( InterlockInfo&, const InterlockInfo& );
InterlockInfo& operator-=( InterlockInfo&, const InterlockInfo& );
InterlockInfo operator*( const InterlockInfo&, double );

std::ostream& operator<<( std::ostream&, const Interlock& );
std::ostream& operator<<( std::ostream&, const InterlockInfo& );
bool operator>( const InterlockInfo&, double  );
#endif
