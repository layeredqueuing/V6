# Makefile for regression test
# Copyright (C) 2005 Greg Franks <greg@sce.carleton.ca>
#
# ------------------------------------------------------------------------
# $Id: Makefile.am 14114 2020-11-21 13:38:09Z greg $
# ------------------------------------------------------------------------

SRVN=	../lqsim
OPTS=	-C1.0,1000 -S1049217653 
#OPTS=	-B3,10000,1000 -M -S1049217653
EPSILON = '-S1.0'

SANITY=	01-sanity.lqxo \
	02-sanity.lqxo \
	03-sanity.lqxo \
	04-sanity.lqxo \
	05-sanity.lqxo \
	06-sanity.lqxo \
	07-sanity.lqxo \
	09-sanity.lqxo 
INTER=	10-interlock.lqxo \
	11-interlock.lqxo \
	12-interlock.lqxo \
	13-interlock.lqxo \
	14-interlock.lqxo \
	15-split-interlock.lqxo \
	16-split-interlock.lqxo \
	17-external-interlock.lqxo \
	19-subchain.lqxo
MULTI=	20-multiserver.lqxo \
	21-multiserver.lqxo \
	22-multiserver.lqxo \
	23-multiserver.lqxo \
	24-multiserver.lqxo
ACT=	40-activities.lqxo \
	41-activities.lqxo \
	42-activities.lqxo \
	43-activities.lqxo \
	44-activities.lqxo \
	45-activities.lqxo \
	46-activities.lqxo 
REPL=	50-replication-flat.lqxo \
	51-replication-flat.lqxo \
	52-replication-flat.lqxo \
	55-replication-flat.lqxo \
	56-replication-flat.lqxo 
SEMA =  60-semaphore.lqxo \
	61-semaphore.lqxo \
	62-semaphore.lqxo \
	65-buffer.lqxo \
	66-rwlock.lqxo \
	67-rwlock.lqxo \
	68-rwlock.lqxo \
	69-rwlock.lqxo
#	63-semaphore.lqxo 
SIM=	70-priority.lqxo \
	75-distributions.lqxo \
	76-distributions.lqxo
FAIR=	71-fair.lqxo \
	72-fair.lqxo 
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

SUFFIXES = .diff .d .svg .lqxo .lqnx 

.lqnx.lqxo:
	-@$(SRVN) $(OPTS) $<
	-@srvndiff -Q $(EPSILON) $@ ../../models/regression/sim-results/$*.p > /dev/null; if test $$? -ne 0; then echo $< failed; fi

.lqnx.d:
	$(SRVN) $(OPTS) $<

.lqxo.diff:
	-if test -f ../../models/regression/petri-results/$*.p; \
	then srvndiff ../../models/regression/{petri,sim}-results/$*.p $< > $@; \
	else srvndiff ../../models/regression/sim-results/$*.p $< > $@; \
	fi

.lqxo.svg:
	lqn2ps -Osvg $<


all:	

quick:
	rm -f $(SANITY)
	make -k OPTS="$(OPTS)" $(SANITY)

check:	$(OBJS)
	echo $(OBJS) | tr -s " " "\n" | srvndiff -Q $(EPSILON) -@- . ../../models/regression-xml/sim-results

check-syntax:
	$(MAKE) OPTS=-n check

quick-check:
	$(MAKE) OPTS=-S1049217653 check

check-verbose:
	FILES=`echo $(OBJS) | tr -s " " "\n" | srvndiff -Q $(EPSILON) -@- . ../../models/regression/sim-results`; \
	for i in $$FILES; do echo srvndiff ../../models/regression-xml/sim-results/$$i $$i; srvndiff ../../models/regression-xml/sim-results/$$i $$i; done

clean:
	@-rm -f $(OBJS) *.out 

distclean:	clean 
	-rm -f Makefile

sanity:	$(SANITY)
	echo $(SANITY) | tr -s " " "\n" | srvndiff -S0.01 -Q -@- . ../../models/regression-xml/sim-results

inter:	$(INTER)
	echo $(INTER) | tr -s " " "\n" | srvndiff -Q -@- . ../../models/regression-xml/sim-results

multi:	$(MULTI)
	echo $(MULTI) | tr -s " " "\n" | srvndiff -Q -@- . ../../models/regression-xml/sim-results

act:	$(ACT)
	echo $(ACT) | tr -s " " "\n" | srvndiff -Q -@- -S0.01 . ../../models/regression-xml/sim-results

fork:	$(FORK)
	echo $(FORK) | tr -s " " "\n" | srvndiff -Q -@- . ../../models/regression-xml/sim-results

fair:	$(FAIR)
	echo $(FAIR) | tr -s " " "\n" | srvndiff -Q -@- . ../../models/regression-xml/sim-results

repl:	$(REPL)
	echo $(REPL) | tr -s " " "\n" | srvndiff -Q -@- -S0.02 . ../../models/regression-xml/sim-results

sim:	$(SIM)
	echo $(SIM) | tr -s " " "\n" | srvndiff -Q -@- -S0.02 . ../../models/regression-xml/sim-results

sema:	$(SEMA)
	echo $(SEMA) | tr -s " " "\n" | srvndiff -Q -@- -S0.02 . ../../models/regression-xml/sim-results

sync:	$(SYNC)
	echo $(SYNC) | tr -s " " "\n" | srvndiff -Q -@- -S0.02 . ../../models/regression-xml/sim-results

other:	$(OTHER)
	echo $(OTHER) | tr -s " " "\n" | srvndiff -Q -@- -S0.02 . ../../models/regression-xml/sim-results
