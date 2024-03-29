# ----------------------------------------------------------------------
# $Id: Makefile.am 16990 2024-01-31 11:58:42Z greg $
# ----------------------------------------------------------------------

SUBDIRS		= 
if COND_PNG
  MAYBE_PNG = lqn2png
endif
if COND_JPEG
  MAYBE_JPEG = lqn2jpeg
endif
if COND_GIF
  MAYBE_GIF = lqn2gif
endif

AM_CPPFLAGS	= -I.. -I../lqiolib/src/headers -I../lqx/libsrc/headers
bin_PROGRAMS 	= lqn2ps 
OTHEREXE	= lqn2emf lqn2fig lqn2out lqn2lqn lqn2svg lqn2xml $(MAYBE_PNG) $(MAYBE_JPEG) $(MAYBE_GIF) rep2flat 
lqn2ps_SOURCES 	= activity.cc actlayer.cc actlist.cc arc.cc call.cc element.cc entity.cc entry.cc errmsg.cc \
		getopt2.cc graphic.cc group.cc help.cc key.cc label.cc layer.cc lqn2ps.cc option.cc model.cc node.cc \
		open.cc phase.cc point.cc pragma.cc processor.cc runlqx.cc share.cc task.cc

noinst_HEADERS 	= actlayer.h activity.h actlist.h arc.h call.h element.h entity.h entry.h errmsg.h \
		getopt2.h graphic.h group.h help.h key.h label.h layer.h lqn2ps.h model.h node.h \
		open.h option.h phase.h point.h pragma.h processor.h runlqx.h share.h task.h 

EXTRA_DIST	= lqn2ps.1 

lqn2ps_CXXFLAGS = -std=c++17 -Wall -Wno-parentheses -Wno-switch -Wno-unknown-pragmas -Wno-overloaded-virtual
lqn2ps_LDADD	= -L../lqx/libsrc/.libs -llqx -L../lqiolib/src/.libs -llqio 
lqn2ps_LDFLAGS	= @STATIC@

man1_MANS	= lqn2ps.1

clean-local:

distclean-local:
	rm -f config.cache

install-exec-hook:
	-for i in $(OTHEREXE); do \
	  (cd $(DESTDIR)$(bindir); rm -f $$i$(EXEEXT); $(LN_S) lqn2ps$(EXEEXT) $$i$(EXEEXT)); \
	done

install-data-hook:
	-for i in $(OTHEREXE); do \
	  (cd $(DESTDIR)$(mandir)/man1; rm -f $$i.1; $(LN_S) $(man1_MANS) $$i.1); \
	done

if HAVE_PS2PDF
all-local: lqn2ps.pdf 

install-data-local: lqn2ps.pdf 

lqn2ps.pdf: $(man1_MANS)
	groff -man $(man1_MANS)  > $<.ps
		ps2pdf $<.ps $@
		rm $<.ps
endif

