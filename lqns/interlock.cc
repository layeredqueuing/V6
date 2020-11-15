/* -*- c++ -*-
 * $Id: interlock.cc 14097 2020-11-15 14:12:41Z greg $
 *
 * Call-chain/interlock finder.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * April 2003
 * ------------------------------------------------------------------------
 */

#include <algorithm>
#include <cmath>
#include <algorithm>
#include <numeric>
#include "entry.h"
#include "interlock.h"
#include "lqns.h"
#include "model.h"
#include "option.h"
#include "task.h"

bool Interlock::CollectTasks::has_entry(const Entry * entry ) const { return std::find( _entryStack.begin(), _entryStack.end(), entry ) != _entryStack.end(); }
bool Interlock::CollectTable::has_entry(const Entry * entry ) const { return std::find( _entryStack.begin(), _entryStack.end(), entry ) != _entryStack.end(); }
bool Interlock::is_via2::operator()( const Entry * entry ) { return entry->owner() == _task2 && !_interlock.via( entry, _entry2, _task2 ); }

/* -------------------------------------------------------------------- */
/* Funky Formatting functions for inline with <<.			*/
/* -------------------------------------------------------------------- */

class StringNManip {
public:
    StringNManip( ostream& (*ff)(ostream&, const std::string&, const unsigned ), const std::string& s, const unsigned n ) : f(ff), _s(s), _n(n) {}
private:
    ostream& (*f)( ostream&, const std::string&, const unsigned );
    const std::string& _s;
    const unsigned int _n;

    friend ostream& operator<<(ostream & os, const StringNManip& m ) { return m.f(os,m._s,m._n); }
};

StringNManip trunc( const std::string&, const unsigned );

/************************************************************************/
/*                     Throughput Interlock Model.                      */
/************************************************************************/

/*
 * Generate the interlock table.  It is impossible to interlock on
 * infinite servers since all calls (by definition) go to unique
 * instances.
 */

Interlock::Interlock( const Entity& aServer )
    : _commonEntries(),
      _allSourceTasks(),
      _ph2SourceTasks(),
      _server(aServer),
      _sources(0)
{
}



Interlock::~Interlock()
{
    _commonEntries.clear();
    _allSourceTasks.clear();
    _ph2SourceTasks.clear();
}


void
Interlock::initialize()
{
    if ( !_server.isInfinite() ) {
	findInterlock();
	pruneInterlock();
	findSources();
    }
}


/*
 * This function searches for interlock paths among all the callers
 * to `aTask'.
 */

void
Interlock::findInterlock()
{
    if ( Options::Debug::interlock() ) {
	cout << "Interlock for server: " << _server.name() << endl;
    }

    /* Locate all callers to myServer */

    std::set<Task *> clients;
    _server.getClients( clients );

    for ( std::set<Task *>::const_iterator clientA = clients.begin(); clientA != clients.end(); ++clientA ) {
	if ( !(*clientA)->isUsed() ) continue;		/* Ignore this task - not used. */

	for ( std::set<Task *>::const_iterator clientC = clients.begin(); clientC != clients.end(); ++clientC ) {
	    if ( (*clientA) == (*clientC) || !(*clientC)->isUsed() ) continue;

	    for ( std::vector<Entry *>::const_iterator entryA = (*clientA)->entries().begin(); entryA != (*clientA)->entries().end(); ++entryA ) {
		for ( std::vector<Entry *>::const_iterator entryC = (*clientC)->entries().begin(); entryC != (*clientC)->entries().end(); ++entryC ) {
		    bool foundAB = false;
		    bool foundCD = false;

		    /* Check that both entries call me. */

		    for ( std::vector<Entry *>::const_iterator entry = _server.entries().begin(); entry != _server.entries().end(); ++entry ) {
			if ( (*entryA)->isInterlocked( (*entry) ) ) foundAB = true;
			if ( (*entryC)->isInterlocked( (*entry) ) ) foundCD = true;
		    }
		    if ( foundAB && foundCD ) {
			findParentEntries( *(*entryA), *(*entryC) );
		    }
		}
	    }
	}
    }
}




/*
 * Locate all common parent entries to A and C.
 */

