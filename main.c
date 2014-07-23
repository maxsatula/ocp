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
#include <string.h>
#include <oci.h>
#include "oracle.h"

#define ORA_RAW_BUFFER_SIZE 0x2000

enum PROGRAM_ACTION { ACTION_READ, ACTION_WRITE, ACTION_LSDIR, ACTION_LS };

void LsDir(struct ORACLEALLINONE *oraAllInOne)
{
	sword ociResult;
	char vDirectory[31];
	char vDirectoryPath[MAX_FMT_SIZE];
	char vGrantable1[MAX_FMT_SIZE];
	char vGrantable2[MAX_FMT_SIZE];

	struct ORACLEDEFINE oraDefinesLsDir[] =
	{
		{ 0, SQLT_STR, vDirectory,     sizeof(vDirectory)-1,     0 },
		{ 0, SQLT_STR, vDirectoryPath, sizeof(vDirectoryPath)-1, 0 },
		{ 0, SQLT_STR, vGrantable1,    sizeof(vGrantable1)-1,    0 },
		{ 0, SQLT_STR, vGrantable2,    sizeof(vGrantable2)-1,    0 }
	};

	struct ORACLESTATEMENT oraStmtLsDir = { "\
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
          AND pw.privilege = 'WRITE'",
	       0, 0, 0, oraDefinesLsDir, sizeof(oraDefinesLsDir)/sizeof(struct ORACLEDEFINE) };

	PrepareStmtAndBind(oraAllInOne, &oraStmtLsDir);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to list oracle directories\n");

	do
	{
		printf("%c%c %-30s (%s)\n",
			   oraStmtLsDir.oraDefines[2].indp == -1 ? '-' :
			   *(char*)oraStmtLsDir.oraDefines[2].value == 'Y' ? 'R' : 'r',
			   oraStmtLsDir.oraDefines[3].indp == -1 ? '-' :
			   *(char*)oraStmtLsDir.oraDefines[3].value == 'Y' ? 'W' : 'w',
			   oraStmtLsDir.oraDefines[0].value,
			   oraStmtLsDir.oraDefines[1].value);

		ociResult = OCIStmtFetch2(oraStmtLsDir.stmthp, oraAllInOne->errhp, 1,
								  OCI_FETCH_NEXT, 1, OCI_DEFAULT);
	}
	while (ociResult == OCI_SUCCESS);

	if (ociResult != OCI_NO_DATA)
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to list oracle directories\n");

	ReleaseStmt(oraAllInOne);
}

void Ls(struct ORACLEALLINONE *oraAllInOne, char* pDirectory)
{
	sword ociResult;
	char vDirectory[31];
	char vFileName[MAX_FMT_SIZE];
	ub8 vBytes;
	char vLastModified[7];
	int i;
	long totalBytes;

	struct BINDVARIABLE oraBindsLs[] =
	{
		{ 0, SQLT_STR, ":directory", vDirectory, sizeof(vDirectory)-1 }
	};

	struct ORACLEDEFINE oraDefinesLs[] =
	{
		{ 0, SQLT_STR, vFileName,     sizeof(vFileName)-1,   0 },
		{ 0, SQLT_INT, &vBytes,       sizeof(vBytes),        0 },
		{ 0, SQLT_DAT, vLastModified, sizeof(vLastModified), 0 }
	};

	struct ORACLESTATEMENT oraStmtLs = { "\
SELECT t.file_name,\
       t.bytes,\
       t.last_modified\
  FROM all_directories d,\
       TABLE(f_ocp_dir_list(d.directory_path)) t\
 WHERE d.directory_name = :directory",
	       0, oraBindsLs, sizeof(oraBindsLs)/sizeof(struct BINDVARIABLE),
	       oraDefinesLs, sizeof(oraDefinesLs)/sizeof(struct ORACLEDEFINE) };

	strncpy(vDirectory, pDirectory, sizeof(vDirectory));
	if (vDirectory[sizeof(vDirectory) - 1])
		ExitWithError(oraAllInOne, 1, ERROR_NONE, "Oracle directory name is too long\n");

	PrepareStmtAndBind(oraAllInOne, &oraStmtLs);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to list files in oracle directory\n");

	printf("Contents of %s directory\n\
%-40s %-12s %s\n\
---------------------------------------- ------------ -------------------\n",
	       vDirectory, "File Name", "    Size", "Last Modified");

	i = 0;
	totalBytes = 0;
	do
	{
		printf("%-40s %12ld %02d/%02d/%d %02d:%02d:%02d\n",
			   vFileName,
			   vBytes,
		       (int)vLastModified[2],
		       (int)vLastModified[3],
			   ((int)vLastModified[0]-100) * 100 + ((int)vLastModified[1] - 100),
		       (int)vLastModified[4] - 1,
		       (int)vLastModified[5] - 1,
		       (int)vLastModified[6] - 1);
		i++;
		totalBytes += vBytes;

		ociResult = OCIStmtFetch2(oraStmtLs.stmthp, oraAllInOne->errhp, 1,
								  OCI_FETCH_NEXT, 1, OCI_DEFAULT);
	}
	while (ociResult == OCI_SUCCESS);

	if (ociResult != OCI_NO_DATA)
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to list files in oracle directory\n");

	printf("---------------------------------------- ------------ -------------------\n\
%5d File(s) %39ld\n", i, totalBytes);

	ReleaseStmt(oraAllInOne);	
}

