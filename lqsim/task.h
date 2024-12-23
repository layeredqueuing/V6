/* -*- c++ -*-
 * Lqsim-parasol task interface.
 *
 * $Id: task.h 17464 2024-11-13 12:55:06Z greg $
 */

/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* May 1996.								*/
/* Nov 2005.								*/
/* Nov 2024.								*/
/************************************************************************/

#ifndef	LQSIM_TASK_H
#define LQSIM_TASK_H

#include <set>
#include <vector>
#include <string>
#include <list>
#include <algorithm>
#include <lqio/dom_task.h>

#include "actlist.h"
#include "message.h"
#include "result.h"

class Activity;
class ActivityList;
class Entry;
class Group;
class Instance;
class ParentGroup;
class Processor;
class Task;
class srn_client;

#define	PRIORITY_OFFSET	10

typedef SYSCALL (*syscall_func_ptr)( double );

class Task {
    friend class Instance;

    /*
     * Compare to tasks by their name.  Used by the set class to
     * insert items
     */

    struct ltTask
    {
	bool operator()(const Task * p1, const Task * p2) const { return p1->name() < p2->name(); }
    };


    /*
     * Compare a task name to a string.  Used by the find_if (and
     * other algorithm type things).
     */

public:
    static std::set <Task *, ltTask> __tasks;	/* Task table.	*/

    /* Update service_routine in task.c when changing this enum */
    enum class Type {
	UNDEFINED,
	CLIENT,
	SERVER,
	MULTI_SERVER,
	INFINITE_SERVER,
	SYNCHRONIZATION_SERVER,
	SEMAPHORE,
	OPEN_ARRIVAL_SOURCE,
	WORKER,
	THREAD,
	TOKEN,
	TOKEN_R,
	SIGNAL,
	RWLOCK,			/* RWLOCK TASK CLASS	*/
	RWLOCK_SERVER,		/* RWLOCK SERVER TOKEN	*/
	WRITER_TOKEN,
	TIMEOUT_QUEUE,
	TIMEOUT_WORKER,
	RETRY_QUEUE,
	RETRY_WORKER
    };

    static const std::map<const Type,const std::string> type_strings;

public:
    static Task * find( const std::string& task_name );
    static Task * add( LQIO::DOM::Task* domTask );

private:
    Task( const Task& ) = delete;
    Task& operator=( const Task& ) = delete;

public:
    Task( const Type type, int priority, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup );
    virtual ~Task();

    LQIO::DOM::Task * getDOM() const{ return _dom; }

    virtual double think_time() const { abort(); return 0.0; }			/* Cached.  see create()	*/
    virtual const std::string& name() const { return _dom->getName(); }
    virtual scheduling_type discipline() const { return _dom->getSchedulingType(); }
    virtual unsigned multiplicity() const;					/* Special access!		*/
    virtual int priority() const { return _priority; }

    Type type() const { return _type; }
    const std::string& type_name() const { return type_strings.at(_type); }
    virtual bool is_not_waiting() const { return false; }

    const std::vector<Entry *>& entries() const { return _entries; }
    const std::vector<Activity *>& activities() const { return _activities; }
    const std::vector<ActivityList *>& precedences() const { return  _precedences; }

    unsigned n_entries() const { return _entries.size(); }
    unsigned max_phases() const { return _max_phases; }
    Task& max_phases( unsigned max_phases ) { _max_phases = std::max( _max_phases, max_phases ); return *this; }

    Instance * add_task ( const char *task_name, Type type, int cpu_no, Instance * rip );
    virtual int std_port() const { return -1; }
    virtual int worker_port() const { return -1; }
    int node_id() const;
    Processor * processor() const { return _processor; }
    int group_id() const { return _group_id; }
    Task& set_group_id( int group_id ) { _group_id = group_id; return *this; }
    Message * alloc_message();
    void free_message( Message * msg );

    Activity * find_activity( const std::string& activity_name ) const;

    bool is_infinite() const;
    bool is_multiserver() const { return multiplicity() > 1; }
    bool is_reference_task() const { return type() == Type::CLIENT; }
    virtual bool is_sync_server() const { return false; }
    virtual bool is_aysnc_inf_server() const { return false; }
    bool has_activities() const { return !_activities.empty(); }	/* True if activities present.	*/
    bool has_threads() const { return !_forks.empty(); }
    bool has_think_time() const;
    bool has_lost_messages() const;
    virtual bool derive_utilization() const;

