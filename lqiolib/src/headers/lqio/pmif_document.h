/* -*- C++ -*-
 *  $Id: json_document.h 12603 2016-06-20 19:38:50Z greg $
 *
 *  Created by Greg Franks.
 */

#ifndef __LQIO_PMIF_DOCUMENT__
#define __LQIO_PMIF_DOCUMENT__

#include <iostream>
#include <string>
#include <cstring>
#include <stdexcept>
#include <map>
#include "rapidxml.h"
#include "rapidxml_print.h"
#include "input.h"

namespace LQIO {

    class PMIF_Document {

	typedef void (PMIF_Document::*self_fptr)( rapidxml::xml_node<> * );

	class ImportModel
	{
	public:
	    ImportModel() : _fptr(0) {}
	    ImportModel( self_fptr fptr ) : _fptr(fptr) {}
	    bool operator()( const std::string& s1, const std::string& s2 ) const { return s1 < s2; }
	    void operator()( PMIF_Document& document, rapidxml::xml_node<> * node ) const { (document.*_fptr)( node ); }

	private:
	    self_fptr _fptr;
	};

    public:
	/* ------------------------------------------------------------------------ */
	/* Classes used to get the data from the model.				    */
	/* ------------------------------------------------------------------------ */

	class XMLNode {
	public:
	    XMLNode( const rapidxml::xml_node<>* node ) : _node(node) {}

	    bool operator()( const std::string& s1, const std::string& s2 ) const { return s1 < s2; }
	    const std::string getName() const;
	    rapidxml::xml_document<>& getDocument() const { return *_node->document(); }

	    void appendNode( rapidxml::xml_node<> * child );
	    void appendAttribute( const char * const attribute, const char * const value );
	    void appendThisNodeTo( rapidxml::xml_document<>& document, const char * const parent );
	    
	    
	protected:
	    const rapidxml::xml_node<>* _node;
	};
	
	class Station : public XMLNode {
	public:
	    class Demand : public XMLNode {
		
	    public:
		Demand() : XMLNode(0), _demand(0.0), _visits(1.0), _service_time(0.0), _scaling(1), _transit() {}
		Demand( const rapidxml::xml_node<> * root );
		Demand( rapidxml::xml_document<>& document, Station& station, const std::string& chain, double visits, double service_time );

		bool addTransit( std::pair<std::string,double>& item ) { return _transit.insert( item ).second; }

		double getServiceTime() const { return _service_time * _scaling; }
		double getVisits() const { return _visits; }

	    private:
		double _demand;			/* Optional */
		double _visits;			/* Optional */
		double _service_time;		/* Optional */
		double _scaling;
		std::map<std::string,double> _transit;
	    };

	    struct PolicyIs {
		PolicyIs( const scheduling_type s ) : _s(s) {}
		bool operator()( const std::pair<const std::string,scheduling_type>& pair ) { return pair.second == _s; }

	    private:
		scheduling_type _s;
	    };

	public:
	    Station() : XMLNode(0), _scheduling(SCHEDULE_DELAY), _multiplicity(1) {}
	    Station( const rapidxml::xml_node<>* node );
	    Station( rapidxml::xml_document<>& document, const std::string& name, scheduling_type scheduling, unsigned int multiplicity );

	    PMIF_Document::Station::Demand& addDemand( const std::string& chain, const rapidxml::xml_node<> * node );
	    PMIF_Document::Station::Demand& addDemand( const std::string& chain, double visits, double service_time );

	    const scheduling_type getScheduling() const { return _scheduling; }
	    const unsigned int getMultiplicity() const { return _multiplicity; }
	    const std::map<std::string,Demand>& getDemands() const { return _demands; }

	private:
	    scheduling_type _scheduling;
	    const unsigned int _multiplicity;
	    std::map<std::string,Demand> _demands;
	};
	
	class Chain : public XMLNode {
	protected:
	    Chain( const rapidxml::xml_node<>* node ) : XMLNode(node) {}
	};

	class ClosedChain : public Chain
	{
	public:
	    ClosedChain() : Chain(0), _customers(0), _think_device(""), _think_time(0.), _scaling(1.0) {}
	    ClosedChain( const rapidxml::xml_node<>* node );
	    ClosedChain( rapidxml::xml_document<>& document, const std::string& name, unsigned int customers, const std::string& device, double think_time );

	    unsigned int getCustomers() const { return _customers; }
	    double getThinkTime() const { return _think_time * _scaling; }

	private:
	    const unsigned int _customers;
	    const std::string _think_device;
	    const double _think_time;
	    const double _scaling;
	};
	
	class OpenChain : public Chain
	{
	public:
	    OpenChain() : Chain(0) {}
	    OpenChain( const rapidxml::xml_node<>* node );
	};

	/* ------------------------------------------------------------------------ */
	
	PMIF_Document( const std::string& input_file_name );
	bool parse();

