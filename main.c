/*******************************************
This is an Oracle Copy utility which allows
to download and upload files from/to Oracle
database directories (e.g. DATA_PUMP_DIR)
via Oracle database SQL Net connection.
Hence no physical or fileystem access required
to a database server.

Things TODO:
	Asking for password interactively, if not specified in the command line
	Display transfer progress
	Listing Oracle DIRECTORY contents - not possible with UTL_FILE
	List of accessible directories with privileges (USER_DIRECTORIES)
	On-the-fly compression
	Verify file contents once transferred (SHA1 or MD5?)
	Check file existence before overwrite (either local or remote)
	If an error occurs during transfer, remove partial destination file, either on the Oracle side or local

Tested and compiled with:
	cc -q64 -I$ORACLE_HOME/rdbms/public -L$ORACLE_HOME/lib -lclntsh -oocp main.c

Author: Max Satula, 2014
*******************************************/
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <oci.h>

#define MAX_FMT_SIZE 4096
#define ORA_RAW_BUFFER_SIZE 0x2000

enum CLEANUP_LEVEL { CLEANUP_NONE, CLEANUP_OCIENV, CLEANUP_OCIERROR, CLEANUP_LOGOFF, CLEANUP_STMT };

enum ERROR_CLASS { ERROR_NONE, ERROR_OCI, ERROR_OS, ERROR_USAGE };

struct BINDVARIABLE
{
	OCIBind *ociBind;
	ub2 dty;
	const char* name;
	void* value;
	sb4 size;
};

struct ORACLESTATEMENT
{
	const char* sql;
	OCIStmt *stmthp;
	struct BINDVARIABLE *bindVariables;
	int bindVarsCount;
};

void ExitWithError(enum CLEANUP_LEVEL cleanupLevel, int exitCode, enum ERROR_CLASS errorClass,
                   OCIError *errhp, OCISvcCtx *svchp, struct ORACLESTATEMENT *oracleStatement,
                   const char *message, ...);

void PrepareStmtAndBind(OCISvcCtx *svchp, OCIError *errhp, struct ORACLESTATEMENT *oracleStatement)
{
	int i;

	if (OCIStmtPrepare2(svchp, &oracleStatement->stmthp, errhp, oracleStatement->sql, strlen(oracleStatement->sql), 0, 0, OCI_NTV_SYNTAX, OCI_DEFAULT))
	{
		ExitWithError(CLEANUP_LOGOFF, 2, ERROR_OCI, errhp, svchp, 0, "Failed to prepare %s\n", oracleStatement->sql);
	}

	for (i = 0; i < oracleStatement->bindVarsCount; i++)
	{
		if (OCIBindByName(oracleStatement->stmthp, &oracleStatement->bindVariables[i].ociBind, errhp,
		                  oracleStatement->bindVariables[i].name, strlen(oracleStatement->bindVariables[i].name),
		                  oracleStatement->bindVariables[i].value, oracleStatement->bindVariables[i].size,
		                  oracleStatement->bindVariables[i].dty, 0, 0, 0, 0, 0, OCI_DEFAULT))
		{
			oracleStatement->bindVarsCount = i;
			ExitWithError(CLEANUP_STMT, 2, ERROR_OCI, errhp, svchp, oracleStatement, "Failed to bind %s\n", oracleStatement->bindVariables[i].name);
		}
	}
}

void ReleaseStmt(OCIError *errhp, struct ORACLESTATEMENT *oracleStatement)
{
	int i;

	for (i = 0; i < oracleStatement->bindVarsCount; i++)
		OCIHandleFree(oracleStatement->bindVariables[i].ociBind, OCI_HTYPE_BIND);
	OCIStmtRelease(oracleStatement->stmthp, errhp, 0, 0, OCI_DEFAULT);
}

