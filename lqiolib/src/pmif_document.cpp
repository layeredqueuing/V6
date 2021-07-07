/* -*- c++ -*-
 * $Id: pmif_document.cpp 14882 2021-07-07 11:09:54Z greg $
 *
 * Handle Performace Model Interface Format for feeding into the MVA solver.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * ------------------------------------------------------------------------
 * July 2016
 * ------------------------------------------------------------------------
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "pmif_document.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>

std::map<const std::string,LQIO::PMIF_Document::ImportModel,LQIO::PMIF_Document::ImportModel> LQIO::PMIF_Document::__model;
std::map<const std::string,LQIO::PMIF_Document::ImportModel,LQIO::PMIF_Document::ImportModel> LQIO::PMIF_Document::__node;
std::map<const std::string,LQIO::PMIF_Document::ImportModel,LQIO::PMIF_Document::ImportModel> LQIO::PMIF_Document::__workload;
std::map<const std::string,LQIO::PMIF_Document::ImportModel,LQIO::PMIF_Document::ImportModel> LQIO::PMIF_Document::__service_request;

std::map<const std::string,scheduling_type> LQIO::PMIF_Document::__scheduling_type;
std::map<const std::string,double> LQIO::PMIF_Document::__time_scaling;


static const char * const XArc		       	= "Arc";
static const char * const XClosedWorkload	= "ClosedWorkload";
static const char * const XDemandServiceRequest	= "DemandServiceRequest";
static const char * const XName			= "Name";
static const char * const XNode			= "Node";
static const char * const XOpenWorkload		= "OpenWorkload";
static const char * const XQueueingNetworkModel = "QueueingNetworkModel";
static const char * const XSchedulingPolicy	= "SchedulingPolicy";
static const char * const XServer		= "Server";
static const char * const XServiceRequest	= "ServiceRequest";
static const char * const XSinkNode		= "SinkNode";
static const char * const XSourceNode		= "SourceNode";
static const char * const XThinkTime		= "ThinkTime";
static const char * const XTimeServiceRequest   = "TimeServiceRequest";
static const char * const XTimeUnits		= "TimeUnits";
static const char * const XWorkUnitServer	= "WorkUnitServer";
static const char * const XWorkUnitServiceRequest = "WorkUnitServiceRequest";
static const char * const XWorkload		= "Workload";

namespace LQIO {

    const char * const toolname = "xx";

    PMIF_Document::PMIF_Document( const std::string& input_file_name ) : _input_file_name(input_file_name), _document()
    {
	init_tables();
    }


    /* 
     * Construct root elements 
     */
    
    PMIF_Document& PMIF_Document::initialize()
    {
	rapidxml::xml_node<> * declaration = _document.allocate_node( rapidxml::node_declaration );
	appendAttribute( declaration, "version", "1.0" );
	appendAttribute( declaration, "encoding", "utf-8" );
	_document.append_node( declaration );
	
	rapidxml::xml_node<> * root = _document.allocate_node( rapidxml::node_element, XQueueingNetworkModel );
	appendAttribute( root, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance" );
	    appendAttribute( root, "xsi:noNamespaceSchemaLocation", "" );
	    appendAttribute( root, XName, "" );
	    _document.append_node( root );
	
	    root->append_node( _document.allocate_node( rapidxml::node_element, XNode ) );
	    root->append_node( _document.allocate_node( rapidxml::node_element, XWorkload ) );
	    root->append_node( _document.allocate_node( rapidxml::node_element, XServiceRequest ) );
	    return *this;
    }
    
    PMIF_Document& PMIF_Document::addClosedChain( const std::string& name, unsigned int customers, double think_time )
    {
	addClosedChain( name, ClosedChain( _document, name, customers, name, think_time ) );
	return *this;
    }

    PMIF_Document::Station& PMIF_Document::addStation( const std::string& name, scheduling_type scheduling, unsigned int multiplicity )
    {
	return addStation( name, Station( _document, name, scheduling, multiplicity ) );
    }
}

namespace LQIO {
    
    bool PMIF_Document::parse()
    {
	struct stat statbuf;
	bool rc = true;
	int input_fd = -1;

	if ( _input_file_name ==  "-" ) {
	    input_fd = fileno( stdin );
	} else if ( ( input_fd = open( _input_file_name.c_str(), O_RDONLY ) ) < 0 ) {
	    std::cerr << toolname << ": Cannot open input file " << _input_file_name << " - " << strerror( errno ) << std::endl;
	    return false;
	}

	if ( isatty( input_fd ) ) {
	    std::cerr << toolname << ": Input from terminal is not allowed." << std::endl;
	    return false;
	} else if ( fstat( input_fd, &statbuf ) != 0 ) {
	    std::cerr << toolname << ": Cannot stat " << _input_file_name << " - " << strerror( errno ) << std::endl;
	    return false;
#if defined(S_ISSOCK)
	} else if ( !S_ISREG(statbuf.st_mode) && !S_ISFIFO(statbuf.st_mode) && !S_ISSOCK(statbuf.st_mode) ) {
#else
	} else if ( !S_ISREG(statbuf.st_mode) && !S_ISFIFO(statbuf.st_mode) ) {
#endif
	    std::cerr << toolname << ": Input from " << _input_file_name << " is not allowed." << std::endl;
	    return false;
	}

	char * buffer = static_cast<char *>(malloc( statbuf.st_size + 1) );
	if ( read( input_fd, buffer, statbuf.st_size ) == statbuf.st_size ) {
	    buffer[statbuf.st_size] = '\0';	/* Null terminate */
	    try {
		_document.parse<0>( buffer );
//	if ( strcmp( root->name(), XQueueingNetworkModel ) != 0 ) throw std::runtime_error( "" );
		handleModel( _document.first_node() );
	    }
	    catch ( const std::runtime_error& e ) {
		std::cerr << toolname << ": " << e.what() << std::endl;
	    }
	} else {
//		std::cerr << toolname << ": Read error on " << _input_file_name << " - " << strerror( errno ) << std::endl;
	}
	free( buffer );
	close( input_fd );
	return rc;
    }

