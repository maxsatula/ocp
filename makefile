ifndef ORACLE_HOME
	$(error Oracle Client not found)
endif

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),AIX)
	CCFLAGS += -q64
	LIBFLAGS += -L/opt/pware64/lib
	INCLUDEFLAGS += -I/opt/pware64/include
# -qlanglvl=ansi
endif

ocp: main.o oracle.o
	cc $(CCFLAGS) $(LIBFLAGS) -L$$ORACLE_HOME/lib -lclntsh -lpopt -lz oracle.o main.o -oocp

main.o: main.c oracle.h
	cc $(CCFLAGS) $(INCLUDEFLAGS) -I$$ORACLE_HOME/rdbms/public -c main.c

oracle.o: oracle.c oracle.h
	cc $(CCFLAGS) $(INCLUDEFLAGS) -I$$ORACLE_HOME/rdbms/public -c oracle.c

clean:
	rm main.o oracle.o ocp
