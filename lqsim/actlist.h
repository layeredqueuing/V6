/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* May  1996.								*/
/* August 2009								*/
/************************************************************************/

/*
 * Activity Lists (for linking the graph of activities).
 *
 * $Id: actlist.h 13831 2020-09-18 12:51:41Z greg $
 */

#ifndef ACTLIST_H
#define ACTLIST_H

#include "result.h"

class Entry;
class Activity;
class Histogram;
class Task;
class Activity;

typedef enum list_type
{
    ACT_FORK_LIST,
    ACT_OR_FORK_LIST,
    ACT_AND_FORK_LIST,
    ACT_LOOP_LIST,
    ACT_JOIN_LIST,
    ACT_AND_JOIN_LIST,
    ACT_OR_JOIN_LIST
} list_type;

class InputActivityList;
class AndForkActivityList;
class OutputActivityList;


class ActivityList {
public:
    typedef std::vector<Activity *>::const_iterator const_iterator;
    
    struct Collect
    {
	typedef double (Activity::*fptr)( ActivityList::Collect& ) const;

	Collect( const Entry * e, fptr f ) : _e(e), rate(1.), phase(1), can_reply(true), _f(f) {};
	const Entry * _e;
	double rate;
	unsigned int phase;
	bool can_reply;
	fptr _f;
    };
	
    ActivityList( list_type type, LQIO::DOM::ActivityList* dom )
	: _type(type),
	  _dom(dom),
	  _list()
	{}
    virtual ~ActivityList() {}

    size_t size() const { return _list.size(); }
    list_type get_type() const { return _type; }
    Activity * at( size_t ix ) const { return _list[ix]; }
    Activity * front() const { return _list.front(); }
    Activity * back() const { return _list.back(); }
    ActivityList::const_iterator begin() { return _list.begin(); }
    ActivityList::const_iterator end() { return _list.end(); }
    
    LQIO::DOM::ActivityList * get_DOM() const { return _dom; }
    const std::string get_name() const;
    
    virtual ActivityList& configure() { return *this; }
    virtual ActivityList& push_back( Activity * activity ) { _list.push_back( activity ); return *this; }
    virtual double collect( std::deque<Activity *>& activity_stack, ActivityList::Collect& data ) const = 0;
    void shuffle();
    ActivityList& initialize();

private:
    const list_type _type;
    LQIO::DOM::ActivityList* _dom;

protected:
    std::vector<Activity *> _list;		/* Array of activities.		*/
};

class InputActivityList : public ActivityList
{
public:
    InputActivityList( list_type type, LQIO::DOM::ActivityList * dom )
	: ActivityList(type,dom),
	  _prev(NULL)
	{}

    OutputActivityList * get_prev() const { return _prev; }
    InputActivityList& set_prev( OutputActivityList * list ) { _prev = list; return *this; }

    virtual double find_children( std::deque<Activity *>& activity_stack, std::deque<AndForkActivityList *>& fork_stack, const Entry * ep ) = 0;
    virtual std::deque<AndForkActivityList *>::iterator fork_backtrack( std::deque<AndForkActivityList *>&, Activity *, Activity * );

private:
    OutputActivityList * _prev;		/* Link to join list.		*/
};

class ForkActivityList : public InputActivityList
{
public:
    ForkActivityList( list_type type, LQIO::DOM::ActivityList * dom )
	: InputActivityList(type,dom),
	  _join(NULL),
	  _visits(0)
	{}

    OutputActivityList * get_join() const { return _join; }
    ForkActivityList& set_join( OutputActivityList * list ) { _join = list; return *this; }
    unsigned int get_visits() const { return _visits; }
    ForkActivityList& set_visits( unsigned int value ) { _visits = value; return *this; }
    ForkActivityList& inc_visits() { _visits += 1; return *this; }

    virtual double find_children( std::deque<Activity *>& activity_stack, std::deque<AndForkActivityList *>& fork_stack, const Entry * ep );
    virtual double collect( std::deque<Activity *>& activity_stack, ActivityList::Collect& data ) const;
    
private:
    OutputActivityList * _join;		/* Link to fork from join.	*/
    unsigned _visits;			/* */
};

class OrForkActivityList : public ForkActivityList
{

public:
    OrForkActivityList( list_type type, LQIO::DOM::ActivityList * dom )
	: ForkActivityList(type,dom),
	  _prob()
	{}
    
    double get_prob_at( size_t ix ) const { return _prob[ix]; }
    