    /* Generic member function for routing elements in tables */
    
    void PMIF_Document::handleChildren( rapidxml::xml_node<> * root, const std::map<const std::string,ImportModel,ImportModel>& table )
    {
	std::cerr << "<" << root->name() << ">" << std::endl;
	for ( rapidxml::xml_node<> * node = root->first_node(); node; node = node->next_sibling() ) {
	    const std::string name(node->name());
	    std::map<const std::string,ImportModel,ImportModel>::const_iterator j = table.find( name );
	    if ( j == table.end() ) {
		error( "Element <%s>, unknown child: \"%s\"", root->name(), name.c_str() );
	    } else {
		j->second( *this, node );
	    }
	}
	std::cerr << "</" << root->name() << ">" << std::endl;
    }

    
    void PMIF_Document::handleSource( rapidxml::xml_node<> * root )
    {
	/* Nop */
    }


    void PMIF_Document::handleSink( rapidxml::xml_node<> * root )
    {
	/* Nop */
    }


    void PMIF_Document::handleClosedWorkload( rapidxml::xml_node<> * root )
    {
	std::cerr << "<" << root->name() << ">" << std::endl;
	addClosedChain( getString( root, "WorkloadName" ), ClosedChain( root ) );
	std::cerr << "</" << root->name() << ">" << std::endl;
    }

    void PMIF_Document::handleOpenWorkload( rapidxml::xml_node<> * root )
    {
	std::cerr << "<" << root->name() << ">" << std::endl;
	const std::string name = getString( root, "WorkloadName" );
	std::pair<std::string,PMIF_Document::OpenChain> item( name, OpenChain( root ) );
	if ( !_open_chains.insert( item ).second ) {
	    error( "Duplicate workload name: %s", name.c_str() );
	}
	std::cerr << "</" << root->name() << ">" << std::endl;
    }
	
    void PMIF_Document::handleXXXServiceRequest( rapidxml::xml_node<> * root )
    {
	std::string name = root->name();
	std::cerr << "<" << name << ">" << std::endl;
	/* Handle atttributes */

	const std::string station = getString( root, "ServerID" );
	const std::map<std::string,PMIF_Document::Station,PMIF_Document::Station>::iterator m = _stations.find( station );
	if ( m == _stations.end() ) {
	    error( "Undefined station: %s", station.c_str() );
	}

	const std::string chain = getString( root, "WorkloadName" );
	if ( _closed_chains.find( chain ) == _closed_chains.end() && _open_chains.find( chain ) == _open_chains.end() ) {
	    error( "Undefined workload: %s", chain.c_str() );
	}

	PMIF_Document::Station::Demand& demand = m->second.addDemand( chain, root );

	/* Handle Children */
	
	for ( rapidxml::xml_node<> * node = root->first_node(); node; node = node->next_sibling() ) {
	    const std::string name(node->name());
	    if ( name != "Transit" ) {
		error( "Element <%s>, unknown child: \"%s\"", root->name(), name.c_str() );
	    } else {
		handleTransit( demand, node );
	    }
	}
	std::cerr << "</" << root->name() << ">" << std::endl;
    }

    void PMIF_Document::handleTransit( Station::Demand& demand, rapidxml::xml_node<> * root )
    {
	std::cerr << "<" << root->name() << ">" << std::endl;
	std::string destination = getString( root, "To" );
	double probability = getUnsignedDouble( root, "Probability" );
	if ( probability < 0.0 || 1.0 < probability ) {
	    error( "Invalid probability %f for transit to %s for workload %s", probability, destination.c_str(), demand.getName().c_str() );
	}
	std::pair<std::string,double> item( destination, probability );
	if ( !demand.addTransit( item ) ) {
	    error( "Duplicate transit to %s for workload %s", destination.c_str(), demand.getName().c_str() );
	}
	std::cerr << "</" << root->name() << ">" << std::endl;
    }
}

namespace LQIO {

    const std::string PMIF_Document::XMLNode::getName() const
    {
	if ( _node ) {
	    return getString( _node, XName );
	} else {
	    return std::string( "" );
	}
    }


