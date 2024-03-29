# $Id: Makefile.am 16990 2024-01-31 11:58:42Z greg $
# -------------------------------------------------------------------- [Libtool libsrvnio2]

# This project uses Libtool to generate a product
SUBDIRS = headers/lqio
lib_LTLIBRARIES = liblqio.la
liblqio_la_SOURCES = bcmp_bindings.cpp \
	bcmp_document.cpp \
	bcmp_to_lqn.cpp \
	commandline.cpp \
	common_io.cpp \
	confidence_intervals.cpp \
	dom_activity.cpp \
	dom_actlist.cpp \
	dom_bindings.cpp \
	dom_call.cpp \
	dom_decision.cpp \
	dom_document.cpp \
	dom_entity.cpp \
	dom_entry.cpp \
	dom_extvar.cpp \
	dom_group.cpp \
	dom_histogram.cpp \
	dom_object.cpp \
	dom_phase.cpp \
	dom_pragma.cpp \
	dom_processor.cpp \
	dom_task.cpp \
	error.cpp \
	filename.cpp \
	getsubopt.cpp \
	gnuplot.cpp \
	json_document.cpp \
	labels.cpp \
	pmif_document.cpp \
	qnio_document.cpp \
	qnap2_document.cpp \
	qnap2_gram.y \
	qnap2_scan.l \
	srvn_gram.y \
	srvn_input.cpp \
	srvn_output.cpp \
	srvn_result_gram.y \
	srvn_result_scan.l \
	srvn_results.cpp \
	srvn_scan.l \
	srvn_spex.cpp \
	xml_input.cpp \
	xml_output.cpp

BUILT_SOURCES = srvn_gram.h srvn_result_gram.h qnap2_gram.h
noinst_HEADERS = srvn_gram.h srvn_result_gram.h qnap2_gram.h

#liblqio_la_LIBADD = ../../lqx/libsrc/liblqx.la 
liblqio_la_LIBADD = -L../../lqx/libsrc/.libs -llqx
if EXPAT
liblqio_la_SOURCES += expat_document.cpp jmva_document.cpp
liblqio_la_LIBADD += -lexpat
endif

liblqio_la_CPPFLAGS = -I ../../lqx/libsrc/headers -I headers/lqio
liblqio_la_CXXFLAGS = -std=c++17 -Wall -Wno-unknown-pragmas
liblqio_la_LDFLAGS =  -no-undefined -version-info 0:1:0
AM_YFLAGS = -d -t -v
# Force rename of variables.  For FLEX -- should check. :-)
srvn_scan.c: srvn_scan.l srvn_gram.h headers/lqio/srvn_input.h
	$(LEX) $(LFLAGS) -Psrvn -o srvn_scan.c srvn_scan.l

# Force rename of variables.  For BISON -- should check. :-)
srvn_gram.c: srvn_gram.y headers/lqio/srvn_input.h
	$(YACC) $(AM_YFLAGS) -p srvn -o srvn_gram.c srvn_gram.y

# Force rename of variables.  For FLEX -- should check. :-)
srvn_result_scan.c: srvn_result_scan.l srvn_result_gram.h
	$(LEX) $(LFLAGS) -Presult -o srvn_result_scan.c srvn_result_scan.l

# Force rename of variables.  For BISON -- should check. :-)
srvn_result_gram.c: srvn_result_gram.y
	$(YACC) $(AM_YFLAGS) -p result -o srvn_result_gram.c srvn_result_gram.y

qnap2_scan.c: qnap2_scan.l qnap2_gram.h
	$(LEX) $(LFLAGS) -Pqnap2 -o qnap2_scan.c qnap2_scan.l

qnap2_gram.c: qnap2_gram.y 
	$(YACC) $(AM_YFLAGS) -p qnap2 -o qnap2_gram.c qnap2_gram.y

# Files to remove during cleanup
MOSTLYCLEANFILES = srvn_gram.h srvn_gram.c srvn_scan.c srvn_result_gram.h srvn_result_gram.c srvn_result_scan.c qnap2_scan.c
