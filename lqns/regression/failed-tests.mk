FILES=	12-interlock.lqxo \
	13-interlock.lqxo \
	16-split-interlock.lqxo \
	17-external-interlock.lqxo \
	44-activities.lqxo \
	50-replication.lqxo \
	51-replication.lqxo \
	55-replication.lqxo \
	56-replication.lqxo \
	85-fork.lqxo \
	86-fork.lqxo \
	87-fork.lqxo \
	90-A01.lqxo \
	90-B07.lqxo \
	91-cs3-1.lqxo \
	93-simple-ucm.lqxo \
	94-5101-a2-q2b1.lqxo

all:
	for i in $(FILES); do \
	  srvndiff -Q $$i ../../../src-judy/lqns/regression/$$i > /dev/null; \
	  if test $$?; then result="fail"; else result="pass"; fi; \
	  echo \"$$i\",$$result; \
	done