    void PMIF_Document::XMLNode::appendNode( rapidxml::xml_node<> * child )
    {
	rapidxml::xml_node<> * node = const_cast<rapidxml::xml_node<> *>(_node);
	node->append_node( child );
    }
    
    void PMIF_Document::XMLNode::appendAttribute( const char * const attribute, const char * const value )
    {
	rapidxml::xml_document<> * document = _node->document();
	rapidxml::xml_node<> * node = const_cast<rapidxml::xml_node<> *>(_node);
	node->append_attribute( document->allocate_attribute( attribute, document->allocate_string( value ) ) );
    }

    void PMIF_Document::XMLNode::appendThisNodeTo( rapidxml::xml_document<>& document, const char * const element )
    {
	rapidxml::xml_node<> * root = document.first_node( XQueueingNetworkModel );
	rapidxml::xml_node<> * parent = root->first_node( element );
	parent->append_node( const_cast<rapidxml::xml_node<> *>(_node) );
    }
}

/* ------------------------------------------------------------------------ */
/* Stations								    */
/* ------------------------------------------------------------------------ */
namespace LQIO {
    
    PMIF_Document::Station::Station( const rapidxml::xml_node<>* node ) :
	XMLNode(node),
	_scheduling( getSchedulingPolicy( node, XSchedulingPolicy ) ),
	_multiplicity( getUnsignedInteger( node, "Quantity" ) )
    {
    }

    PMIF_Document::Station::Station( rapidxml::xml_document<>& document, const std::string& name, scheduling_type scheduling, unsigned int multiplicity ) :
	XMLNode( document.allocate_node( rapidxml::node_element, document.allocate_string( XServer ) ) ),
	_scheduling( scheduling ),
	_multiplicity( multiplicity )
    {
	appendThisNodeTo( document, XNode );	/* Do first to initialize document! */

	char buf[32];
	appendAttribute( XName, name.c_str() );
	snprintf( buf, 32, "%u", multiplicity );
	appendAttribute( "Quantity", buf );
	std::map<const std::string,scheduling_type>::const_iterator attr = find_if( __scheduling_type.begin(), __scheduling_type.end(), PolicyIs( scheduling ) );
	if ( attr !=  __scheduling_type.end() ) {
	    appendAttribute( XSchedulingPolicy, attr->first.c_str() );
	} else {
	    appendAttribute( XSchedulingPolicy, "IS" );		/* punt */
	}
    }

    PMIF_Document::Station::Demand& PMIF_Document::Station::addDemand( const std::string& chain, const rapidxml::xml_node<>* node )
    {
	std::pair<std::string,PMIF_Document::Station::Demand> item( chain, Station::Demand( node ) );
	std::pair<std::map<std::string,PMIF_Document::Station::Demand>::iterator,bool> result = _demands.insert( item );
	if ( !result.second ) {
	    error( "Duplicate demand %s for station %s", chain.c_str(), getName().c_str() );
	} 
	return result.first->second;
    }

    PMIF_Document::Station::Demand& PMIF_Document::Station::addDemand( const std::string& chain, double visits, double service_time )
    {
	std::pair<std::string,PMIF_Document::Station::Demand> item( chain, Station::Demand( getDocument(), *this, chain, visits, service_time ) );
	std::pair<std::map<std::string,PMIF_Document::Station::Demand>::iterator,bool> result = _demands.insert( item );
	if ( !result.second ) {
	    error( "Duplicate demand %s for station %s", chain.c_str(), getName().c_str() );
	} 
	return result.first->second;
    }
}

/* ------------------------------------------------------------------------ */
/* Demands								    */
/* ------------------------------------------------------------------------ */
namespace LQIO {

    PMIF_Document::Station::Demand::Demand( const rapidxml::xml_node<>* node ) :
	XMLNode(node),
	_demand( getOptionalUnsignedDouble( node, "ServiceDemand" ) ),
	_visits( getOptionalUnsignedDouble( node, "NumberOfVisits" ) ),
	_service_time( getOptionalUnsignedDouble( node, "ServiceTime" ) ),
	_scaling( getOptionalTimeScaling( node, XTimeUnits ) ),
	_transit()
    {
	rapidxml::xml_node<>* parent = node->parent();
	const bool work_unit_service_request = (strcmp( node->name(), XWorkUnitServiceRequest ) == 0);
	if ( work_unit_service_request ) {
	    const bool work_unit_server = (strcmp( parent->name(), XWorkUnitServer ) == 0 );
	    if ( !work_unit_server ) {
		error( "Server for element %s is not a WorkUnitServer", node->name() );
	    } 
	    _demand = getUnsignedDouble( parent, "ServiceTime" );
	    _scaling = getOptionalTimeScaling( parent, XTimeUnits );
	}
    }

    PMIF_Document::Station::Demand::Demand( rapidxml::xml_document<>& document, PMIF_Document::Station& station, const std::string& chain, double visits, double service_time ) :
	XMLNode( document.allocate_node( rapidxml::node_element, document.allocate_string( XTimeServiceRequest ) ) ),
	_demand( visits * service_time ),
	_visits( visits ),
	_service_time( service_time ),
	_scaling( 1.0 ),
	_transit()
    {
	station.appendNode( const_cast<rapidxml::xml_node<> *>(_node) );

	char buf[32];
	snprintf( buf, 32, "%.8g", _demand );
	appendAttribute( "ServiceDemand", buf );
	snprintf( buf, 32, "%.8g", service_time );
	appendAttribute( "ServiceTime", buf );
	snprintf( buf, 32, "%.8g", visits );
	appendAttribute( "NumberOfVisits", buf );

    }
}