    void set_start_activity( LQIO::DOM::Entry* theDOMEntry );
    Activity * add_activity( LQIO::DOM::Activity * activity );
    unsigned max_activities() const { return _activities.size(); }	/* Max # of activities.		*/
    Task& add_list( ActivityList * list ) { _precedences.push_back( list ); return *this; }
    Task& add_fork( AndForkActivityList * list ) { _forks.push_back( list ); return *this; }
    Task& add_join( AndJoinActivityList * list ) { _joins.push_back( list ); return *this; }

    virtual Task& configure();		/* Called after recalulateDynamicVariables but before create */
    virtual Task& create();
    Task& initialize();			/* Called after create() and start()	*/
    virtual bool start() = 0;
    virtual Task& kill() = 0;

    virtual Task& reset_stats();
    virtual Task& accumulate_data();
    virtual Task& insertDOMResults();
    virtual std::ostream& print( std::ostream& ) const;

protected:
    virtual void create_instance() = 0;

private:
    bool has_send_no_reply() const;

    void alloc_pool();

    double throughput() const;
    double throughput_variance() const;

private:
    LQIO::DOM::Task* _dom;			/* Stores all of the data.      */
    const int _priority;			/* Parasol priority		*/

    Processor * _processor;			/* node			        */
    int _group_id;				/* group  			*/
    syscall_func_ptr _compute_func;		/* function to use to "compute"	*/
    unsigned _active;				/* Number of active instances.	*/
    unsigned _max_phases;			/* Max # phases, this task.	*/

    std::vector<Entry *> _entries;		/* entry array		        */
    std::vector<Activity *>_activities;		/* List of activities.		*/
    std::vector<ActivityList *> _precedences;	/* activity list array 		*/
    std::vector<AndForkActivityList *> _forks;	/* List of forks for this task	*/
    std::vector<AndJoinActivityList *> _joins; 	/* List of joins for this task	*/
    std::list<Message *> _pending_msgs;		/* Messages blocked by join.	*/
    double _join_start_time;			/* non-zero if in sync-join	*/

    std::list<Message *> _free_msgs;		/* Pool of messages 		*/

protected:
    Type _type;

public:
    bool trace_flag;				/* True if task is to be traced	*/

    Histogram * _hist_data;            		/* Structure which stores histogram data for this task */
    SampleResult r_cycle;			/* Cycle time.		        */
    VariableResult r_util;			/* Utilization.		        */
    VariableResult r_group_util;		/* group Utilization.		*/

    unsigned _hold_active;			/* Number of active instances.	*/
};


class Reference_Task : public Task
{
private:
    Reference_Task( const Reference_Task& ) = delete;
    Reference_Task& operator=( const Reference_Task& ) = delete;

public:
    Reference_Task( const Type type, int priority, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup );

    virtual bool is_not_waiting() const;

    virtual double think_time() const { return _think_time; }			/* Cached.  see create()	*/

#if !BUG_289
    virtual bool start();
#endif
    virtual Reference_Task& kill();

protected:
    virtual void create_instance();

private:
    double _think_time;				/* Cached copy of think time.	*/
    std::vector<srn_client *> _clients;		/* task id's of clients		*/
};

class Server_Task : public Task
{
public:
    Server_Task( const Type type, int priority, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup );

    virtual int std_port() const;
    virtual bool is_not_waiting() const;

    void set_synchronization_server();
    virtual bool is_sync_server() const { return _sync_server; }
    virtual bool is_aysnc_inf_server() const;
    virtual int worker_port() const { return _worker_port; }

    virtual bool start();
    virtual Server_Task& kill();

protected:
    virtual void create_instance();

protected:
    Instance * _task;				/* task id of main inst	        */
    int _worker_port;				/* Port for workers to send to.	*/
    bool _sync_server;				/* True if we sync here		*/
};

class Timeout_Task : public Server_Task
{
public:
    Timeout_Task( const Task::Type type, int priority, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup );

    int ready_port() const { return _ready_port; }
    Instance * server_task() const { return _server_task; }

    virtual Timeout_Task& create();
    virtual bool start();
    virtual Timeout_Task& kill();

    virtual Timeout_Task& reset_stats();
    virtual Timeout_Task& accumulate_data();
    virtual Timeout_Task& insertDOMResults();

    virtual std::ostream& print( std::ostream& ) const;

protected:
    virtual void create_instance();

public:
    SampleResult r_timeout;			/* Service time.		*/
    SampleResult r_timeout_sqr;
    SampleResult r_timeout_cycle;		/* Cycle time.		        */
    VariableResult r_timeout_util;		/* Utilization.		        */
    SampleResult r_timeout_prob;		/* Timeout prob.	        */
    SampleResult r_forward;
    SampleResult r_calldelay;
    double _timeout;
    double _cleanup;
protected:
    Instance * _server_task;			/* 				*/
    int  _ready_port;				/* Port for server ready signals.	*/
};

