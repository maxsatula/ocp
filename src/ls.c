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
#include <string.h>
#include <oci.h>
#include "oracle.h"
#include "ocp.h"

void Ls(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, char* patterns, int patternLength, enum HASH_ALGORITHM hashAlgorithm)
{
	sword ociResult;
	char vFileName[MAX_FMT_SIZE];
	char vHashAlgorithm[8];
	ub8 vBytes;
	char vLastModified[7];
	unsigned char vDigest[20];
	int hashLength;
	int i, j;
	long long totalBytes;
	int foundKnownSize, foundUnknownSize;

	struct BINDVARIABLE oraBindsLs[] =
	{
		{ 0, SQLT_CHR, ":patterns",        patterns,         patternLength           },
		{ 0, SQLT_STR, ":hash_algorithm",  vHashAlgorithm,   sizeof(vHashAlgorithm)  },
		{ 0, SQLT_STR, ":directory",       pDirectory,       ORA_IDENTIFIER_SIZE + 1 },
		{ 0 }
	};

	struct ORACLEDEFINE oraDefinesLs[] =
	{
		{ 0, SQLT_STR, vFileName,     sizeof(vFileName)-1,   0 },
		{ 0, SQLT_INT, &vBytes,       sizeof(vBytes),        0 },
		{ 0, SQLT_DAT, vLastModified, sizeof(vLastModified), 0 },
		{ 0, SQLT_BIN, vDigest,       sizeof(vDigest),       0 },
		{ 0 }
	};

	struct ORACLESTATEMENT oraStmtLs = {
	       "\
SELECT t.file_name,\n\
       t.bytes,\n\
       t.last_modified,\n\
       t.digest\n\
  FROM all_directories d,\n\
       TABLE(f_ocp_dir_list(d.directory_path, :patterns, :hash_algorithm)) t\n\
 WHERE d.directory_name = :directory",
	       0, oraBindsLs, oraDefinesLs };

	switch (hashAlgorithm)
	{
	case HASH_MD5:
		strcpy(vHashAlgorithm, "MD5");
		hashLength = 16;
		break;
	case HASH_SHA1:
		strcpy(vHashAlgorithm, "SHA1");
		hashLength = 20;
		break;
	case HASH_NONE:
		vHashAlgorithm[0] = '\0';
		break;
	}
	SetSessionAction(oraAllInOne, "LS");
	PrepareStmtAndBind(oraAllInOne, &oraStmtLs);

	if (hashAlgorithm != HASH_NONE)
	{
		printf("Contents of %s directory\n\
%-40s %s hash\n\
---------------------------------------- ",
		       pDirectory, "File Name", vHashAlgorithm);
		for (j = 0; j < hashLength*2; j++)
			printf("-");
		printf("\n");
	}
	else
	{
		printf("Contents of %s directory\n\
%-40s %-12s %s\n\
---------------------------------------- ------------ -------------------\n",
		       pDirectory, "File Name", "    Size", "Last Modified");
	}

	i = 0;
	totalBytes = 0;
	foundKnownSize = foundUnknownSize = 0;
	ociResult = ExecuteStmt(oraAllInOne);
	while (ociResult == OCI_SUCCESS)
	{
		if (hashAlgorithm != HASH_NONE)
		{
			printf("%-40s ",
			       vFileName);
			if (oraStmtLs.oraDefines[3].indp != -1)
			{
				for (j = 0; j < hashLength; j++)
				{
					printf("%02x", vDigest[j]);
				}
			}
			printf("\n");
		}
		else if (oraStmtLs.oraDefines[1].indp != -1 &&
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

	if (hashAlgorithm == HASH_NONE)
	{
		if (i)
			printf("---------------------------------------- ------------ -------------------\n");
		printf("%5d File(s)", i);
		if (!foundKnownSize && foundUnknownSize)
			printf("\n");
		else
			printf(" %39lld%s\n", totalBytes, foundUnknownSize ? "+" : "");
	}

	ReleaseStmt(oraAllInOne);	
	SetSessionAction(oraAllInOne, 0);
}