/* ------------------------------------------------------------------------ */
/* Chains								    */
/* ------------------------------------------------------------------------ */
namespace LQIO {
	
    PMIF_Document::ClosedChain::ClosedChain( const rapidxml::xml_node<>* node ) : 
	Chain( node ),
	_customers( getUnsignedInteger( node, "NumberOfJobs" ) ),
	_think_device( getString( node, "ThinkDevice" ) ),
	_think_time( getUnsignedDouble( node, XThinkTime ) ),
	_scaling( getOptionalTimeScaling( node, XTimeUnits ) )
    {
    }

    PMIF_Document::ClosedChain::ClosedChain( rapidxml::xml_document<>& document, const std::string& name, unsigned int customers, const std::string& device, double think_time ) :
	Chain( document.allocate_node( rapidxml::node_element, document.allocate_string( XClosedWorkload ) ) ),
	_customers( customers ),
	_think_device( device ),
	_think_time( think_time ),
	_scaling(1.0)
    {
	appendThisNodeTo( document, XWorkload );		/* Do first! */
	
	char buf[32];
	snprintf( buf, 32, "%u", customers );
	appendAttribute( XName, name.c_str() );
	appendAttribute( "NumberOfJobs", buf );
	appendAttribute( "ThinkDevice", device.c_str() );
	snprintf( buf, 32, "%.8g", think_time );
	appendAttribute( XThinkTime, buf );
    }
    

    PMIF_Document::OpenChain::OpenChain( const rapidxml::xml_node<>* node ) : Chain( node )
    {
    }
}

namespace LQIO {

    std::string PMIF_Document::getString( const rapidxml::xml_node<> * node, const char * name )
    {
	const rapidxml::xml_attribute<> * attr = node->first_attribute( name );
	if ( !attr ) {
	    error( "Missing attribute: %s", name );
	} else if ( node->last_attribute( name ) != attr ) {
	    error( "Duplicate attribute: %s", name );
	}
	return std::string( attr->value() );
    }

    
    unsigned int PMIF_Document::getUnsignedInteger( const rapidxml::xml_node<> * node, const char * name )
    {
	const rapidxml::xml_attribute<> * attr = node->first_attribute( name );
	if ( !attr ) {
	    error( "Missing attribute: %s", name );
	} else if ( node->last_attribute( name ) != attr ) {
	    error( "Duplicate attribute: %s", name );
	}
	char * endptr = 0;
	const unsigned int value = strtol( attr->value(), &endptr, 10 );
	if ( (endptr != NULL && *endptr != '\0') ) {
	    error( "Invalid non-negative integer: %s=\"%s\"", name, attr->value() );
	}
	return value;
    }


    double PMIF_Document::getUnsignedDouble( const rapidxml::xml_node<> * node, const char * name )
    {
	const rapidxml::xml_attribute<> * attr = node->first_attribute( name );
	if ( !attr ) {
	    error( "Missing attribute: %s", name );
	}
	return getOptionalUnsignedDouble( node, name );
    }

    double PMIF_Document::getOptionalUnsignedDouble( const rapidxml::xml_node<> * node, const char * name )
    {
	const rapidxml::xml_attribute<> * attr = node->first_attribute( name );
	if ( !attr ) {
	    return 0.;
	} else if ( node->last_attribute( name ) != attr ) {
	    error( "Duplicate attribute: %s", name );
	}
	char * endptr = 0;
	const double value = strtod( attr->value(), &endptr );
	if ( (endptr != NULL && *endptr != '\0') || value < 0.0 ) {
	    error( "Invalid non-negative double: %s=\"%s\"", name, attr->value() );
	}
	return value;
    }

    double PMIF_Document::getOptionalTimeScaling( const rapidxml::xml_node<> * node, const char * name )
    {
	const rapidxml::xml_attribute<> * attr = node->first_attribute( name );
	double value = 1.0;
	if ( !attr ) {
	    return value;
	} else if ( node->last_attribute( name ) != attr ) {
	    error( "Duplicate attribute: %s", name );
	}
	std::map<const std::string,double>::const_iterator i = __time_scaling.find( attr->value() );
	if ( i != __time_scaling.end() ) {
	    value = i->second;
	} else {
	    error( "Invalid time unit: %s=\"%s\"", name, attr->value() );
	}
	return value;
    }

