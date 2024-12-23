/* -*- c++ -*-
 * $Id: qnio_document.cpp 17360 2024-10-12 10:59:43Z greg $
 *
 * Superclass for Queueing Network models.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * ------------------------------------------------------------------------
 * September 2022
 * ------------------------------------------------------------------------
 */

#include <cmath>
#include <lqx/SyntaxTree.h>
#include "filename.h"
#include "qnio_document.h"
#include "dom_document.h"
#include "bcmp_to_lqn.h"

QNIO::Document::Comprehension&
QNIO::Document::Comprehension::operator=( const QNIO::Document::Comprehension& src )
{
    _name = src._name;
    _begin = src._begin; 
    _step = src._step;
    _size = src._size;
    return *this;
}

/*
 * Convert a string of the form "v_1;v_2;...;v_n" to v_1, v_n, n.
 * The assumption is that all of the values "v" are monotonically
 * increasing and evenly distributed.
 */
    
void
QNIO::Document::Comprehension::convert( const std::string& s )
{
    /* tokenize the string on ';' */
    const char delim = ';';
    std::vector<double> values;
    size_t start;
    size_t finish = 0;
    while ((start = s.find_first_not_of(delim, finish)) != std::string::npos) {
	finish = s.find(delim, start);
	const std::string token = s.substr(start, finish - start);
	char * endptr = nullptr;
	values.push_back(::strtod( token.c_str(), &endptr ));
	if ( *endptr != '\0') throw std::domain_error( std::string("invalid double: ") + s );
    }
    if ( values.size() == 0 ) return;

    /* Now compute the parameters for the generator */
    _size = values.size();
    _begin = values.front();
    if ( _size > 1 ) {
	_step = (values.at(1) - _begin);
    } else {
	_step = 0;
    }
    if ( isInteger() ) {
	_step = ::rint( _step );
    }
    if ( _step == 0. ) throw std::domain_error( name() + ": step size is zero" );
}


LQX::VariableExpression *
QNIO::Document::Comprehension::getVariable() const
{
    return new LQX::VariableExpression( _name, false );
}


/*
 * Return a loop with all of the items in the the set.
 */

LQX::SyntaxTreeNode *
QNIO::Document::Comprehension::collect( std::vector<LQX::SyntaxTreeNode *>* loop_body ) const
{
#if 0
    return new LQX::LoopStatementNode( new LQX::AssignmentStatementNode( getVariable(), new LQX::ConstantValueExpression( begin() ) ),
				       new LQX::ComparisonExpression( LQX::ComparisonExpression::LESS_THAN, getVariable(), new LQX::ConstantValueExpression( end() ) ),
				       new LQX::AssignmentStatementNode( getVariable(), new LQX::MathExpression( LQX::MathExpression::ADD, getVariable(), new LQX::ConstantValueExpression( step() ) ) ),
				       new LQX::CompoundStatementNode( loop_body ) );
#else
    std::vector<LQX::SyntaxTreeNode *>* items = new std::vector<LQX::SyntaxTreeNode *>();
    items->reserve( size() );
    for ( size_t i = 0; i < size(); ++i ) {
	items->push_back( new LQX::ConstantValueExpression( begin() + i * step() ) );
    }
    return new LQX::ForeachStatementNode( "", name(), false, false, new LQX::MethodInvocationExpression( "array_create", items ), new LQX::CompoundStatementNode( loop_body ) );
#endif
}


std::ostream&
QNIO::Document::Comprehension::print( std::ostream& output ) const
{
    for ( double value = begin(); value < end(); value += step() ) {
	if ( value != begin() ) output << ";";
	output << value;
    }
    return output;
}

const std::map<QNIO::Document::Comprehension::Type,const std::string> QNIO::Document::Comprehension::__type_name = {
    { QNIO::Document::Comprehension::Type::ARRIVAL_RATES, "Arrival Rate" },
    { QNIO::Document::Comprehension::Type::CUSTOMERS,     "Customers" },
    { QNIO::Document::Comprehension::Type::DEMANDS,	  "Service Demands" },
    { QNIO::Document::Comprehension::Type::SERVERS,       "Servers" },
    { QNIO::Document::Comprehension::Type::SCALE,         "Beta" }
};

QNIO::Document::Document( const std::filesystem::path& input_file_name, const BCMP::Model& model )
    : _input_file_name(input_file_name), _comment(), _pragmas(), _bounds_only(false), _model(model),
      _input_variables(), _comprehensions()
{
    LQIO::DOM::Document::__input_file_name = input_file_name;
}

QNIO::Document::Document( const BCMP::Model& model )
    : _input_file_name(), _comment(), _pragmas(), _bounds_only(false), _model(model),
      _input_variables(), _comprehensions()
{
}

QNIO::Document::Document( const Document& document )
    : _input_file_name(), _comment(document._comment), _pragmas(document._pragmas), _bounds_only(false), _model(document._model),
      _input_variables(document._input_variables), _comprehensions(document._comprehensions)
{
}

QNIO::Document::~Document()
{
}

bool
QNIO::Document::convertToLQN( LQIO::DOM::Document& document ) const
{
    return LQIO::DOM::BCMP_to_LQN( model(), document ).convert();
}