void
Interlock::findParentEntries( const Entry& srcA, const Entry& srcC )
{
    const Entry& entryA = srcA;
    const Entry& entryC = srcC;
    const unsigned a = entryA.entryId();
    const unsigned c = entryC.entryId();

    if ( Options::Debug::interlock() ) {
	cout << "  Common parents for entries " << srcA.name() << " and " << srcC.name() << ": ";
    }

    /* Figure 6 in interlock paper. */

    for ( std::set<Task *>::const_iterator aTask = Model::__task.begin(); aTask != Model::__task.end(); ++aTask ) {

	/* x calls a, a calls b; y calls c, c calls d */

	for ( std::vector<Entry *>::const_iterator srcX = (*aTask)->entries().begin(); srcX != (*aTask)->entries().end(); ++srcX ) {
	    for ( std::vector<Entry *>::const_iterator srcY = (*aTask)->entries().begin(); srcY != (*aTask)->entries().end(); ++srcY ) {
		if ( (*srcX)->_interlock[a].all > 0.0 && (*srcY)->_interlock[c].all > 0.0 ) {

		    /* Prune here (branch point?) */

		    if ( Options::Debug::interlock() ) {
			cout << (*srcX)->name();
		    }

		    if ( isBranchPoint( *(*srcX), entryA, *(*srcY), entryC ) ) {
			_commonEntries.insert(*srcX);
			if ( Options::Debug::interlock() ) {
			    cout << "* ";
			}
		    } else if ( Options::Debug::interlock() ) {
			cout << " ";
		    }
		}
	    }
	}
    }

    if ( Options::Debug::interlock() ) {
	cout << endl;
    }
}


bool
Interlock::hasSingleSource() const
{
    for ( std::set<const Entry *>::const_iterator entry = commonEntries().begin(); entry != commonEntries().end(); ++entry ) {
	if ( (*entry)->owner()->population() < getNsources() ) return false;
    }
    return true;
}


bool
Interlock::isCommonEntry( const Entry * srcA ) const
{
    return commonEntries().find( srcA ) != commonEntries().end();
}


bool
Interlock::via( const Entry* comE, const Entry* serverEntry, const Task* viatask) const
{
    const unsigned b = serverEntry->entryId();	
    const std::vector<Entry *>& entries = viatask->entries();
    for ( std::vector<Entry *>::const_iterator viaE = entries.begin(); viaE != entries.end(); ++viaE ) {
	const unsigned a = (*viaE)->entryId();
	if ( comE->_interlock[a].all > 0.0 && (*viaE)->_interlock[b].all > 0.0 ){
	    return true;
	}
    }
    return false;
}



bool
Interlock::hasCommonEntry( const Entry* se1, const Task* viatask1 ) const
{
    for ( std::set<const Entry *>::const_iterator entry = commonEntries().begin(); entry != commonEntries().end(); ++entry ) {
	if ( (*entry)->owner() == viatask1 && via( *entry, se1, viatask1) ) {
	    return true;
	}
    }
    return false;
}



bool
Interlock::hasCommonEntry( const Entry* se1, const Entry* se2, const Task* viatask1, const Task* viatask2 ) const
{
    for ( std::set<const Entry *>::const_iterator entry = commonEntries().begin(); entry != commonEntries().end(); ++entry ) {
	if (via( (*entry), se1, viatask1) && via( (*entry), se2, viatask2)) return true;
    }
    return false;
}



std::set<const Entry* >
Interlock::getCommonEntries( const Entry* se1, const Entry* se2, const Task* viatask1, const Task* viatask2 ) const
{
    std::set<const Entry* > common_entries;
    for ( std::set<const Entry *>::const_iterator entry = commonEntries().begin(); entry != commonEntries().end(); ++entry ) {
	if (via( (*entry), se1, viatask1) && via( (*entry), se2, viatask2)) {
	    common_entries.insert( *entry );
	}
    }
    return common_entries;
}



bool
Interlock::hasCommonEntry( const Entry& srcA, const Entry& srcC ) const
{
    const unsigned e1 =srcA.entryId();
    const unsigned e2 =srcC.entryId();

    for ( std::set<const Entry *>::const_iterator entry = commonEntries().begin(); entry != commonEntries().end(); ++entry ) {
	if ( (*entry)->_interlock[e1].all > 0.0 && (*entry)->_interlock[e2].all > 0.0 ) return true;
    }
    return false;
}


/*
 * Type3 sending interlocks
 *   1. viatask2 is multi-entry client;
 *   2. one of entries is a parent of a sending interlock;
 *   3. the child (viatask1) and the other entries of viatask2 are also interlocked.
 * Especially when the multiplicity of viatask1 and viatask2 is 1,
 * the customer from viatask1 is not able to see any requsts from any entry of viatask2.
 */

