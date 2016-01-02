bin_PROGRAMS = ocp
ocp_CPPFLAGS = -I$(top_srcdir)/progressmeter -I$(top_srcdir)/yesno @ORACLE_OCI_CFLAGS@
ocp_LDADD = ../progressmeter/libprogressmeter.a ../yesno/libyesno.a @ORACLE_OCI_LDFLAGS@
ocp_SOURCES = oracle.h ocp.h oracle.c longopsmeter.h longopsmeter.c \
              orafileattr.c trydir.c transfer.c compress.c transfercompr.c \
              lsdir.c ls.c rm.c install.c main.c
dist_ocp_SOURCES = Globs.text j_ocp_DirList.text
BUILT_SOURCES = Globs.text j_ocp_DirList.text
dist_noinst_DATA = Globs.java j_ocp_DirList.java

CONVERT_TO_C_LITERAL=sed -e 's/\\/\\\\/g;s/$$/\\n\\/g;s/"/\\"/g;1s/^/"/;$$s/\\n\\$$/"/'

Globs.text: Globs.java Makefile
	$(CONVERT_TO_C_LITERAL) $< >$@

j_ocp_DirList.text: j_ocp_DirList.java Makefile
	$(CONVERT_TO_C_LITERAL) $< >$@
