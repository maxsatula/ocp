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

void Ls(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, const char* patterns, int patternLength)
{
	sword ociResult;
	char vFileName[MAX_FMT_SIZE];
	ub8 vBytes;
	char vLastModified[7];
	int i;
	long long totalBytes;
	int foundKnownSize, foundUnknownSize;

	struct BINDVARIABLE oraBindsLs[] =
	{
		{ 0, SQLT_CHR, ":patterns",  patterns,   patternLength           },
		{ 0, SQLT_STR, ":directory", pDirectory, ORA_IDENTIFIER_SIZE + 1 },
		{ 0 }
	};

	struct ORACLEDEFINE oraDefinesLs[] =
	{
		{ 0, SQLT_STR, vFileName,     sizeof(vFileName)-1,   0 },
		{ 0, SQLT_INT, &vBytes,       sizeof(vBytes),        0 },
		{ 0, SQLT_DAT, vLastModified, sizeof(vLastModified), 0 },
		{ 0 }
	};

	struct ORACLESTATEMENT oraStmtLs = {
	       "\
SELECT t.file_name,\
       t.bytes,\
       t.last_modified\
  FROM all_directories d,\
       TABLE(f_ocp_dir_list(d.directory_path, :patterns)) t\
 WHERE d.directory_name = :directory",
	       0, oraBindsLs, oraDefinesLs };

	SetSessionAction(oraAllInOne, "LS");
	PrepareStmtAndBind(oraAllInOne, &oraStmtLs);

	printf("Contents of %s directory\n\
%-40s %-12s %s\n\
---------------------------------------- ------------ -------------------\n",
	       pDirectory, "File Name", "    Size", "Last Modified");

	i = 0;
	totalBytes = 0;
	foundKnownSize = foundUnknownSize = 0;
	ociResult = ExecuteStmt(oraAllInOne);
	while (ociResult == OCI_SUCCESS)
	{
		if (oraStmtLs.oraDefines[1].indp != -1 &&
		    oraStmtLs.oraDefines[2].indp != -1)
		{
			printf("%-40s %12lld %02d/%02d/%d %02d:%02d:%02d\n",
			       vFileName,
			       (long long)vBytes,
			       (int)vLastModified[2],
			       (int)vLastModified[3],
			       ((int)vLastModified[0]-100) * 100 + ((int)vLastModified[1] - 100),
			       (int)vLastModified[4] - 1,
			       (int)vLastModified[5] - 1,
			       (int)vLastModified[6] - 1);
			totalBytes += vBytes;
			foundKnownSize = 1;
		}
		else
		{
			printf("%-40s  (no access) (no access)\n", vFileName);
			foundUnknownSize = 1;
		}
		i++;

		ociResult = OCIStmtFetch2(oraStmtLs.stmthp, oraAllInOne->errhp, 1,
								  OCI_FETCH_NEXT, 1, OCI_DEFAULT);
	}

	if (ociResult != OCI_NO_DATA)
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to list files in oracle directory\n");

	if (i)
		printf("---------------------------------------- ------------ -------------------\n");
	printf("%5d File(s)", i);
	if (!foundKnownSize && foundUnknownSize)
		printf("\n");
	else
		printf(" %39lld%s\n", totalBytes, foundUnknownSize ? "+" : "");

	ReleaseStmt(oraAllInOne);	
	SetSessionAction(oraAllInOne, 0);
}
