ifndef ORACLE_HOME
	$(error Oracle Client not found)
endif

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),AIX)
	CCFLAGS += -q64
endif

ocp: main.o oracle.o
	cc $(CCFLAGS) -L$$ORACLE_HOME/lib -lclntsh oracle.o main.o -oocp

main.o: main.c oracle.h
	cc $(CCFLAGS) -I$$ORACLE_HOME/rdbms/public -c main.c

oracle.o: oracle.c oracle.h
	cc $(CCFLAGS) -I$$ORACLE_HOME/rdbms/public -c oracle.c

clean:
	rm main.o oracle.o ocp