void TransferFile(struct ORACLEALLINONE *oraAllInOne, int readingDirection,
                  char* pDirectory, char* pRemoteFile, char* pLocalFile)
{
	unsigned char vBuffer[ORA_RAW_BUFFER_SIZE];
	int vSize;
	FILE *fp;
	char vOpenMode[3];
	ub4 vActualSize;
	ub4 vFHandle1;
	ub4 vFHandle2;

	OCIBind *ociBind;

	/* 
	It is not originally possible to get a UTL_FILE.FILE_TYPE handle outside
	of PL/SQL and store in OCI, and there is no corresponding OCI data type
	available. Hence we have to use a dirty hack, extracting two
	BINARY_INTEGER members of UTL_FILE.FILE_TYPE record to separate bind
	variables.
	*/

	struct BINDVARIABLE bindVariablesOpen[] =
	{
		{ 0, SQLT_STR, ":directory", pDirectory,  strlen(pDirectory)  + 1 }, /* cannot do sizeof() */
		{ 0, SQLT_STR, ":filename",  pRemoteFile, strlen(pRemoteFile) + 1 }, /* cannot do sizeof() */
		{ 0, SQLT_STR, ":openmode",  vOpenMode,   sizeof(vOpenMode)       },
		{ 0, SQLT_INT, ":fhandle1",  &vFHandle1,  sizeof(vFHandle1)       },
		{ 0, SQLT_INT, ":fhandle2",  &vFHandle2,  sizeof(vFHandle2)       }
	};

	struct ORACLESTATEMENT oraStmtOpen = { "\
declare \
  handle utl_file.file_type; \
begin \
  handle := utl_file.fopen(:directory, :filename, :openmode); \
  :fhandle1 := handle.id; \
  :fhandle2 := handle.datatype; \
end;",
	       0, bindVariablesOpen, sizeof(bindVariablesOpen)/sizeof(struct BINDVARIABLE), 0, 0 };

	struct BINDVARIABLE bindVariablesClose[] =
	{
		{ 0, SQLT_INT, ":fhandle1", &vFHandle1, sizeof(vFHandle1) },
		{ 0, SQLT_INT, ":fhandle2", &vFHandle2, sizeof(vFHandle2) }
	};

	struct ORACLESTATEMENT oraStmtClose = { "\
declare \
  handle utl_file.file_type; \
begin \
  handle.id := :fhandle1; \
  handle.datatype := :fhandle2; \
  utl_file.fclose(handle); \
end;",
	       0, bindVariablesClose, sizeof(bindVariablesClose)/sizeof(struct BINDVARIABLE), 0, 0 };

	struct BINDVARIABLE bindVariablesRead[] =
	{
		{ 0, SQLT_BIN, ":buffer",   vBuffer,    sizeof(vBuffer)   },
		{ 0, SQLT_INT, ":size",     &vSize,     sizeof(vSize)     },
		{ 0, SQLT_INT, ":fhandle1", &vFHandle1, sizeof(vFHandle1) },
		{ 0, SQLT_INT, ":fhandle2", &vFHandle2, sizeof(vFHandle2) }
	};

	struct ORACLESTATEMENT oraStmtRead = { "\
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
end;",
	       0, bindVariablesRead, sizeof(bindVariablesRead)/sizeof(struct BINDVARIABLE), 0, 0 };

	struct BINDVARIABLE bindVariablesWrite[] =
	{
		{ 0, SQLT_BIN, ":buffer",   vBuffer,    sizeof(vBuffer)   },
		{ 0, SQLT_INT, ":fhandle1", &vFHandle1, sizeof(vFHandle1) },
		{ 0, SQLT_INT, ":fhandle2", &vFHandle2, sizeof(vFHandle2) }
	};

	struct ORACLESTATEMENT oraStmtWrite = { "\
declare \
  handle utl_file.file_type; \
begin \
  handle.id := :fhandle1; \
  handle.datatype := :fhandle2; \
  utl_file.put_raw(handle, :buffer); \
end;",
	       0, bindVariablesWrite, sizeof(bindVariablesWrite)/sizeof(struct BINDVARIABLE), 0, 0 };


	strcpy(vOpenMode, readingDirection ? "rb" : "wb");

	PrepareStmtAndBind(oraAllInOne, &oraStmtOpen);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to open an Oracle remote file for %s\n",
					  readingDirection ? "reading" : "writing");

	ReleaseStmt(oraAllInOne);

	PrepareStmtAndBind(oraAllInOne, readingDirection ? &oraStmtRead : &oraStmtWrite);

	if ((fp = fopen(pLocalFile, readingDirection ? "wb" : "rb")) == NULL)
	{
		ExitWithError(oraAllInOne, 4, ERROR_OS, "Error opening a local %s file for %s\n",
					  readingDirection ? "destination" : "source",
					  readingDirection ? "writing"     : "reading");
		/* 4 - Local filesystem related errors */
	}

	if (readingDirection)
	{
		do
		{
			vSize = sizeof(vBuffer);
			if (ExecuteStmt(oraAllInOne))
			{
				fclose(fp);
				/* TODO: remove partially downloaded file */
				ExitWithError(oraAllInOne, 3, ERROR_OCI, "Failed execution of %s\n",
							  oraAllInOne->currentStmt->sql);
			}
			else
			{
				fwrite(vBuffer, sizeof(unsigned char), vSize, fp);
				if (ferror(fp))
				{
					fclose(fp);
					/* TODO: remove partially downloaded file */
					ExitWithError(oraAllInOne, 4, ERROR_OS, "Error writing to a local file\n");
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
				ExitWithError(oraAllInOne, 4, ERROR_OS, "Error reading from a local file\n");
			}

			if (OCIBindByName(oraAllInOne->currentStmt->stmthp, &ociBind, oraAllInOne->errhp,
							  ":buffer", strlen(":buffer"),
							  vBuffer, vActualSize, SQLT_BIN, 0, 0, 0, 0, 0, OCI_DEFAULT))
			{
				fclose(fp);
				ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to bind :buffer\n");
			}

			if (ExecuteStmt(oraAllInOne))
			{
				fclose(fp);
				OCIHandleFree(ociBind, OCI_HTYPE_BIND);
				ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed execution of %s\n",
							  oraAllInOne->currentStmt->sql);
			}
			OCIHandleFree(ociBind, OCI_HTYPE_BIND);
		}
	}

	fclose(fp);
	ReleaseStmt(oraAllInOne);

	PrepareStmtAndBind(oraAllInOne, &oraStmtClose);
	if (ExecuteStmt(oraAllInOne))
	{
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Error closing an Oracle remote file\n");
	}
	ReleaseStmt(oraAllInOne);
}

