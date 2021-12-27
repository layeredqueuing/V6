#
# $Id: Makefile.am 15272 2021-12-27 12:46:09Z greg $
#----------------------------------------------------------------------

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I../config
AM_CPPFLAGS	= -I..  -I../lqiolib/src/headers -I../lqx/libsrc/headers
lqn2csv_CXXFLAGS = -std=c++11 -Wall 
bin_PROGRAMS 	= lqn2csv
lqn2csv_SOURCES = gnuplot.cc lqn2csv.cc model.cc
noinst_HEADERS  = gnuplot.h lqn2csv.h model.h
lqn2csv_LDADD	= -L../lqiolib/src/.libs -llqio

lqn2csv_LDFLAGS= @STATIC@

EXTRA_DIST	= lqn2csv.1

man1_MANS	= lqn2csv.1

if HAVE_PS2PDF
install-data-local:	lqn2csv.pdf

lqn2csv.pdf:	$(man1_MANS)
		groff -man $< | ps2pdf - $@

endif

lqn2csv.txt:	$(man1_MANS)
		nroff -man $(man1_MANS) | col -b > lqn2csv.txt
