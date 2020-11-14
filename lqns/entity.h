/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/lqns/entity.h $
 *
 * Pure virtual class for tasks and processors.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 *
 * $Id: entity.h 14091 2020-11-13 02:59:17Z greg $
 *
 * ------------------------------------------------------------------------
 */

#if	!defined(ENTITY_H)
#define ENTITY_H

#include "dim.h"
#include <vector>
#include <lqio/input.h>
#include <lqio/dom_processor.h>
#include "vector.h"
#include "prob.h"
#include "call.h"

class Entity;
class Entry;
class Group;
class Interlock;
class MVASubmodel;
class Model;
class Processor;
class Server;
class Submodel;
class Task;

typedef Vector<unsigned> ChainVector;

ostream& operator<<( ostream&, const Entity& );
int operator==( const Entity&, const Entity& );

#define TYPE_STR_BUFSIZ	20

/* ----------------------- Abstract Superclass ------------------------ */

class Entity {
    friend class Generate;

public:
    /*
     * Compare two entities by their submodel. 
     */

    struct LT
    {
	bool operator()(const Entity * e1, const Entity * e2) const {
	    return (e1->submodel() < e2->submodel()) 
		|| (e1->getDOM() && (!e2->getDOM() || e1->getDOM()->getSequenceNumber() < e2->getDOM()->getSequenceNumber() ));
	}
    };

    struct update_wait {
	update_wait( Entity& entity ) : _entity(entity) {}
	void operator()( const Submodel* submodel ) { _entity.updateWait( *submodel, 1.0 ); }
    private:
	Entity& _entity;
    };

    struct set_chain_IL_rate {
	set_chain_IL_rate( const Entity& entity, double rate ) : _station(entity._station), _serverChains(entity._serverChains), _rate(rate) {}
	void operator()( unsigned int k );
    private:
	Server * _station;
	const ChainVector _serverChains;
	const double _rate;
    };

private:
    class SRVNManip {
    public:
	SRVNManip( ostream& (*f)(ostream&, const Entity & ), const Entity & entity ) : _f(f), _entity(entity) {}
    private:
	ostream& (*_f)( ostream&, const Entity& );
	const Entity & _entity;

	friend ostream& operator<<(ostream & os, const SRVNManip& m ) { return m._f(os,m._entity); }
    };

private:

#if 0
    struct set_max_customers {
	set_max_customers( const Entry * server_entry, unsigned int k ) : _server_entry(server_entry), _k {}
	void operator()( const Task * client ) const { client->setMaxCustomers( _submodel, _server_entry ); }
    private:
	const MVASubmodel& _submodel;
	const Entry * _server_entry;
    };
#endif
    
    struct add_weight {
	add_weight( const MVASubmodel& submodel ) : _submodel(submodel) {}
	double operator()( double sum, Call * call ) const { return sum + call->computeWeight( _submodel ); }
    private:
	const MVASubmodel& _submodel;
    };

public:
    Entity( LQIO::DOM::Entity* domVersion, const std::vector<Entry *>& entries );
    virtual ~Entity();

private:
    Entity( const Entity& ) = delete;		/* Copying is verbotten */
    Entity& operator=( const Entity& ) = delete;

public:
    /* Initialization */

    virtual bool check() const = 0;
    virtual Entity& configure( const unsigned );
    virtual unsigned findChildren( Call::stack&, const bool ) const;
    virtual Entity& initWait();
    Entity& initInterlock();
    virtual Entity& initThroughputBound() { return *this; }
    virtual Entity& initPopulation() = 0;
    virtual Entity& initJoinDelay() { return *this; }
    virtual Entity& initThreads() { return *this; }
    virtual Entity& initClient( const Vector<Submodel *>& ) { return *this; }
    virtual Entity& reinitClient( const Vector<Submodel *>& ) { return *this; }
    virtual Entity& initServer( const Vector<Submodel *>& );
    virtual Entity& reinitServer( const Vector<Submodel *>& );
	
    /* Instance Variable Access */
	   
    virtual LQIO::DOM::Entity * getDOM() const { return _dom; }
    virtual int priority() const { return 0; }
    virtual Entity& priority( const int ) { return *this; }
    virtual scheduling_type scheduling() const { return getDOM()->getSchedulingType(); }
    virtual unsigned copies() const;
    virtual unsigned replicas() const;
    double population() const { return _population; }
    virtual const Processor * getProcessor() const { return 0; }
    unsigned submodel() const { return _submodel; }
    Entity& setSubmodel( const unsigned submodel ) { _submodel = submodel; return *this; }
    virtual double thinkTime( const unsigned = 0, const unsigned = 0 ) const { return _thinkTime; }
    virtual Entity& setOverlapFactor( const double ) { return *this; }

