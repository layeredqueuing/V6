/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/qnsolver/boundsmodel.h $
 *
 * Bounds solver.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * February 2022.
 *
 * $Id: boundsmodel.h 15424 2022-02-03 11:01:20Z greg $
 *
 * ------------------------------------------------------------------------
 */

#if !defined(BOUNDSMODEL_H)
#define BOUNDSMODEL_H
#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <map>
#include <string>
#include <lqio/bcmp_document.h>
#include "model.h"

class BoundsModel : public Model {
public:
    friend class Model;
    
    BoundsModel( Model& parent, BCMP::JMVA_Document& input );
    virtual ~BoundsModel();

    explicit operator bool() const { return _result == true; }
    bool construct();
    bool solve();
    virtual void saveResults();
    
private:
    virtual BCMP::Model::Chain::Type type() const { return BCMP::Model::Chain::Type::UNDEFINED; }
    virtual bool isParent() const { return false; }

private:
    Model& _parent;
    std::map<std::string,BCMP::Model::Bound> _bounds;		/* Chain, Bounds */
};
#endif
