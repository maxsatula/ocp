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
	last_modified DATE,\n\
	digest RAW(20));" },
		{ "\
CREATE OR REPLACE\n\
	TYPE t_ocp_file_list IS TABLE OF t_ocp_file" },
		{
#include "j_ocp_DirList.text"
#include "Globs.text"
	},
		{ "\
CREATE OR REPLACE\n\
	FUNCTION f_ocp_dir_list(p_directory IN VARCHAR2, p_pattern IN VARCHAR2, p_hashalgorithm IN VARCHAR2)\n\
RETURN t_ocp_file_list\n\
AS LANGUAGE JAVA\n\
NAME 'j_ocp_DirList.getList( java.lang.String, java.lang.String, java.lang.String ) return oracle.sql.ARRAY';" },
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
