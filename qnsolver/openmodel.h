/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/qnsolver/openmodel.h $
 *
 * SRVN command line interface.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * December 2020
 *
 * $Id: openmodel.h 14498 2021-02-27 23:08:51Z greg $
 *
 * ------------------------------------------------------------------------
 */

#if !defined(OPENMODEL_H)
#define OPENMODEL_H
#include <config.h>
#include <map>
#include <string>
#include <lqio/bcmp_document.h>
#include <mva/vector.h>
#include <mva/open.h>
#include "model.h"

class Server;
class ClosedModel;
class Open;

class OpenModel : public Model {
public:
    friend class Model;
    
    OpenModel( Model& parent, BCMP::JMVA_Document& input );
    virtual ~OpenModel();

    explicit operator bool() const { return _result == true; }
    bool construct();
    bool instantiate();
    bool convert( const ClosedModel* );
    bool solve( const ClosedModel* );
    virtual void saveResults();
    
    std::ostream& debug( std::ostream& output ) const;

private:
    virtual BCMP::Model::Chain::Type type() const { return BCMP::Model::Chain::Type::OPEN; }
    virtual bool isParent() const { return false; }

private:
    Model& _parent;
    Open* _solver;
};
#endif
