/*
 *  Intrinsics.cpp
 *  ModLang
 *
 *  Created by Martin Mroz on 26/01/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <config.h>
#include <algorithm>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include "Intrinsics.h"
#include "SymbolTable.h"
#include "Environment.h"
#include "LanguageObject.h"
#include "RuntimeFlowControl.h"
#include "Array.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace LQX {

    namespace Helpers {

	struct GetMax
	{	
	public:
	    GetMax() : _max() { _max = Symbol::encodeNull(); }
	    void operator()( const std::pair<SymbolAutoRef,SymbolAutoRef>& item ) { (*this)( item.second ); }
	    void operator()( const SymbolAutoRef& current ) { 
		if ( _max->getType() == Symbol::SYM_NULL || _max < current ) {
		    _max = Symbol::duplicate(current);
		}
	    }
	    SymbolAutoRef value() const { return _max; }

	private:
	    SymbolAutoRef _max;
	};

	struct GetMin
	{	
	public:
	    GetMin() : _min() { _min = Symbol::encodeNull();}
	    void operator()( const std::pair<SymbolAutoRef,SymbolAutoRef>& item ) { (*this)( item.second ); }
	    void operator()( const SymbolAutoRef& current ) { 
		if ( _min->getType() == Symbol::SYM_NULL || current < _min ) {
		    _min = Symbol::duplicate(current);
		}
	    }
	    SymbolAutoRef value() const { return _min; }

	private:
	    SymbolAutoRef _min;
	};

    }

    struct Random {
    private:
	static std::random_device __random_device;
	static std::mt19937 __generator;
	static std::uniform_real_distribution<double> __f;
    public:
	static double number() { return __f(__generator); }	/* effectively drand48() */
    };

    std::random_device Random::__random_device;
    std::mt19937 Random::__generator(__random_device());
    std::uniform_real_distribution<double> Random::__f(0.,1.);
	
    namespace Intrinsics {

	static double exp_rv( double a )
	{
	    return -a * log( Random::number() );
	}

	static double erlang_rv( double a, unsigned int b )
	{
	    double prod = 1.0;
	    for ( unsigned int i = 0; i < b; ++i ) {
		prod *= Random::number();
	    }
	    return -a * log( prod );
	}

	static double beta_rv( double a, double b )
	{
	    if ( a >= 1 ) {
		throw InvalidArgumentException("beta(a,b)","a >= 1");
	    } else if ( b >= 1 ) {
		throw InvalidArgumentException("beta(a,b)","b >= 1");
	    }
	    for (;;) {
		const double x = pow( Random::number(), 1/a );
		const double y = pow( Random::number(), 1/b );
		if ( x + y <= 1 ) return x / (x + y);
	    }
	}

	static double gamma_rv( double a, double b )
	{
	    if ( a <= 0 ) {
		throw InvalidArgumentException("gamma(a,b)", "a <= 0" );
	    } else if ( b <= 0 ) {
		throw InvalidArgumentException("gamma(a,b)", "b <= 0" );
	    }
	    if ( b < 1 ) {
		const double x = beta_rv(b, 1-b);
		const double y = exp_rv(1);
		return a * x * y;
	    } else if ( b == floor( b ) ) {
		return erlang_rv( a, static_cast<unsigned int>(floor( b )) );
	    } else {
		return erlang_rv( a, static_cast<unsigned int>(floor( b )) ) + gamma_rv( a, b - floor(b) );
	    }
	}

	/* This method on the other hand actually does all the heavy lifting */
	SymbolAutoRef Copyright::invoke(Environment*, std::vector<SymbolAutoRef >& )
	{
	    /* Prints out all the given values if any */
	    std::cout << "Copyright (C) 2009 Carleton University." << std::endl;
	    std::cout << "Written by Martin Mroz for C.M.Woodside and G.Franks" << std::endl;
	    return Symbol::encodeNull();
	}
    
	/* This method on the other hand actually does all the heavy lifting */
	SymbolAutoRef PrintSymbolTable::invoke(Environment* env, std::vector<SymbolAutoRef >& )
	{
	    /* Debug print the table */
	    std::stringstream ss;
	    env->getSymbolTable()->dump(ss);
	    std::cout << ss.str() << std::endl;
	    return Symbol::encodeNull();
	}
    
	/* This method on the other hand actually does all the heavy lifting */
	SymbolAutoRef PrintSpecialTable::invoke(Environment* env, std::vector<SymbolAutoRef >& )
	{
	    /* Debug print the table */
	    std::stringstream ss;
	    env->getSpecialSymbolTable()->dump(ss);
	    std::cout << ss.str() << std::endl;
	    return Symbol::encodeNull();
	}
    
    }
  
    namespace Intrinsics {
    
	/* This method on the other hand actually does all the heavy lifting */
	SymbolAutoRef Floor::invoke(Environment* , std::vector<SymbolAutoRef >& args)
	{
	    /* Debug print the table */
	    double arg = decodeDouble(args, 0);
	    return Symbol::encodeDouble(floor(arg));
	}
    
	/* This method on the other hand actually does all the heavy lifting */
	SymbolAutoRef Ceil::invoke(Environment* , std::vector<SymbolAutoRef >& args)
	{
	    /* Debug print the table */
	    double arg = decodeDouble(args, 0);
	    return Symbol::encodeDouble(ceil(arg));
	}
    
	/* This method on the other hand actually does all the heavy lifting */
	SymbolAutoRef Abs::invoke(Environment* , std::vector<SymbolAutoRef >& args)
	{
	    /* Debug print the table */
	    double arg = decodeDouble(args, 0);
	    return Symbol::encodeDouble(fabs(arg));
	}
    
	/* This method on the other hand actually does all the heavy lifting */
	SymbolAutoRef Pow::invoke(Environment* , std::vector<SymbolAutoRef >& args)
	{
	    /* Debug print the table */
	    double arg1 = decodeDouble(args, 0);
	    double arg2 = decodeDouble(args, 1);
	    return Symbol::encodeDouble(pow(arg1, arg2));
	}
    
	SymbolAutoRef Rand::invoke(Environment* , std::vector<SymbolAutoRef >& )
	{
	    return Symbol::encodeDouble(Random::number());		/* We use this because others may use drand48() */
	}

	SymbolAutoRef Exp::invoke(Environment* , std::vector<SymbolAutoRef >& args)
	{
	    double arg1 = decodeDouble(args, 0);
	    return Symbol::encodeDouble(exp(arg1));
	}

	SymbolAutoRef Log::invoke(Environment* , std::vector<SymbolAutoRef >& args)
	{
	    double arg1 = decodeDouble(args, 0);
	    if ( arg1 < 0 ) throw InvalidArgumentException("log(a)", "a < 0");
	    return Symbol::encodeDouble(log(arg1));
	}

	SymbolAutoRef Max::invoke(Environment*, std::vector<SymbolAutoRef >& args)
	{
	    SymbolAutoRef max = Symbol::encodeNull();
	    ArrayObject* arrayObject;
	    if ( args.size() == 1 && args[0]->getType() == Symbol::SYM_OBJECT && (arrayObject = dynamic_cast<ArrayObject *>(args[0]->getObjectValue())) != NULL) {
		max = for_each( arrayObject->begin(), arrayObject->end(), Helpers::GetMax() ).value();
	    } else {
		max = for_each( args.begin(), args.end(), Helpers::GetMax() ).value();
	    }
	    return max;
	}
    
	SymbolAutoRef Min::invoke(Environment*, std::vector<SymbolAutoRef >& args)
	{
	    SymbolAutoRef min = Symbol::encodeNull();
	    ArrayObject* arrayObject;
	    if ( args.size() == 1 && args[0]->getType() == Symbol::SYM_OBJECT && (arrayObject = dynamic_cast<ArrayObject *>(args[0]->getObjectValue())) != NULL) {
		min = for_each( arrayObject->begin(), arrayObject->end(), Helpers::GetMin() ).value();
	    } else {
		min = for_each( args.begin(), args.end(), Helpers::GetMin() ).value();
	    }
	    return min;
	}
    
	SymbolAutoRef Round::invoke(Environment* , std::vector<SymbolAutoRef >& args)
	{
	    double arg1 = decodeDouble(args, 0);
	    return Symbol::encodeDouble(round(arg1));
	}

	SymbolAutoRef Sqrt::invoke(Environment* , std::vector<SymbolAutoRef >& args)
	{
	    double arg1 = decodeDouble(args, 0);
	    if ( arg1 < 0 ) throw InvalidArgumentException("sqrt(a)", "a < 0");
	    return Symbol::encodeDouble(sqrt(arg1));
	}

	SymbolAutoRef Normal::invoke(Environment* , std::vector<SymbolAutoRef >& args)
	{
	    /* See Jain, Pg 494 - Convolution method with n = 12 */
	    const double mean = decodeDouble(args, 0);
	    const double stddev = decodeDouble(args, 1);
	    
	    double sum = 0.0;
	    for ( unsigned int i = 0; i < 12; ++i ) {
		sum += Random::number();
	    }

	    return Symbol::encodeDouble( mean + stddev * (sum - 6) );
	}

	SymbolAutoRef Gamma::invoke( Environment *, std::vector<SymbolAutoRef>& args )
	{
	    const double mean = decodeDouble( args, 0 );
	    const double b = decodeDouble( args, 1 );	// shape 
	    if ( mean <= 0 ) {
		throw InvalidArgumentException("gamma(a,b)", "a <= 0" );
	    } else if ( b <= 0 ) {
		throw InvalidArgumentException("gamma(a,b)", "b <= 0" );
	    }
	    const double a = mean / b;
	    return Symbol::encodeDouble( gamma_rv( a, b ) );
	}

	SymbolAutoRef Uniform::invoke( Environment *, std::vector<SymbolAutoRef>& args )
	{
	    const double low = decodeDouble( args, 0 );
	    const double high = decodeDouble( args, 1 );	// shape 
	    if ( low >= high ) throw InvalidArgumentException("uniform(a,b)", "a >= b" );
	    return Symbol::encodeDouble( Random::number() *  ( high - low ) + low );
	}

	SymbolAutoRef Poisson::invoke( Environment *, std::vector<SymbolAutoRef>& args )
	{
	    const double mean = decodeDouble( args, 0 );
	    const double limit = exp(-mean);
	    double prod = 1;
	    unsigned int i = 0;
	    while ( prod >= limit ) {
		prod *= Random::number();
		i += 1;
	    }
	    return Symbol::encodeDouble( static_cast<double>(i) );
	}
    }
  
    namespace Intrinsics {
    
	/* This method on the other hand actually does all the heavy lifting */
	SymbolAutoRef Str::invoke(Environment*, std::vector<SymbolAutoRef >& args)
	{
	    /* Result string goes here */
	    std::stringstream ss;
      
	    /* Prints out all the given values if any */
	    std::vector<SymbolAutoRef>::iterator iter;
	    for (iter = args.begin(); iter != args.end(); ++iter) {
		ss << *iter;
	    } 
      
	    /* Pass the arguments up to strcmp */
	    return Symbol::encodeString(ss.str().c_str(), false);
	}
    
	/* This method on the other hand actually does all the heavy lifting */
	SymbolAutoRef Double::invoke(Environment*, std::vector<SymbolAutoRef >& args)
	{
	    SymbolAutoRef& current = args[0];
	    double decodedValue = 0.0;
	    bool got = true;
      
	    /* Try to do the type conversion */
	    switch (current->getType()) {
	    case Symbol::SYM_BOOLEAN: {
		decodedValue = (current->getBooleanValue() == true) ? 1.0 : 0.0;
		break;
	    } 
	    case Symbol::SYM_DOUBLE: {
		decodedValue = current->getDoubleValue();
		break;
	    } 
	    case Symbol::SYM_STRING: {
		const char* rawValue = current->getStringValue();
		if (!isdigit(rawValue[0])) { got = false; break; }
		char* endPtr = NULL;
		decodedValue = strtod(rawValue, &endPtr);
		if (endPtr[0] != '\0') { got = false; }
		break;
	    } 
	    case Symbol::SYM_NULL: {
		decodedValue = 0.0;
		break;
	    } 
	    case Symbol::SYM_OBJECT: {
		got = false;
		break;
	    } 
	    default: {
		throw InternalErrorException("Unsupported type passed to str() function.");
		break;
	    }
	    }
      
	    /* We didn't get anything good here */
	    if (got == false) {
		return Symbol::encodeNull();
	    }
      
	    /* Return the decoded value from the provided input */
	    return Symbol::encodeDouble(decodedValue);
	}
    
	/* This method on the other hand actually does all the heavy lifting */
	SymbolAutoRef Boolean::invoke(Environment*, std::vector<SymbolAutoRef >& args)
	{
	    SymbolAutoRef& current = args[0];
	    bool decodedValue = false;
	    bool got = true;
      
	    /* Try to do the type conversion */
	    switch (current->getType()) {
	    case Symbol::SYM_BOOLEAN: {
		decodedValue = current->getBooleanValue();
		break;
	    } 
	    case Symbol::SYM_DOUBLE: {
		decodedValue = current->getDoubleValue() != 0.0;
		break;
	    } 
	    case Symbol::SYM_STRING: {
		const char* rawValue = current->getStringValue();
		if (!strcasecmp(rawValue, "true") || !strcasecmp(rawValue, "yes")) { decodedValue = true; }
		else if (!strcasecmp(rawValue, "false") || !strcasecmp(rawValue, "no")) { decodedValue = false; }
		else { got = false; }
		break;
	    } 
	    case Symbol::SYM_NULL: {
		decodedValue = false;
		break;
	    } 
	    case Symbol::SYM_OBJECT: {
		got = false;
		break;
	    } 
	    default: {
		throw InternalErrorException("Unsupported type passed to str() function.");
		break;
	    }
	    }
      
	    /* We didn't get anything good here */
	    if (got == false) {
		return Symbol::encodeNull();
	    }
      
	    /* Return the decoded value from the provided input */
	    return Symbol::encodeBoolean(decodedValue);      
	}
    
    
    }
  
    namespace Intrinsics {
    
	SymbolAutoRef Abort::invoke(Environment*, std::vector<SymbolAutoRef >& args)
	{
	    const char* reason = decodeString(args, 1);
	    double code = decodeDouble(args, 0);
	    throw AbortException(reason, code);
	}
    
	SymbolAutoRef Assert::invoke(Environment*, std::vector<SymbolAutoRef >& args)
	{
	    SymbolAutoRef& symbol = args[0];
	    Symbol::Type t = symbol->getType();
	    if ( symbol->getType() != Symbol::SYM_BOOLEAN || symbol->getBooleanValue() != true ) {
		throw AbortException("Assert failed.", 0);
	    }
	    return Symbol::encodeNull();
	}
    
	SymbolAutoRef TypeID::invoke(Environment*, std::vector<SymbolAutoRef >& args)
	{
	    /* Find out what the argument here is */
	    SymbolAutoRef& symbol = args[0];
	    Symbol::Type t = symbol->getType();
      
	    /* If this is an object, return the Object Type ID */
	    if (t == Symbol::SYM_OBJECT) {
		LanguageObject* lo = symbol->getObjectValue();
		return Symbol::encodeDouble(lo->getTypeId());
	    }
      
	    /* All of the language types are negative */
	    return Symbol::encodeDouble(-1 * static_cast<double>(t));
	}
    
    }

    void RegisterIntrinsics(MethodTable* table)
    {
	/* Register all of the intrinsic methods in the table */
	table->registerMethod(new Intrinsics::Copyright());
	table->registerMethod(new Intrinsics::PrintSymbolTable());
	table->registerMethod(new Intrinsics::PrintSpecialTable());
	table->registerMethod(new Intrinsics::Abs());
	table->registerMethod(new Intrinsics::Ceil());
	table->registerMethod(new Intrinsics::Exp());
	table->registerMethod(new Intrinsics::Floor());
	table->registerMethod(new Intrinsics::Log());
	table->registerMethod(new Intrinsics::Max());
	table->registerMethod(new Intrinsics::Min());
	table->registerMethod(new Intrinsics::Pow());
	table->registerMethod(new Intrinsics::Round());
	table->registerMethod(new Intrinsics::Sqrt());
	table->registerMethod(new Intrinsics::Rand());
	table->registerMethod(new Intrinsics::Normal());
	table->registerMethod(new Intrinsics::Gamma());
	table->registerMethod(new Intrinsics::Poisson());
	table->registerMethod(new Intrinsics::Uniform());
	table->registerMethod(new Intrinsics::Str());
	table->registerMethod(new Intrinsics::Double());
	table->registerMethod(new Intrinsics::Boolean());
	table->registerMethod(new Intrinsics::Assert());
	table->registerMethod(new Intrinsics::Abort());
	table->registerMethod(new Intrinsics::TypeID());
    }
  
    static inline void registerConstantDouble(SymbolTable* symbolTable, const std::string& name, double value)
    {
	/* Define and set up the constant */
	symbolTable->define(name);
	SymbolAutoRef sar = symbolTable->get(name);
	sar->assignDouble(value);
	sar->setIsConstant(true);
    }
  
    void RegisterIntrinsicConstants(SymbolTable* symbolTable)
    {
	/* Register the variable type constants for all of the intrinsic types. Classes have to deal with their own... */
	registerConstantDouble(symbolTable, "@type_un",      -1 * static_cast<double>(Symbol::SYM_UNINITIALIZED));
	registerConstantDouble(symbolTable, "@type_boolean", -1 * static_cast<double>(Symbol::SYM_BOOLEAN));
	registerConstantDouble(symbolTable, "@type_double",  -1 * static_cast<double>(Symbol::SYM_DOUBLE));
	registerConstantDouble(symbolTable, "@type_string",  -1 * static_cast<double>(Symbol::SYM_STRING));
	registerConstantDouble(symbolTable, "@type_null",    -1 * static_cast<double>(Symbol::SYM_NULL));
    
	/* Register the Infinity Constant */
	registerConstantDouble(symbolTable, "@infinity", std::numeric_limits<double>::infinity());
    }
}