    scheduling_type PMIF_Document::getSchedulingPolicy( const rapidxml::xml_node<> * node, const char * name )
    {
	const rapidxml::xml_attribute<> * attr = node->first_attribute( name );
	scheduling_type scheduling = SCHEDULE_DELAY;
	if ( !attr ) {
	    error( "Missing attribute: %s", name );
	} else if ( node->last_attribute( name ) != attr ) {
	    error( "Duplicate attribute: %s", name );
	} else {
	    std::map<const std::string,scheduling_type>::const_iterator i = __scheduling_type.find( attr->value() );
	    if ( i != __scheduling_type.end() ) {
		scheduling = i->second;
	    } else {
		error( "Invalid attribute: %s=\"%s\"", name, attr->value() );
	    }
	}
	return scheduling;
    }

    PMIF_Document& PMIF_Document::addClosedChain( const std::string& name, const ClosedChain& chain )
    {
	std::pair<std::string,PMIF_Document::ClosedChain> item( name, chain );
	if ( !_closed_chains.insert( item ).second ) {
	    error( "Duplicate workload name: %s", name.c_str() );
	}
	return *this;
    }

    PMIF_Document::Station& PMIF_Document::addStation( const std::string& name, const Station& station )
    {
    	std::pair<std::string,PMIF_Document::Station> item( name, station );
	const std::pair<std::map<std::string,PMIF_Document::Station>::iterator,bool> result = _stations.insert( item );
	if ( !result.second ) {
	    error( "Duplicate server name: %s", name.c_str() );
	}
	return result.first->second;
    }


    void PMIF_Document::appendAttribute( rapidxml::xml_node<> * node, const char * const attribute, const char * const value )
    {
	node->append_attribute( _document.allocate_attribute( attribute, value ) );
    }

    
    void PMIF_Document::init_tables()
    {
	if ( __model.size() > 0 ) return;
	
	/* Elements */
	__model[XNode]			= ImportModel( &PMIF_Document::handleNode );
	__model[XArc]			= ImportModel( &PMIF_Document::handleArc );
	__model[XWorkload]		= ImportModel( &PMIF_Document::handleWorkload );
	__model[XServiceRequest]	= ImportModel( &PMIF_Document::handleServiceRequest );

	__node[XServer]			= ImportModel(&PMIF_Document::handleServer );
	__node[XWorkUnitServer]		= ImportModel(&PMIF_Document::handleServer );
	__node[XSinkNode]		= ImportModel(&PMIF_Document::handleSource );
	__node[XSourceNode]		= ImportModel(&PMIF_Document::handleSink );
		   
	__workload[XOpenWorkload]	= ImportModel(&PMIF_Document::handleOpenWorkload );
	__workload[XClosedWorkload]	= ImportModel(&PMIF_Document::handleClosedWorkload );

	__service_request[XDemandServiceRequest]    = ImportModel(&PMIF_Document::handleXXXServiceRequest );
	__service_request[XTimeServiceRequest]	    = ImportModel(&PMIF_Document::handleXXXServiceRequest );
	__service_request[XWorkUnitServiceRequest]  = ImportModel(&PMIF_Document::handleXXXServiceRequest );

	/* Attributes */
	__scheduling_type["FCFS"] 	= SCHEDULE_FIFO;
	__scheduling_type["IS"] 	= SCHEDULE_DELAY;
	__scheduling_type["PS"] 	= SCHEDULE_PS;

	__time_scaling["Day"]	= 24.0 * 60.0 * 60.0;
	__time_scaling["day"]	= 24.0 * 60.0 * 60.0;
	__time_scaling["Hr"]	= 60.0 * 60.0;
	__time_scaling["hr"]	= 60.0 * 60.0;
	__time_scaling["Min"]	= 60.0;
	__time_scaling["min"]	= 60.0;
	__time_scaling["Sec"]	= 1.0;
	__time_scaling["sec"]	= 1.0;
	__time_scaling["Ms"]	= 0.001;
	__time_scaling["ms"]	= 0.001;
	__time_scaling["Ns"]	= 0.001 * 0.001;
	__time_scaling["ns"]	= 0.001 * 0.001;
    }

    void PMIF_Document::error( const char * fmt, ... )
    {
	char buf[1024];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buf, 1024, fmt, args );
	va_end( args );
	throw std::runtime_error( buf );
    }
    
}

/* ------------------------------------------------------------------------ */
/* Export to QNAP							    */
/* ------------------------------------------------------------------------ */

namespace LQIO
{
    std::ostream& Export::QNAP::print( std::ostream& output ) const
    {
// /declare/
// /station name=x;
// /exec/ begin
	return output;
    }
}



#if defined(QNAP_OUTPUT)
/*
 * Print the submodel as a queueing network for QNAP.
 */

