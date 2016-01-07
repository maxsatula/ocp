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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef _WIN32
# include <libgen.h>
#endif

#include <oci.h>
#include "oracle.h"
#include "ocp.h"
#ifndef _WIN32
# include "progressmeter.h"
#endif

void TransferFile(struct ORACLEALLINONE *oraAllInOne, int readingDirection,
                  char* pDirectory, char* pRemoteFile, char* pLocalFile,
                  int isKeepPartial, int isResume)
{
	unsigned char vBuffer[ORA_RAW_BUFFER_SIZE];
	int vSize;
	FILE *fp;
	char vOpenMode[3];
	ub4 vActualSize;
	ub8 vSkipBytes;
	ub4 vFHandle1;
	ub4 vFHandle2;
	char vServerType[10];
#ifndef _WIN32
	int showProgress;
#endif
	int isStdUsed;
	off_t cnt;
	off_t sourceSize;
	struct stat fileStat;
	struct ORACLEFILEATTR oracleFileAttr;

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
		{ 0, SQLT_STR, ":directory", pDirectory,  ORA_IDENTIFIER_SIZE + 1 },
		{ 0, SQLT_STR, ":filename",  pRemoteFile, MAX_FMT_SIZE            },
		{ 0, SQLT_STR, ":openmode",  vOpenMode,   sizeof(vOpenMode)       },
		{ 0, SQLT_INT, ":skipbytes", &vSkipBytes, sizeof(vSkipBytes)      },
		{ 0, SQLT_INT, ":fhandle1",  &vFHandle1,  sizeof(vFHandle1)       },
		{ 0, SQLT_INT, ":fhandle2",  &vFHandle2,  sizeof(vFHandle2)       },
		{ 0 }
	};

	struct ORACLESTATEMENT oraStmtOpen = { "\
declare\n\
  handle utl_file.file_type;\n\
begin\n\
  handle := utl_file.fopen(:directory, :filename, :openmode);\n\
  if :skipbytes > 0 then\n\
    declare\n\
      buffer_ raw(16384);\n\
      leftToSkip_ number := :skipbytes;\n\
      size_ number;\n\
    begin\n\
      while leftToSkip_ > 0 loop\n\
        size_ := least(leftToSkip_, 16384);\n\
        utl_file.get_raw(handle, buffer_, size_);\n\
        leftToSkip_ := leftToSkip_ - size_;\n\
      end loop;\n\
    end;\n\
  end if;\n\
  :fhandle1 := handle.id;\n\
  :fhandle2 := handle.datatype;\n\
end;",
	       0, bindVariablesOpen, NO_ORACLE_DEFINES };

	struct BINDVARIABLE bindVariablesClose[] =
	{
		{ 0, SQLT_INT, ":fhandle1", &vFHandle1, sizeof(vFHandle1) },
		{ 0, SQLT_INT, ":fhandle2", &vFHandle2, sizeof(vFHandle2) },
		{ 0 }
	};

	struct ORACLESTATEMENT oraStmtClose = { "\
declare\n\
  handle utl_file.file_type;\n\
begin\n\
  handle.id := :fhandle1;\n\
  handle.datatype := :fhandle2;\n\
  utl_file.fclose(handle);\n\
end;",
	       0, bindVariablesClose, NO_ORACLE_DEFINES };

	struct BINDVARIABLE bindVariablesRead[] =
	{
		{ 0, SQLT_BIN, ":buffer",   vBuffer,    sizeof(vBuffer)   },
		{ 0, SQLT_INT, ":size",     &vSize,     sizeof(vSize)     },
		{ 0, SQLT_INT, ":fhandle1", &vFHandle1, sizeof(vFHandle1) },
		{ 0, SQLT_INT, ":fhandle2", &vFHandle2, sizeof(vFHandle2) },
		{ 0 }
	};

	struct ORACLESTATEMENT oraStmtRead = { "\
declare\n\
  handle utl_file.file_type;\n\
begin\n\
  handle.id := :fhandle1;\n\
  handle.datatype := :fhandle2;\n\
  utl_file.get_raw(handle, :buffer, :size);\n\
  :size := utl_raw.length(:buffer);\n\
exception\n\
  when no_data_found then\n\
    :size := 0;\n\
end;",
	       0, bindVariablesRead, NO_ORACLE_DEFINES };

	struct BINDVARIABLE bindVariablesWrite[] =
	{
		{ 0, SQLT_BIN, ":buffer",   vBuffer,    sizeof(vBuffer)   },
		{ 0, SQLT_INT, ":fhandle1", &vFHandle1, sizeof(vFHandle1) },
		{ 0, SQLT_INT, ":fhandle2", &vFHandle2, sizeof(vFHandle2) },
		{ 0 }
	};

	struct ORACLESTATEMENT oraStmtWrite = { "\
declare\n\
  handle utl_file.file_type;\n\
begin\n\
  handle.id := :fhandle1;\n\
  handle.datatype := :fhandle2;\n\
  utl_file.put_raw(handle, :buffer);\n\
end;",
	       0, bindVariablesWrite, NO_ORACLE_DEFINES };

	struct ORACLEDEFINE oraDefinesGetServerType[] =
	{
		{ 0, SQLT_STR, vServerType, sizeof(vServerType), 0 },
		{ 0 }
	};

	struct ORACLESTATEMENT oraStmtGetServerType = { "\
select server\n\
  from v$session\n\
 where audsid = sys_context('USERENV', 'SESSIONID')",
	       0, NO_BIND_VARIABLES, oraDefinesGetServerType };

	if (readingDirection)
	{
		PrepareStmtAndBind(oraAllInOne, &oraStmtGetServerType);
		if (ExecuteStmt(oraAllInOne) != OCI_SUCCESS)
		{
			fprintf(stderr, "WARNING: unable to detect whether DEDICATED server is used\n");
		}
		else if (strcmp(vServerType, "DEDICATED"))
		{
			ExitWithError(oraAllInOne, 4, ERROR_NONE, "Must connect through DEDICATED server, got %s\n", vServerType);
		}
		ReleaseStmt(oraAllInOne);
	}

	isStdUsed = !strcmp(pLocalFile, "-");
	if (isStdUsed)
	{
		isResume = 0;
		if (readingDirection)
			isKeepPartial = 1;
	}

	cnt = 0;
	vSkipBytes = 0;

	if (isResume)
	{
		if (readingDirection)
		{
			if (!stat(pLocalFile, &fileStat))
				vSkipBytes = cnt = fileStat.st_size;
		}
		else
		{
			GetOracleFileAttr(oraAllInOne, pDirectory, pRemoteFile, &oracleFileAttr);
			if (oracleFileAttr.bExists)
				cnt = oracleFileAttr.length;
		}
		if (!cnt)
			isResume = 0;
	}


	strcpy(vOpenMode, readingDirection ? "rb" : isResume ? "ab" : "wb");
	SetSessionAction(oraAllInOne, readingDirection ? "DOWNLOAD" : "UPLOAD");
	PrepareStmtAndBind(oraAllInOne, &oraStmtOpen);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to open an Oracle remote file for %s\n",
					  readingDirection ? "reading" : "writing");

	ReleaseStmt(oraAllInOne);

