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
import java.sql.Connection;\n\
import java.sql.SQLException;\n\
import java.sql.Timestamp;\n\
import oracle.sql.ARRAY;\n\
import oracle.sql.ArrayDescriptor;\n\
import oracle.jdbc.driver.OracleDriver;\n\
\n\
public class j_ocp_DirList\n\
{\n\
	public static ARRAY getList(String directory)\n\
		throws SQLException\n\
	{\n\
		Connection conn = new OracleDriver().defaultConnection();\n\
		ArrayDescriptor arrayDescriptor = new ArrayDescriptor(\"T_OCP_FILE_LIST\", conn);\n\
		File path = new File(directory);\n\
		File[] files = path.listFiles();\n\
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
	FUNCTION f_ocp_dir_list(p_directory IN VARCHAR2)\n\
RETURN t_ocp_file_list\n\
AS LANGUAGE JAVA\n\
NAME 'j_ocp_DirList.getList( java.lang.String ) return oracle.sql.ARRAY';" },
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
