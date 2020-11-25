/* -*- c++ -*-
 * submodel.h	-- Greg Franks
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * June 2007
 *
 * $Id: submodel.h 14141 2020-11-25 20:57:44Z greg $
 */

#ifndef _SUBMODEL_H
#define _SUBMODEL_H


#include "dim.h"
#include <set>
#include <cstdlib>
#include <lqio/input.h>
#include "vector.h"
#include "pop.h"
#include "task.h"

class Call;
class Processor;
class Entry;
class Model;
class MVA;
class MVACount;
class Open;
class Server;
class SubModelManip;
class Group;

/* ------- Submodel Abstract Superclass.  Subclassed as needed. ------- */
	 
class Submodel {
    class SubmodelManip {
    public:
	SubmodelManip( std::ostream& (*ff)(std::ostream&, const Submodel&, const unsigned long ),
		       const Submodel& aSubmodel, const unsigned long anInt )
	    : f(ff), submodel(aSubmodel), arg(anInt) {}
    private:
	std::ostream& (*f)( std::ostream&, const Submodel&, const unsigned long );
	const Submodel& submodel;
	const unsigned long arg;

	friend std::ostream& operator<<(std::ostream & os, const SubmodelManip& m ) { return m.f(os,m.submodel,m.arg); }
    };

public:
    Submodel( const unsigned n ) : _submodel_number(n), _n_chains(0) {}
    virtual ~Submodel() {}

private:
    Submodel( const Submodel& ) { abort(); }		/* Copying is verbotten */
    Submodel& operator=( const Submodel& ) { abort(); return *this; }

public:
    int operator==( const Submodel& aSubmodel ) const { return &aSubmodel == this; }
    int operator!() const { return _servers.size() + _clients.size() == 0; }	/* Submodel is empty! */

    void addClient( Task * aTask ) { _clients.insert(aTask); }
    void addServer( Entity * anEntity ) { _servers.insert(anEntity); }

    virtual const char * const submodelType() const = 0;
    unsigned number() const { return _submodel_number; }
    Submodel& number( const unsigned );

    virtual VectorMath<double> * getOverlapFactor() const { return nullptr; } 
    unsigned nChains() const { return _n_chains; }
    unsigned customers( const unsigned i ) const { return _customers[i]; }
    double thinkTime( const unsigned i ) const { return _thinkTime[i]; }
    void setThinkTime( unsigned int i, double thinkTime ) { _thinkTime[i] = thinkTime; }
    unsigned priority( const unsigned i ) const { return _priority[i]; }

    virtual Submodel& initServers( const Model& );
    virtual Submodel& reinitServers( const Model& ) { return *this; }
    virtual Submodel& initInterlock() { return *this; }
    virtual Submodel& build() { return *this; }
    virtual Submodel& rebuild() { return *this; }

    virtual double nrFactor( const Server *, const unsigned, const unsigned ) const { return 0; }

    virtual unsigned n_closedStns() const { return 0; }
    virtual unsigned n_openStns() const { return 0; }

    virtual Submodel& solve( long, MVACount&, const double ) = 0;
	
    virtual std::ostream& print( std::ostream& ) const = 0;

    void debug_stop( const unsigned long, const double ) const;

protected:
    void setNChains( unsigned int n ) { _n_chains = n; }
    SubmodelManip print_submodel_header( const Submodel& aSubModel, const unsigned long iterations  ) { return SubmodelManip( &Submodel::submodel_header_str, aSubModel, iterations ); }

private:
    static std::ostream& submodel_header_str( std::ostream& output, const Submodel& aSubmodel, const unsigned long iterations );

protected:
    std::set<Task *> _clients;		/* Table of clients 		*/
    std::set<Entity *> _servers;	/* Table of servers 		*/

private:
    unsigned _submodel_number;		/* Submodel number.  Set once.	*/
    unsigned _n_chains;			/* Number of chains K		*/
	
protected:
    /* MVA Stuff */

    Population _customers;		/* Customers by chain k		*/
    VectorMath<double> _thinkTime;	/* Think time for chain k	*/
    Vector<unsigned> _priority;		/* Priority for chain k.	*/
};

inline std::ostream& operator<<( std::ostream& output, const Submodel& self) { return self.print( output ); }

/* ---------------------- Standard MVA Submodel ----------------------- */


class MVASubmodel : public Submodel {
    friend class Generate;
    friend class Entity;		/* closedModel */
    friend class Task;			/* closedModel */
    friend class Processor;		/* closedModel */
    friend class CFS_Processor;

public:
    MVASubmodel( const unsigned );
    virtual ~MVASubmodel();
	
    const std::set<Task *>& getClients() const { return _clients; }			/* Table of clients 		*/
    const char * const submodelType() const { return "Submodel"; }

    virtual MVASubmodel& initServers( const Model& );
    virtual MVASubmodel& reinitServers( const Model& );
    virtual MVASubmodel& initInterlock();
    virtual MVASubmodel& build();
    virtual MVASubmodel& rebuild();
		
    virtual unsigned n_closedStns() const { return closedStation.size(); }
    virtual unsigned n_openStns() const { return openStation.size(); }
    virtual VectorMath<double> * getOverlapFactor() const { return _overlapFactor; } 

    virtual double nrFactor( const Server *, const unsigned e, const unsigned k ) const;

    virtual MVASubmodel& solve( long, MVACount&, const double );
	
    virtual std::ostream& print( std::ostream& ) const;

private:
    bool hasReplication() const { return _hasReplication; }
    bool hasThreads() const { return _hasThreads; }
    bool hasSynchs() const { return _hasSynchs; }

protected:
    unsigned makeChains();
    void saveWait( Entry *, const Server * );

    std::ostream& printClosedModel( std::ostream& ) const;
    std::ostream& printOpenModel( std::ostream& ) const;
    std::ostream& printMaxCustomers( std::ostream& output ) const;	/*+ interlock -*/

private:
    bool _hasReplication;		/* True if replication present.	*/
    bool _hasThreads;			/* True if client has forks.	*/
    bool _hasSynchs;			/* True if server has joins.	*/

    /* MVA Stuff */
	
    Vector<Server *> closedStation;
    Vector<Server *> openStation;
protected:
    MVA * closedModel;
private:
    Open * openModel;

    /* Fork-Join stuff. */
	
    VectorMath<double> * _overlapFactor;
};


/* ---------------------- Fair Share MVA Submodel ----------------------- */
	
class CFSSubmodel : public MVASubmodel {
private:
    struct redistribute {
	redistribute( CFSSubmodel& submodel ) : _submodel(submodel) {}
	void operator()( Entity * server );

	bool redistributed() const { return _submodel._redistributed; }
	void setRedistributed( bool redistributed ) { _submodel._redistributed = redistributed; }
	std::set<Task *>& clients() { return _submodel._clients; }
	
    private:
	CFSSubmodel& _submodel;
    };
	
public:
    CFSSubmodel( const unsigned n, const Model& model ) : MVASubmodel(n), _model(model), _redistributed(false) {}

    const char * const submodelType() const { return "CFS Submodel"; }

    virtual CFSSubmodel& initServers( const Model& );
    virtual CFSSubmodel& reinitServers( const Model& );
    virtual CFSSubmodel& build();
    virtual CFSSubmodel& rebuild();

    virtual CFSSubmodel& solve( long, MVACount&, const double );

private:
    const Model& _model;
    bool _redistributed;
};


/* -------------------------------------------------------------------- */
/* Funky Formatting functions for inline with <<.			*/
/* -------------------------------------------------------------------- */

SubModelManip print_submodel_header( const Submodel &, const unsigned long );
#endif
