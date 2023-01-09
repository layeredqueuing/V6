# Makefile for bookstore
# Copyright (C) 2005 Greg Franks <rgfranks@alumni.uwaterloo.ca>
# ------------------------------------------------------------------------
# $Id: Makefile.am 16302 2023-01-09 00:12:54Z greg $
# ------------------------------------------------------------------------

SRCS=	entry-activities.fig	phase.fig		request-snr.fig \
	entry-phases.fig	request-fwd.fig		slice.fig \
	meta-model.fig		request-rnv.fig
OBJS = $(SRCS:.fig=.eps) 

EXTRA_DIST = $(SRCS)
noinst_DATA = $(OBJS)

SUFFIXES = .pdf .ps .dvi .bbl .eps .pstex .eps .fig .tex

.fig.eps:
if HAVE_FIG2DEV
	fig2dev -L eps -m 0.75 $< $@
endif

distclean-local:
	rm -f *.bak

