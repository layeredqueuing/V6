/* -*- C++ -*-
 * synmodel.h	-- Greg Franks
 *
 * $Id: synmodel.h 16945 2024-01-26 13:02:36Z greg $
 */

#ifndef _SYNMODEL_H
#define _SYNMODEL_H

#include "submodel.h"

/* -------------- Special Submodel for Synchronization  --------------- */

class SynchSubmodel : public Submodel
{
public:
    SynchSubmodel( const unsigned n ) : Submodel( n ) {}
    virtual ~SynchSubmodel();
	
    const char * const submodelType() const { return "Synch Submodel"; }

    virtual SynchSubmodel& initClients( const Model& ) { return *this; }
    virtual SynchSubmodel& initInterlock() { return *this; }
    virtual SynchSubmodel& build() { return *this; }
		
    virtual SynchSubmodel& solve( long, MVACount&, const double );
	
    virtual std::ostream& print( std::ostream& ) const;

private:
    std::ostream& printSyncModel( std::ostream& output ) const;
};
#endif
