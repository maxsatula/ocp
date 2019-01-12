#!/bin/sh -e

sqlplus -L -S sys/$ORACLE_PWD@127.0.0.1:1521/XEPDB1 AS SYSDBA <<SQL
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

BEGIN
  FOR d IN (SELECT directory_path
              FROM dba_directories
             WHERE directory_name = 'DATA_PUMP_DIR') LOOP
    dbms_java.grant_permission( 'OCPTEST', 'SYS:java.io.FilePermission', d.directory_path, 'read' );
    dbms_java.grant_permission( 'OCPTEST', 'SYS:java.io.FilePermission', d.directory_path || '/*', 'read' );
  END LOOP;
END;
/

SQL

expdp system/$ORACLE_PWD@127.0.0.1:1521/XEPDB1 logfile=expdp_system.log dumpfile=somefile.dmp directory=data_pump_dir
