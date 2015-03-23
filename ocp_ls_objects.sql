/*****************************************************************************
Copyright (C) 2014  Max Satula

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

-- Requires CREATE TYPE, CREATE PROCEDURE

DROP TYPE t_ocp_file_list
/

CREATE OR REPLACE
	TYPE t_ocp_file AS OBJECT (
	file_name VARCHAR2(200),
	bytes NUMBER,
	last_modified DATE);
/

CREATE OR REPLACE
	TYPE t_ocp_file_list IS TABLE OF t_ocp_file
/

CREATE OR REPLACE
	AND COMPILE
	JAVA SOURCE NAMED "j_ocp_DirList"
AS
import java.io.File;
import java.sql.Connection;
import java.sql.SQLException;
import java.sql.Timestamp;
import oracle.sql.ARRAY;
import oracle.sql.ArrayDescriptor;
import oracle.jdbc.driver.OracleDriver;

public class j_ocp_DirList
{
	public static ARRAY getList(String directory)
		throws SQLException
	{
		Connection conn = new OracleDriver().defaultConnection();
		ArrayDescriptor arrayDescriptor = new ArrayDescriptor("T_OCP_FILE_LIST", conn);
		File path = new File(directory);
		File[] files = path.listFiles();
		Object[][] result = new Object[files.length][3];
		for (int i = 0; i < files.length; i++)
		{
			result[i][0] = files[i].getName();
			result[i][1] = new Long(files[i].length());
			result[i][2] = new Timestamp(files[i].lastModified());
		}
		return new ARRAY(arrayDescriptor, conn, result);
	}
}
/

CREATE OR REPLACE
	FUNCTION f_ocp_dir_list(p_directory IN VARCHAR2)
RETURN t_ocp_file_list
AS LANGUAGE JAVA
NAME 'j_ocp_DirList.getList( java.lang.String ) return oracle.sql.ARRAY';
/
