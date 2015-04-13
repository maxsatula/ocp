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

#include <stdio.h>
#include <oci.h>
#include "oracle.h"
#include "ocp.h"

void LsDir(struct ORACLEALLINONE *oraAllInOne)
{
	sword ociResult;
	char vDirectory[ORA_IDENTIFIER_SIZE + 1];
	char vDirectoryPath[MAX_FMT_SIZE];
	char vGrantable1[MAX_FMT_SIZE];
	char vGrantable2[MAX_FMT_SIZE];

	struct ORACLEDEFINE oraDefinesLsDir[] =
	{
		{ 0, SQLT_STR, vDirectory,     sizeof(vDirectory)-1,     0 },
		{ 0, SQLT_STR, vDirectoryPath, sizeof(vDirectoryPath)-1, 0 },
		{ 0, SQLT_STR, vGrantable1,    sizeof(vGrantable1)-1,    0 },
		{ 0, SQLT_STR, vGrantable2,    sizeof(vGrantable2)-1,    0 }
	};

	struct ORACLESTATEMENT oraStmtLsDir = { "\
SELECT d.directory_name,\
       d.directory_path,\
       pr.grantable,\
       pw.grantable\
  FROM all_directories d\
       LEFT JOIN all_tab_privs pr\
       ON d.directory_name = pr.table_name\
          AND d.owner = pr.table_schema\
          AND pr.grantee = USER\
          AND pr.privilege = 'READ'\
       LEFT JOIN all_tab_privs pw\
       ON d.directory_name = pw.table_name\
          AND d.owner = pr.table_schema\
          AND pw.grantee = USER\
          AND pw.privilege = 'WRITE'",
	       0, 0, 0, oraDefinesLsDir, sizeof(oraDefinesLsDir)/sizeof(struct ORACLEDEFINE) };

	SetSessionAction(oraAllInOne, "LSDIR");
	PrepareStmtAndBind(oraAllInOne, &oraStmtLsDir);

	ociResult = ExecuteStmt(oraAllInOne);

	while (ociResult == OCI_SUCCESS)
	{
		printf("%c%c %-30s (%s)\n",
			   oraStmtLsDir.oraDefines[2].indp == -1 ? '-' :
			   *(char*)oraStmtLsDir.oraDefines[2].value == 'Y' ? 'R' : 'r',
			   oraStmtLsDir.oraDefines[3].indp == -1 ? '-' :
			   *(char*)oraStmtLsDir.oraDefines[3].value == 'Y' ? 'W' : 'w',
			   oraStmtLsDir.oraDefines[0].value,
			   oraStmtLsDir.oraDefines[1].value);

		ociResult = OCIStmtFetch2(oraStmtLsDir.stmthp, oraAllInOne->errhp, 1,
								  OCI_FETCH_NEXT, 1, OCI_DEFAULT);
	}

	if (ociResult != OCI_NO_DATA)
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to list oracle directories\n");

	ReleaseStmt(oraAllInOne);
	SetSessionAction(oraAllInOne, 0);
}