    virtual unsigned int fanOut( const Entity * ) const = 0;
    virtual unsigned int fanIn( const Task * ) const = 0;

    double throughput() const;
    double utilization() const;
    double saturation() const;

    /* Queries */

    virtual bool hasVariance() const { return attributes.variance; }
    bool hasDeterministicPhases() const { return attributes.deterministic; }
    bool hasSecondPhase() const { return (bool)(_maxPhase > 1); }
    bool hasOpenArrivals() const;
    
    virtual unsigned hasClientChain( const unsigned, const unsigned ) const { return 0; }
    unsigned hasServerChain( const unsigned k ) const;
    virtual bool hasActivities() const { return false; }
    bool hasThreads() const { return nThreads() > 1 ? true : false; }
    virtual bool hasSynchs() const { return false; }
    bool hasILWait() const ;
    bool isInOpenModel() const { return attributes.open_model ? true : false; }
    Entity& isInOpenModel( const bool yesOrNo ) { attributes.open_model = yesOrNo; return *this; }
    bool isInClosedModel() const { return attributes.closed_model ? true : false; }
    Entity& isInClosedModel( const bool yesOrNo ) { attributes.closed_model = yesOrNo; return *this; }
    Entity& isPureServer( const bool yesOrNo ) { attributes.pure_server = yesOrNo; return *this; }
    bool isPureServer() const { return (bool)attributes.pure_server; }
    Entity& isPureDelay( const bool yesOrNo ) { attributes.pure_delay = yesOrNo; return *this; }
    bool isPureDelay() const { return (bool)attributes.pure_delay; }
    Entity& initialized( const bool yesOrNo ) { attributes.initialized = yesOrNo; return *this; }
    bool initialized() const { return (bool)attributes.initialized; }
    virtual bool isUsed() const { return submodel() > 0; }

    virtual bool isTask() const          { return false; }
    virtual bool isReferenceTask() const { return false; }
    virtual bool isProcessor() const     { return false; }
    virtual bool isCFSserver() const     { return false; }
    virtual bool isInfinite() const;
    virtual bool isViaTask() const  	 { return false; }

    bool isCalledBy( const Task* ) const;
    bool isMultiServer() const   	 { return copies() > 1; }
    bool isReplicated() const		 { return replicas() > 1; }

    bool schedulingIsOk( const unsigned bits ) const;

    const std::vector<Entry *>& entries() const { return _entries; }
    Entity& addEntry( Entry * );
    Entry * entryAt( const unsigned index ) const { return _entries.at(index); }

    Entity& addTask( const Task * aTask ) { _tasks.insert( aTask ); return *this; }
    Entity& removeTask( const Task * aTask ) { _tasks.erase( aTask ); return *this; }
    const std::set<const Task *>& tasks() const { return _tasks; }

    virtual const std::string& name() const { return getDOM()->getName(); }
    unsigned maxPhase() const { return _maxPhase; }

    unsigned nEntries() const { return _entries.size(); }
    virtual unsigned nClients() const = 0;
    const Entity& clients( std::set<Task *>& ) const;
    double nCustomers( ) const;
    const std::set<const Entry *>& commonEntries() const { return _interlock.commonEntries(); }
    double num_sources( const Task * viaTask, const Entry * serverEntry ) const { return _interlock.numberOfSources( *viaTask, serverEntry ); }
    bool isCommonEntry( const Entry * entry ) const { return _interlock.isCommonEntry( entry ); }
    bool hasCommonEntry( const Entry * e1, const Entry * e2, const Task * t1 , const Task * t2 ) const { return _interlock.hasCommonEntry( e1, e2, t1, t2 ); }
    std::set<const Entry*> getCommonEntries( const Entry * e1, const Entry * e2, const Task * t1 , const Task * t2 ) const { return _interlock.getCommonEntries( e1, e2, t1, t2 ); }
    bool isType3Sending( const Entry * e1, const Entry * e2, const Task * t1 , const Task * t2 ) const { return _interlock.isType3Sending( e1, e2, t1, t2 ); }
    bool hasCommonEntry( const Entry& e1, const Entry& e2 ) const { return _interlock.hasCommonEntry( e1, e2 ); } 

    Entity& addServerChain( const unsigned k ) { _serverChains.push_back(k); return *this; }
    const ChainVector& serverChains() const { return _serverChains; }
    Server * serverStation() const { return _station; }

    bool markovOvertaking() const;
    double openArrivalRate() const;

    /* Interlock */
	
