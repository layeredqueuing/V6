# Makefile for .
# Copyright (C) 2003 Greg Franks <rgfranks@alumni.uwaterloo.ca>
# $Id: Makefile.am 15272 2021-12-27 12:46:09Z greg $
# ----------------------------------------------------------------------
ACLOCAL_AMFLAGS=-I ../config
SUBDIRS		= 

AM_CPPFLAGS	= -I.. -I../lqiolib/src/headers -I../lqx/libsrc/headers 
bin_PROGRAMS 	= lqngen
OTHEREXE	= lqn2lqx
lqngen_SOURCES  = lqngen.cc generate.cc help.cc randomvar.cc 
noinst_HEADERS 	= generate.h help.h lqngen.h randomvar.h 

EXTRA_DIST	= lqngen.1 lqn2lqx.1

lqngen_CXXFLAGS = -std=c++11 -Wall -Wno-parentheses -Wno-switch -Wno-unknown-pragmas -Wno-overloaded-virtual
lqngen_LDADD 	= -L../lqiolib/src/.libs -llqio -L../lqx/libsrc/.libs -llqx 
lqngen_LDFLAGS	= @STATIC@

man1_MANS	= lqngen.1 lqn2lqx.1

if HAVE_PS2PDF
install-data-local:	lqngen.pdf
endif

distclean-local:
	rm -f config.cache

if HAVE_PS2PDF
install-data-local: lqngen.pdf lqn2lqx.pdf

lqngen.pdf: lqngen.1
	groff -man $< | ps2pdf - $@

lqn2lqx.pdf: lqn2lqx.1
	groff -man $< | ps2pdf - $@
endif
