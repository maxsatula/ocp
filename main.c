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

******************************************************************************

This is an Oracle Copy command line tool for downloading and uploading files
from/to Oracle Database directories (e.g. DATA_PUMP_DIR) using Oracle SQL Net
connection only.
Hence no physical or file system access required to a database server. 

Tested and compiled with:
	cc -q64 -I$ORACLE_HOME/rdbms/public -L$ORACLE_HOME/lib -lclntsh -oocp main.c

*****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <oci.h>

#define MAX_FMT_SIZE 4096
#define ORA_RAW_BUFFER_SIZE 0x2000

enum PROGRAM_ACTION { ACTION_TRANSFER, ACTION_LS };
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
	struct ORACLESTATEMENT *currentStmt;
};

void ExitWithError(struct ORACLEALLINONE *oraAllInOne, int exitCode, enum ERROR_CLASS errorClass,
                   const char *message, ...);

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
	if (oraAllInOne->svchp)
	{
		OCILogoff(oraAllInOne->svchp, oraAllInOne->errhp);
		oraAllInOne->svchp = 0;
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

	if (OCILogon2(oraAllInOne->envhp, oraAllInOne->errhp, &oraAllInOne->svchp,
	              (text*)userName, (ub4)strlen(userName),
	              (text*)password, (ub4)strlen(password),
	              (text*)connection, (ub4)strlen(connection), OCI_DEFAULT))
	{
		ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed to login to a database\n");
		/* 3 - Failed to login to a database */
	}
}

