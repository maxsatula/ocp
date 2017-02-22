bin_PROGRAMS = ocp
ocp_CPPFLAGS = -I$(top_srcdir)/progressmeter -I$(top_srcdir)/yesno @ORACLE_OCI_CFLAGS@
ocp_LDADD = ../progressmeter/libprogressmeter.a ../yesno/libyesno.a -lc @ORACLE_OCI_LDFLAGS@
ocp_SOURCES = oracle.h ocp.h oracle.c longopsmeter.h longopsmeter.c \
              orafileattr.c trydir.c transfer.c compress.c transfercompr.c \
              lsdir.c ls.c rm.c install.c main.c
dist_ocp_SOURCES = Globs.text j_ocp_DirList.text compress.text uncompress.text compress_bg.text uncompress_bg.text downloadcompr.text uploadcompr.text
BUILT_SOURCES = Globs.text j_ocp_DirList.text compress.text uncompress.text compress_bg.text uncompress_bg.text downloadcompr.text uploadcompr.text
dist_noinst_DATA = Globs.java j_ocp_DirList.java compress.sql uncompress.sql compress_bg.sql uncompress_bg.sql downloadcompr.sql uploadcompr.sql

SUFFIXES = .sql .java .text
CONVERT_TO_C_LITERAL=sed -e 's/\\/\\\\/g;s/$$/\\n\\/g;s/"/\\"/g;1s/^/"/;$$s/\\n\\$$/"/'

.sql.text: Makefile
	$(CONVERT_TO_C_LITERAL) $< >$@

.java.text: Makefile
	$(CONVERT_TO_C_LITERAL) $< >$@
