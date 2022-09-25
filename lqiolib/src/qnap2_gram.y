/*
 * $id: srvn_gram.y 14381 2021-01-19 18:52:02Z greg $ 
 */

%{
#include <stdio.h>
#include "qnap2_document.h"

extern void qnap2error( const char * fmt, ... );
extern int qnap2lex();

static void * curr_station = NULL;
static bool station_found = false;
%}

%token QNAP_ASSIGNMEMT QNAP_LESS_EQUAL QNAP_GREATER_EQUAL QNAP_POWER 
%token QNAP_CONTROL QNAP_DECLARE QNAP_END QNAP_EXEC QNAP_RESTART QNAP_REBOOT QNAP_STATION QNAP_TERMINAL 

%token QNAP_ALL QNAP_AND QNAP_ANY QNAP_BEGIN QNAP_DO QNAP_ELSE QNAP_FALSE QNAP_FOR QNAP_FORWARD QNAP_GENERIC
%token QNAP_GOTO QNAP_IF QNAP_IN  QNAP_IS QNAP_NIL QNAP_NOT QNAP_OBJECT QNAP_OR 
%token QNAP_REF QNAP_REPEAT QNAP_STEP QNAP_THEN QNAP_TRUE QNAP_UNTIL QNAP_VAR QNAP_WATCHED
%token QNAP_WHILE QNAP_WITH
%token RANGE_ERR 

%token <aString>	STRING QNAP_IDENTIFIER 
%token <aPointer>	
%token <aCode>		QNAP_BOOLEAN QNAP_REAL QNAP_INTEGER QNAP_STRING		/* scanner codes */
%token <aReal>		DOUBLE
%token <aLong>		LONG
%token QNAP_QUEUE QNAP_CLASS
%token QNAP_NAME QNAP_INIT QNAP_PRIO QNAP_QUANTUM QNAP_RATE QNAP_SCHED QNAP_SERVICE QNAP_TRANSIT QNAP_TYPE

%union {
    int aCode;
    long aLong;
    double aReal;
    char * aString;
    void * aPointer;
}

%type <aPointer>	identifier identifier_list variable arrayref power factor term expression transit 
%type <aCode>		variable_type
%type <aString>		station_type_pair
%%

qnap2			: declare { qnap2_default_class(); } station_list exec QNAP_END
			;

/*
command_list		: command_list command
			| command
			;

command			| QNAP_TERMINAL
			| QNAP_REBOOT
			| QNAP_RESTART
			;
*/

/* ------------------------------------------------------------------------ */

declare			: QNAP_DECLARE declare_list
			;

declare_list		: declare_list ';' declare_statement
			| declare_statement ';'
			;

declare_statement	: QNAP_QUEUE identifier_list		{ qnap2_add_queue( $2 ); }
			| QNAP_CLASS identifier_list		{ qnap2_add_class( $2 ); }
			| QNAP_CLASS variable_type identifier_list
			| variable_type identifier_list		{ qnap2_add_variable( $1, $2 ); }
			;

variable_type 		: QNAP_INTEGER { $$ = $1; }
			| QNAP_REAL { $$ = $1; }
			| QNAP_BOOLEAN { $$ = $1; }
			| QNAP_STRING { $$ = $1; }

identifier_list		: identifier				{ $$ = qnap2_append_identifier( NULL, $1 ); free( $1 ); }
			| identifier_list ',' identifier	{ $$ = qnap2_append_identifier( $1, $3 ); free( $3 ); }
			;

identifier		: QNAP_IDENTIFIER			{ $$ = $1; }
/*			| QNAP_IDENTIFIER '=' sublist */
			;

/*
sublist			: simple_sublist
			| sublist ',' simple_sublist
			;

simple_sublist		: expression
			;
*/


/* ------------------------------------------------------------------------ */

station_list		: station
			| station_list station
			;

station			: QNAP_STATION parameter_list 		{ qnap2_define_station(); }
			;

parameter_list		: parameter ';'
			| parameter_list parameter ';'
			;

parameter		: QNAP_NAME '=' QNAP_IDENTIFIER		{ qnap2_set_station_name( $3 ); free( $3 ); }
			| QNAP_INIT '=' variable		{ qnap2_set_station_init( $3 ); }
			| QNAP_PRIO '=' variable		{ qnap2_set_station_prio( $3 ); }
			| QNAP_QUANTUM '=' variable		{ qnap2_set_station_quantum( $3 ); }
			| QNAP_RATE '=' variable		{ qnap2_set_station_rate( $3 ); }
			| QNAP_SCHED '=' QNAP_IDENTIFIER	{ qnap2_set_station_sched( $3 ); free( $3 ); }
			| QNAP_SERVICE '=' factor		{ qnap2_set_station_service( $3 ); }
			| QNAP_TRANSIT '=' transit		{ qnap2_set_station_transit( $3 ); }
			| QNAP_TYPE '=' station_type
			;

transit			: QNAP_IDENTIFIER
			| QNAP_IDENTIFIER ',' variable
			;

station_type		: QNAP_IDENTIFIER			{ qnap2_set_station_type( $1, 1 ); free( $1 ); }
			| QNAP_IDENTIFIER '(' QNAP_INTEGER ')'	{ qnap2_set_station_type( $1, $3 ); free( $1 ); }
			| station_type_pair			{ qnap2_set_station_type( $1, 1 ); free( $1 ); }
			| station_type_pair '(' QNAP_INTEGER ')'{ qnap2_set_station_type( $1, $3 ); free( $1 ); }
			;

station_type_pair	: QNAP_IDENTIFIER ',' QNAP_IDENTIFIER	{ $$ = qnap2_get_station_type( $1, $3 ); free( $1 ); free( $3 ); }
			;

/* ------------------------------------------------------------------------ */

exec 			: QNAP_EXEC statement
			;

statement		: QNAP_IDENTIFIER ';'
			| QNAP_BEGIN compound_statement QNAP_END ';'
			;

compound_statement	: statement
			| compound_statement statement
			;


expression		: expression '+' term
			| expression '-' term
			| term
			;

term			: term '*' power
			| term '/' power
			| term '%' power
			| power
			;

power			: arrayref QNAP_POWER power
			| arrayref
			;

/*
prefix			: TOK_LOGIC_NOT arrayref
			| arrayref
			;
*/


arrayref		: arrayref '[' expression ']'
			| factor
			;

factor			: '(' expression ')'			{ $$ = $2; }
			| identifier '(' ')'
			| identifier '(' expression_list ')'
			| variable				{ $$ = $1; }
			;

expression_list		: expression
			| expression_list ',' expression
			;

variable		: identifier				{ $$ = qnap2_get_variable( $1 ); }
			| LONG 					{ $$ = qnap2_get_integer( $1 ); }
			| DOUBLE				{ $$ = qnap2_get_real( $1 ); }
			| STRING				{ $$ = qnap2_get_string( $1 ); }
			;
%%
