/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/branches/merge-V5-V6/lqns/group.h $
 *
 * Groups.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 2008
 *
 * $Id: group.h 14882 2021-07-07 11:09:54Z greg $
 *
 * ------------------------------------------------------------------------
 */

#if	!defined(GROUP_H)
#define GROUP_H

#include <set>
#include <map>
#include <lqio/dom_group.h>
#include "entity.h"

class Task;
class CFS_Processor;
class DeviceEntry;
class Server;
class Group;

class Group {
private:
    Group( const Group& );
    Group& operator=( const Group& );
    
public:
    enum class status_t { RECEIVING=-1, THROTTLE=0, CONTRIBUTING=1 };
    
    Group( LQIO::DOM::Group *, const CFS_Processor * );
    virtual ~Group();
    static void create( const std::pair<std::string,LQIO::DOM::Group*>& );  

    /*  */

    bool check() const;
    /*  virtual void configure( const unsigned );
     */
    /* addTask is perform after this task is added. */
    Group& addTask( Task * task ) { _tasks.insert(task); return *this; }
    Group& removeTask( Task * task )  { _tasks.erase(task); return *this; }
    Group& recalculateDynamicValues();
    void initialize();
    void reinitialize();
    Group& reset();
    Group& initGroupTask();
    unsigned getReplicaNumber() const { return _replica_number; }

    void changeShare( double ratio ) { _share *= (1.0 + ratio); }

    /* Query */
    double getRatio1() const { return _ratio1; }
    double getRatio2() const { return _ratio2; }
    double getShare() const { return _share; }
    double getOriginalShare() const { return  _dom->getGroupShareValue(); }
    bool isReceiving() const { return _state == status_t::RECEIVING; }
    bool isContributing() const { return _state == status_t::CONTRIBUTING; }

    bool isCap() const { return _dom->getCap(); }
    bool isUtilLessThanShare() const;

    double couldContribute(); 		/* Changes _state */
    bool wouldReceive(); 		/* Changes _state */
    Group& setSpareStatus();
    double getCFSDelay() const;
 
    Group& computeCFSDelay();
    double utilization() const;

    const CFS_Processor * processor() const { return _processor; }
    const std::set<Task *>& tasks() const { return _tasks; }
    unsigned size() const { return _tasks.size(); }

    /* Printing */

    std::ostream& print( std::ostream& output ) const { return output; }
    const std::string& name() const { return _dom->getName(); }

    /* DOM insertion of results */

    virtual const Group& insertDOMResults() const;

private:
    bool hasSpareShare() const { return _status == status_t::CONTRIBUTING; }

public:
    static Group * find( const std::string&, unsigned int=1 );
private:
    static std::map<const status_t,const std::string> state_str;
    
private:
    LQIO::DOM::Group* _dom; 		/* DOM Element to Store Data	*/
    std::set<Task *> _tasks;	        /* List of processor's tasks	*/
    const CFS_Processor * _processor;
 	
    double _share;
    status_t _status;			/* State that I am moving to	*/
    status_t _state;			/* State that I am in		*/
    double _ratio1;                    	/* ratio for the processor entry. */
    double _ratio2;                     /* ratio for the adjustment, based on (util-share) */
    unsigned int _replica_number;
};

inline std::ostream& operator<<( std::ostream& output, const Group& self ) { return self.print( output ); }

#endif

