main.o: main.c
	cc -q64 -I$$ORACLE_HOME/rdbms/public -L$$ORACLE_HOME/lib -lclntsh -oocp main.c
