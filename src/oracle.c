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

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "oracle.h"

struct BINDVARIABLE NO_BIND_VARIABLES[] = { { 0 } };
struct ORACLEDEFINE NO_ORACLE_DEFINES[] = { { 0 } };

void PrepareStmtAndBind(struct ORACLEALLINONE *oraAllInOne, struct ORACLESTATEMENT *oracleStatement)
{
	PrepareStmtAndBind2(oraAllInOne, oracleStatement, 0);
}

sword ExecuteStmt(struct ORACLEALLINONE *oraAllInOne)
{
	return ExecuteStmt2(oraAllInOne, 0);
}

void ReleaseStmt(struct ORACLEALLINONE *oraAllInOne)
{
	ReleaseStmt2(oraAllInOne, 0);
}

void SetSessionAction(struct ORACLEALLINONE *oraAllInOne, const char* action)
{
	SetSessionAction2(oraAllInOne, action, 0);
}

void PrepareStmtAndBind2(struct ORACLEALLINONE *oraAllInOne, struct ORACLESTATEMENT *oracleStatement, int index)
{
	int i;

	oraAllInOne->currentStmt[index] = oracleStatement;

	if (OCIStmtPrepare2(oraAllInOne->svchp[index],
	                    &oracleStatement->stmthp,
	                    oraAllInOne->errhp,
	                    oracleStatement->sql,
	                    strlen(oracleStatement->sql),
	                    0, 0, OCI_NTV_SYNTAX, OCI_DEFAULT))
	{
		ExitWithError(oraAllInOne, 2, ERROR_OCI, "Failed to prepare %s\n", oracleStatement->sql);
	}

	for (i = 0; oracleStatement->oraDefines[i].value; i++)
	{
		if (OCIDefineByPos(oracleStatement->stmthp,
		                   &oracleStatement->oraDefines[i].ociDefine,
		                   oraAllInOne->errhp, i + 1,
		                   oracleStatement->oraDefines[i].value,
		                   oracleStatement->oraDefines[i].size,
		                   oracleStatement->oraDefines[i].dty,
		                   &oracleStatement->oraDefines[i].indp,
		                   0, 0, OCI_DEFAULT))
		{
			ExitWithError(oraAllInOne, 2, ERROR_OCI, "Failed to set up SQL query output field #%d\n", i + 1);
		}
	}

	for (i = 0; oracleStatement->bindVariables[i].value; i++)
	{
		if (OCIBindByName(oracleStatement->stmthp,
		                  &oracleStatement->bindVariables[i].ociBind,
		                  oraAllInOne->errhp,
		                  oracleStatement->bindVariables[i].name,
		                  strlen(oracleStatement->bindVariables[i].name),
		                  oracleStatement->bindVariables[i].value,
		                  oracleStatement->bindVariables[i].size,
		                  oracleStatement->bindVariables[i].dty,
		                  0, 0, 0, 0, 0, OCI_DEFAULT))
		{
			ExitWithError(oraAllInOne, 2, ERROR_OCI, "Failed to bind %s\n", oracleStatement->bindVariables[i].name);
		}
	}
}

sword ExecuteStmt2(struct ORACLEALLINONE *oraAllInOne, int index)
{
	return OCIStmtExecute(oraAllInOne->svchp[index], oraAllInOne->currentStmt[index]->stmthp,
	                      oraAllInOne->errhp, 1, 0, 0, 0, OCI_DEFAULT);
}

void ReleaseStmt2(struct ORACLEALLINONE *oraAllInOne, int index)
{
	int i;

	for (i = 0; oraAllInOne->currentStmt[index]->oraDefines[i].value; i++)
		if (oraAllInOne->currentStmt[index]->oraDefines[i].ociDefine)
		{
			OCIHandleFree(oraAllInOne->currentStmt[index]->oraDefines[i].ociDefine, OCI_HTYPE_DEFINE);
			oraAllInOne->currentStmt[index]->oraDefines[i].ociDefine = 0;
		}

	for (i = 0; oraAllInOne->currentStmt[index]->bindVariables[i].value; i++)
		if (oraAllInOne->currentStmt[index]->bindVariables[i].ociBind)
		{
			OCIHandleFree(oraAllInOne->currentStmt[index]->bindVariables[i].ociBind, OCI_HTYPE_BIND);
			oraAllInOne->currentStmt[index]->bindVariables[i].ociBind = 0;
		}

	if (oraAllInOne->currentStmt[index]->stmthp)
	{
		OCIStmtRelease(oraAllInOne->currentStmt[index]->stmthp, oraAllInOne->errhp, 0, 0, OCI_DEFAULT);
		oraAllInOne->currentStmt[index]->stmthp = 0;
	}

	oraAllInOne->currentStmt[index] = 0;
}