int main(int argc, char *argv[])
{
	char connectionString[MAX_FMT_SIZE];
	char *pwdptr, *dbconptr;
	enum PROGRAM_ACTION programAction;
	char vDirectory[31];
	char vLocalFile[MAX_FMT_SIZE];
	char vRemoteFile[MAX_FMT_SIZE];
	char* fileNamePtr;
	char *localArg, *remoteArg;
	struct ORACLEALLINONE oraAllInOne = { 0, 0, 0, 0 };

	if (argc != 4 && (argc != 3 || strcmp(argv[2], "-lsdir")))
		ExitWithError(&oraAllInOne, 1, ERROR_USAGE, "Invalid number of arguments\n");
		/* 1 - Error in command line arguments */
	strncpy(connectionString, argv[1], sizeof(connectionString));
	if (connectionString[sizeof(connectionString) - 1])
		ExitWithError(&oraAllInOne, 1, ERROR_NONE, "Oracle connection string is too long\n");
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
		programAction = ACTION_LSDIR;
	}
	else if (!strcmp(argv[2], "-ls"))
	{
		programAction = ACTION_LS;
	}
	else
	{
		fileNamePtr = strchr(argv[2], ':');
		if (fileNamePtr)
		{
			programAction = ACTION_READ;
			localArg = argv[3];
			remoteArg = argv[2];
		}
		else
		{
			programAction = ACTION_WRITE;
			fileNamePtr = strchr(argv[3], ':');
			if (!fileNamePtr)
				ExitWithError(&oraAllInOne, 1, ERROR_USAGE, "Missing a colon ':' in one of arguments to specify a remote (Oracle) site\n");
			localArg = argv[2];
			remoteArg = argv[3];
		}

		strncpy(vDirectory, remoteArg, fileNamePtr - remoteArg);
		vDirectory[fileNamePtr - remoteArg] = '\0';

		strncpy(vLocalFile, localArg, sizeof(vLocalFile));
		if (vLocalFile[sizeof(vLocalFile) - 1])
			ExitWithError(&oraAllInOne, 1, ERROR_NONE, "Local file name is too long\n");
		strncpy(vRemoteFile, fileNamePtr + 1, sizeof(vRemoteFile));
		if (vRemoteFile[sizeof(vRemoteFile) - 1])
			ExitWithError(&oraAllInOne, 1, ERROR_NONE, "Remote file name is too long\n");
#ifdef DEBUG
		printf("Copying %s Oracle. Local file %s, remote file %s, directory %s\n",
			   programAction == ACTION_READ ? "FROM" : "TO", vLocalFile, vRemoteFile, vDirectory);
#endif
	}

	OracleLogon(&oraAllInOne, connectionString, pwdptr, dbconptr);

	switch (programAction)
	{
	case ACTION_LSDIR:
		LsDir(&oraAllInOne);
		break;
	case ACTION_LS:
		Ls(&oraAllInOne, argv[3]);
		break;
	case ACTION_READ:
	case ACTION_WRITE:
		TransferFile(&oraAllInOne, programAction == ACTION_READ, vDirectory, vRemoteFile, vLocalFile);
		break;
	}

	ExitWithError(&oraAllInOne, 0, ERROR_NONE, 0);
}