ostream&
Layer::printQNAP( ostream& output ) const
{
    const bool multi_class = nChains() > 1;
    Sequence<const Entity *> nextClient( myClients );
    Sequence<Entity *> nextServer( entities() );
    const Entity * aServer;
    const Entity * aClient;

    /* Stick all useful stations into a single collection */

    Cltn<const Entity *> stations;
    while ( aServer = nextServer() ) {
	if ( aServer->isSelected() ) {
	    stations += aServer;
	}
    }
    while ( aClient = nextClient() ) {
	if ( aClient->isSelectedIndirectly() ) {
	    stations += aClient;
	}
    }

    Sequence<const Entity *> nextStation( stations );
    const Entity * aStation;

    set_indent(0);
    output << indent(+1) << "/declare/" << endl;

    /* Class info */

    if ( multi_class ) {
	output << indent(0) << "class ";
	for ( unsigned k = 1; k <= nChains(); ++k ) {
	    if ( k > 1 ) {
		output << ", ";
	    }
	    output << "k" << k;
	}
	output << ";" << endl;
    }

    /* Dimensions for replicated stations */

    bool first = true;
    while ( aStation = nextStation() ) {
	if ( aStation->isReplicated() ) {
	    if ( first ) {
		output << indent(0) << "integer i, ";
		first = false;
	    } else {
		output << ", ";
	    }
	    output << qnap_replicas( *aStation ) << " = " << aStation->replicas();
	}
    }
    if ( !first ) {
	output << ";" << endl;
    }

    first = true;
    while ( aClient = nextClient() ) {
	if ( !dynamic_cast<const Task *>(aClient) ) continue;
	Sequence<EntityCall *> nextCall( dynamic_cast<const Task *>(aClient)->callList() );
	EntityCall * aCall;
	while ( aCall = nextCall() ) {
	    if ( aCall->fanOut() > 1 ) {
		if ( first ) {
		    output << indent(0) << "real ";
		    first = false;
		} else {
		    output << ", ";
		}
		output << qnap_visits( *aCall ) << "(" << aCall->fanOut() << ")"; 
	    }
	}
    }
    if ( !first ) {
	output << ";" << endl;
    }

    /* The stations themselves */

    first = true;
    output << indent(0) << "queue ";
    while ( aStation = nextStation() ) {
	if ( !first ) {
	    output << ", ";
	} else {
	    first = false;
	}
	if ( dynamic_cast<const OpenArrivalSource *>(aStation)  ) {
	    output << "src";
	}
	output << aStation->name();
	if ( aStation->isReplicated() ) {
	    output << "(" << qnap_replicas( *aStation ) << ")";
	}
    }
    output << indent(-1) << ";" << endl;

    output << "&" << endl << "& ---------- Clients ----------" << endl << "&" << endl;

    while ( aClient = nextClient() ) {
	const bool is_in_closed_model = aClient->isInClosedModel( entities() );
	const bool is_in_open_model = aClient->isInOpenModel( entities() );

	output << indent(+1) << "/station/" << endl;
	aClient->printQNAPClient( output, is_in_open_model, is_in_closed_model, multi_class );
	output << indent(-1) << endl;
    }

    output << "&" << endl << "& ---------- Servers ----------" << endl << "&" << endl;
    while ( aServer = nextServer() ) {
	if ( !aServer->isSelected() ) continue;
	output << indent(+1) << "/station/" << endl;
	aServer->printQNAPServer( output, multi_class );
	output << indent(-1) << endl;
    }
    
    output << "&" << endl << "&" << endl << "&" << endl;

    if ( multi_class ) {
	output << indent(+1) << "/control/" << endl;
	output << indent(0) << "class = all queue;" << endl;
	output << indent(-1) << endl;
    }
    output << indent(+1) << "/exec/" << endl
	   << indent(+1) << "begin" << endl;

#if 0
    while ( aClient = nextClient() ) {
	if ( !dynamic_cast<const Task *>(aClient) ) continue;
	Sequence<EntityCall *> nextCall( dynamic_cast<const Task *>(aClient)->callList() );
	EntityCall * aCall;
	while ( aCall = nextCall() ) {
	    if ( aCall->fanOut() > 1 ) {
		output << indent(0) << "for i := (1, step 1 until " << aCall->fanOut() << ") do" << endl;
		output << indent(0) << "   " << qnap_visits( *aCall ) << "(i) := " << aCall->rendezvous() << ";" << endl;
	    }
	}
    }
    /* Set service times */
    while ( aClient = nextClient() ) {
	output << indent(0) << aClient->name() << ".service:=" << 0.0 << ";" << endl;
    }
    while ( aServer = nextServer() ) {
	output << indent(0) << aServer->name() << ".service:=" << aServer->serviceTime(1) << ";" << endl;
    }
#endif

    output << indent(0) << "solve;" << endl;
    output << indent(-1) << "end;" << endl;
    output << indent(-1) << "/end/" << endl;
    return output;
}



/*
 * QNAP queueing network.
 */

ostream&
Entity::printQNAPServer( ostream& output, const bool multi_class ) const
{
    output << indent(0) << "name = " << qnap_name( *this ) << ";" << endl;
    if ( isMultiServer() ) {
	output << indent(0) << "type = multiple(" << copies() << ");" << endl;
    } else if ( isInfinite() ) {
	output << indent(0) << "type = infinite;" << endl;
    }
    switch ( scheduling() ) {
    case SCHEDULE_FIFO: output << indent(0) << "sched = fifo;" << endl; break;
    case SCHEDULE_PS:   output << indent(0) << "sched = ps;" << endl; break;
    case SCHEDULE_PPR:  output << indent(0) << "sched = prior, preempt;" << endl; break;
    }
    for ( unsigned i = 1; i <= myServerChains.size(); ++i ) {
	output << indent(0) << "service" << server_chain( *this, multi_class, i ) 
	       << " = exp(" << serviceTimeForQueueingNetwork(myServerChains[i],&Element::hasServerChain) 
	       << ");" << endl;
    }
    printQNAPReplies( output, multi_class );
    return output;
}


