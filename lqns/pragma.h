/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk/lqns/pragma.h $
 *
 * Pragma processing.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 1994
 *
 * $Id: pragma.h 14185 2020-12-08 14:14:28Z greg $
 *
 * ------------------------------------------------------------------------
 */

#if	!defined(PRAGMA_H)
#define	PRAGMA_H

#include "dim.h"
#include <string>
#include <map>
#include <lqio/input.h>
#include <lqio/dom_document.h>
#include "help.h"

/* -------------------------------------------------------------------- */

class Pragma {

public:
    typedef void (Pragma::*fptr)(const std::string&);

    enum class Layering { BACKPROPOGATE_BATCHED, BATCHED, METHOD_OF_LAYERS, BACKPROPOGATE_METHOD_OF_LAYERS, SRVN, SQUASHED, HWSW };
    enum class MVA { LINEARIZER, EXACT, SCHWEITZER, FAST, ONESTEP, ONESTEP_LINEARIZER };
    enum class Multiserver { DEFAULT, CONWAY, REISER, REISER_PS, ROLIA, ROLIA_PS, BRUELL, SCHMIDT, SURI };
    enum class Overtaking { MARKOV, ROLIA, SIMPLE, SPECIAL, NONE };
#if HAVE_LIBGSL && HAVE_LIBGSLCBLAS
    enum class QuorumDistribution { DEFAULT, THREEPOINT, GAMMA, CLOSEDFORM_GEOMETRIC, CLOSEDFORM_DETRMINISTIC };
    enum class QuorumDelayedCalls { DEFAULT, KEEP_ALL, ABORT_ALL, ABORT_LOCAL_ONLY, ABORT_REMOTE_ONLY };
    enum class QuorumIdleTime { DEFAULT, JOINDELAY, ROOTENTRY };
#endif
    enum class Variance { DEFAULT, NONE, STOCHASTIC, MOL };
    enum class Threads { MAK_LUNDSTROM, HYPER, NONE };
    enum class ForceMultiserver { NONE, PROCESSORS, TASKS, ALL };

private:
    Pragma();
    virtual ~Pragma() 
	{
	    __cache = nullptr;
	}

public:
    static bool allowCycles()
	{
	    assert( __cache != nullptr );
	    return __cache->_allow_cycles;
	}

    static bool exponential_paths()
	{
	    assert( __cache != nullptr );
	    return __cache->_exponential_paths;
	}

    static bool forceMultiserver( Pragma::ForceMultiserver arg )
	{
	    assert( __cache != nullptr );
	    return (__cache->_force_multiserver != ForceMultiserver::NONE && arg == __cache->_force_multiserver)
		|| (__cache->_force_multiserver == ForceMultiserver::ALL  && arg != ForceMultiserver::NONE );
	}
    
    static bool interlock()
	{
	    assert( __cache != nullptr );
	    return __cache->_interlock;
	}
    
    static Layering layering()
	{
	    assert( __cache != nullptr );
	    return __cache->_layering;
	}

    static Multiserver multiserver()
	{
	    assert( __cache != nullptr );
	    return __cache->_multiserver;
	}

    static MVA mva() 
	{
	    assert( __cache != nullptr );
	    return __cache->_mva;
	}

    static Overtaking overtaking()
	{
	    assert( __cache != nullptr );
	    return __cache->_overtaking;
	}
    
    static bool overtaking( Overtaking arg )
	{
	    return overtaking() == arg;
	}

    static bool defaultProcessorScheduling()
	{
	    assert( __cache != nullptr );
	    return __cache->_default_processor_scheduling;
	}
    
    static bool disableProcessorCFS()
	{
	    assert( __cache != nullptr );
	    return __cache->_disable_processor_cfs;
	}
    
    static scheduling_type processorScheduling()
	{
	    assert( __cache != nullptr );
	    return __cache->_processor_scheduling;
	}
    
#if HAVE_LIBGSL && HAVE_LIBGSLCBLAS
    static QuorumDistribution getQuorumDistribution()
	{
	    assert( __cache != nullptr );
	    return __cache->_quorumDistribution;
	}
    
    static QuorumDelayedCalls getQuorumDelayedCalls()
	{
	    assert( __cache != nullptr );
	    return __cache->_quorumDelayedCalls;
	}
    static QuorumIdleTime getQuorumIdleTime()
	{
	    assert( __cache != nullptr );
	    return __cache->_quorumIdleTime;
	}
#endif
#if RESCHEDULE
    static bool getRescheduleOnAsyncSend()
	{
	    assert( __cache != nullptr );
	    return __cache->_reschedule_on_async_send;
	}
#endif

