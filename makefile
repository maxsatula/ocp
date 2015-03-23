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
ifeq ($(UNAME_S),Linux)
	LIBFLAGS += -lrt
endif

ocp: main.o oracle.o strlcat.o atomicio.o misc.o progressmeter.o
	cc $(CCFLAGS) $(LIBFLAGS) -L$$ORACLE_HOME/lib -lclntsh -lpopt -lz oracle.o strlcat.o atomicio.o misc.o progressmeter.o main.o -oocp

main.o: main.c oracle.h progressmeter.h
	cc $(CCFLAGS) $(INCLUDEFLAGS) -I$$ORACLE_HOME/rdbms/public -c main.c

oracle.o: oracle.c oracle.h
	cc $(CCFLAGS) $(INCLUDEFLAGS) -I$$ORACLE_HOME/rdbms/public -c oracle.c

strlcat.o: strlcat.c
	cc $(CCFLAGS) $(INCLUDEFLAGS) -I$$ORACLE_HOME/rdbms/public -c strlcat.c

atomicio.o: atomicio.c atomicio.h
	cc $(CCFLAGS) $(INCLUDEFLAGS) -I$$ORACLE_HOME/rdbms/public -c atomicio.c

misc.o: misc.c
	cc $(CCFLAGS) $(INCLUDEFLAGS) -I$$ORACLE_HOME/rdbms/public -c misc.c

progressmeter.o: progressmeter.c progressmeter.h atomicio.h misc.h
	cc $(CCFLAGS) $(INCLUDEFLAGS) -I$$ORACLE_HOME/rdbms/public -c progressmeter.c

clean:
	rm main.o oracle.o strlcat.o atomicio.o misc.o progressmeter.o ocp