/*
 * Generate the "transits" that correspond to the replies.  Open classes go "out."
 */

ostream& 
Entity::printQNAPReplies( ostream& output, const bool multi_class ) const
{
    Sequence<GenericCall *> nextCall( myCallers );
    GenericCall * aCall;

    while ( aCall = nextCall() ) {
	if ( !aCall->isSelected() ) continue;
	output << indent(0) << "transit";
	if ( aCall->hasSendNoReply() ) {
	    output << open_chain( *aCall->srcTask(), multi_class, 1 ) << " = out;" << endl;
	} else {
	    output << closed_chain( *aCall->srcTask(), multi_class, 1 ) << " = " << aCall->srcName() << ",1;" << endl;
	}
    } 

    return output;
}



static ostream&
qnap_name_str( ostream& output, const Entity& anEntity )
{
    output << anEntity.name();
    if ( anEntity.replicas() > 1 ) {
	output << "( 1 step 1 until " << qnap_replicas( anEntity ) << ")";
    }
    return output;
}


static ostream&
qnap_replicas_str( ostream& output, const Entity& anEntity )
{
    output << "n_" << anEntity.name();
    return output;
}

SRVNEntityManip 
qnap_name( const Entity & anEntity )
{
    return SRVNEntityManip( &qnap_name_str, anEntity );
}

SRVNEntityManip 
qnap_replicas( const Entity & anEntity )
{
    return SRVNEntityManip( &qnap_replicas_str, anEntity );
}

/*
 * QNAP queueing network.
 */

ostream&
OpenArrivalSource::printQNAPClient( ostream& output, const bool is_in_open_model, const bool is_in_closed_model, const bool multi_class ) const
{
    output << indent(0) << "name = src" << name() << ";" << endl
	   << indent(0) << "type = source;" << endl
	   << indent(0) << "service = exp(" << 1.0 / myCalls[1]->sumOfSendNoReply() << ");" << endl;
    printQNAPRequests( output, multi_class );
    return output;
}


/*
 * QNAP queueing network.
 */

ostream& 
OpenArrivalSource::printQNAPRequests( ostream& output, const bool multi_class ) const
{
    Sequence<OpenArrival *> nextCall( myCalls );
    OpenArrival * aCall;
    
    bool first = true;
    while ( aCall = nextCall() ) {
	if ( !aCall->isSelected() ) {
	    continue;
	} else if ( first ) {
	    output << indent(0) << "transit" << " = ";		/* transitions must be class independant for a source */
	    first = false;
	} else {
	    output << ", ";
	}
	output << aCall->dstTask()->name() << "," << 1;		/* Always 1. */
    }
    if ( !first ) {
	output << ";" << endl;
    }
    return output;
}
/*
 * QNAP queueing network.
 */

ostream&
Task::printQNAPClient( ostream& output, const bool is_in_open_model, const bool is_in_closed_model, const bool multi_class ) const
{
    output << indent(0) << "name = " << name() << ";" << endl;
    if ( is_in_closed_model ) {
	output << indent(0) << "type = infinite;" << endl;
	for ( unsigned i = 1; i <= myClientClosedChains.size(); ++i ) {
	    output << indent(0) << "service" << closed_chain( *this, multi_class, i )
		   << " = exp(" << sliceTimeForQueueingNetwork( myClientClosedChains[i], &Element::hasClientClosedChain )
		   << ");" << endl;
	    output << indent(0) << "init" << closed_chain( *this, multi_class, i ) << " = " << copies() << ";" << endl;
	}
    } else if ( is_in_open_model ) {
	output << indent(0) << "type = source;" << endl;
    }
    printQNAPRequests( output, is_in_open_model, is_in_closed_model, multi_class );
    return output;
}



/*
 * QNAP queueing network.
 * Modify me for BUG_182.
 */

ostream&
Task::printQNAPRequests( ostream& output, const bool is_in_open_model, const bool is_in_closed_model, const bool multi_class ) const
{
    Sequence<EntityCall *> nextCall( myCalls );

    if ( is_in_closed_model ) {
	printQNAPRequests( output, nextCall, multi_class, &closed_chain, &GenericCall::sumOfRendezvous );
    }
    if ( is_in_open_model ) {
	printQNAPRequests( output, nextCall, multi_class, &open_chain, &GenericCall::sumOfSendNoReply );
    }
    return output;
}


