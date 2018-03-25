#!/bin/sh -e

"$ORACLE_HOME/bin/sqlplus" -L -S / AS SYSDBA <<SQL
CREATE USER ocptest IDENTIFIED BY test;

GRANT CREATE SESSION,
      CREATE TYPE,
      CREATE PROCEDURE,
      CREATE JOB
   TO ocptest;

GRANT READ, WRITE
   ON DIRECTORY data_pump_dir
   TO ocptest;

GRANT EXECUTE
   ON utl_file
   TO ocptest;

GRANT SELECT
   ON v_\$session
   TO ocptest;
SQL

"$ORACLE_HOME/bin/expdp" system/travis logfile=expdp_system.log dumpfile=somefile.dmp directory=data_pump_dir