void ExitWithError(enum CLEANUP_LEVEL cleanupLevel, int exitCode, enum ERROR_CLASS errorClass,
                   OCIError *errhp, OCISvcCtx *svchp, struct ORACLESTATEMENT *oracleStatement,
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
		if (cleanupLevel >= CLEANUP_OCIERROR)
		{
			OCIErrorGet(errhp, 1, 0, &errorCode, errorMsg, sizeof(errorMsg), OCI_HTYPE_ERROR);
			fprintf(stderr, "%s", errorMsg);
		}
		break;
	case ERROR_OS:
		fprintf(stderr, "%s", strerror(errno));
		break;
	case ERROR_USAGE:
		fprintf(stderr, "Usage:\n\
   ocp username/password@connect_identifier DIRECTORY:remotefile localfile\n\
   ocp username/password@connect_identifier localfile DIRECTORY:remotefile\n");
		break;
	}

	switch (cleanupLevel)
	{
	case CLEANUP_STMT:
		ReleaseStmt(errhp, oracleStatement);
	case CLEANUP_LOGOFF:
		OCILogoff(svchp, errhp);
	case CLEANUP_OCIERROR:
		OCIHandleFree(errhp, OCI_HTYPE_ERROR);
	case CLEANUP_OCIENV:
		OCITerminate(OCI_DEFAULT);
	case CLEANUP_NONE:
		;
	}

	exit(exitCode);
}

