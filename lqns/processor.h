/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk-V6/lqns/processor.h $
 *
 * Processors.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 * May, 2009
 *
 * ------------------------------------------------------------------------
 * $Id: processor.h 16965 2024-01-28 19:30:13Z greg $
 * ------------------------------------------------------------------------
 */

#ifndef LQNS_PROCESSOR_H
#define LQNS_PROCESSOR_H

#include <lqio/dom_processor.h>
#include <set>
#include "entity.h"

class Task;
class Processor;
class Server;
class Group;

class Processor : public Entity {

public:
    static void create( const std::pair<std::string,LQIO::DOM::Processor*>& );

protected:
    Processor( LQIO::DOM::Processor* );
    Processor( const Processor& processor, unsigned int replica );

public:
    virtual ~Processor();

protected:
    virtual Processor * clone( unsigned int replica ) { return new Processor( *this, replica ); }
    
public:
    /* Initialization */
    virtual void initializeServer();

    virtual bool check() const;
    virtual Processor& configure( const unsigned );
    virtual void recalculateDynamicValues() {}

    /* Instance Variable access */

    virtual LQIO::DOM::Processor * getDOM() const { return dynamic_cast<LQIO::DOM::Processor *>(Entity::getDOM()); }
    virtual double rate() const;
    virtual unsigned int fanOut( const Entity * ) const;
    virtual unsigned int fanIn( const Task * ) const;

    /* Queries */

    virtual bool isProcessor() const { return true; }
    bool isInteresting() const;
    virtual const unsigned nGroups() const { return 0; }
    virtual bool hasVariance() const;
    bool hasPriorities() const;

    virtual const Entity& sanityCheck() const;

    /* Model Building. */

    virtual Entity* mapToReplica( size_t ) const;
    Processor& expand();
    
    Server * makeServer( const unsigned nChains );
    virtual double computeUtilization( const MVASubmodel&, const Server& );

    /* DOM insertion of results */

    virtual const Processor& insertDOMResults() const;
    virtual std::ostream& print( std::ostream& ) const;
    std::ostream& printTasks( std::ostream& output, unsigned int submodel ) const;

protected:
    virtual bool schedulingIsOK() const;

public:
    static Processor * find( const std::string&, unsigned int=1 );

private:
    static bool __prune;
};


class CFS_Processor : public Processor
{
    friend void Processor::create( const std::pair<std::string,LQIO::DOM::Processor*>& );
    
protected:
    CFS_Processor( LQIO::DOM::Processor* dom ) : Processor(dom), _groups(), _all_caps(false) {}

public:
//    virtual ~CFS_Processor();
    virtual bool check() const;

    /* Model Building. */

    virtual const unsigned nGroups() const { return _groups.size(); }
    Processor& addGroup( Group * aGroup ) { _groups.insert( aGroup ); return *this; }
    const std::set<Group *>& groups() const { return _groups; }
    bool allCaps() const { return _all_caps; }

    /* CFS scheduling */

    virtual bool isCFSserver() const { return !_groups.empty(); }
    bool runDPS() const;
    CFS_Processor& resetGroups();
    CFS_Processor& initGroups();
    
    virtual CFS_Processor& saveServerResults( const MVASubmodel&, double );

private:
    std::set<Group *> _groups;			/* List of processor's Group	*/
    bool _all_caps;				/* All shares are caps 		*/
};


/*
 * Special processor class which is used to implement delays in the
 * model such as phase think times.  There is no DOM attached to it,
 * so we have to fake out some of the get methods.
 */

class DelayServer : public Processor
{
public:
    DelayServer( const std::string& name ) : Processor( nullptr ), _name(name) {}		/* No Dom */
    virtual ~DelayServer() = default;

private:
    DelayServer( const DelayServer& ) = delete;
    DelayServer& operator=( const DelayServer& ) = delete;

public:
    virtual Processor * clone() { throw std::runtime_error( "DelayServer::clone()" ); return nullptr; }
    virtual bool check() const { return true; }

    virtual double rate() const { return 1.0; }
    virtual const std::string& name() const { return _name; }
    virtual scheduling_type scheduling() const { return SCHEDULE_DELAY; }
    virtual unsigned copies() const { return 1; }
    virtual unsigned replicas() const { return 1; }
    virtual bool isInfinite() const { return true; }

    virtual const DelayServer& insertDOMResults() const { return *this; }	/* NOP */

private:
    const std::string _name;
};
#endif
