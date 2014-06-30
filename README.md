ocp
===

ocp is a command line tool for downloading and uploading files from/to Oracle Database directories (e.g. DATA_PUMP_DIR)
using Oracle SQL Net connection only. Hence no physical or filesystem access required to a database server. 

The name stands for 2 things:

* Abbreviation for Oracle CoPy
* Reference to a program author who is Oracle Certified Professional ;)

## Why?

Starting from version 10g, Oracle introduced Data Pump. One of the major differences is dump file stored on a server side, not a client side, unlike legacy exp/imp utilities. Going further, in Oracle 11g exp/imp utilities declared as unsupported.

Working with dump files on a server side was a great idea from Oracle and has many advantages (performance in the first place). Sounds good. However, sometimes users need to transfer dump files across different databases which can be on the different parts of the globe. And it makes access to dump files more difficult for ordinary database users who do not have an operating system account on the database server.

There are several ways to establish user access to server stored dump files, network shared filesystems for example (NFS, Samba). Now let's imagine we do not need to operate with huge dumps, but rather using relatively small dumps to deploy some initial schema on multiple sites. Why not to deviate from Oracle strategy a little bit and not to keep a Data Pump dump file on a client site, later copying it to the database server without exposing its filesystems via unnecessary protocols?

The solution is to make it possible to transfer files from Oracle and back just using a regular database connection!

Oracle has a UTL_FILE package which can serve our needs. A few OCI calls invoking this package procedures, and we are all set.
