#!/bin/sh

# This script pulls newer versions of "borrowed" sources from other projects
# and then applies necessary patches to them to make them work here.
# It is not a part of build process and is never called automatically,
# because newer versions of foreign source files may cause a need in
# additional development.
# Use this script only when its containing directory is a current directory,
# like that:
# cd _ocp_project_dir_ && ./updateforeigns.sh
# and only if you are ready to fix potential errors

# Copyright (C) 2014  Max Satula
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


# 1. Download originals

function download {
	curl "$1" > "$2"
}

for file in progressmeter.h progressmeter.c atomicio.h atomicio.c misc.c; do
	download "http://cvsweb.openbsd.org/cgi-bin/cvsweb/~checkout~/src/usr.bin/ssh/${file}" \
	         progressmeter/${file}.orig
done
download "http://cvsweb.openbsd.org/cgi-bin/cvsweb/~checkout~/src/lib/libc/string/strlcat.c" \
         progressmeter/strlcat.c.orig
download "http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_lib_oracle_oci.m4" \
         m4/ax_lib_oracle_oci.m4.orig
download "http://git.savannah.gnu.org/gitweb/?p=gnulib.git;a=blob_plain;f=lib/yesno.h;hb=HEAD" \
         yesno/yesno.h
download "http://git.savannah.gnu.org/gitweb/?p=gnulib.git;a=blob_plain;f=lib/yesno.c;hb=HEAD" \
         yesno/yesno.c
download "https://raw.githubusercontent.com/rtyley/globs-for-java/master/src/main/java/com/madgag/globs/openjdk/Globs.java" \
         src/Globs.java.orig

# 2. Apply patches

# 2.1. Easy case, sources work well unmodified

for file in progressmeter.h atomicio.h; do
	cp progressmeter/${file}.orig progressmeter/${file}
done

# 2.2. Medium case, sources need a slight patch

for file in progressmeter/progressmeter.c progressmeter/atomicio.c progressmeter/strlcat.c \
        m4/ax_lib_oracle_oci.m4 src/Globs.java; do
	patch --backup-if-mismatch -u -o ${file} ${file}.orig ${file}.patch
done

# 2.3. Hard case, sources need a significant rewrite

# rebuild misc.h from scratch

(
cat << EOF
/*
 * This file has not just been taken from OpenSSH, because the original one
 * had a lot of declarations unused in ocp
 * Instead, this file has been generated from scratch based on source files,
 * pulling the only functionality really needed to compile ocp program.
 */

#ifndef _MISC_H
#define _MISC_H

EOF

grep -Ezo '(\w+)(\s*)monotime_double([^)]*)\)' progressmeter/misc.c.orig

cat << EOF
; /* taken from the original OpenSSH misc.h/misc.c */
EOF

grep -Ezo '(\w+)(\s*)strlcat([^)]*)\)' progressmeter/strlcat.c.orig

cat << EOF
; /* declaration for strlcat.c */

#endif /* _MISC_H */
EOF
) > progressmeter/misc.h

#drastically cut misc.c

(
filename=progressmeter/misc.c.orig
head $filename -n $(grep -n '\*/' $filename | sed -n '2p' |cut -f1 -d':')
cat << EOF

/*
 * Modified by Max Satula to keep only functionality needed for ocp program
 */

#include <sys/types.h>
#include <errno.h>
#include <time.h>

EOF
grep -Ezo '(\w+)(\s*)monotime_double([^}]*)}' ${filename} | sed \
	-e 's/if (/\/*if (*\//' \
	-e 's/ != 0)/\/* != 0)/' \
	-e 's/strerror(errno))/strerror(errno))*\//'
) > progressmeter/misc.c

# 3. Remove intermediate files

rm progressmeter/progressmeter.h.orig \
   progressmeter/progressmeter.c.orig \
   progressmeter/atomicio.h.orig \
   progressmeter/atomicio.c.orig \
   progressmeter/misc.c.orig \
   progressmeter/strlcat.c.orig \
   m4/ax_lib_oracle_oci.m4.orig \
   src/Globs.java.orig

