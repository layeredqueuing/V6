# $Id: Makefile.am 12908 2017-02-02 17:57:21Z greg $
# ------------------------------------------------------------------------
# Binaries that need to be produced
bin_PROGRAMS = lqx
lqx_CPPFLAGS = -W -Wall -Wno-sign-compare -Wno-unused -Wno-unknown-pragmas -I../libsrc/headers
lqx_SOURCES = main.cpp
#lqx_LDADD = ../libsrc/liblqx.la
lqx_LDADD = -L../libsrc/.libs -llqx
lqx_LDFLAGS = -all-static

check:	lqx
	for i in `ls ../examples/*.ml`; do echo $$i; ./lqx $$i; echo "--------"; done