bool
Interlock::isType3Sending( const Entry* se1, const Entry* se2, const Task* viatask1, const Task* viatask2 ) const
{
    std::set<const Entry* > task1_common_entries;
    std::accumulate( commonEntries().begin(), commonEntries().end(), task1_common_entries, insert_if_via1( *this, se1, viatask1 ) );
    return std::any_of( task1_common_entries.begin(), task1_common_entries.end(), is_via2( *this, se2, viatask2 ) );
}



/*
 * Given a set of entries that are branch points, find interlocked flow.
 * Interlock probability is affected by the number of threads of viaTask.
 * This method is called each time we generate an MVA model.
 */

double
Interlock::interlockedFlow( const Task& viaTask ) const
{
    if ( _sources == 0 ) return 0.0;

    /* Find all flow from the common parent list to viaTask. */

    double sum = 0.0;
    for ( std::set<const Entry *>::const_iterator srcEntry = commonEntries().begin(); srcEntry != commonEntries().end(); ++srcEntry ) {
	sum = std::accumulate( viaTask.entries().begin(), viaTask.entries().end(), sum, flow_src_dst( *this, *srcEntry ) );
    }

    /*
     * Find out the rate of the interlocked flows. Now is in the task
     * level. maybe need to extend to the entry level, if there are
     * interlocked flows from different common entries
     */

    double rate = std::min( sum / viaTask.throughput(), 1.0 );

    //myServer->setChainILRate( viaTask, rate);

    if ( flags.trace_interlock ) {
	cout << "Interlock sum=" << sum << ", viaTask: " << viaTask.throughput() <<", IL_rate : "<< rate
	     << ", threads=" << viaTask.concurrentThreads()  << ", sources=" << _sources << endl;
    }
    return std::min( sum, viaTask.throughput() ) / (viaTask.throughput() * viaTask.concurrentThreads() * _sources  * _sources );
//    return min( sum, viaTask.throughput() ) / (viaTask.throughput() * viaTask.concurrentThreads() * _sources );
}



/*
 * find out the weight of interlocked flow based on viaTask' throughput.
 * This method is called each time we generate an MVA model.
 */
double
Interlock::interlockedFlow( const Entry * dstEntry ) const
{
    if ( getNsources() == 0 || dstEntry->throughput() == 0.0 ) return 0.0;

//    const unsigned aa = aServerEntry->entryId();
    /* Find all flow from the common parent list to viaTask. */

    double sum = std::accumulate( commonEntries().begin(), commonEntries().end(), 0.0, flow_dst_src( *this, dstEntry ) );
	
    /* find out the rate of the interlocked flows */
    /* now is in the task level. maybe need to extend to the entry level,*/
    /* if there are interlocked flows from different common entries*/
    double rate = std::min( sum / dstEntry->throughput(), 1.0 );

    if ( flags.trace_interlock ) {
//	cout << "Interlock sum=" << sum << ", viaTask: " << viaTask.throughput() <<", IL_rate : "<< rate
//	     << ", threads=" << viaTask.concurrentThreads()  << ", sources=" << getNsources() << endl;
    }
    // return min( sum, dstA->throughput() ) / (viaTask.throughput());
    return rate;
}