    Entity& initWeights( MVASubmodel& submodel );
    const Entity& setMaxCustomers( const MVASubmodel& ) const;
    const Entity& setInterlock( const MVASubmodel& ) const;
    const Entity& setInterlockRelation( const MVASubmodel& ) const;

    Probability prInterlock( const Task& ) const;
    Probability prInterlock( const Entry * aClientEntry ) const;
    Probability prInterlock( const Task& aClient, const Entry * aServerEntry, double& il_rate, bool& moreThanFour ) const;

    bool isInterlocked() const { return _interlock.getNsources() > 0; }
    void setChainILRate(const Task&  aClient, double rate) const;
    void setChainILRate(const Task& aClient, const Entry& viaTaskEntry, double rate) const;
    double getWeight() const { return _weight;}
    bool hasSingleSource() const { return _interlock.hasSingleSource(); }

private:
    void setInterlockPr_upper( const MVASubmodel& ) const;
    const Entity& setRealCustomers( const MVASubmodel& ) const;


    /* Computation */
public:
    virtual double prOt( const unsigned, const unsigned, const unsigned ) const { return 0.0; }

    Entity& updateAllWaits( const Vector<Submodel *>& );
    void setIdleTime( const double );
    virtual Entity& computeVariance();
    void setOvertaking( const unsigned, const std::set<Task *>& );
    virtual Entity& updateWait( const Submodel&, const double ) { return *this; }	/* NOP */
    virtual double updateWaitReplication( const Submodel&, unsigned& ) { return 0.0; }	/* NOP */
    virtual double deltaUtilization() const;

    /* Dynamic Updates / Late Finalization */
    /* In order to integrate LQX's support for model changes we need to have a way  */
    /* of re-calculating what used to be static for all dynamically editable values */
	
    /* Sanity Check */

    virtual const Entity& sanityCheck() const;
    virtual bool openModelInfinity() const;

    virtual Entity& resetGroups() { return *this; }
    virtual Entity& initGroups() { return *this; }

    /* MVA interface */

    Entity& clear();
    virtual Server * makeServer( const unsigned ) = 0;
    Entity& initServerStation( MVASubmodel& );
    virtual Entity& saveServerResults( const MVASubmodel&, double );

    /* Threads */
	
    virtual unsigned nThreads() const { return 1; }		/* Return the number of threads. */
    virtual unsigned concurrentThreads() const { return 1; }	/* Return the number of threads. */
    virtual void joinOverlapFactor( const Submodel& ) const {};	/* NOP? */
	
    /* Printing */

    virtual ostream& print( ostream& ) const = 0;
    virtual ostream& printJoinDelay( ostream& output ) const { return output; }
    static SRVNManip print_server_chains( const Entity& entity ) { return SRVNManip( output_server_chains, entity ); }
    static SRVNManip print_info( const Entity& entity ) { return SRVNManip( output_entity_info, entity ); }

private:
    static ostream& output_server_chains( ostream& output, const Entity& aServer );
    static ostream& output_entity_info( ostream& output, const Entity& aServer );
    
protected:
    virtual unsigned validScheduling() const;
    virtual scheduling_type defaultScheduling() const { return SCHEDULE_FIFO; }
    double computeIdleTime( const unsigned, const double ) const;
	
private:
    void setServiceTime( const Entry * anEntry, unsigned k ) const;

protected:
    LQIO::DOM::Entity* _dom;		/* The DOM Representation	*/
    std::vector<Entry *> _entries;	/* Entries for this entity.	*/
    std::set<const Task *> _tasks;	/* Clients of this entity	*/
    
    double _population;			/* customers when I'm a client	*/
    double _thinkTime;			/* Think time.			*/
    Server * _station;			/* Servers by submodel.		*/

    struct {
	unsigned initialized:1;		/* Task was initialized.	*/
	unsigned closed_model:1;	/* Stn in in closed model.	*/
	unsigned open_model:1;		/* Stn is in open model.	*/
	unsigned deterministic:1;	/* an entry has det. phase.	*/
	unsigned pure_delay:1;		/* Wierd task.			*/
	unsigned pure_server:1;		/* Can use FCFS schedulging.	*/
 	unsigned variance:1;		/* an entry has Cv_sqn != 1.	*/
    } attributes;

private:
    Interlock _interlock;		/* For interlock calculation.	*/
    unsigned _submodel;			/* My submodel, 0 == ref task.	*/
    unsigned _maxPhase;			/* Largest phase.		*/
    double _utilization;		/* Utilization			*/
    mutable double _lastUtilization;	/* For convergence test.	*/
    double _weight;			/* weighting interlock flow  	*/
    /* MVA interface */

    ChainVector _serverChains;		/* Chains for this server.	*/
};
#endif
