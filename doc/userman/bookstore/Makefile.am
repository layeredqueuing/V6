# Makefile for bookstore
# Copyright (C) 2005 Greg Franks <rgfranks@alumni.uwaterloo.ca>
# $Id: Makefile.am 16325 2023-01-12 17:47:10Z greg $

SRCS=	bookstore-result.fig     bookstore-service.fig 		bookstore.fig
OBJS = $(SRCS:.fig=.eps) 

EXTRA_DIST = $(SRCS) $(OBJS)
noinst_DATA = $(OBJS)

SUFFIXES = .pdf .ps .dvi .bbl .eps .pstex .eps .fig .tex

.fig.eps:
if HAVE_FIG2DEV
	fig2dev -L eps -m 0.5 $< $@
endif


distclean-local:
	rm -f *.bak

bookstore-result.eps: bookstore-result.fig
if HAVE_FIG2DEV
	fig2dev -L eps -m 0.75 $< $@
endif

bookstore-service.eps: bookstore-service.fig
if HAVE_FIG2DEV
	fig2dev -L eps -m 0.75 $< $@
endif
