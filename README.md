ocp
===

`ocp` is a command line tool to download and upload files from/to Oracle Database directories (e.g. DATA_PUMP_DIR)
using Oracle SQL Net connection only. Hence no other kind of filesystem access required to a database server.

The `ocp` name stands for 2 things:

* Abbreviation of Oracle CoPy
* Reference to a program author who is an Oracle Certified Professional ;)

## Why?

Starting from database version 10g, Oracle proudly introduced Data Pump utilities.
One of the major differences is dump file stored on a database server side, not on a client side anymore,
as it used to be with original exp/imp utilities.
Going further, original exp/imp utilities became desupported for general use as of Oracle Database 11g.
So that eventually we have no choice but to use Data Pump for moving data around.

Working with dump files on a server side was a great idea from Oracle and has many advantages (performance gain in the first place).
Sounds good so far.
However, sometimes users have a need to transfer a dump file across different databases which can be on the different parts of the globe.
And accessing of a dump file became more difficult for ordinary database users who do not have an operating system account on the database server.

There are several ways to establish user access to dump files stored on server, and network filesystems is an example (NFS, Samba, FTP etc), but do we really have to open more protocols/ports on a database server?
Or maybe we can use some protocol we already have there?
Yes, we can.
And that is a regular database connection!

How to transfer files from and to Oracle using a database connection, or SQL Net protocol, which is the same?
Oracle has UTL_FILE package which can serve our needs.
A few OCI calls which invoke procedures of that package, and we are all set. And this is exactly how `ocp` works.

## Required Privileges

Here is a set of database privileges required for a database user:

* Basic functionality
  * `CREATE SESSION`
  * `READ` and/or `WRITE ON DIRECTORY <directory_name>`
  * assuming `EXECUTE ON UTL_FILE` is granted to `PUBLIC`, otherwise grant explicitly
* To install supporting database objects required for `--ls` support. These can be revoked as soon as `ocp ... --install` has been executed.
  * `CREATE TYPE`
  * `CREATE PROCEDURE`
* `--ls` functionality
  * `dbms_java.grant_permission( '<user>', 'SYS:java.io.FilePermission', '<oracle_directory_path>', 'read' )`
  * `dbms_java.grant_permission( '<user>', 'SYS:java.io.FilePermission', '<oracle_directory_path>/*', 'read' )`
* `--background` option
  * `CREATE JOB`