int main(int argc, char *argv[])
{
	char connectionString[MAX_FMT_SIZE];
	char *pwdptr, *dbconptr;
	enum PROGRAM_ACTION programAction;
	int readingDirection;
	char* fileNamePtr;
	char *localArg, *remoteArg;
	char vDirectory[MAX_FMT_SIZE];
	char vDirectoryPath[MAX_FMT_SIZE];
	char vGrantable1[MAX_FMT_SIZE];
	char vGrantable2[MAX_FMT_SIZE];
	char vLocalFile[MAX_FMT_SIZE];
	char vRemoteFile[MAX_FMT_SIZE];
	unsigned char vBuffer[ORA_RAW_BUFFER_SIZE];
	int vSize;
	FILE *fp;
	int i;
	char vOpenMode[3];
	ub4 vActualSize;
	ub4 vFHandle1;
	ub4 vFHandle2;
	sword ociResult;

	const char* list_directories = "\
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
          AND pw.privilege = 'WRITE'";

	struct ORACLEDEFINE oraDefinesLsDir[] =
	{
		{ 0, SQLT_STR, vDirectory,     sizeof(vDirectory)-1,     0 },
		{ 0, SQLT_STR, vDirectoryPath, sizeof(vDirectoryPath)-1, 0 },
		{ 0, SQLT_STR, vGrantable1,    sizeof(vGrantable1)-1,    0 },
		{ 0, SQLT_STR, vGrantable2,    sizeof(vGrantable2)-1,    0 }
	};

	/*const char* list_dir_files = "";*/


	/* 
	It is not originally possible to get a UTL_FILE.FILE_TYPE handle outside
	of PL/SQL and store in OCI, and there is no corresponding OCI data type
	available. Hence we have to use a dirty hack, extracting two
	BINARY_INTEGER members of UTL_FILE.FILE_TYPE record to separate bind
	variables.
	*/

	const char* utl_file_fopen = "\
declare \
  handle utl_file.file_type; \
begin \
  handle := utl_file.fopen(:directory, :filename, :openmode); \
  :fhandle1 := handle.id; \
  :fhandle2 := handle.datatype; \
end;";

	struct BINDVARIABLE bindVariablesOpen[] =
	{
		{ 0, SQLT_STR, ":directory", vDirectory,  sizeof(vDirectory)  },
		{ 0, SQLT_STR, ":filename",  vRemoteFile, sizeof(vRemoteFile) },
		{ 0, SQLT_STR, ":openmode",  vOpenMode,   sizeof(vOpenMode)   },
		{ 0, SQLT_INT, ":fhandle1",  &vFHandle1,  sizeof(vFHandle1)   },
		{ 0, SQLT_INT, ":fhandle2",  &vFHandle2,  sizeof(vFHandle2)   }
	};

	const char* utl_file_fclose = "\
declare \
  handle utl_file.file_type; \
begin \
  handle.id := :fhandle1; \
  handle.datatype := :fhandle2; \
  utl_file.fclose(handle); \
end;";

	struct BINDVARIABLE bindVariablesClose[] =
	{
		{ 0, SQLT_INT, ":fhandle1", &vFHandle1, sizeof(vFHandle1) },
		{ 0, SQLT_INT, ":fhandle2", &vFHandle2, sizeof(vFHandle2) }
	};

	const char* utl_file_read = "\
declare \
  handle utl_file.file_type; \
begin \
  handle.id := :fhandle1; \
  handle.datatype := :fhandle2; \
  utl_file.get_raw(handle, :buffer, :size); \
  :size := utl_raw.length(:buffer); \
exception \
  when no_data_found then \
    :size := 0; \
end;";

	struct BINDVARIABLE bindVariablesRead[] =
	{
		{ 0, SQLT_BIN, ":buffer",   vBuffer,    sizeof(vBuffer)   },
		{ 0, SQLT_INT, ":size",     &vSize,     sizeof(vSize)     },
		{ 0, SQLT_INT, ":fhandle1", &vFHandle1, sizeof(vFHandle1) },
		{ 0, SQLT_INT, ":fhandle2", &vFHandle2, sizeof(vFHandle2) }
	};

	const char* utl_file_write = "\
declare \
  handle utl_file.file_type; \
begin \
  handle.id := :fhandle1; \
  handle.datatype := :fhandle2; \
  utl_file.put_raw(handle, :buffer); \
end;";

	struct BINDVARIABLE bindVariablesWrite[] =
	{
		{ 0, SQLT_BIN, ":buffer",   vBuffer,    sizeof(vBuffer)   },
		{ 0, SQLT_INT, ":fhandle1", &vFHandle1, sizeof(vFHandle1) },
		{ 0, SQLT_INT, ":fhandle2", &vFHandle2, sizeof(vFHandle2) }
	};

	OCIBind *ociBind;

	struct ORACLEALLINONE oraAllInOne = { 0, 0, 0, 0 };

	struct ORACLESTATEMENT oraStmtLsDir = { list_directories, 0, 0, 0, oraDefinesLsDir, sizeof(oraDefinesLsDir)/sizeof(struct ORACLEDEFINE) };
	struct ORACLESTATEMENT oraStmtOpen = { utl_file_fopen, 0, bindVariablesOpen, sizeof(bindVariablesOpen)/sizeof(struct BINDVARIABLE), 0, 0 };
	struct ORACLESTATEMENT oraStmtClose = { utl_file_fclose, 0, bindVariablesClose, sizeof(bindVariablesClose)/sizeof(struct BINDVARIABLE), 0, 0 };
	struct ORACLESTATEMENT oraStmtRead = { utl_file_read, 0, bindVariablesRead, sizeof(bindVariablesRead)/sizeof(struct BINDVARIABLE), 0, 0 };
	struct ORACLESTATEMENT oraStmtWrite = { utl_file_write, 0, bindVariablesWrite, sizeof(bindVariablesWrite)/sizeof(struct BINDVARIABLE), 0, 0 };

	if (argc != 4 && (argc != 3 || strcmp(argv[2], "-lsdir")))
		ExitWithError(&oraAllInOne, 1, ERROR_USAGE, "Invalid number of arguments\n");
		/* 1 - Error in command line arguments */
	strcpy(connectionString, argv[1]); /* TODO: replace with strncpy to avoid buffer overflow */
	pwdptr = strchr(connectionString, '/');
	if (pwdptr)
		*pwdptr++ = '\0';
	dbconptr = strchr(pwdptr, '@');
	if (dbconptr)
		*dbconptr++ = '\0';
	else
		ExitWithError(&oraAllInOne, 1, ERROR_USAGE, "Invalid connection string format\n");

#ifdef DEBUG
	printf("Database connection: %s@%s\n", connectionString, dbconptr);
#endif

	if (argc == 3)
	{
		programAction = ACTION_LS;
		readingDirection = 2;
	}
	else if (!strcmp(argv[2], "-ls"))
	{
		programAction = ACTION_LS;
		readingDirection = 3;
	}
	else
	{
		programAction = ACTION_TRANSFER;
		fileNamePtr = strchr(argv[2], ':');
		if (fileNamePtr)
		{
			readingDirection = 1;
			localArg = argv[3];
			remoteArg = argv[2];
		}
		else
		{
			readingDirection = 0;
			fileNamePtr = strchr(argv[3], ':');
			if (!fileNamePtr)
				ExitWithError(&oraAllInOne, 1, ERROR_USAGE, "Missing a colon ':' in one of arguments to specify a remote (Oracle) site\n");
			localArg = argv[2];
			remoteArg = argv[3];
		}

		strncpy(vDirectory, remoteArg, fileNamePtr - remoteArg);
		vDirectory[fileNamePtr - remoteArg] = '\0';

		strcpy(vLocalFile, localArg); /* TODO: replace with strncpy to avoid buffer overflow */
		strcpy(vRemoteFile, fileNamePtr + 1); /* TODO: replace with strncpy to avoid buffer overflow */
#ifdef DEBUG
		printf("Copying %s Oracle. Local file %s, remote file %s, directory %s\n",
			   readingDirection ? "FROM" : "TO", vLocalFile, vRemoteFile, vDirectory);
#endif
	}

	OracleLogon(&oraAllInOne, connectionString, pwdptr, dbconptr);

	switch (programAction)
	{
	case ACTION_LS:
		if (readingDirection == 2)
		{
			PrepareStmtAndBind(&oraAllInOne, &oraStmtLsDir);

			if (ExecuteStmt(&oraAllInOne))
				ExitWithError(&oraAllInOne, 4, ERROR_OCI, "Failed to list oracle directories\n");

			do
			{
				printf("%c%c %-30s (%s)\n",
				       oraStmtLsDir.oraDefines[2].indp == -1 ? '-' :
				       *(char*)oraStmtLsDir.oraDefines[2].value == 'Y' ? 'R' : 'r',
				       oraStmtLsDir.oraDefines[3].indp == -1 ? '-' :
				       *(char*)oraStmtLsDir.oraDefines[3].value == 'Y' ? 'W' : 'w',
				       oraStmtLsDir.oraDefines[0].value,
				       oraStmtLsDir.oraDefines[1].value);

				ociResult = OCIStmtFetch2(oraStmtLsDir.stmthp, oraAllInOne.errhp, 1,
				                          OCI_FETCH_NEXT, 1, OCI_DEFAULT);
			}
			while (ociResult == OCI_SUCCESS);

			if (ociResult != OCI_NO_DATA)
				ExitWithError(&oraAllInOne, 4, ERROR_OCI, "Failed to list oracle directories\n");

			ReleaseStmt(&oraAllInOne);
		}
		else
		{
			ExitWithError(&oraAllInOne, 5, ERROR_NONE, "Not implemented yet: -ls DIRECTORY\n");
		}
		break;

	case ACTION_TRANSFER:
		strcpy(vOpenMode, readingDirection ? "rb" : "wb");

		PrepareStmtAndBind(&oraAllInOne, &oraStmtOpen);

		if (ExecuteStmt(&oraAllInOne))
			ExitWithError(&oraAllInOne, 4, ERROR_OCI, "Failed to open an Oracle remote file for %s\n",
		                  readingDirection ? "reading" : "writing");

		ReleaseStmt(&oraAllInOne);

		PrepareStmtAndBind(&oraAllInOne, readingDirection ? &oraStmtRead : &oraStmtWrite);

		if ((fp = fopen(vLocalFile, readingDirection ? "wb" : "rb")) == NULL)
		{
			ExitWithError(&oraAllInOne, 4, ERROR_OS, "Error opening a local %s file for %s\n",
			              readingDirection ? "destination" : "source",
			              readingDirection ? "writing"     : "reading");
			/* 4 - Local filesystem related errors */
		}

		if (readingDirection)
		{
			do
			{
				vSize = sizeof(vBuffer);
				if (ExecuteStmt(&oraAllInOne))
				{
					fclose(fp);
					/* TODO: remove partially downloaded file */
					ExitWithError(&oraAllInOne, 3, ERROR_OCI, "Failed execution of %s\n",
					              oraAllInOne.currentStmt->sql);
				}
				else
				{
					fwrite(vBuffer, sizeof(unsigned char), vSize, fp);
					if (ferror(fp))
					{
						fclose(fp);
						/* TODO: remove partially downloaded file */
						ExitWithError(&oraAllInOne, 4, ERROR_OS, "Error writing to a local file\n");
					}
				}
			}
			while (vSize);
		}
		else
		{
			while (vActualSize = fread(vBuffer, sizeof(unsigned char), sizeof(vBuffer), fp))
			{
				if (ferror(fp))
				{
					fclose(fp);
					ExitWithError(&oraAllInOne, 4, ERROR_OS, "Error reading from a local file\n");
				}

				if (OCIBindByName(oraAllInOne.currentStmt->stmthp, &ociBind, oraAllInOne.errhp,
				                  ":buffer", strlen(":buffer"),
								  vBuffer, vActualSize, SQLT_BIN, 0, 0, 0, 0, 0, OCI_DEFAULT))
				{
					fclose(fp);
					ExitWithError(&oraAllInOne, 4, ERROR_OCI, "Failed to bind :buffer\n");
				}

				if (ExecuteStmt(&oraAllInOne))
				{
					fclose(fp);
					OCIHandleFree(ociBind, OCI_HTYPE_BIND);
					ExitWithError(&oraAllInOne, 4, ERROR_OCI, "Failed execution of %s\n",
					              oraAllInOne.currentStmt->sql);
				}
				OCIHandleFree(ociBind, OCI_HTYPE_BIND);
			}
		}

		fclose(fp);
		ReleaseStmt(&oraAllInOne);

		PrepareStmtAndBind(&oraAllInOne, &oraStmtClose);
		if (ExecuteStmt(&oraAllInOne))
		{
			ExitWithError(&oraAllInOne, 4, ERROR_OCI, "Error closing an Oracle remote file\n");
		}
		ReleaseStmt(&oraAllInOne);
		break;
	}

	ExitWithError(&oraAllInOne, 0, ERROR_NONE, 0);
}
