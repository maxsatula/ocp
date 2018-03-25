#!/bin/sh
PATH=$PATH:$PWD/src
CON=ocptest/test@localhost:1521/xe

# Oracle XE does not support Java, thus no tests for --ls

ocp $CON --list-directories
ocp $CON DATA_PUMP_DIR:somefile.dmp localfile.dmp
ocp $CON -9 DATA_PUMP_DIR:somefile.dmp onthefly.dmp
ocp $CON -9 onthefly.dmp DATA_PUMP_DIR:
ocp $CON --gzip DATA_PUMP_DIR:somefile.dmp
ocp $CON --rm DATA_PUMP_DIR:onthefly.dmp
