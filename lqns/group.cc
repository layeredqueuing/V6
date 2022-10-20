/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/lqns/group.cc $
 * 
 * Everything you wanted to know about a task, but were afraid to ask.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 2008
 *
 * ------------------------------------------------------------------------
 * $Id: group.cc 15969 2022-10-13 19:49:43Z greg $
 * ------------------------------------------------------------------------
 */

#include "lqns.h"
#include <cmath>
#include <numeric>
#include <lqio/error.h>
#include <lqio/dom_group.h>
#include "errmsg.h"
#include "flags.h"
#include "group.h"
#include "lqns.h"
#include "pragma.h"
#include "processor.h"
#include "model.h"
#include "task.h"


std::map<const Group::status_t,const std::string> Group::state_str = {
    {status_t::RECEIVING,"receiving"},
    {status_t::THROTTLE,"throttle"},
    {status_t::CONTRIBUTING,"contributing"}
};

/* ------------------------ Constructors etc. ------------------------- */

Group::Group( LQIO::DOM::Group* dom, const CFS_Processor * processor )
    : _dom(dom),
      _tasks(),
      _processor(processor),
      _share(0.0),
      _replica_number(1)
{ 
    initialize();
}

Group::~Group()
{
}

/*
 * Check that the group share is within range.
 */

bool
Group::check() const
{
    const bool rc = 0 < getOriginalShare() && getOriginalShare() <= 1.0;
    if ( !rc ) {
	LQIO::input_error2( LQIO::ERR_INVALID_SHARE, name().c_str(), getOriginalShare() );
    }
    return rc;
}


/* 
 * Do not set share here. in case lqx has yet to set a value to the
 * group share.
 */

void 
Group::initialize()
{
    _status = status_t::RECEIVING;
    _state = status_t::THROTTLE;
    _ratio1 = 1.0;
    _ratio2 = 0.0;
}



Group&
Group::recalculateDynamicValues()
{
    _share = getOriginalShare();    /* read group share from lqx;  */
    initialize();
    return *this;
}



double 
Group::utilization() const
{
    if ( Pragma::disableProcessorCFS() ) {
	return std::accumulate( tasks().begin(), tasks().end(), 0., add_using<Task>( &Task::processorUtilization ) );
    } else {
	return std::accumulate( tasks().begin(), tasks().end(), 0., add_using<Task>( &Task::getGroupUtilization ) );
    }
}



/*
 * This group can donate excess share.  The value returned is the amount of share available.
 */

double
Group::couldContribute()
{
    if ( _status != status_t::CONTRIBUTING ) return 0.;
   
    const double group_util = utilization();
    if ( group_util >= getOriginalShare() * 0.95 ) return 0.;
   
    const double diff = getOriginalShare() - group_util;
    //if (diff <0.01) return 0.;
   
    _state = status_t::CONTRIBUTING;
    return diff;
}


/*
 * This group would like some excess share.
 */

bool
Group::wouldReceive()
{
    if ( isCap() ) return false;

    if ( _status != status_t::CONTRIBUTING ) {
	_state = status_t::RECEIVING;
	// cout<<"group:"<<name()<<" is able to receive, current-share="<<current_share<<endl;
    } else {
	_state = status_t::THROTTLE;
    }
   
    return _state == status_t::RECEIVING;
}


Group&
Group::reset()
{
    _share = getOriginalShare();
     
    _ratio1 = 1.0;
    _ratio2 = 0.;
    _status = status_t::RECEIVING;
    _state = status_t::THROTTLE;
    for_each( tasks().begin(), tasks().end(), Exec<Task>( &Task::resetCFSDelay ) );

    const double groupdelay = getCFSDelay();
    if ( groupdelay > 0. ) {
	std::cerr <<"group "<<name()<<" has a nonzero group delay ("<<groupdelay<<") after reseting..."<< std::endl;
    }
    return *this;
}



Group&
Group::initGroupTask()
{
    for_each( tasks().begin(), tasks().end(), Exec<Task>( &Task::initGroupTask ) );
    return *this;
}



Group&
Group::computeCFSDelay()
{
    if ( flags.trace_cfs ) {
	std::cout << "Group: " << name() << ", status=" << state_str[_status] << ", state=" << state_str[_state] << std::endl;
    }
    
    const double group_util = utilization();
    if ( group_util <= 0. || _status != status_t::THROTTLE ) return *this;

    if ( std::abs( (group_util - _share) / _share ) <= 0.01 ) {
	_ratio2 = 0.;

    } else if ( group_util > _share ) {
	_ratio1 *= _share / group_util;
	_ratio2  = ( group_util - _share * 0.95 ) / _share;

    } else if ( group_util < _share && getCFSDelay() > 0. ) {
	_ratio1 *= _share / group_util;
	_ratio2  = ( group_util -_share ) / group_util;
    }

    for_each( tasks().begin(), tasks().end(), Exec<Task>( &Task::computeCFSDelay ) );
    return *this;
}



Group& 
Group::setSpareStatus()
{
    if ( _status != status_t::RECEIVING ) return *this;

    double util = utilization();
    if ( util < 0. || 1. < util ) return *this;
    if ( util > getOriginalShare() ) {
	_status = status_t::THROTTLE;
    } else {
	_status = status_t::CONTRIBUTING;
    }
    return *this;
}



double 
Group::getCFSDelay() const
{
    return std::accumulate( tasks().begin(), tasks().end(), 0., add_using<Task>( &Task::getCFSDelay ) );
}


bool 
Group::isUtilLessThanShare() const
{
    const double util = utilization();
    return util <= _share * 0.95 && getCFSDelay() == 0 || util < _share * 1.05;
}


const Group&
Group::insertDOMResults() const
{
    _dom->setResultUtilization( utilization() );
    return *this;
}

/* ----------------------- External functions. ------------------------ */


Group *
Group::find( const std::string& group_name, unsigned int replica )
{
    const std::set<Group *>::const_iterator group = find_if( Model::__group.begin(), Model::__group.end(), EqualsReplica<Group>( group_name, replica ) );
    return group != Model::__group.end() ? *group : nullptr;
}


/*
 * Add a group to the model.   
 */

void 
Group::create( const std::pair<std::string,LQIO::DOM::Group*>& p )
{
    LQIO::DOM::Group* dom = p.second;
    const std::string& group_name = p.first;
    
    /* Extract variables from the DOM */
    CFS_Processor* processor = dynamic_cast<CFS_Processor *>(Processor::find(dom->getProcessor()->getName()));
    if (processor == nullptr) { return; }
	
    /* Check that no group was added with the existing name */
    if ( Group::find( group_name ) != nullptr ) {
	dom->runtime_error(LQIO::ERR_DUPLICATE_SYMBOL );
	return;
    } 
       	
    /* Generate a new group with the parameters and add it to the list */
    Group * group = new Group( dom, processor );

    processor->addGroup( group );
    Model::__group.insert( group );

    /* Generate a new group with the parameters and add it to the list */
    Model::__group.insert( new Group( dom, processor ) );
}
