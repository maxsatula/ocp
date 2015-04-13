bin_PROGRAMS = ocp
ocp_CPPFLAGS = -I$(top_srcdir)/progressmeter -I$(top_srcdir)/yesno @ORACLE_OCI_CFLAGS@
ocp_LDADD = ../progressmeter/libprogressmeter.a ../yesno/libyesno.a @ORACLE_OCI_LDFLAGS@
ocp_SOURCES = oracle.h ocp.h oracle.c \
              orafileattr.c transfer.c compress.c transfercompr.c \
              lsdir.c ls.c rm.c install.c main.c