class Retry_Task : public Server_Task
{
public:
    Retry_Task( const Task::Type type, int priority, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup );

    int ready_port() const { return _ready_port; }
    Instance * server_task() const { return _server_task; }

    virtual Retry_Task& create();
    virtual bool start();
    virtual Retry_Task& kill();

    virtual Retry_Task& reset_stats();
    virtual Retry_Task& accumulate_data();
    virtual Retry_Task& insertDOMResults();

    virtual std::ostream& print( std::ostream& ) const;
    bool isInfRetry() {return _maxRetries <0;}

protected:
    virtual void create_instance();

public:
    SampleResult r_nretry;			/* Mean number of retries.	*/
    SampleResult r_Yretry;
    SampleResult r_tretry;
    SampleResult r_abort_prob;			/* Abort prob.	                */
    double _sleep;
    double _cleanup;
    double _maxRetries;

protected:
    Instance * _server_task;			/* 				        */
    int  _ready_port;				/* Port for server ready signals.	*/
};


class Semaphore_Task : public Server_Task
{
public:
    Semaphore_Task( const Type type, int priority, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup );

    int signal_port() const { return _signal_port; }
    Instance * signal_task() const { return _signal_task; }

    virtual bool start();
    virtual Semaphore_Task& kill();

    virtual Semaphore_Task& reset_stats();
    virtual Semaphore_Task& accumulate_data();
    virtual Semaphore_Task& insertDOMResults();

    virtual std::ostream& print( std::ostream& ) const;

protected:
    virtual void create_instance();

public:
    SampleResult r_hold;			/* Service time.		*/
    SampleResult r_hold_sqr;			/* Service time.		*/
    VariableResult r_hold_util;

protected:
    Instance * _signal_task;			/* 				*/
    int _signal_port;				/* Port for signals to send to.	*/
};

class ReadWriteLock_Task : public Semaphore_Task
{
public:
    ReadWriteLock_Task( const Type type, int priority, LQIO::DOM::Task* domTask, Processor * aProc, Group * aGroup );

    Instance * writer() const { return _writer; }
    Instance * reader() const { return _reader; }

    int writerQ_port() const { return _writerQ_port; }	/* for RWLOCK */
    int readerQ_port() const { return _readerQ_port; }
    int signal_port2() const { return _signal_port2; }

    virtual bool start();
    virtual ReadWriteLock_Task& kill();

    virtual ReadWriteLock_Task& reset_stats();
    virtual ReadWriteLock_Task& accumulate_data();
    virtual ReadWriteLock_Task& insertDOMResults();

    virtual std::ostream& print( std::ostream& ) const;

protected:
    virtual void create_instance();

public:
    SampleResult r_reader_hold;			/* Reader holding time		*/
    SampleResult r_reader_hold_sqr;
    SampleResult r_reader_wait;			/* Reader blocked time		*/
    SampleResult r_reader_wait_sqr;
    VariableResult r_reader_hold_util;

    SampleResult r_writer_hold;			/* writer holding time		*/
    SampleResult r_writer_hold_sqr;
    SampleResult r_writer_wait;			/* writer blocked time		*/
    SampleResult r_writer_wait_sqr;
    VariableResult r_writer_hold_util;

private:
    Instance * _reader;				/* task id for readers' queue   */
    Instance * _writer;	  	        	/* task id for writer_token	*/

    int _signal_port2;				/* Signal Port for writer_token	*/
    int _writerQ_port;				/* Port for writer message queue*/
    int _readerQ_port;				/* Port for reader message queue*/

};


class Pseudo_Task : public Task
{
public:
    Pseudo_Task( const std::string& name ) : Task( Type::OPEN_ARRIVAL_SOURCE, MAX_PRIORITY, nullptr, nullptr, nullptr ), _name(name) {}

    virtual const std::string& name() const { return _name; }
    virtual scheduling_type discipline() const { return SCHEDULE_DELAY; }
    virtual unsigned multiplicity() const { return 1; }			/* Special access!		*/
    virtual int priority() const { return 0; }				/* priority		        */
    virtual double think_time() const { return 0.0; }			/* Think time for ref. tasks.	*/
    virtual void * task_element() const { return 0; }
    virtual bool derive_utilization() const { return true; }

    virtual Pseudo_Task& insertDOMResults();

    virtual bool start();
    virtual Pseudo_Task& kill();

protected:
    virtual void create_instance();

private:
    const std::string _name;
    Instance * _task;			/* task id of main inst	        */
};


extern unsigned total_tasks;
#endif
