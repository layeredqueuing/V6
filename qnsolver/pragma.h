/* -*- c++ -*-
 * $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk-V6/qnsolver/pragma.h $
 *
 * Pragma processing.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * November, 2021
 *
 * $Id: pragma.h 17239 2024-05-27 14:02:21Z greg $
 *
 * ------------------------------------------------------------------------
 */

#if	!defined(PRAGMA_H)
#define	PRAGMA_H

#include <map>
#include <lqio/dom_pragma.h>
#include "model.h"

/* -------------------------------------------------------------------- */

class Pragma {

public:
    typedef void (Pragma::*fptr)(const std::string&);

private:
    Pragma();
    virtual ~Pragma()
	{
	    __cache = nullptr;
	}

public:
    static bool defaultOutput()
	{
	    assert( __cache != nullptr );
	    return __cache->_default_output;
	}

    static bool forceMultiserver()
	{
	    assert( __cache != nullptr );
	    return __cache->_force_multiserver;
	}

    static Model::HVFCFS hvfcfsAlgorithm()
	{
	    assert( __cache != nullptr );
	    return __cache->_hvfcfs_algorithm;
	}

    static Model::Multiserver multiserver()
	{
	    assert( __cache != nullptr );
	    return __cache->_multiserver;
	}

    static Model::Solver mva()
	{
	    assert( __cache != nullptr );
	    return __cache->_mva;
	}

    static void noDefaultOutput( bool enable )
	{
	    assert( __cache != nullptr );
	    __cache->_default_output = !enable;		/* true turns off default output */
	}
	    
private:
    void setDefaultOutput(const std::string&);
    void setForceMultiserver(const std::string&);
    void setHVFCFSAlgorithm(const std::string&);
    void setMVA(const std::string&);
    void setMultiserver(const std::string&);

public:
    static void set( const std::map<std::string,std::string>& );
    static std::ostream& usage( std::ostream& );
    static const std::map<const std::string,const Pragma::fptr>& getPragmas() { return __set_pragma; }

private:
    bool _default_output;			/* True to call Model::print() */
    Model::HVFCFS _hvfcfs_algorithm;		/* HVFCFS algorirth */
    bool _force_multiserver;			/* True to force all stations (except delay) to use the multisever algorithnm */
    Model::Multiserver _multiserver;		/* Multiserver algorithm */
    Model::Solver _mva;				/* Solver algorithm */

    /* --- */

    static Pragma * __cache;
    static const std::map<const std::string,const Pragma::fptr> __set_pragma;
};
#endif