    virtual OrForkActivityList& push_back( Activity * activity );
    virtual OrForkActivityList& configure();
    virtual double find_children( std::deque<Activity *>& activity_stack, std::deque<AndForkActivityList *>& fork_stack, const Entry * ep );
    virtual double collect( std::deque<Activity *>& activity_stack, ActivityList::Collect& data ) const;

private:
    std::vector<double> _prob;		/* Array of probabilities.	*/
};

class AndForkActivityList : public ForkActivityList
{
public:
    AndForkActivityList( list_type type, LQIO::DOM::ActivityList * dom )
	: ForkActivityList(type,dom),
	  _visit()
	{}

    virtual AndForkActivityList& push_back( Activity * activity );
    virtual AndForkActivityList& configure();
    virtual double find_children( std::deque<Activity *>& activity_stack, std::deque<AndForkActivityList *>& fork_stack, const Entry * ep );
    virtual std::deque<AndForkActivityList *>::iterator fork_backtrack( std::deque<AndForkActivityList *>&, Activity *, Activity * );
    virtual double collect( std::deque<Activity *>& activity_stack, ActivityList::Collect& data ) const;
    
private:
    std::vector<bool> _visit;		/* true if I visit a join.	*/
};

class LoopActivityList : public InputActivityList
{
public:
    LoopActivityList( list_type type, LQIO::DOM::ActivityList * dom )
	: InputActivityList(type,dom),
	  _exit(NULL),
	  _count(),
	  _total(0.0)
	{}

    Activity * get_exit() const { return _exit; }
    double get_total() const { return _total; }
    double get_count_at( size_t ix ) const { return _count[ix]; }
    
    virtual LoopActivityList& push_back( Activity * activity );
    LoopActivityList end_list( Activity * activity ) { _exit = activity; return *this; }
    virtual LoopActivityList& configure();
    virtual double find_children( std::deque<Activity *>& activity_stack, std::deque<AndForkActivityList *>& fork_stack, const Entry * ep );
    virtual double collect( std::deque<Activity *>& activity_stack, ActivityList::Collect& data ) const;

private:
    Activity * _exit;			/* For repeat nodes. 		*/
    std::vector<double> _count;		/* array of iterations		*/
    double _total;			/* total iterations.		*/
};

/* ------------------------------------------------------------------------ */

class OutputActivityList : public ActivityList
{
public:
    OutputActivityList( list_type type, LQIO::DOM::ActivityList * dom )
	: ActivityList(type,dom),
	  _next(NULL)
	{}
	  
    InputActivityList * get_next() const { return _next; }
    OutputActivityList& set_next( InputActivityList * list ) { _next = list; return *this; }
    
    virtual double find_children( std::deque<Activity *>& activity_stack, std::deque<AndForkActivityList *>& fork_stack, const Entry * ep );
    std::deque<AndForkActivityList *>::iterator join_backtrack( std::deque<AndForkActivityList *>& fork_stack, Activity * start_activity );
    virtual double collect( std::deque<Activity *>& activity_stack, ActivityList::Collect& data ) const;

private:
    InputActivityList * _next;		/* Link to fork list.		*/
};

class AndJoinActivityList : public OutputActivityList
{
public:
    typedef enum join_type
    {
	JOIN_UNDEFINED,
	JOIN_INTERNAL_FORK_JOIN,
	JOIN_SYNCHRONIZATION
    } join_type;

    AndJoinActivityList( list_type type, LQIO::DOM::ActivityList * dom );
    virtual ~AndJoinActivityList();

    virtual AndJoinActivityList& configure();
    virtual AndJoinActivityList& push_back( Activity * activity );

    bool set_join_type( join_type type );
    bool join_type_is( join_type type ) const { return type == _join_type; }
    bool add_to_join_list( unsigned i, Activity * activity );
    unsigned int get_quorum_count() const { return _quorum_count; }

    void join_check();

    virtual double find_children( std::deque<Activity *>& activity_stack, std::deque<AndForkActivityList *>& fork_stack, const Entry * ep );
    virtual double collect( std::deque<Activity *>& activity_stack, ActivityList::Collect& data ) const;

    AndJoinActivityList& insertDOMResults();

private:
    std::vector<ForkActivityList *> _fork;	/* Link to join from fork.	*/
    std::vector<Activity *> _source;		/* Link to source activity 	*/
    join_type _join_type;
    unsigned int _quorum_count; 		/* tomari quorum		*/

public:
    result_t r_join;			/* results for join delays	*/
    result_t r_join_sqr;		/* results for delays.		*/
    Histogram * _hist_data;
};



void print_activity_connectivity( FILE *, Activity * );

/* Used by load.cc */

void complete_activity_connections ();
#endif
