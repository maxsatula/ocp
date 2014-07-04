ocp: main.o oracle.o
	cc -q64 -L$$ORACLE_HOME/lib -lclntsh oracle.o main.o -oocp

main.o: main.c oracle.h
	cc -q64 -I$$ORACLE_HOME/rdbms/public -c main.c

oracle.o: oracle.c oracle.h
	cc -q64 -I$$ORACLE_HOME/rdbms/public -c oracle.c

clean:
	rm main.o oracle.o ocp