Probability
Interlock::interlockedFlow( const Task& viaTask, const Entry * serverEntry, double& il_rate, bool& moreThan3 ) const
{
    il_rate = 0.0;

    if ( getNsources() == 0 ) {
	moreThan3 = false;
	return 0.0;
    }
	
    double sum_task = 0.0;
    double sum_pr_il_task = 0.0;
    double sum_tput = 0.0;
    double sum_cust = 0;
    const std::vector<Entry *>& entries = viaTask.entries();
    for ( std::vector<Entry *>::const_iterator dstA = entries.begin(); dstA != entries.end(); ++dstA ) {
	const double dstA_throughput = (*dstA)->throughput();
	
	if ( (*dstA)->_interlock[serverEntry->entryId()].all == 0.0 || dstA_throughput == 0.0 || !serverEntry->isCalledBy(*dstA) ) continue;

	/* Find all flow from the common parent list to viaTask. */

	std::pair<double,double> sum(0.0,0.0);		/* first=sum, second=pr_il */
	sum = std::accumulate( commonEntries().begin(), commonEntries().end(), sum, ilrate_pril_flow( *this, *dstA, sum_cust, serverEntry->isProcessorEntry() ) );
		
	/*find out the rate of the interlocked flows */
	/* now is in the task level. maybe need to extend to the entry level,*/
	/* if there are interlocked flows from different common entries*/
	double rate = sum.first / dstA_throughput;

	if ( flags.trace_interlock && sum.first > 0.0 ) {
	    cout << "Interlock sum =" << sum.first << ", viaEntry: " << dstA_throughput
		 <<", IL_rate : "<< rate
		 <<", Pril : "<< sum.second/rate << endl;
	}
	sum_task += sum.first;
	sum_pr_il_task += sum.second;
	sum_tput += dstA_throughput;

    }//while (dstA)

	
    /* find out the rate of the interlocked flows */
    /* now is in the task level. maybe need to extend to the entry level,*/
    /* if there are interlocked flows from different common entries*/

    if ( sum_task == 0.0 ) {
	il_rate = 0.0;
	if ( flags.trace_interlock ) {
	    cout << "Interlock, viaTask: " << viaTask.throughput()
		 <<", IL_rate : "<< il_rate  << endl;
	}
	moreThan3 = false;
	return 0.0;
    }

    const double rate = sum_task / sum_tput;
    sum_pr_il_task /= sum_tput;

    il_rate = std::min( rate, 1.0 );

    if ( flags.trace_interlock ) {
	cout << "Interlock sum=" << sum_task << ", viaTask: " << viaTask.throughput()
	     <<", IL_rate : "<< il_rate  <<", Pril : "<< sum_pr_il_task
	     << ", threads=" << viaTask.concurrentThreads()  << ", sources=" << getNsources() << endl;
    }

    moreThan3 = sum_cust > 3;
    return sum_pr_il_task / rate;
}



/*
 * Given a set of entries that are branch points, find interlocked flow.
 * Interlock probability is affected by the number of threads of viaTask.
 * This method finds interlock probabilities and interlock rate.
 */

double
Interlock::numberOfSources( const Task& viaTask, const Entry * aServerEntry) const
{
    if ( getNsources() == 0 ) return 0.0;
	
    double num=0.0;
    std::set<const Entity *> source_tasks;
    const unsigned b =aServerEntry->entryId();

    for ( std::set<const Entry *>::const_iterator srcE = commonEntries().begin(); srcE != commonEntries().end(); ++srcE ) {
	double m = (*srcE)->getMaxCustomers();
	const Entity *srctask = (*srcE)->owner();

	const std::vector<Entry *>& entries = viaTask.entries();
	for( std::vector<Entry *>::const_iterator dstA = entries.begin(); dstA != entries.end(); ++dstA ) {
	    const unsigned a = (*dstA)->entryId();
	
	    if ((*dstA)->_interlock[b].all > 0.0  &&  (*srcE)->_interlock[a].all > 0.0){
		if ( source_tasks.find(srctask) == source_tasks.end() ) {
		    num += m;
		    source_tasks.insert( (*srcE)->owner() );
		}
	    }
	}
    }
    if ( flags.trace_interlock ) {
	cout << "Interlock:: count_sources(viaTask= " << viaTask.name()
	     <<", se= : "<< aServerEntry->name()  <<") = "<< num
	     << ", while Nsources()=" << getNsources() << endl;
    }
    return num;
}



/*
 * Find all sources for flow along interlock paths by descending down the
 * call paths to ``myServer'' or some other terminus.  Only count those
 * paths which actually go to ``myServer''.
 */

