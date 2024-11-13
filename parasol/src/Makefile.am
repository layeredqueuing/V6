# -*- Makefile -*-
# $HeadURL: http://rads-svn.sce.carleton.ca:8080/svn/lqn/trunk-V6/parasol/src/Makefile.am $
# Makefile for Parasol libraries.
#
# ------------------------------------------------------------------------
# $Id: Makefile.am 14125 2020-11-24 19:53:30Z greg $
# ------------------------------------------------------------------------

AM_CPPFLAGS	= -I../include
noinst_LIBRARIES =  libparasol.a libparasolc.a libparasol++.a

libparasol_a_SOURCES = para_library.c standalone.c 
#libparasol_a_CFLAGS = -Wall -Wno-switch
libparasol_a_CFLAGS = -Wno-switch
EXTRA_libparasol_a_SOURCES = drand48.c
libparasol_a_DEPENDENCIES = @DRAND48@
libparasol_a_LIBADD = @DRAND48@
libparasolc_a_SOURCES = para_library.c 
#libparasolc_a_CFLAGS = -Wall -Wno-switch
libparasolc_a_CFLAGS = -Wno-switch
EXTRA_libparasolc_a_SOURCES = drand48.c
libparasolc_a_LIBADD = @DRAND48@
libparasolc_a_DEPENDENCIES = @DRAND48@

libparasol___a_SOURCES = carrier.cc node.cc semaphore.cc statistic.cc spinlock.cc para_entity.cc \
	task.cc port.cc genesis.cc