    static LQIO::severity_t severityLevel()
	{
	    assert( __cache != nullptr );
	    return __cache->_severity_level;
	}
    
    static bool spexHeader()
	{
	    assert( __cache != nullptr );
	    return __cache->_spex_header;
	}

    static bool stopOnMessageLoss()
	{
	    assert( __cache != nullptr );
	    return __cache->_stop_on_message_loss;
	}

    static double stopOnBogusUtilization()
	{
	    assert( __cache != nullptr );
	    return __cache->_stop_on_bogus_utilization;
	}

    static double tau()
	{
	    assert( __cache != nullptr );
	    return __cache->_tau;
	}

    static Threads threads()
	{
	    assert( __cache != nullptr );
	    return __cache->_threads;
	}

    static bool threads( Threads arg ) 
	{
	    return threads() == arg;
	}

    static Variance variance()
	{
	    assert( __cache != nullptr );
	    return __cache->_variance;
	}

    static bool variance( Variance arg )
	{
	    assert( __cache != nullptr );
	    return variance() == arg;
	}

    static bool entry_variance()
	{
	    assert( __cache != nullptr );
	    return __cache->_entry_variance;
	}

    static bool init_variance_only()
	{
	    assert( __cache != nullptr );
	    return __cache->_init_variance_only;
	}

private:
    void setAllowCycles(const std::string&);
    void setExponential_paths(const std::string&);
    void setForceMultiserver(const std::string&);
    void setInterlock(const std::string&);
    void setLayering(const std::string&);
    void setMultiserver(const std::string&);
    void setMva(const std::string&);
    void setOvertaking(const std::string&);
    void setProcessorScheduling(const std::string&);
#if HAVE_LIBGSL && HAVE_LIBGSLCBLAS
    void setQuorumDistribution(const std::string&);
    void setQuorumDelayedCalls(const std::string&);
    void setQuorumIdleTime(const std::string&);
#endif
#if RESCHEDULE
    void setRescheduleOnAsyncSend(const std::string&);
#endif
    void setSeverityLevel(const std::string&);
    void setSpexHeader(const std::string&);
    void setStopOnBogusUtilization(const std::string&);
    void setStopOnMessageLoss(const std::string&);
    void setTau(const std::string&);
    void setThreads(const std::string&);
    void setVariance(const std::string&);
    
    static bool isTrue(const std::string&);

public:
    static void set( const std::map<std::string,std::string>& );
    static std::ostream& usage( std::ostream&  );
    static const std::map<const std::string,const Pragma::fptr>& getPragmas() { return __set_pragma; }
    
private:
    bool _allow_cycles;
    bool _exponential_paths;
    ForceMultiserver _force_multiserver;
    bool _interlock;
    Layering  _layering;
    Multiserver _multiserver;
    MVA _mva;
    Overtaking _overtaking;
    scheduling_type _processor_scheduling;
#if HAVE_LIBGSL && HAVE_LIBGSLCBLAS
    QuorumDistribution _quorumDistribution; 
    QuorumDelayedCalls _quorumDelayedCalls;
    QuorumIdleTime _quorumIdleTime;           
#endif
#if RESCHEDULE
    bool _reschedule_on_async_send;
#endif
    LQIO::severity_t _severity_level;
    bool _spex_header;
    double _stop_on_bogus_utilization;
    bool _stop_on_message_loss;
    unsigned  _tau;
    Threads _threads;
    Variance _variance;
    /* bonus */
    bool _default_processor_scheduling;
    bool _disable_processor_cfs;
    bool _entry_variance;
    bool _init_variance_only;
    
    /* --- */

    static Pragma * __cache;
    static const std::map<const std::string,const Pragma::fptr> __set_pragma;

    static const std::map<const std::string,const Pragma::ForceMultiserver> __force_multiserver;
    static const std::map<const std::string,const Pragma::Layering> __layering_pragma;
    static const std::map<const std::string,const Pragma::MVA> __mva_pragma;
    static const std::map<const std::string,const Pragma::Multiserver> __multiserver_pragma;
    static const std::map<const std::string,const Pragma::Overtaking> __overtaking_pragma;
    static const std::map<const std::string,const scheduling_type> __processor_scheduling_pragma;
    static const std::map<const std::string,const LQIO::severity_t> __serverity_level_pragma;
    static const std::map<const std::string,const Pragma::Threads> __threads_pragma;
    static const std::map<const std::string,const Pragma::Variance> __variance_pragma;
};
#endif