void ExitWithError(struct ORACLEALLINONE *oraAllInOne, int exitCode, enum ERROR_CLASS errorClass,
                   const char *message, ...)
{
	int i;
	sb4 errorCode;
	char errorMsg[MAX_FMT_SIZE];

	fflush(stdout);
	if (message)
	{
		va_list argptr;
		va_start(argptr, message);
		vsprintf(errorMsg, message, argptr);
		va_end(argptr);
		fprintf(stderr, "%s", errorMsg);
	}

	switch (errorClass)
	{
	case ERROR_NONE:
		break;
 	case ERROR_OCI:
		if (oraAllInOne->errhp)
		{
			OCIErrorGet(oraAllInOne->errhp, 1, 0, &errorCode, errorMsg, sizeof(errorMsg), OCI_HTYPE_ERROR);
			fprintf(stderr, "%s", errorMsg);
		}
		break;
	case ERROR_OS:
		fprintf(stderr, "%s", strerror(errno));
		break;
	}

	for (i = 0; i < MAX_ORA_SESSIONS; i++)
		if (oraAllInOne->currentStmt[i])
			ReleaseStmt2(oraAllInOne, i);

	if (oraAllInOne->blob)
	{
		OCIDescriptorFree(oraAllInOne->blob, OCI_DTYPE_LOB);
		oraAllInOne->blob = 0;
	}

	if (exitCode == -1)
		return;

	for (i = 0; i < MAX_ORA_SESSIONS; i++)
	{
		if (oraAllInOne->svchp[i])
		{
			OCISessionEnd(oraAllInOne->svchp[i], oraAllInOne->errhp, oraAllInOne->usrhp[i], OCI_DEFAULT);
			OCIHandleFree(oraAllInOne->svchp[i], OCI_HTYPE_SVCCTX);
			oraAllInOne->svchp[i] = 0;
		}
		if (oraAllInOne->usrhp[i])
		{
			OCIHandleFree(oraAllInOne->usrhp[i], OCI_HTYPE_SESSION);
			oraAllInOne->usrhp[i] = 0;
		}
		if (oraAllInOne->srvhp[i])
		{
			OCIServerDetach(oraAllInOne->srvhp[i], oraAllInOne->errhp, OCI_DEFAULT);
			OCIHandleFree(oraAllInOne->srvhp[i], OCI_HTYPE_SERVER);
			oraAllInOne->srvhp[i] = 0;
		}
	}

	if (oraAllInOne->errhp)
	{
		OCIHandleFree(oraAllInOne->errhp, OCI_HTYPE_ERROR);
		oraAllInOne->errhp = 0;
	}
	if (oraAllInOne->envhp)
	{
		OCITerminate(OCI_DEFAULT);
		oraAllInOne->envhp = 0;
	}

	exit(exitCode);
}

