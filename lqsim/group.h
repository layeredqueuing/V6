/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/lqsim/group.h $
 * Global vars for simulation.
 *
 * $Id: group.h 14996 2021-09-27 14:14:50Z greg $
 */

/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* Sept.2008								*/
/************************************************************************/

#ifndef	GROUP_H
#define GROUP_H

#include <lqio/dom_group.h>
#include "task.h"

class Group 
{
public:
    static Group * find( const std::string& );
    static void add( const std::pair<std::string,LQIO::DOM::Group*>& );

public:
    Group( LQIO::DOM::Group * group, const Processor& processor );

    LQIO::DOM::Group * getDOMGroup() const { return _domGroup; }
    const char * name() const { return _domGroup->getName().c_str(); }
    bool cap() const { return _domGroup->getCap(); }		/* Cap share		*/
    const Processor& processor() const { return _processor; }
    Group& create();

    Group& reset_stats() { r_util.reset(); return *this; }
    Group& accumulate_data() { r_util.accumulate(); return *this; }
    Group& insertDOMResults();

public:
    std::map<Task *,int> _tasks;	/* Maps task to group 		*/

private:
    LQIO::DOM::Group * _domGroup;
    const Processor &_processor;
    const unsigned int _total_tasks;
    result_t r_util;			/* Utilization.			*/

    std::set<Task*> _task_list;
};

/*
 * Compare to processors by their name.  Used by the set class to insert items
 */

struct ltGroup
{
    bool operator()(const Group * p1, const Group * p2) const { return strcmp( p1->name(), p2->name() ) < 0; }
};


/*
 * Compare a group name to a string.  Used by the find_if (and other algorithm type things.
 */

struct eqGroupStr 
{
    eqGroupStr( const std::string& s ) : _s(s) {}
    bool operator()(const Group * p1 ) const { return _s == p1->name(); }

private:
    const std::string _s;
};

extern std::set<Group *, ltGroup> group;
#endif
