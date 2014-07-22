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

void PrepareStmtAndBind(struct ORACLEALLINONE *oraAllInOne, struct ORACLESTATEMENT *oracleStatement)
{
	int i;

	oraAllInOne->currentStmt = oracleStatement;

	if (OCIStmtPrepare2(oraAllInOne->svchp,
	                    &oracleStatement->stmthp,
	                    oraAllInOne->errhp,
	                    oracleStatement->sql,
	                    strlen(oracleStatement->sql),
	                    0, 0, OCI_NTV_SYNTAX, OCI_DEFAULT))
	{
		ExitWithError(oraAllInOne, 2, ERROR_OCI, "Failed to prepare %s\n", oracleStatement->sql);
	}

	for (i = 0; i < oracleStatement->oraDefineCount; i++)
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

	for (i = 0; i < oracleStatement->bindVarsCount; i++)
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

sword ExecuteStmt(struct ORACLEALLINONE *oraAllInOne)
{
	return OCIStmtExecute(oraAllInOne->svchp, oraAllInOne->currentStmt->stmthp,
	                      oraAllInOne->errhp, 1, 0, 0, 0, OCI_DEFAULT);
}

void ReleaseStmt(struct ORACLEALLINONE *oraAllInOne)
{
	int i;

	for (i = oraAllInOne->currentStmt->oraDefineCount - 1; i >= 0; i--)
		if (oraAllInOne->currentStmt->oraDefines[i].ociDefine)
		{
			OCIHandleFree(oraAllInOne->currentStmt->oraDefines[i].ociDefine, OCI_HTYPE_DEFINE);
			oraAllInOne->currentStmt->oraDefines[i].ociDefine = 0;
		}

	for (i = oraAllInOne->currentStmt->bindVarsCount - 1; i >= 0; i--)
		if (oraAllInOne->currentStmt->bindVariables[i].ociBind)
		{
			OCIHandleFree(oraAllInOne->currentStmt->bindVariables[i].ociBind, OCI_HTYPE_BIND);
			oraAllInOne->currentStmt->bindVariables[i].ociBind = 0;
		}

	if (oraAllInOne->currentStmt->stmthp)
	{
		OCIStmtRelease(oraAllInOne->currentStmt->stmthp, oraAllInOne->errhp, 0, 0, OCI_DEFAULT);
		oraAllInOne->currentStmt->stmthp = 0;
	}

	oraAllInOne->currentStmt = 0;
}

void ExitWithError(struct ORACLEALLINONE *oraAllInOne, int exitCode, enum ERROR_CLASS errorClass,
                   const char *message, ...)
{
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
	case ERROR_USAGE:
		fprintf(stderr, "Usage:\n\
   ocp username/password@connect_identifier DIRECTORY:remotefile localfile\n\
   ocp username/password@connect_identifier localfile DIRECTORY:remotefile\n\
   ocp username/password@connect_identifier -lsdir\n\
   ocp username/password@connect_identifier -ls DIRECTORY\n");
		break;
	}

	if (oraAllInOne->currentStmt)
		ReleaseStmt(oraAllInOne);

	if (oraAllInOne->srvhp)
	{
		if (oraAllInOne->svchp)
		{
			OCISessionEnd(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->usrhp, OCI_DEFAULT);
			OCIHandleFree(oraAllInOne->svchp, OCI_HTYPE_SVCCTX);
			oraAllInOne->svchp = 0;
		}
		if (oraAllInOne->usrhp)
		{
			OCIHandleFree(oraAllInOne->usrhp, OCI_HTYPE_SESSION);
			oraAllInOne->usrhp = 0;
		}
		OCIServerDetach(oraAllInOne->srvhp, oraAllInOne->errhp, OCI_DEFAULT);
		OCIHandleFree(oraAllInOne->srvhp, OCI_HTYPE_SERVER);
		oraAllInOne->srvhp = 0;
	}
	else
	{
		if (oraAllInOne->svchp)
		{
			OCILogoff(oraAllInOne->svchp, oraAllInOne->errhp);
			oraAllInOne->svchp = 0;
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
                 const char* connection)
{
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

	if (*userName)
	{
		if (OCILogon2(oraAllInOne->envhp, oraAllInOne->errhp, &oraAllInOne->svchp,
		              (text*)userName, (ub4)strlen(userName),
		              (text*)password, (ub4)strlen(password),
		              (text*)connection, (ub4)strlen(connection), OCI_DEFAULT))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
			/* 3 - Failed to login to a database */
		}
	}
	else
	{
		if (OCIHandleAlloc(oraAllInOne->envhp, (void*) &oraAllInOne->srvhp, OCI_HTYPE_SERVER, 0, 0))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to attach to a server\n");
		}

		if (OCIServerAttach(oraAllInOne->srvhp, oraAllInOne->errhp,
		                    (text*)connection, (ub4)strlen(connection), OCI_DEFAULT))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to attach to server\n");
		}

		if (OCIHandleAlloc(oraAllInOne->envhp, (void*) &oraAllInOne->svchp, OCI_HTYPE_SVCCTX, 0, 0))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
		}

		if (OCIAttrSet(oraAllInOne->svchp, OCI_HTYPE_SVCCTX,
		               oraAllInOne->srvhp, 0, OCI_ATTR_SERVER, oraAllInOne->errhp))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
		}

		if (OCIHandleAlloc(oraAllInOne->envhp, (void*) &oraAllInOne->usrhp, OCI_HTYPE_SESSION, 0, 0))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
		}

		if (OCISessionBegin(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->usrhp, OCI_CRED_EXT, OCI_DEFAULT))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
		}

		if (OCIAttrSet(oraAllInOne->svchp, OCI_HTYPE_SVCCTX,
		               oraAllInOne->usrhp, 0, OCI_ATTR_SESSION, oraAllInOne->errhp))
		{
			ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
		}
	}
}