void
Interlock::findSources()
{
    std::set<const Entity *> interlockedTasks;

    /* Look for all parent tasks */

    for ( std::set<const Entry *>::const_iterator entry = commonEntries().begin(); entry != commonEntries().end(); ++entry ) {
	const Entity * aTask = (*entry)->owner();

	/* Add tasks corresponding to branch point entries */

	_allSourceTasks.insert( aTask );

	if ( (*entry)->maxPhase() > 1 ) {
	    _ph2SourceTasks.insert( aTask );
	}

	/* Locate all tasks on interlocked paths. */

	CollectTasks data( _server, interlockedTasks );
	(*entry)->getInterlockedTasks( data );
    }

    /*
     * Prune out tasks in sourceTasks that are also in
     * interlockedTasks for allSrcTasks.  Ph2Tasks is the opposite
     * of what was prunded out as phase 2 sources are independent.
     */

#ifdef	DEBUG_INTERLOCK
    if ( Options::Debug::interlock() ) {
	cout <<         "    All Sourcing Tasks: ";
	for ( std::set<const Entity *>::const_iterator task = _allSourceTasks.begin(); task != _allSourceTasks.end(); ++task ) {
	    cout << (*task)->name() << " ";
	}
	cout << endl << "    Interlocked Tasks:  ";
	for ( std::set<const Entity *>::const_iterator task = interlockedTasks.begin(); task != interlockedTasks.end(); ++task ) {
	    cout << (*task)->name() << " ";
	}
	cout << endl;
    }
#endif

    std::set<const Entity *> difference;
    std::set_difference( _allSourceTasks.begin(), _allSourceTasks.end(),
			 interlockedTasks.begin(), interlockedTasks.end(),
			 std::inserter( difference, difference.end() ) );
    _allSourceTasks = difference;

    std::set<const Entity *> intersection;
    std::set_intersection( _ph2SourceTasks.begin(), _ph2SourceTasks.end(),
			   interlockedTasks.begin(), interlockedTasks.end(),
			   std::inserter( intersection, intersection.end() ) );
    _ph2SourceTasks = intersection;

#ifdef	DEBUG_INTERLOCK
    if ( Options::Debug::interlock() ) {
	cout <<         "    Common Parent Tasks (all): ";
	for ( std::set<const Entity *>::const_iterator task = _allSourceTasks.begin(); task != _allSourceTasks.end(); ++task ) {
	    cout << (*task)->name() << " ";
	}
	cout << endl << "    Common Parent Tasks (Ph2): ";
	for ( std::set<const Entity *>::const_iterator task = _ph2SourceTasks.begin(); task != _ph2SourceTasks.end(); ++task ) {
	    cout << (*task)->name() << " ";
	}
	cout << endl;
    }
#endif

    /* Now count up the instances on each task in sourceTasks */
    /* And add on all other sources */

    _sources = countSources( interlockedTasks );

    /* Useful trivia. */

    if ( Options::Debug::interlock() ) {
	for ( std::set<const Entity *>::const_iterator task = interlockedTasks.begin(); task != interlockedTasks.end(); ++task ) {
	    cout << (*task)->name() << " ";
	}
	cout << endl << "    Sourcing Tasks:    ";
	for ( std::set<const Entity *>::const_iterator task = _allSourceTasks.begin(); task != _allSourceTasks.end(); ++task ) {
	    cout << (*task)->name() << " ";
	}
	cout << endl << endl;
    }
}



/*
 * This procedure is used to locate all of the other sources that are along the
 * call paths.  Tasks found cannot be in sources or in paths.
 */

unsigned
Interlock::countSources( const std::set<const Entity *>& interlockedTasks )
{
    /*
     * Sequence over arriving arcs and add task to sources
     * provided that it is not in paths.
     */

    for ( std::set<const Entity *>::const_iterator task = interlockedTasks.begin(); task != interlockedTasks.end(); ++task ) {
	const std::vector<Entry *>& entries = (*task)->entries();
	for ( std::vector<Entry *>::const_iterator entry = entries.begin(); entry != entries.end(); ++entry ) {
	    const std::set<Call *>& callerList = (*entry)->callerList();
	    for ( std::set<Call *>::const_iterator call = callerList.begin(); call != callerList.end(); ++call ) {
		if ( !(*call)->hasRendezvous() ) continue;

		const Task * parentTask = (*call)->srcTask();
		if ( interlockedTasks.find( parentTask ) == interlockedTasks.end() ) {
		    _allSourceTasks.insert( parentTask );
		}
	    }
	}
    }

    /*
     * Now count up all the copies of sourcing tasks.  Note that we
     * have to use the special population() call to account
     * properly for infinite servers.
     */

    unsigned n = std::accumulate( _allSourceTasks.begin(), _allSourceTasks.end(), 0., add_using<const Entity>( &Entity::population ) );

    /*
     * Add in phase 2 sources of tasks that are on the interlock
     * path.  These tasks count as additional sources because phase
     * 2+ flow is (quasi) independent of phase 1 flow.  Ignore all
     * tasks that are immediate parents of the interlockee (mva
     * interlock attends to this case).
     */

    const std::vector<Entry *>& dst_entries = _server.entries();
    for ( std::set<const Entity *>::const_iterator task = interlockedTasks.begin(); task != interlockedTasks.end(); ++task ) {
	const std::vector<Entry *>& src_entries = (*task)->entries();
	for ( std::vector<Entry *>::const_iterator srcEntry = src_entries.begin(); srcEntry != src_entries.end(); ++srcEntry ) {
	    for ( std::vector<Entry *>::const_iterator dstEntry = dst_entries.begin(); dstEntry != dst_entries.end(); ++dstEntry ) {
		const unsigned e = (*dstEntry)->entryId();
		const double ph2 = (*srcEntry)->_interlock[e].all - (*srcEntry)->_interlock[e].ph1;
		if ( ph2 > 0.0 ) {
		    n += 1;
		    goto nextTask;	/* Any hit counts... */
		}
	    }
	}
    nextTask: ;
    }

    /* All done! */

    return n;
}