ostream&
Task::printQNAPRequests( ostream& output, Sequence<EntityCall *> &nextCall,
			 const bool multi_class, QNAP_Element_func chain_func, callFunc2 call_func ) const
{
    bool first = true;
    EntityCall * aCall;

    while ( aCall = nextCall() ) {
	if ( !aCall->isSelected() ) {
	    continue;
	} else if ( first ) {
	    output << indent(0) << "transit" << (*chain_func)( *this, multi_class, 1 ) << " = ";
	    first = false;
	} else {
	    output << "," << endl << indent(0) << "    ";		/* Start a new line.  */
	}
	output << aCall->dstName();
	if ( aCall->fanOut() > 1 ) {
	    output << "(1 step 1 until " << aCall->fanOut() << "), "
		   << qnap_visits( *aCall )
		   << "(1 step 1 until " << aCall->fanOut() << ")";
	} else if ( aCall->isProcessorCall() ) {
	    output << "," << countCalls( &GenericCall::sumOfRendezvous ) + 1.0;
	} else {
	    output << "," << (aCall->*call_func)();
	}
    }
    if ( !first ) {
	output << ";" << endl;
    }
    return output;
}

EntityCallManip
qnap_visits( const EntityCall& aCall )
{
    return EntityCallManip( &qnap_visits_str, aCall );
}

/*
 * No need for fancy forwarding like print_calls because we never use
 * this to generate srvn input/output.
 */

static ostream&
qnap_visits_str( ostream& output, const EntityCall& aCall )
{
    output << "v_" << aCall.srcName() << "_" << aCall.dstName();
    return output;
}

static ostream&
server_chain_of_str( ostream& output, const Element& anElement, const bool multi_class, const unsigned i )
{
    if ( multi_class ) {
	output << "(k" << anElement.serverChainAt(i) << ")";
    } 
    return output;
}

static ostream&
closed_chain_of_str( ostream& output, const Element& anElement, const bool multi_class, const unsigned i )
{
    if ( multi_class ) {
	output << "(k" << anElement.closedChainAt(i) << ")";
    } 
    return output;
}

static ostream&
open_chain_of_str( ostream& output, const Element& anElement, const bool multi_class, const unsigned i )
{
    if ( multi_class ) {
	output << "(k" << anElement.openChainAt(i) << ")";
    } 
    return output;
}

QNAPElementManip 
server_chain( const Element& anElement, const bool multi_class, const unsigned i )
{
    return QNAPElementManip( &server_chain_of_str, anElement, multi_class, i );
}

QNAPElementManip 
closed_chain( const Element& anElement, const bool multi_class, const unsigned i )
{
    return QNAPElementManip( &closed_chain_of_str, anElement, multi_class, i );
}

QNAPElementManip 
open_chain( const Element& anElement, const bool multi_class, const unsigned i )
{
    return QNAPElementManip( &open_chain_of_str, anElement, multi_class, i );
}

class QNAPElementManip {
public:
    QNAPElementManip( ostream& (*ff)(ostream&, const Element &, const bool, const unsigned ), 
		     const Element & theElement, const bool multi_server, const unsigned theChain ) 
	: f(ff), anElement(theElement), aMultiServer(multi_server), aChain(theChain) {}

private:
    ostream& (*f)( ostream&, const Element&, const bool, const unsigned );
    const Element & anElement;
    const bool aMultiServer;
    const unsigned aChain;

    friend ostream& operator<<(ostream & os, const QNAPElementManip& m ) 
	{ return m.f(os,m.anElement,m.aMultiServer,m.aChain); }
};

QNAPElementManip server_chain( const Element& anElement, const bool, const unsigned );
QNAPElementManip closed_chain( const Element& anElement, const bool, const unsigned );
QNAPElementManip open_chain( const Element& anElement, const bool, const unsigned );
typedef QNAPElementManip (*QNAP_Element_func)( const Element& anElement, const bool multi_class, const unsigned i );

const Entry& 
Entry::labelQueueingNetworkVisits( Label& aLabel ) const
{
    Sequence<GenericCall *> nextCall( callerList() );
    GenericCall * aCall;

    while ( aCall = nextCall() ) {
	if ( !dynamic_cast<Call *>(aCall) ) continue;
	if ( aCall->hasRendezvous() ) {
	    aLabel << aCall->srcName() << ':' << aCall->dstName() << " ("
		   << print_rendezvous( *dynamic_cast<Call *>(aCall) ) << ")";
	    aLabel.newLine();
	}
	if ( aCall->hasSendNoReply() ) {
	    aLabel << aCall->srcName() << ':' << aCall->dstName() << " ("
		   << print_sendnoreply( *dynamic_cast<Call *>(aCall) ) << ")";
	    aLabel.newLine();
	}
    }

    return *this;
}



const Entry& 
Entry::labelQueueingNetworkWaiting( Label& aLabel ) const
{
    Sequence<GenericCall *> nextCall( callerList() );
    GenericCall * aCall;

    while ( aCall = nextCall() ) {
	const Call * theCall = dynamic_cast<Call *>(aCall);
	if ( theCall ) {
	    aLabel << theCall->srcName() << ':' << theCall->dstName() << "="
		   << print_wait( *theCall );
	    aLabel.newLine();
	}
    }

    return *this;
}



const Entry& 
Entry::labelQueueingNetworkService( Label& aLabel ) const
{
    aLabel.newLine();
    aLabel << name() << " [";
    for ( unsigned p = 1; p <= maxPhase(); ++p ) {
	if ( p != 1 ) {
	    aLabel << ',';
	}
	aLabel << serviceTimeForQueueingNetwork(p);
    }
    aLabel << "]";
    return *this;
}
#endif