	PMIF_Document& initialize();
	PMIF_Document& addClosedChain( const std::string& name, unsigned int customers, double think_time );
	Station& addStation( const std::string& name, scheduling_type scheduling, unsigned int multiplicity );

	const std::map<std::string,PMIF_Document::Station,PMIF_Document::Station>& getStations() const { return _stations; }
	const std::map<std::string,PMIF_Document::ClosedChain,PMIF_Document::ClosedChain>& getClosedChains() const { return _closed_chains; }
	const std::map<std::string,PMIF_Document::OpenChain,PMIF_Document::OpenChain>& getOpenChains() const { return _open_chains; }

	std::ostream& print( std::ostream& output ) const { return rapidxml::print(output,_document); }

    private:
	PMIF_Document( const PMIF_Document& );
	PMIF_Document& operator=( const PMIF_Document& );

	Station& addStation( const std::string& name, const Station& station );
	PMIF_Document& addClosedChain( const std::string& name, const ClosedChain& chain );

	void handleChildren( rapidxml::xml_node<> * root, const std::map<const std::string,PMIF_Document::ImportModel,PMIF_Document::ImportModel>& table );

	void handleModel( rapidxml::xml_node<> *root ) { handleChildren( root, PMIF_Document::__model ); }
	void handleNode( rapidxml::xml_node<> *root ) { handleChildren( root, PMIF_Document::__node ); }
	void handleWorkload( rapidxml::xml_node<> * root ) { handleChildren( root, PMIF_Document::__workload ); }
	void handleServiceRequest( rapidxml::xml_node<> * root ) { handleChildren( root, PMIF_Document::__service_request); }

	void handleArc( rapidxml::xml_node<> * root ) {}
	void handleServer( rapidxml::xml_node<> * root ) { addStation( getString( root, "Name" ), Station( root ) ); }
	void handleSource( rapidxml::xml_node<> * root );
	void handleSink( rapidxml::xml_node<> * root );
	void handleClosedWorkload( rapidxml::xml_node<> * root );
	void handleOpenWorkload( rapidxml::xml_node<> * root );
	void handleXXXServiceRequest( rapidxml::xml_node<> * root );
	void handleTransit( PMIF_Document::Station::Demand& demand, rapidxml::xml_node<> * root );

	void appendAttribute( rapidxml::xml_node<> *, const char * const attribute, const char * const value );
	
	static void init_tables();

	static void error( const char *, ... );
	static std::string getString( const rapidxml::xml_node<> * node, const char * attr );
	static unsigned int getUnsignedInteger( const rapidxml::xml_node<> * node, const char * attr );
	static double getUnsignedDouble( const rapidxml::xml_node<> * node, const char * attr );
	static double getOptionalUnsignedDouble( const rapidxml::xml_node<> * node, const char * attr );
	static double getOptionalTimeScaling( const rapidxml::xml_node<> * node, const char * attr );
	static scheduling_type getSchedulingPolicy( const rapidxml::xml_node<> * node, const char * attr );

    private:
	const std::string& _input_file_name;
	rapidxml::xml_document<> _document;
	std::map<std::string,PMIF_Document::Station,PMIF_Document::Station> _stations;
	std::map<std::string,PMIF_Document::OpenChain,PMIF_Document::OpenChain> _open_chains;
	std::map<std::string,PMIF_Document::ClosedChain,PMIF_Document::ClosedChain> _closed_chains;

	static std::map<const std::string,PMIF_Document::ImportModel,PMIF_Document::ImportModel> __model;
	static std::map<const std::string,PMIF_Document::ImportModel,PMIF_Document::ImportModel> __node;
	static std::map<const std::string,PMIF_Document::ImportModel,PMIF_Document::ImportModel> __workload;
	static std::map<const std::string,PMIF_Document::ImportModel,PMIF_Document::ImportModel> __service_request;

	static std::map<const std::string,scheduling_type> __scheduling_type;
	static std::map<const std::string,double> __time_scaling;
    };

    namespace Export{
	class LQN {
	public:
	    LQN( const PMIF_Document& document ) : _document(document) {}

	    std::ostream& print( std::ostream& output ) const;

	private:
	    const PMIF_Document& _document;
	};

	class QNAP {
	public:
	    QNAP( const PMIF_Document& document ) : _document(document) {}

	    std::ostream& print( std::ostream& output ) const;

	private:
	    const PMIF_Document& _document;
	};
	    

	class JMVA {
	    JMVA( const PMIF_Document& document ) : _document(document) {}

	    std::ostream& print( std::ostream& output ) const;

	private:
	    const PMIF_Document& _document;
	};
    }
}

inline std::ostream& operator<<( std::ostream& output, const LQIO::PMIF_Document& self ) { return self.print( output ); }
#endif /* __LQIO_PMIF_DOCUMENT__ */