void OracleLogon(struct ORACLEALLINONE *oraAllInOne,
                 const char* userName,
                 const char* password,
                 const char* connection,
                 ub4 adminMode,
                 const char* module,
                 int numberOfConnections)
{
	int i;
	ub4 attrType;

	if (OCIEnvCreate(&oraAllInOne->envhp, (ub4)OCI_DEFAULT,
					(void  *)0, (void  * (*)(void  *, size_t))0,
					(void  * (*)(void  *, void  *, size_t))0,
					(void (*)(void  *, void  *))0,
					(size_t)0, (void  **)0))
	{
		ExitWithError(oraAllInOne, 2, ERROR_NONE, "Failed to create OCI environment\n");
		/* 2 - Error in OCI object initialization */
	}

	if (OCIHandleAlloc( (dvoid *) oraAllInOne->envhp, (dvoid **) &oraAllInOne->errhp,
	                    (ub4) OCI_HTYPE_ERROR, 0, (dvoid **) 0))
	{
		ExitWithError(oraAllInOne, 2, ERROR_NONE, "Failed to initialize OCIError\n");
	}

	for (i = 0; i < numberOfConnections; i++)
	{
		if (OCIHandleAlloc(oraAllInOne->envhp, (void*) &oraAllInOne->srvhp[i], OCI_HTYPE_SERVER, 0, 0))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to attach to a server\n");
		}

		if (OCIServerAttach(oraAllInOne->srvhp[i], oraAllInOne->errhp,
		                    (text*)connection, (ub4)strlen(connection), OCI_DEFAULT))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to attach to server\n");
		}

		if (OCIHandleAlloc(oraAllInOne->envhp, (void*) &oraAllInOne->svchp[i], OCI_HTYPE_SVCCTX, 0, 0))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
		}

		if (OCIAttrSet(oraAllInOne->svchp[i], OCI_HTYPE_SVCCTX,
		               oraAllInOne->srvhp[i], 0, OCI_ATTR_SERVER, oraAllInOne->errhp))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
		}

		if (OCIHandleAlloc(oraAllInOne->envhp, (void*) &oraAllInOne->usrhp[i], OCI_HTYPE_SESSION, 0, 0))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
		}

		if (*userName)
		{
			if (OCIAttrSet(oraAllInOne->usrhp[i], OCI_HTYPE_SESSION,
			               (text*)userName, (ub4)strlen(userName),
			               OCI_ATTR_USERNAME, oraAllInOne->errhp) ||
			    OCIAttrSet(oraAllInOne->usrhp[i], OCI_HTYPE_SESSION,
			               (text*)password, (ub4)strlen(password),
			               OCI_ATTR_PASSWORD, oraAllInOne->errhp))
			{
				ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
			}
			attrType = OCI_CRED_RDBMS;
		}
		else
			attrType = OCI_CRED_EXT;

		switch (OCISessionBegin(oraAllInOne->svchp[i], oraAllInOne->errhp,
		                        oraAllInOne->usrhp[i], attrType, adminMode))
		{
		case OCI_SUCCESS_WITH_INFO:
			ExitWithError(oraAllInOne, -1, ERROR_OCI, 0);
		case OCI_SUCCESS:
			break;
		default:
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
			/* 3 - Failed to login to a database */
		}

		if (OCIAttrSet(oraAllInOne->svchp[i], OCI_HTYPE_SVCCTX,
		               oraAllInOne->usrhp[i], 0, OCI_ATTR_SESSION, oraAllInOne->errhp))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
		}

		if (OCIAttrSet(oraAllInOne->usrhp[i], OCI_HTYPE_SESSION,
		               (void*)module, strlen(module), OCI_ATTR_MODULE, oraAllInOne->errhp))
		{
			ExitWithError(oraAllInOne, -1, ERROR_OCI, "Could not set MODULE in V$SESSION\n");
		}
	}
}

void SetSessionAction2(struct ORACLEALLINONE *oraAllInOne, const char* action, int index)
{
	if (OCIAttrSet(oraAllInOne->usrhp[index], OCI_HTYPE_SESSION,
	               (void*)action, action ? strlen(action) : 0, OCI_ATTR_ACTION, oraAllInOne->errhp))
	{
		ExitWithError(oraAllInOne, -1, ERROR_OCI, "Could not set ACTION in V$SESSION\n");
	}
}

void ExecuteSimpleSqls(struct ORACLEALLINONE *oraAllInOne, struct ORACLESIMPLESQL *oracleSimpleSqls)
{
	while (oracleSimpleSqls->sql)
	{
		sb4 errorCode;
		char errorMsg[MAX_FMT_SIZE];
		struct ORACLESTATEMENT stmt;

		stmt.sql = oracleSimpleSqls->sql;
		stmt.stmthp = 0;
		stmt.bindVariables = NO_BIND_VARIABLES;
		stmt.oraDefines = NO_ORACLE_DEFINES;
		PrepareStmtAndBind(oraAllInOne, &stmt);
		if (ExecuteStmt(oraAllInOne))
		{
			if (!oracleSimpleSqls->errCodeToIgnore ||
			    (OCIErrorGet(oraAllInOne->errhp, 1, 0, &errorCode, errorMsg, sizeof(errorMsg), OCI_HTYPE_ERROR),
			     errorCode != oracleSimpleSqls->errCodeToIgnore))
				ExitWithError(oraAllInOne, 4, ERROR_OCI, 0);
		}
		ReleaseStmt(oraAllInOne);
		oracleSimpleSqls++;
	}
}

void GetSessionId(struct ORACLEALLINONE *oraAllInOne, struct ORACLESESSIONID *oracleSessionId, int index)
{
	struct ORACLEDEFINE oraDefinesGetSid[] =
	{
		{ 0, SQLT_INT, &oracleSessionId->sid,      sizeof(oracleSessionId->sid),      0 },
		{ 0, SQLT_INT, &oracleSessionId->instance, sizeof(oracleSessionId->instance), 0 },
		{ 0 }
        };

	struct ORACLESTATEMENT oraStmtGetSid = { "\
select sys_context('USERENV', 'SID'),\n\
       sys_context('USERENV', 'INSTANCE')\n\
  from dual",
		0, NO_BIND_VARIABLES, oraDefinesGetSid };

	PrepareStmtAndBind2(oraAllInOne, &oraStmtGetSid, index);
	if (ExecuteStmt2(oraAllInOne, index))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Cannot get SID of the current session\n");
	ReleaseStmt2(oraAllInOne, index);
}
