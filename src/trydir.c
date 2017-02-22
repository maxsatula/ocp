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

#include <ctype.h>
#include <oci.h>
#include "oracle.h"
#include "ocp.h"

void TryDirectory(struct ORACLEALLINONE *oraAllInOne, char* pDirectory)
{
	sb1 vFlag;
	char* ptr;

	struct BINDVARIABLE bindVariablesTryDirectory[] =
	{
		{ 0, SQLT_STR, ":directory", pDirectory, ORA_IDENTIFIER_SIZE + 1 },
		{ 0 }
	};

	struct ORACLEDEFINE oraDefinesTryDirectory[] =
	{
		{ 0, SQLT_INT, &vFlag, sizeof(vFlag), 0 },
                { 0 }
        };

	struct ORACLESTATEMENT oraStmtTryDirectory = { "\
SELECT MIN(DECODE(directory_name, :directory, 1, 2))\n\
  FROM all_directories\n\
 WHERE directory_name IN (:directory, UPPER(:directory))\
",
	       0, bindVariablesTryDirectory, oraDefinesTryDirectory };

        PrepareStmtAndBind(oraAllInOne, &oraStmtTryDirectory);

        if (ExecuteStmt(oraAllInOne))
                ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to get an Oracle Directory\n");

        ReleaseStmt(oraAllInOne);

	switch (vFlag)
	{
	case 1:
		break;
	case 2:
		ExitWithError(oraAllInOne, -1, ERROR_NONE, "WARNING: directory \"%s\" not found, converting to uppercase\n", pDirectory);
		for (ptr = pDirectory; *ptr; ptr++)
			*ptr = toupper(*ptr);
		break;
	default:
		ExitWithError(oraAllInOne, 4, ERROR_NONE, "Directory \"%s\" does not exist\n", pDirectory);
	}
}
