/*****************************************************************************
Copyright (C) 2015  Max Satula

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*****************************************************************************/

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "oracle.h"
#include "ocp.h"

void InstallObjects(struct ORACLEALLINONE* oraAllInOne)
{
	struct ORACLESIMPLESQL sqls[] =
	{
		{ "\
DROP TYPE t_ocp_file_list", 4043 },
		{ "\
CREATE OR REPLACE\n\
	TYPE t_ocp_file AS OBJECT (\n\
	file_name VARCHAR2(200),\n\
	bytes NUMBER,\n\
	last_modified DATE);" },
		{ "\
CREATE OR REPLACE\n\
	TYPE t_ocp_file_list IS TABLE OF t_ocp_file" },
		{ "\
CREATE OR REPLACE\n\
	AND COMPILE\n\
	JAVA SOURCE NAMED \"j_ocp_DirList\"\n\
AS\n\
import java.io.File;\n\
import java.io.FilenameFilter;\n\
import java.sql.Connection;\n\
import java.sql.SQLException;\n\
import java.sql.Timestamp;\n\
import oracle.sql.ARRAY;\n\
import oracle.sql.ArrayDescriptor;\n\
import oracle.jdbc.driver.OracleDriver;\n\
\n\
//START OF GLOB\n\
//URL https://github.com/rtyley/globs-for-java/raw/master/src/main/java/com/madgag/globs/openjdk/Globs.java\n\
/*\n\
 * Copyright (c) 2008, 2009, Oracle and/or its affiliates. All rights reserved.\n\
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.\n\
 *\n\
 * This code is free software; you can redistribute it and/or modify it\n\
 * under the terms of the GNU General Public License version 2 only, as\n\
 * published by the Free Software Foundation.  Oracle designates this\n\
 * particular file as subject to the \"Classpath\" exception as provided\n\
 * by Oracle in the LICENSE file that accompanied this code.\n\
 *\n\
 * This code is distributed in the hope that it will be useful, but WITHOUT\n\
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or\n\
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License\n\
 * version 2 for more details (a copy is included in the LICENSE file that\n\
 * accompanied this code).\n\
 *\n\
 * You should have received a copy of the GNU General Public License version\n\
 * 2 along with this work; if not, write to the Free Software Foundation,\n\
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.\n\
 *\n\
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA\n\
 * or visit www.oracle.com if you need additional information or have any\n\
 * questions.\n\
 */\n\
\n\
//COMMENT THIS OUT#package com.madgag.globs.openjdk;\n\
\n\
import java.util.regex.PatternSyntaxException;\n\
\n\
public class Globs {\n\
    private Globs() { }\n\
\n\
    private static final String regexMetaChars = \".^$+{[]|()\";\n\
    private static final String globMetaChars = \"\\\\*?[{\";\n\
\n\
    private static boolean isRegexMeta(char c) {\n\
        return regexMetaChars.indexOf(c) != -1;\n\
    }\n\
\n\
    private static boolean isGlobMeta(char c) {\n\
        return globMetaChars.indexOf(c) != -1;\n\
    }\n\
    private static char EOL = 0;  //TBD\n\
\n\
    private static char next(String glob, int i) {\n\
        if (i < glob.length()) {\n\
            return glob.charAt(i);\n\
        }\n\
        return EOL;\n\
    }\n\
\n\
    /**\n\
     * Creates a regex pattern from the given glob expression.\n\
     *\n\
     * @throws  PatternSyntaxException\n\
     */\n\
    private static String toRegexPattern(String globPattern, boolean isDos) {\n\
        boolean inGroup = false;\n\
        StringBuilder regex = new StringBuilder(\"^\");\n\
\n\
        int i = 0;\n\
        while (i < globPattern.length()) {\n\
            char c = globPattern.charAt(i++);\n\
            switch (c) {\n\
                case '\\\\':\n\
                    // escape special characters\n\
                    if (i == globPattern.length()) {\n\
                        throw new PatternSyntaxException(\"No character to escape\",\n\
                                globPattern, i - 1);\n\
                    }\n\
                    char next = globPattern.charAt(i++);\n\
                    if (isGlobMeta(next) || isRegexMeta(next)) {\n\
                        regex.append('\\\\');\n\
                    }\n\
                    regex.append(next);\n\
                    break;\n\
                case '/':\n\
                    if (isDos) {\n\
                        regex.append(\"\\\\\\\\\");\n\
                    } else {\n\
                        regex.append(c);\n\
                    }\n\
                    break;\n\
                case '[':\n\
                    // don't match name separator in class\n\
                    if (isDos) {\n\
                        regex.append(\"[[^\\\\\\\\]&&[\");\n\
                    } else {\n\
                        regex.append(\"[[^/]&&[\");\n\
                    }\n\
                    if (next(globPattern, i) == '^') {\n\
                        // escape the regex negation char if it appears\n\
                        regex.append(\"\\\\^\");\n\
                        i++;\n\
                    } else {\n\
                        // negation\n\
                        if (next(globPattern, i) == '!') {\n\
                            regex.append('^');\n\
                            i++;\n\
                        }\n\
                        // hyphen allowed at start\n\
                        if (next(globPattern, i) == '-') {\n\
                            regex.append('-');\n\
                            i++;\n\
                        }\n\
                    }\n\
                    boolean hasRangeStart = false;\n\
                    char last = 0;\n\
                    while (i < globPattern.length()) {\n\
                        c = globPattern.charAt(i++);\n\
                        if (c == ']') {\n\
                            break;\n\
                        }\n\
                        if (c == '/' || (isDos && c == '\\\\')) {\n\
                            throw new PatternSyntaxException(\"Explicit 'name separator' in class\",\n\
                                    globPattern, i - 1);\n\
                        }\n\
                        // TBD: how to specify ']' in a class?\n\
                        if (c == '\\\\' || c == '[' ||\n\
                                c == '&' && next(globPattern, i) == '&') {\n\
                            // escape '\\', '[' or \"&&\" for regex class\n\
                            regex.append('\\\\');\n\
                        }\n\
                        regex.append(c);\n\
\n\
                        if (c == '-') {\n\
                            if (!hasRangeStart) {\n\
                                throw new PatternSyntaxException(\"Invalid range\",\n\
                                        globPattern, i - 1);\n\
                            }\n\
                            if ((c = next(globPattern, i++)) == EOL || c == ']') {\n\
                                break;\n\
                            }\n\
                            if (c < last) {\n\
                                throw new PatternSyntaxException(\"Invalid range\",\n\
                                        globPattern, i - 3);\n\
                            }\n\
                            regex.append(c);\n\
                            hasRangeStart = false;\n\
                        } else {\n\
                            hasRangeStart = true;\n\
                            last = c;\n\
                        }\n\
                    }\n\
                    if (c != ']') {\n\
                        throw new PatternSyntaxException(\"Missing ']\", globPattern, i - 1);\n\
                    }\n\
                    regex.append(\"]]\");\n\
                    break;\n\
                case '{':\n\
                    if (inGroup) {\n\
                        throw new PatternSyntaxException(\"Cannot nest groups\",\n\
                                globPattern, i - 1);\n\
                    }\n\
                    regex.append(\"(?:(?:\");\n\
                    inGroup = true;\n\
                    break;\n\
                case '}':\n\
                    if (inGroup) {\n\
                        regex.append(\"))\");\n\
                        inGroup = false;\n\
                    } else {\n\
                        regex.append('}');\n\
                    }\n\
                    break;\n\
                case ',':\n\
                    if (inGroup) {\n\
                        regex.append(\")|(?:\");\n\
                    } else {\n\
                        regex.append(',');\n\
                    }\n\
                    break;\n\
                case '*':\n\
                    if (next(globPattern, i) == '*') {\n\
                        // crosses directory boundaries\n\
                        regex.append(\".*\");\n\
                        i++;\n\
                    } else {\n\
                        // within directory boundary\n\
                        if (isDos) {\n\
                            regex.append(\"[^\\\\\\\\]*\");\n\
                        } else {\n\
                            regex.append(\"[^/]*\");\n\
                        }\n\
                    }\n\
                    break;\n\
                case '?':\n\
                   if (isDos) {\n\
                       regex.append(\"[^\\\\\\\\]\");\n\
                   } else {\n\
                       regex.append(\"[^/]\");\n\
                   }\n\
                   break;\n\
\n\
                default:\n\
                    if (isRegexMeta(c)) {\n\
                        regex.append('\\\\');\n\
                    }\n\
                    regex.append(c);\n\
            }\n\
        }\n\
\n\
        if (inGroup) {\n\
            throw new PatternSyntaxException(\"Missing '}\", globPattern, i - 1);\n\
        }\n\
\n\
        return regex.append('$').toString();\n\
    }\n\
\n\
    public static String toUnixRegexPattern(String globPattern) {\n\
        return toRegexPattern(globPattern, false);\n\
    }\n\
\n\
    public static String toWindowsRegexPattern(String globPattern) {\n\
        return toRegexPattern(globPattern, true);\n\
    }\n\
}\n\
//END OF GLOB\n\
\n\
public class j_ocp_DirList\n\
{\n\
	public static ARRAY getList(String directory, String pattern)\n\
		throws SQLException\n\
	{\n\
		Connection conn = new OracleDriver().defaultConnection();\n\
		ArrayDescriptor arrayDescriptor = new ArrayDescriptor(\"T_OCP_FILE_LIST\", conn);\n\
		File path = new File(directory);\n\
		File[] files;\n\
		if (pattern != null) {\n\
			final String[] patterns = pattern.split(\"\\0\");\n\
			FilenameFilter globFilter = new FilenameFilter() {\n\
				public boolean accept(File dir, String name) {\n\
					for (String pattern : patterns) {\n\
						if (name.matches(Globs.toUnixRegexPattern(pattern))) {\n\
							return true;\n\
						}\n\
					}\n\
					return false;\n\
				}\n\
			};\n\
			files = path.listFiles(globFilter);\n\
		} else {\n\
			files = path.listFiles();\n\
		}\n\
		Object[][] result = new Object[files.length][3];\n\
		for (int i = 0; i < files.length; i++)\n\
		{\n\
			result[i][0] = files[i].getName();\n\
			try\n\
			{\n\
				result[i][1] = new Long(files[i].length());\n\
				result[i][2] = new Timestamp(files[i].lastModified());\n\
			}\n\
			catch ( java.security.AccessControlException e ) {}\n\
		}\n\
		return new ARRAY(arrayDescriptor, conn, result);\n\
	}\n\
}" },
		{ "\
CREATE OR REPLACE\n\
	FUNCTION f_ocp_dir_list(p_directory IN VARCHAR2, p_pattern IN VARCHAR2)\n\
RETURN t_ocp_file_list\n\
AS LANGUAGE JAVA\n\
NAME 'j_ocp_DirList.getList( java.lang.String, java.lang.String ) return oracle.sql.ARRAY';" },
		{ 0 }
	};

	ExecuteSimpleSqls(oraAllInOne, sqls);
}

void DeinstallObjects(struct ORACLEALLINONE* oraAllInOne)
{
	struct ORACLESIMPLESQL sqls[] =
	{
		{ "DROP FUNCTION f_ocp_dir_list",       4043 },
		{ "DROP JAVA SOURCE \"j_ocp_DirList\"", 4043 },
		{ "DROP TYPE t_ocp_file_list",          4043 },
		{ "DROP TYPE t_ocp_file" ,              4043 },
		{ 0 }
	};

	ExecuteSimpleSqls(oraAllInOne, sqls);
}
