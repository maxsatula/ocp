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
		{ 0, SQLT_INT, ":exists",    &oraFileAttr->bExists, sizeof(oraFileAttr->bExists) }
	};

	struct ORACLESTATEMENT oraStmtFattr = { "\
declare \
  exists_ BOOLEAN; \
  length_ NUMBER; \
  blocksize_ NUMBER; \
begin \
  utl_file.fgetattr(:directory, :filename, exists_, length_, blocksize_); \
  if not exists_ and length_ is null then \
    :length := 0; \
  else \
    :length := length_; \
  end if; \
  :exists := case when exists_ then 1 else 0 end; \
end;",
	       0, bindVariablesFattr, sizeof(bindVariablesFattr)/sizeof(struct BINDVARIABLE), 0, 0 };

        PrepareStmtAndBind(oraAllInOne, &oraStmtFattr);

        if (ExecuteStmt(oraAllInOne))
                ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to get remote file attributes\n");

        ReleaseStmt(oraAllInOne);
}