/*
 * Return 1 if X and Y follow paths to different tasks on the
 * correct call path.
 */

bool
Interlock::isBranchPoint( const Entry& srcX, const Entry& entryA, const Entry& srcY, const Entry& entryB ) const
{
    /*
     * I have to ensure that a call to myself (which is not feasible...)
     */

    if ( srcX.owner() == entryA.owner()
	 && srcY.owner() == entryB.owner()
	 && entryA.owner() == entryB.owner() ) return false;	// Multiserver client

    /*
     * Quick check -- send interlock?
     */

    if ( srcX == entryA || srcY == entryB ) return true;

#ifdef	NOTDEF
    if ( Options::Debug::interlock() ) {
	cout << "  isBranchPoint: " << srcX.name() << (char *)(srcX.isProcessorEntry() ? "*, " : ", ")
	     << entryA.name() << (char *)(entryA.isProcessorEntry() ? "*, " : ", ")
	     << srcY.name() << (char *)(srcY.isProcessorEntry() ? "*, " : ", ")
	     << entryB.name() << (char *)(entryB.isProcessorEntry() ? "*, " : ", ")
	     << endl;
    }
#endif

    /*
     * Sequence over all calls from X and Y and see if they go to
     * different tasks.  Only consider those entries which ultimately
     * go to a and b respectively
     */

    CallInfo nextX( srcX, Call::RENDEZVOUS_CALL );
    CallInfo nextY( srcY, Call::RENDEZVOUS_CALL );

    const unsigned a = entryA.entryId();
    const unsigned b = entryB.entryId();

    for ( std::vector<CallInfoItem>::const_iterator yxe = nextX.begin(); yxe != nextX.end(); ++yxe ) {
	const Entry * dstE = yxe->dstEntry();

	if ( dstE->_interlock[a].all == 0.0 ) continue;

	for ( std::vector<CallInfoItem>::const_iterator yyf = nextY.begin(); yyf != nextY.end(); ++yyf ) {
	    const Entry * dstF = yyf->dstEntry();

	    if ( dstF->_interlock[b].all > 0.0 && dstE->owner() != dstF->owner() ) return true;
	}
    }

    return false;
}



/*
 * Go through the interlock list and remove entries from parents
 * See prune above.
 */

void
Interlock::pruneInterlock()
{
    /* For all tasks in common entry... subtract off their common entries */

    for ( std::set<const Entry *>::const_iterator i = commonEntries().begin(); i != commonEntries().end(); ++i ) {
	const Entity * dst = (*i)->owner();
	const std::set<const Entry *>& dst_entries = dst->commonEntries();
	for ( std::set<const Entry *>::const_iterator entry = dst_entries.begin(); entry != dst_entries.end(); ++entry ) {
	    _commonEntries.erase( *entry );		// Nop if not found
	}
    }
}

/*
 * Print path information.
 */

ostream&
Interlock::print( ostream& output ) const
{
    output << _server.name() << ": Sources=" << _sources << ", entries: " ;

    for ( std::set<const Entry *>::const_iterator srcE = commonEntries().begin(); srcE != commonEntries().end(); ++srcE ) {
	output << (*srcE)->name() << " ";
    }

    return output;
}



/*
 * Print out path table.
 */

