# Makefile for regression test
# Copyright (C) 2005 Greg Franks <greg@sce.carleton.ca>
#
# ------------------------------------------------------------------------
# $Id: Makefile.am 13831 2020-09-18 12:51:41Z greg $
# ------------------------------------------------------------------------


SRVN=	../petrisrvn
OPTS=	-w
EPSILON = '0.1'

SANITY=	01-sanity.lqxo \
	02-sanity.lqxo \
	03-sanity.lqxo \
	04-sanity.lqxo \
	05-sanity.lqxo \
	06-sanity.lqxo \
	07-sanity.lqxo 
INTER=	10-interlock.lqxo \
	11-interlock.lqxo \
	12-interlock.lqxo \
	13-interlock.lqxo 
	15-split-interlock.lqxo \
	16-open-arrival-lqxo
MULTI=	20-multiserver.lqxo \
	21-multiserver.lqxo \
	22-multiserver.lqxo \
	23-multiserver.lqxo 
ACT=	40-activities.lqxo \
	41-activities.lqxo \
	42-activities.lqxo \
	43-activities.lqxo \
	44-activities.lqxo \
	45-activities.lqxo \
	46-activities.lqxo 
SEMA =  60-semaphore.lqxo \
	61-semaphore.lqxo \
	62-semaphore.lqxo 
SIM=	70-priority.lqxo 
SYNC=	80-sync.lqxo \
	81-sync.lqxo \
	82-sync.lqxo \
	83-sync.lqxo 
FORK=	85-fork.lqxo \
	86-fork.lqxo \
	87-fork.lqxo \
	88-quorum.lqxo \
	89-quorum.lqxo 
OTHER=	90-A01.lqxo \
	90-B07.lqxo \
	91-cs3-1.lqxo \
	92-open.lqxo \
	93-simple-ucm.lqxo

OBJS=	$(SANITY) $(INTER) $(MULTI) $(ACT) $(REPL) $(SEMA) $(SIM) $(SYNC) $(FORK) $(OTHER)

SRCS=	$(OBJS:.lqxo=.lqnx)

EXTRA_DIST = $(SRCS)

SUFFIXES = .diff .d .lqxo .lqnx 

.lqnx.lqxo:
	$(SRVN) $(OPTS) $<
	@srvndiff -Q -S$(EPSILON) $@ ../../models/regression/petri-results/$*.p
.lqnx.d:
	$(SRVN) $(OPTS) $<

.lqxo.diff:
	-srvndiff ../../models/regression/{petri,sim}-results/$*.p $< > $@

check:	$(OBJS)
	echo $(OBJS) | tr -s " " "\n" | srvndiff -QI -S$(EPSILON) -@- . ../../models/regression/petri-results/

check-verbose:
	FILES=`echo $(OBJS) | tr -s " " "\n" | srvndiff -QI -S$(EPSILON) -@- . ../../models/regression/petri-results`; \
	for i in $$FILES; do srvndiff ../../models/regression-xml/petri-results/$$i $$i; done

all:

clean-am:
	@-rm -rf $(OBJS) *.out nets empty

sanity:	$(SANITY)
	echo $(SANITY) | tr -s " " "\n" | srvndiff -S$(EPSILON) -Q -@- . ../../models/regression/petri-results

inter:	$(INTER)
	echo $(INTER) | tr -s " " "\n" | srvndiff -Q -S$(EPSILON) -@- . ../../models/regression/petri-results

multi:	$(MULTI)
	echo $(MULTI) | tr -s " " "\n" | srvndiff -Q -S$(EPSILON) -@- . ../../models/regression/petri-results

act:	$(ACT)
	echo $(ACT) | tr -s " " "\n" | srvndiff -Q -S$(EPSILON) -@- . ../../models/regression/petri-results

fork:	$(FORK)
	echo $(FORK) | tr -s " " "\n" | srvndiff -Q -@- . ../../models/regression/petri-results

sim:	$(SIM)
	echo $(SIM) | tr -s " " "\n" | srvndiff -Q -@- -S$(EPSILON) . ../../models/regression/petri-results

sema:	$(SEMA)
	echo $(SEMA) | tr -s " " "\n" | srvndiff -Q -@- -S$(EPSILON) . ../../models/regression/petri-results

sync:	$(SYNC)
	echo $(SYNC) | tr -s " " "\n" | srvndiff -Q -@- -S$(EPSILON) . ../../models/regression/petri-results

other:	$(OTHER)
	echo $(OTHER) | tr -s " " "\n" | srvndiff -Q -@- -S$(EPSILON) . ../../models/regression/petri-results

