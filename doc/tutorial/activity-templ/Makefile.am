# Copyright the Real-Time and Distributed Systems Group,
# Department of Systems and Computer Engineering,
# Carleton University, Ottawa, Ontario, Canada. K1S 5B6
#
#----------------------------------------------------------------------
# $Id: Makefile.am 15450 2022-03-05 01:08:38Z greg $
#----------------------------------------------------------------------

SUBDIRS = 
SRCS =	activity-templ.fig

EXTRA_DIST = $(SRCS) activity-templ.lqnx activity-templ.lqn
pkgdata_DIST = $(OBJS)

SUFFIXES = .pdf .ps .dvi .bbl .eps .pstex .eps .fig .tex

OBJS = $(SRCS:.fig=.eps) 

.fig.eps:
if HAVE_FIG2DEV
	fig2dev -L eps $< $@
endif

clean-local:
	rm -f $(OBJS)  $(DST).aux $(DST).bbl $(DST).blg $(DST).dvi $(DST).log $(DST).pdf $(DST).ps $(DST).out *.fig.bak