int main(int argc, char *argv[])
{
	char connectionString[MAX_FMT_SIZE];
	char *pwdptr, *dbconptr;
	int readingDirection;
	char* fileNamePtr;
	char *localArg, *remoteArg;
	char vDirectory[MAX_FMT_SIZE];
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
	struct ORACLESTATEMENT *workingOraStmt;

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

	OCIEnv *envhp;
	OCIError *errhp;
	OCISvcCtx *svchp;
	OCIBind *ociBind;

	struct ORACLESTATEMENT oraStmtOpen = { utl_file_fopen, 0, bindVariablesOpen, sizeof(bindVariablesOpen)/sizeof(struct BINDVARIABLE) };
	struct ORACLESTATEMENT oraStmtClose = { utl_file_fclose, 0, bindVariablesClose, sizeof(bindVariablesClose)/sizeof(struct BINDVARIABLE) };
	struct ORACLESTATEMENT oraStmtRead = { utl_file_read, 0, bindVariablesRead, sizeof(bindVariablesRead)/sizeof(struct BINDVARIABLE) };
	struct ORACLESTATEMENT oraStmtWrite = { utl_file_write, 0, bindVariablesWrite, sizeof(bindVariablesWrite)/sizeof(struct BINDVARIABLE) };

	if (argc != 4)
		ExitWithError(CLEANUP_NONE, 1, ERROR_USAGE, errhp, svchp, 0, "Invalid number of arguments\n");
		/* 1 - Error in command line arguments */
	strcpy(connectionString, argv[1]); /* TODO: replace with strncpy to avoid buffer overflow */
	pwdptr = strchr(connectionString, '/');
	if (pwdptr)
		*pwdptr++ = '\0';
	dbconptr = strchr(pwdptr, '@');
	if (dbconptr)
		*dbconptr++ = '\0';
	else
		ExitWithError(CLEANUP_NONE, 1, ERROR_USAGE, errhp, svchp, 0, "Invalid connection string format\n");

#ifdef DEBUG
	printf("Database connection: %s@%s\n", connectionString, dbconptr);
#endif

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
			ExitWithError(CLEANUP_NONE, 1, ERROR_USAGE, errhp, svchp, 0, "Missin a colon ':' in one of arguments to specify a remote (Oracle) site\n");
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

	if (OCIEnvCreate(&envhp, (ub4)OCI_DEFAULT,
					(void  *)0, (void  * (*)(void  *, size_t))0,
					(void  * (*)(void  *, void  *, size_t))0,
					(void (*)(void  *, void  *))0,
					(size_t)0, (void  **)0))
	{
		ExitWithError(CLEANUP_NONE, 2, ERROR_NONE, errhp, svchp, 0, "Failed to create OCI environment\n");
		/* 2 - Error in OCI object initialization */
	}

	if (OCIHandleAlloc( (dvoid *) envhp, (dvoid **) &errhp, (ub4) OCI_HTYPE_ERROR, 0, (dvoid **) 0))
	{
		ExitWithError(CLEANUP_OCIENV, 2, ERROR_NONE, errhp, svchp, 0, "Failed to initialize OCIError\n");
	}

	if (OCILogon2(envhp, errhp, &svchp, (text*)connectionString, (ub4)strlen(connectionString),
	              (text*)pwdptr, (ub4)strlen(pwdptr), (text*)dbconptr, (ub4)strlen(dbconptr), OCI_DEFAULT))
	{
		ExitWithError(CLEANUP_OCIERROR, 3, ERROR_OCI, errhp, svchp, 0, "Failed to login to a database\n");
		/* 3 - Failed to login to a database */
	}

	strcpy(vOpenMode, readingDirection ? "rb" : "wb");

	PrepareStmtAndBind(svchp, errhp, &oraStmtOpen);
	if (OCIStmtExecute(svchp, oraStmtOpen.stmthp, errhp, 1, 0, 0, 0, OCI_DEFAULT))
	{
		ExitWithError(CLEANUP_STMT, 4, ERROR_OCI, errhp, svchp, &oraStmtOpen, "Failed to open an Oracle remote file for %s\n", readingDirection ? "reading" : "writing");
	}

	ReleaseStmt(errhp, &oraStmtOpen);

	workingOraStmt = readingDirection ? &oraStmtRead : &oraStmtWrite;
	PrepareStmtAndBind(svchp, errhp, workingOraStmt);

	if ((fp = fopen(vLocalFile, readingDirection ? "wb" : "rb")) == NULL)
	{
		ExitWithError(CLEANUP_STMT, 4, ERROR_OS, errhp, svchp, workingOraStmt, "Error opening a local %s file for %s\n", readingDirection ? "destination" : "source", readingDirection ? "writing" : "reading");
		/* 4 - Local filesystem related errors */
	}

	if (readingDirection)
	{
		do
		{
			vSize = sizeof(vBuffer);
			if (OCIStmtExecute(svchp, workingOraStmt->stmthp, errhp, 1, 0, 0, 0, OCI_DEFAULT))
			{
				fclose(fp);
				/* TODO: remove partially downloaded file */
				ExitWithError(CLEANUP_STMT, 3, ERROR_OCI, errhp, svchp, workingOraStmt, "Failed execution of %s\n", workingOraStmt->sql);
			}
			else
			{
				fwrite(vBuffer, sizeof(unsigned char), vSize, fp);
				if (ferror(fp))
				{
					fclose(fp);
					/* TODO: remove partially downloaded file */
					ExitWithError(CLEANUP_STMT, 4, ERROR_OS, errhp, svchp, workingOraStmt, "Error writing to a local file\n");
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
				ExitWithError(CLEANUP_STMT, 4, ERROR_OS, errhp, svchp, workingOraStmt, "Error reading from a local file\n");
			}

			if (OCIBindByName(workingOraStmt->stmthp, &ociBind, errhp, ":buffer", strlen(":buffer"),
			                  vBuffer, vActualSize, SQLT_BIN, 0, 0, 0, 0, 0, OCI_DEFAULT))
			{
				fclose(fp);
				ExitWithError(CLEANUP_STMT, 4, ERROR_OCI, errhp, svchp, workingOraStmt, "Failed to bind :buffer\n");
			}

			if (OCIStmtExecute(svchp, workingOraStmt->stmthp, errhp, 1, 0, 0, 0, OCI_DEFAULT))
			{
				fclose(fp);
				OCIHandleFree(ociBind, OCI_HTYPE_BIND);
				ExitWithError(CLEANUP_STMT, 4, ERROR_OCI, errhp, svchp, workingOraStmt, "Failed execution of %s\n", workingOraStmt->sql);
			}
			OCIHandleFree(ociBind, OCI_HTYPE_BIND);
		}
	}

	fclose(fp);
	ReleaseStmt(errhp, workingOraStmt);

	PrepareStmtAndBind(svchp, errhp, &oraStmtClose);
	if (OCIStmtExecute(svchp, oraStmtClose.stmthp, errhp, 1, 0, 0, 0, OCI_DEFAULT))
	{
		ExitWithError(CLEANUP_STMT, 4, ERROR_OCI, errhp, svchp, &oraStmtClose, "Error closing an Oracle remote file\n");
	}
	ReleaseStmt(errhp, &oraStmtClose);

	ExitWithError(CLEANUP_LOGOFF, 0, ERROR_NONE, errhp, svchp, 0, 0);
}
