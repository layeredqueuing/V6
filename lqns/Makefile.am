# 
# $Id: Makefile.am 14109 2020-11-19 17:32:58Z greg $
#
#----------------------------------------------------------------------

ACLOCAL_AMFLAGS=-I ../config
SUBDIRS		= unit-test regression 
AM_CPPFLAGS	= -I.. -I../lqiolib/src/headers -I../lqx/libsrc/headers/
bin_PROGRAMS 	= lqns qnsolver
lqns_SOURCES	= activity.cc actlist.cc call.cc dim.cc entity.cc entry.cc \
		  entrythread.cc errmsg.cc fpgoop.cc gamma.cc generate.cc group.cc help.cc interlock.cc \
		  model.cc multserv.cc mva.cc open.cc option.cc overtake.cc ph2serv.cc \
		  phase.cc pop.cc pragma.cc prob.cc processor.cc randomvar.cc report.cc runlqx.cc \
		  server.cc slice.cc lqns.cc submodel.cc  \
		  synmodel.cc task.cc variance.cc 
qnsolver_SOURCES= qnsolver.cc \
		  dim.cc fpgoop.cc multserv.cc mva.cc ph2serv.cc open.cc pop.cc prob.cc server.cc 
noinst_HEADERS	= activity.h actlist.h call.h dim.h entity.h entry.h entrythread.h \
		  errmsg.h fpgoop.h gamma.h generate.h group.h help.h interlock.h model.h \
		  multserv.h mva.h open.h option.h overtake.h ph2serv.h phase.h pop.h pragma.h \
		  prob.h processor.h randomvar.h report.h server.h slice.h lqns.h runlqx.h \
		  submodel.h  synmodel.h task.h variance.h vector.h

EXTRA_DIST	= lqns.1 

lqns_CXXFLAGS   = -std=c++11 -Wall -Wno-parentheses -Wno-unknown-pragmas -Wno-overloaded-virtual
lqns_LDADD	= -L../lqx/libsrc/.libs -llqx -L../lqiolib/src/.libs -llqio 
qnsolver_CXXFLAGS = -Wall -Wno-parentheses -Wno-unknown-pragmas -Wno-overloaded-virtual
qnsolver_LDADD	= -L../lqx/libsrc/.libs -llqx -L../lqiolib/src/.libs -llqio 
lqns_LDFLAGS	= @STATIC@
man1_MANS	= lqns.1

clean-local:

install-data-local:	lqns.pdf

distclean-local:
		rm -f config.cache

lqns.ps:	$(man1_MANS)
		tbl < $(man1_MANS)  | groff -man > lqns.ps

lqns.pdf:	lqns.ps
		ps2pdf lqns.ps

lqns.txt:	$(man1_MANS)
		tbl < $(man1_MANS)  | nroff -man | col -b > lqns.txt

quick:		lqns
		(cd regression; make quick)