#ifndef _WIN32
	showProgress = 1;
	if (!isatty(STDOUT_FILENO) || isStdUsed)
		showProgress = 0;

	if (showProgress)
	{
		if (readingDirection)
		{
			GetOracleFileAttr(oraAllInOne, pDirectory, pRemoteFile, &oracleFileAttr);
			sourceSize = oracleFileAttr.length;
		}
		else
		{
			stat(pLocalFile, &fileStat);
			sourceSize = fileStat.st_size;
		}

		start_progress_meter(readingDirection ? pRemoteFile : basename(pLocalFile), sourceSize, &cnt);
	}
#endif

	PrepareStmtAndBind(oraAllInOne, readingDirection ? &oraStmtRead : &oraStmtWrite);

	if (!isStdUsed && (fp = fopen(pLocalFile, readingDirection ? (isResume ? "ab" : "wb") : "rb")) == NULL)
	{
		ExitWithError(oraAllInOne, -1, ERROR_OS, "Error opening a local %s file for %s\n",
					  readingDirection ? "destination" : "source",
					  readingDirection ? "writing"     : "reading");
		/* 4 - Local filesystem related errors */
		if (!readingDirection && !isKeepPartial)
		{
			PrepareStmtAndBind(oraAllInOne, &oraStmtClose);
			if (ExecuteStmt(oraAllInOne))
			{
				ExitWithError(oraAllInOne, -1, ERROR_OCI, "Error closing an Oracle remote file\n");
			}
			ReleaseStmt(oraAllInOne);
			Rm(oraAllInOne, pDirectory, pRemoteFile);
		}
		ExitWithError(oraAllInOne, 4, ERROR_NONE, 0);
	}

	if (isStdUsed)
		fp = readingDirection ? stdout : stdin;

	if (readingDirection)
	{
		do
		{
			vSize = sizeof(vBuffer);
			if (ExecuteStmt(oraAllInOne))
			{
				if (!isStdUsed)
					fclose(fp);
				ExitWithError(oraAllInOne, -1, ERROR_OCI, "Failed execution of %s\n",
							  oraAllInOne->currentStmt[0]->sql);
				if (!isKeepPartial)
				{
					if (unlink(pLocalFile))
						ExitWithError(oraAllInOne, 3, ERROR_OS, "Could not remove partial file %s\n", pLocalFile);
				}
				ExitWithError(oraAllInOne, 3, ERROR_NONE, 0);
			}
			else
			{
				fwrite(vBuffer, sizeof(unsigned char), vSize, fp);
				if (ferror(fp))
				{
					if (!isStdUsed)
						fclose(fp);
					ExitWithError(oraAllInOne, -1, ERROR_OS, "Error writing to a local file\n");
					if (!isKeepPartial)
					{
						if (unlink(pLocalFile))
							ExitWithError(oraAllInOne, 4, ERROR_OS, "Could not remove partial file %s\n", pLocalFile);
					}
					ExitWithError(oraAllInOne, 4, ERROR_NONE, 0);
				}
				cnt += vSize;
			}
		}
		while (vSize);
	}
	else
	{
		if (cnt > 0)
		{
			if (fseek(fp, cnt, SEEK_SET))
			{
				fclose(fp);
				ExitWithError(oraAllInOne, 4, ERROR_OS, "Error setting reading position in a local file\n");
			}
		
		}
		while (vActualSize = fread(vBuffer, sizeof(unsigned char), sizeof(vBuffer), fp))
		{
			if (ferror(fp))
			{
				if (!isStdUsed)
					fclose(fp);
				ExitWithError(oraAllInOne, -1, ERROR_OS, "Error reading from a local file\n");
				if (!isKeepPartial)
				{
					PrepareStmtAndBind(oraAllInOne, &oraStmtClose);
					if (ExecuteStmt(oraAllInOne))
					{
						ExitWithError(oraAllInOne, -1, ERROR_OCI, "Error closing an Oracle remote file\n");
					}
					ReleaseStmt(oraAllInOne);
					Rm(oraAllInOne, pDirectory, pRemoteFile);
				}
				ExitWithError(oraAllInOne, 4, ERROR_NONE, 0);
			}

			if (OCIBindByName(oraAllInOne->currentStmt[0]->stmthp, &ociBind, oraAllInOne->errhp,
							  ":buffer", strlen(":buffer"),
							  vBuffer, vActualSize, SQLT_BIN, 0, 0, 0, 0, 0, OCI_DEFAULT))
			{
				if (!isStdUsed)
					fclose(fp);
				ExitWithError(oraAllInOne, -1, ERROR_OCI, "Failed to bind :buffer\n");
				if (!isKeepPartial)
				{
					PrepareStmtAndBind(oraAllInOne, &oraStmtClose);
					if (ExecuteStmt(oraAllInOne))
					{
						ExitWithError(oraAllInOne, -1, ERROR_OCI, "Error closing an Oracle remote file\n");
					}
					ReleaseStmt(oraAllInOne);
					Rm(oraAllInOne, pDirectory, pRemoteFile);
				}
				ExitWithError(oraAllInOne, 4, ERROR_NONE, 0);
			}

			if (ExecuteStmt(oraAllInOne))
			{
				if (!isStdUsed)
					fclose(fp);
				OCIHandleFree(ociBind, OCI_HTYPE_BIND);
				ExitWithError(oraAllInOne, -1, ERROR_OCI, "Failed execution of %s\n",
							  oraAllInOne->currentStmt[0]->sql);
				if (!isKeepPartial)
				{
					PrepareStmtAndBind(oraAllInOne, &oraStmtClose);
					if (ExecuteStmt(oraAllInOne))
					{
						ExitWithError(oraAllInOne, -1, ERROR_OCI, "Error closing an Oracle remote file\n");
					}
					ReleaseStmt(oraAllInOne);
					Rm(oraAllInOne, pDirectory, pRemoteFile);
				}
				ExitWithError(oraAllInOne, 4, ERROR_NONE, 0);
			}
			OCIHandleFree(ociBind, OCI_HTYPE_BIND);
			cnt += vActualSize;
		}
	}

#ifndef _WIN32
	if (showProgress)
		stop_progress_meter();
#endif
	if (!isStdUsed)
		fclose(fp);
	ReleaseStmt(oraAllInOne);

	PrepareStmtAndBind(oraAllInOne, &oraStmtClose);
	if (ExecuteStmt(oraAllInOne))
	{
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Error closing an Oracle remote file\n");
	}
	ReleaseStmt(oraAllInOne);
	SetSessionAction(oraAllInOne, 0);
}