ostream&
Interlock::printPathTable( ostream& output )
{
    std::set<Entry *>::const_iterator srcEntry;
    std::set<Entry *>::const_iterator dstEntry;
    static const unsigned int columns = 5;

    ios_base::fmtflags oldFlags = output.setf( ios::left, ios::adjustfield );
    output << "src\\dst   ";
    unsigned i;
    unsigned j;
    for ( i = 1, srcEntry = Model::__entry.begin(); srcEntry != Model::__entry.end(); ++srcEntry, ++i ) {
	const Entry * anEntry = *srcEntry;
	output << trunc( anEntry->name(), 10 );
	if ( i % columns == 0 ) {
	    output << ' ';
	}
    }
    output << endl;

    for ( i = 0, srcEntry = Model::__entry.begin(); srcEntry != Model::__entry.end(); ++srcEntry, ++i ) {
	const Entry * src = *srcEntry;

	if ( i % columns == 0 ) {
	    output << "----------";
	    for ( j = 1; j <= Model::__entry.size(); ++j ) {
		output << "----------";
		if ( j % columns == 0 ) {
		    output << '+';
		}
	    }
	    output << endl;
	}

	output << trunc( src->name(), 10 );
	for ( j = 1, dstEntry = Model::__entry.begin(); dstEntry != Model::__entry.end(); ++dstEntry, ++j ) {
	    output.setf( ios::right, ios::adjustfield );
	    output << setw(4) << src->_interlock[(*dstEntry)->entryId()].all << ",";
	    output.setf( ios::left, ios::adjustfield );
	    output << setw(4) << src->_interlock[(*dstEntry)->entryId()].ph1 << " ";
	    if ( j % columns == 0 ) {
		output << '|';
	    }
	}
	output << endl;
    }
    output.flags(oldFlags);
    output << endl;

    return output;
}

bool
InterlockInfo::operator==( const InterlockInfo& arg ) const
{
    return all == arg.all && ph1 == arg.ph1;
}

ostream&
operator<<( ostream& output, const InterlockInfo& self )
{
    output << self.all << self.ph1 << endl;
    return output;
}

/* -------------------------------------------------------------------- */
/*		    Hoisted code for finding flow			*/
/* -------------------------------------------------------------------- */

double
Interlock::flow_common::flow( const Entry * srcEntry, const Entry * dstEntry, bool any_phase ) const
{
    const Entity * srcTask = srcEntry->owner();
    const unsigned dst = dstEntry->entryId();
    double sum = 0.0;

    if ( srcEntry->_interlock[dst].all > 0.0 && (any_phase || srcEntry->maxPhase() == 1) && _allSourceTasks.find( srcTask ) != _allSourceTasks.end() ) {
	sum += srcEntry->throughput() * srcEntry->_interlock[dst].all;
    } else if (srcEntry->_interlock[dst].ph1 > 0.0 && srcEntry->maxPhase() > 1 && _allSourceTasks.find( srcTask ) != _allSourceTasks.end() ){
	sum += srcEntry->throughput() * srcEntry->_interlock[dst].ph1;	
    }
		
    _ph2 = srcEntry->_interlock[dst].all - srcEntry->_interlock[dst].ph1;
    if ( _ph2 > 0.0 && _ph2SourceTasks.find( srcTask ) != _ph2SourceTasks.end() ) {
	sum += srcEntry->throughput() * _ph2;
    }

    return sum;
}



double
Interlock::flow_src_dst::operator()( double sum, const Entry * dstEntry ) const
{
    sum += flow( _srcEntry, dstEntry );

    if ( flags.trace_interlock ) {
	const unsigned a = dstEntry->entryId();
	cout << "  Interlock E=" << _srcEntry->name() << " A=" << dstEntry->name()
	     << " Throughput=" << _srcEntry->throughput()
	     << ", interlock[" << a << "]={" << _srcEntry->_interlock[a].all << "," << _ph2
	     << "}, sum=" << sum << endl;
	cout<< "server entry: "<< dstEntry->name()<<"Util ="<< dstEntry->utilization()<<" , "<<endl;
    }
    return sum;
}


double
Interlock::flow_dst_src::operator()( double sum, const Entry * srcEntry ) const
{
    sum += flow( srcEntry, _dstEntry, true );

    if ( flags.trace_interlock ) {
	const unsigned a = _dstEntry->entryId();
	cout << "  Interlock E=" << srcEntry->name() << " A=" << _dstEntry->name()
	     << " Throughput=" << srcEntry->throughput()
	     << ", interlock[" << a << "]={" << srcEntry->_interlock[a].all << "," << _ph2
//		 << "} , interlock[" << aa << "]={" << srcEntry->_interlock[aa].all << "," << srcEntry->_interlock[aa].all-srcEntry->_interlock[aa].ph1
	     << "}, sum=" << sum << endl;
	cout<< "server entry: "<<_dstEntry->name()<<"Util ="<<_dstEntry->utilization()<<" , "<<endl;
    }
    return sum;
}



