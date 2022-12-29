# Makefile for bookstore
# Copyright (C) 2005 Greg Franks <rgfranks@alumni.uwaterloo.ca>
#
# ------------------------------------------------------------------------
# $Id: Makefile.am 16206 2022-12-29 00:45:48Z greg $
# ------------------------------------------------------------------------

SRCS=	flat.fig          multiserver.fig   replicaserver.fig replicated.fig    
OBJS = $(SRCS:.fig=.eps) 

EXTRA_DIST = $(SRCS) $(OBJS)
pkgdata_DATA =  $(OBJS)

SUFFIXES = .pdf .ps .dvi .bbl .eps .pstex .eps .fig .tex

.fig.eps:
if HAVE_FIG2DEV
	fig2dev -L eps -m 0.75 $< $@
endif

distclean-local:
	rm -f *.bak
