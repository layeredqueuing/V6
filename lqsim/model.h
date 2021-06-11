/* -*- c++ -*- */
/************************************************************************/
/* Copyright the Real-Time and Distributed Systems Group,		*/
/* Department of Systems and Computer Engineering,			*/
/* Carleton University, Ottawa, Ontario, Canada. K1S 5B6		*/
/* 									*/
/* September 1991.							*/
/************************************************************************/

/*
 * Global vars for simulation.
 *
 * $Id: model.h 14796 2021-06-11 14:48:04Z greg $
 */

#ifndef LQSIM_MODEL_H
#define LQSIM_MODEL_H

#include <lqsim.h>
#include <lqio/dom_document.h>
#if HAVE_REGEX_H
#include <regex.h>
#endif
#include "result.h"
#include <lqio/common_io.h>

namespace LQIO {
    namespace DOM {
	class Document;
    }
}

extern matherr_type matherr_disposition;    /* What to do on math fault     */
extern FILE * stddbg;

#if HAVE_REGEX_H
extern regex_t * processor_match_pattern;   /* Pattern to match.	    */
extern regex_t * task_match_pattern;	    /* Pattern to match.	    */
#endif

/*
 * Information unique to a particular instance of a task.
 */

extern bool abort_on_dropped_message;
extern bool reschedule_on_async_send;
extern bool messages_lost;

extern "C" void ps_genesis(void);

class Model {
    friend void ps_genesis(void);

public:
    struct simulation_parameters {
	friend class Model;
	
	static const double DEFAULT_TIME;

    simulation_parameters() : _seed(123456),
	    _run_time(50000),
	    _precision(0),
	    _max_blocks(1),
	    _initial_delay(0),
	    _block_period(50000)
	    {}

	void set( const std::map<std::string,std::string>&, double );

    private:
	bool set( unsigned long& parameter, const std::map<std::string,std::string>& pragmas, const char * value );
	bool set( double& parameter, const std::map<std::string,std::string>& pragmas, const char * value );

    private:
	static const unsigned long MAX_BLOCKS	    = 30;
	static const unsigned long INITIAL_LOOPS    = 100;

	unsigned long _seed;
	double _run_time;
	double _precision;
	unsigned long _max_blocks;
	double _initial_delay;
	double _block_period;
    };

    struct simulation_status {
	simulation_status() : _valid(false), _confidence(0.) {}
	bool _valid;
	double _confidence;
    };


private:
    Model( const Model& );
    Model& operator=( const Model& );

public:
    Model( LQIO::DOM::Document* document, const std::string&, const std::string& );
    virtual ~Model();
    
    bool operator!() const { return _document == 0; }
    bool construct();	/* Step 1 */
    bool hasVariables() const;

    bool start();
    bool reload();		/* Load results from LQX */
    bool restart();
    
    static LQIO::DOM::Document* load( const std::string&, const std::string& );
    static int genesis_task_id() { return __genesis_task_id; }
    static double block_period() { return __model->_parameters._block_period; }
    static void set_block_period( double block_period ) { __model->_parameters._block_period = block_period; }

private:
    void reset_stats();
    void accumulate_data();
    void insertDOMResults();

    bool hasOutputFileName() const { return _output_file_name.size() > 0 && _output_file_name != "="; }
    
    bool create();		/* Step 2 */

    void print();
    void print_intermediate();
    void print_raw_stats( FILE * output ) const;
    std::string createDirectory() const;
    
    bool run( int );
    static double rms_confidence();
    static double normalized_conf95( const result_t& stat );

private:
    LQIO::DOM::Document* _document;
    std::string _input_file_name;
    std::string _output_file_name;
    LQIO::DOM::CPUTime _start_time;
    simulation_parameters _parameters;
    double _confidence;
    static int __genesis_task_id;
    static Model * __model;

public:
    static double max_service;	/* Max service time found.	*/
    static LQIO::DOM::Document::InputFormat input_format;

    inline static double get_infinity()
    {
#if defined(INFINITY)
	return INFINITY;
#else
	union {
	    unsigned char c[8];
	    double f;
	} x;

#if defined(WORDS_BIGENDIAN)
	x.c[0] = 0x7f;
	x.c[1] = 0xf0;
	x.c[2] = 0x00;
	x.c[3] = 0x00;
	x.c[4] = 0x00;
	x.c[5] = 0x00;
	x.c[6] = 0x00;
	x.c[7] = 0x00;
#else
	x.c[7] = 0x7f;
	x.c[6] = 0xf0;
	x.c[5] = 0x00;
	x.c[4] = 0x00;
	x.c[3] = 0x00;
	x.c[2] = 0x00;
	x.c[1] = 0x00;
	x.c[0] = 0x00;
#endif
	return x.f;
#endif
    }
};

double square( const double arg );
#endif