std::pair<double,double>&
Interlock::ilrate_pril_flow::operator()( std::pair<double,double>& sum, const Entry * srcEntry ) const
{
    const unsigned dst = _dstEntry->entryId();
    if ( srcEntry->_interlock[dst].all <= 0.0 ) return sum;

    const double src_sum = flow( srcEntry, _dstEntry );
    double m = static_cast<double>(srcEntry->getMaxCustomers());
    if ( m == 0 ) {
	m = srcEntry->owner()->population();
    } else {
	m = std::min( m, static_cast<double>( srcEntry->owner()->population() ) );
    }
    _n_cust += m;

    if ( _is_processor_entry ) {
	if ( m > 3 ) {
	    m += m;
	} else {
	    m *= m;
	}
    }
    sum.first  += src_sum;
    sum.second += src_sum / m;

    if ( flags.trace_interlock ) {
	cout << "  Interlock common E=" << srcEntry->name() << " A=" << _dstEntry->name()
	     << " Throughput=" << srcEntry->throughput()
	     << ", interlock[" << dst << "]={" << srcEntry->_interlock[dst].all << "," << _ph2
	     << "}, il_rate=" << src_sum <<", pril=1/"<<m
	     << ", sum_IR*PrIL ="<<sum.second<<endl;

	cout << "server entry: " << _dstEntry->name() << " Util =" << _dstEntry->utilization() << " , " << endl;
    }

    return sum;
}


/*
 * not quite flow_common::flow()
 */

double
Interlock::ilrate_pril_flow::flow( const Entry * srcEntry, const Entry * dstEntry ) const
{
    const unsigned dst = dstEntry->entryId();
    double sum = 0.0;

    if ( srcEntry->_interlock[dst].all > 0.0 && srcEntry->maxPhase() == 1 ){		//&& _allSourceTasks.find( srcTask ) ) {
	sum += srcEntry->throughput() * srcEntry->_interlock[dst].all;
    } else if ( srcEntry->_interlock[dst].ph1 > 0.0 && srcEntry->maxPhase() > 1 ){ 	//&& _allSourceTasks.find( srcTask )){
	sum += srcEntry->throughput() * srcEntry->_interlock[dst].ph1;	
    }

    _ph2 = srcEntry->_interlock[dst].all - srcEntry->_interlock[dst].ph1;
    if ( _ph2 > 0.0 ) { 								//&& _allSourceTasks.find( srcTask ) ) {
	sum += srcEntry->throughput() * _ph2;
    }

    return sum;
}

InterlockInfo&
InterlockInfo::operator=( const InterlockInfo& anEntry )
{
    if ( this == &anEntry ) return *this;

    ph1 = anEntry.ph1;
    all = anEntry.all;

    return *this;
}



InterlockInfo&
operator+=( InterlockInfo& arg1, const InterlockInfo& arg2 )
{
    arg1.ph1 += arg2.ph1;
    arg1.all += arg2.all;

    return arg1;
}



InterlockInfo&
operator-=( InterlockInfo& arg1, const InterlockInfo& arg2 )
{
    arg1.ph1 -= arg2.ph1;
    arg1.all -= arg2.all;

    return arg1;
}


InterlockInfo
operator*( const InterlockInfo& multiplicand, double multiplier )
{
    InterlockInfo product;

    product.all = multiplicand.all * multiplier;
    product.ph1 = multiplicand.ph1 * multiplier;

    return product;
}

bool
operator>( const InterlockInfo& lhs, double rhs )
{
    return ((lhs.all>rhs) || (lhs.ph1>rhs));
}

static ostream& trunc_str( ostream& output, const std::string& s, const unsigned n )
{
    if ( s.size() > n ) {
	output.write( s.c_str(), n );
    } else {
	ios_base::fmtflags oldFlags = output.setf( ios::left, ios::adjustfield );
	output << setw(n) << setfill( ' ' ) << s;
	output.flags(oldFlags);
    }
    return output;
}

StringNManip trunc( const std::string& s, const unsigned n ) { return StringNManip( trunc_str, s, n ); }

