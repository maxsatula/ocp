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

#ifndef oracle_h
#define oracle_h

#include <oci.h>

#define MAX_FMT_SIZE 4096

enum ERROR_CLASS { ERROR_NONE, ERROR_OCI, ERROR_OS, ERROR_USAGE };

struct BINDVARIABLE
{
	OCIBind *ociBind;
	ub2 dty;
	const char* name;
	void* value;
	sb4 size;
};

struct ORACLEDEFINE
{
	OCIDefine *ociDefine;
	ub2 dty;
	void* value;
	sb4 size;
	sb2 indp;
};

struct ORACLESTATEMENT
{
	const char* sql;
	OCIStmt *stmthp;
	struct BINDVARIABLE *bindVariables;
	int bindVarsCount;
	struct ORACLEDEFINE *oraDefines;
	int oraDefineCount;
};

struct ORACLEALLINONE
{
	OCIEnv *envhp;
	OCIError *errhp;
	OCISvcCtx *svchp;
	OCIServer *srvhp;
	OCISession *usrhp;
	struct ORACLESTATEMENT *currentStmt;
};

void PrepareStmtAndBind(struct ORACLEALLINONE *oraAllInOne, struct ORACLESTATEMENT *oracleStatement);

sword ExecuteStmt(struct ORACLEALLINONE *oraAllInOne);

void ReleaseStmt(struct ORACLEALLINONE *oraAllInOne);

void ExitWithError(struct ORACLEALLINONE *oraAllInOne, int exitCode, enum ERROR_CLASS errorClass,
                   const char *message, ...);

void OracleLogon(struct ORACLEALLINONE *oraAllInOne,
                 const char* userName,
                 const char* password,
                 const char* connection);

#endif
