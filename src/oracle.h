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

#ifndef _ORACLE_H_
#define _ORACLE_H_

#include <oci.h>

#define MAX_FMT_SIZE 4096
#define ORA_IDENTIFIER_SIZE 30
#define MAX_ORA_SESSIONS 2

enum ERROR_CLASS { ERROR_NONE, ERROR_OCI, ERROR_OS };

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
	struct ORACLEDEFINE *oraDefines;
};

struct ORACLESIMPLESQL
{
	const char* sql;
	sb4 errCodeToIgnore;
};

struct ORACLEALLINONE
{
	OCIEnv *envhp;
	OCIError *errhp;
	OCISvcCtx *svchp[MAX_ORA_SESSIONS];
	OCIServer *srvhp[MAX_ORA_SESSIONS];
	OCISession *usrhp[MAX_ORA_SESSIONS];
	struct ORACLESTATEMENT *currentStmt[MAX_ORA_SESSIONS];
	OCILobLocator *blob;
};

extern struct BINDVARIABLE NO_BIND_VARIABLES[];
extern struct ORACLEDEFINE NO_ORACLE_DEFINES[];

struct ORACLESESSIONID
{
	ub4 sid;
	ub4 instance;
};

void PrepareStmtAndBind(struct ORACLEALLINONE *oraAllInOne, struct ORACLESTATEMENT *oracleStatement);
void PrepareStmtAndBind2(struct ORACLEALLINONE *oraAllInOne, struct ORACLESTATEMENT *oracleStatement, int index);

sword ExecuteStmt(struct ORACLEALLINONE *oraAllInOne);
sword ExecuteStmt2(struct ORACLEALLINONE *oraAllInOne, int index);

void ReleaseStmt(struct ORACLEALLINONE *oraAllInOne);
void ReleaseStmt2(struct ORACLEALLINONE *oraAllInOne, int index);

/* if exitCode == -1 then do not exit the program; just print an error message and proceed */
void ExitWithError(struct ORACLEALLINONE *oraAllInOne, int exitCode, enum ERROR_CLASS errorClass,
                   const char *message, ...);

void OracleLogon(struct ORACLEALLINONE *oraAllInOne,
                 const char* userName,
                 const char* password,
                 const char* connection,
                 ub4 adminMode,
                 const char* module,
                 int numberOfConnections);

void SetSessionAction(struct ORACLEALLINONE *oraAllInOne, const char* action);
void SetSessionAction2(struct ORACLEALLINONE *oraAllInOne, const char* action, int index);

void ExecuteSimpleSqls(struct ORACLEALLINONE *oraAllInOne, struct ORACLESIMPLESQL *oracleSimpleSqls);

void GetSessionId(struct ORACLEALLINONE *oraAllInOne, struct ORACLESESSIONID *oracleSessionId, int index);

#endif
