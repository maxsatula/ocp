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

#include <oci.h>
#include "oracle.h"
#include "ocp.h"

void GetOracleFileAttr(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, char* pFileName, struct ORACLEFILEATTR *oraFileAttr)
{
	struct BINDVARIABLE bindVariablesFattr[] =
	{
		{ 0, SQLT_STR, ":directory", pDirectory,            ORA_IDENTIFIER_SIZE + 1      },
		{ 0, SQLT_STR, ":filename",  pFileName,             MAX_FMT_SIZE                 },
		{ 0, SQLT_INT, ":length",    &oraFileAttr->length,  sizeof(oraFileAttr->length)  },
		{ 0, SQLT_INT, ":exists",    &oraFileAttr->bExists, sizeof(oraFileAttr->bExists) },
		{ 0 }
	};

	struct ORACLESTATEMENT oraStmtFattr = { "\
declare\n\
  exists_ BOOLEAN;\n\
  length_ NUMBER;\n\
  blocksize_ NUMBER;\n\
begin\n\
  utl_file.fgetattr(:directory, :filename, exists_, length_, blocksize_);\n\
  if not exists_ and length_ is null then\n\
    :length := 0;\n\
  else\n\
    :length := length_;\n\
  end if;\n\
  :exists := case when exists_\n\
                  then 1\n\
                  else 0\n\
             end;\n\
end;",
	       0, bindVariablesFattr, NO_ORACLE_DEFINES };

        PrepareStmtAndBind(oraAllInOne, &oraStmtFattr);

        if (ExecuteStmt(oraAllInOne))
                ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to get remote file attributes\n");

        ReleaseStmt(oraAllInOne);
}
