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

*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <popt.h>
#include <zlib.h>
#include <oci.h>
#include "oracle.h"
#include "progressmeter.h"

#define ORA_RAW_BUFFER_SIZE 0x4000
#define ORA_BLOB_BUFFER_SIZE 0x10000
#define ORA_IDENTIFIER_SIZE 30

enum PROGRAM_ACTION { ACTION_READ, ACTION_WRITE, ACTION_LSDIR, ACTION_LS, ACTION_RM,
	ACTION_GZIP, ACTION_GUNZIP, ACTION_INSTALL, ACTION_DEINSTALL };

void LsDir(struct ORACLEALLINONE *oraAllInOne)
{
	sword ociResult;
	char vDirectory[ORA_IDENTIFIER_SIZE + 1];
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

void Ls(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, const char* sql)
{
	sword ociResult;
	char vFileName[MAX_FMT_SIZE];
	ub8 vBytes;
	char vLastModified[7];
	int i;
	long totalBytes;

	struct BINDVARIABLE oraBindsLs[] =
	{
		{ 0, SQLT_STR, ":directory", pDirectory, ORA_IDENTIFIER_SIZE + 1 }
	};

	struct ORACLEDEFINE oraDefinesLs[] =
	{
		{ 0, SQLT_STR, vFileName,     sizeof(vFileName)-1,   0 },
		{ 0, SQLT_INT, &vBytes,       sizeof(vBytes),        0 },
		{ 0, SQLT_DAT, vLastModified, sizeof(vLastModified), 0 }
	};

	struct ORACLESTATEMENT oraStmtLs = {
	       sql,
	       0, oraBindsLs, sizeof(oraBindsLs)/sizeof(struct BINDVARIABLE),
	       oraDefinesLs, sizeof(oraDefinesLs)/sizeof(struct ORACLEDEFINE) };

	PrepareStmtAndBind(oraAllInOne, &oraStmtLs);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to list files in oracle directory\n");

	printf("Contents of %s directory\n\
%-40s %-12s %s\n\
---------------------------------------- ------------ -------------------\n",
	       pDirectory, "File Name", "    Size", "Last Modified");

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

void Rm(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, char* pFileName)
{
	struct BINDVARIABLE oraBindsRm[] =
	{
		{ 0, SQLT_STR, ":directory", pDirectory, ORA_IDENTIFIER_SIZE + 1 },
		{ 0, SQLT_STR, ":filename",  pFileName,  MAX_FMT_SIZE }
	};

	struct ORACLESTATEMENT oraStmtRm = {
	       "BEGIN utl_file.fremove(:directory, :filename); END;",
	       0, oraBindsRm, sizeof(oraBindsRm)/sizeof(struct BINDVARIABLE), 0, 0 };

	PrepareStmtAndBind(oraAllInOne, &oraStmtRm);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to remove file in oracle directory\n");

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
	int showProgress;
	off_t cnt;
	off_t sourceSize;
	struct stat fileStat;

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

	struct BINDVARIABLE bindVariablesFsize[] =
	{
		{ 0, SQLT_STR, ":directory", pDirectory,  ORA_IDENTIFIER_SIZE + 1 },
		{ 0, SQLT_STR, ":filename",  pRemoteFile, MAX_FMT_SIZE            },
		{ 0, SQLT_INT, ":length",    &sourceSize, sizeof(sourceSize)      }
	};

	struct ORACLESTATEMENT oraStmtFsize = { "\
declare \
  exists_ BOOLEAN;\
  blocksize_ NUMBER; \
begin \
  utl_file.fgetattr(:directory, :filename, exists_, :length, blocksize_); \
end;",
	       0, bindVariablesFsize, sizeof(bindVariablesFsize)/sizeof(struct BINDVARIABLE), 0, 0 };

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

	showProgress = 1;
	if (!isatty(STDOUT_FILENO))
		showProgress = 0;
	cnt = 0;
	if (showProgress)
	{
		if (readingDirection)
		{
			PrepareStmtAndBind(oraAllInOne, &oraStmtFsize);
			if (ExecuteStmt(oraAllInOne))
				ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to obtain Oracle remote file size\n");
			ReleaseStmt(oraAllInOne);
		}
		else
		{
			stat(pLocalFile, &fileStat);
			sourceSize = fileStat.st_size;
		}

		start_progress_meter(readingDirection ? pRemoteFile : pLocalFile, sourceSize, &cnt);
	}

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
				cnt += vSize;
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
			cnt += vActualSize;
		}
	}

	if (showProgress)
		stop_progress_meter();
	fclose(fp);
	ReleaseStmt(oraAllInOne);

	PrepareStmtAndBind(oraAllInOne, &oraStmtClose);
	if (ExecuteStmt(oraAllInOne))
	{
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Error closing an Oracle remote file\n");
	}
	ReleaseStmt(oraAllInOne);
}

void Compress(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, int compressionLevel,
              char* pOriginalFileName, char* pCompressedFileName)
{
	ub4 vCompressionLevel;

	struct BINDVARIABLE oraBindsCompress[] =
	{
		{ 0, SQLT_STR, ":directory",           pDirectory,          ORA_IDENTIFIER_SIZE + 1   },
		{ 0, SQLT_INT, ":compression_level",   &vCompressionLevel,  sizeof(vCompressionLevel) },
		{ 0, SQLT_STR, ":original_filename"  , pOriginalFileName,   MAX_FMT_SIZE              },
		{ 0, SQLT_STR, ":compressed_filename", pCompressedFileName, MAX_FMT_SIZE              }
	};

	struct ORACLESTATEMENT oraStmtCompress = { "\
DECLARE\
	f_handle UTL_FILE.FILE_TYPE;\
	c_handle BINARY_INTEGER;\
	raw_buffer RAW(32767);\
	blob_buffer BLOB;\
	actual_size NUMBER;\
	pos BINARY_INTEGER;\
	blobsize BINARY_INTEGER; \
BEGIN\
	f_handle := UTL_FILE.FOPEN(:directory, :original_filename, 'rb');\
	DBMS_LOB.CREATETEMPORARY(blob_buffer, TRUE, DBMS_LOB.CALL);\
	IF :compression_level > 0 THEN\
		c_handle := UTL_COMPRESS.LZ_COMPRESS_OPEN(blob_buffer, :compression_level);\
	ELSE\
		c_handle := UTL_COMPRESS.LZ_COMPRESS_OPEN(blob_buffer);\
	END IF;\
	LOOP\
		BEGIN\
			UTL_FILE.GET_RAW(f_handle, raw_buffer, 16384);\
			UTL_COMPRESS.LZ_COMPRESS_ADD(c_handle, blob_buffer, raw_buffer);\
		EXCEPTION\
			WHEN NO_DATA_FOUND THEN\
				EXIT;\
		END;\
	END LOOP;\
	UTL_COMPRESS.LZ_COMPRESS_CLOSE(c_handle, blob_buffer);\
	UTL_FILE.FCLOSE(f_handle);\
\
	f_handle := UTL_FILE.FOPEN(:directory, :compressed_filename, 'wb');\
	pos := 0;\
	blobsize := DBMS_LOB.GETLENGTH(blob_buffer);\
	WHILE pos < blobsize LOOP\
		actual_size := LEAST(blobsize - pos, 16384);\
		DBMS_LOB.READ(blob_buffer, actual_size, pos + 1, raw_buffer);\
		UTL_FILE.PUT_RAW(f_handle, raw_buffer);\
		pos := pos + actual_size;\
	END LOOP;\
	DBMS_LOB.FREETEMPORARY(blob_buffer);\
	UTL_FILE.FCLOSE(f_handle);\
END;\
",
	       0, oraBindsCompress, sizeof(oraBindsCompress)/sizeof(struct BINDVARIABLE), 0, 0 };

	vCompressionLevel = compressionLevel;

	PrepareStmtAndBind(oraAllInOne, &oraStmtCompress);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to compress file in oracle directory\n");

	ReleaseStmt(oraAllInOne);	
}

void Uncompress(struct ORACLEALLINONE *oraAllInOne, char* pDirectory,
                char* pOriginalFileName, char* pUncompressedFileName)
{
	struct BINDVARIABLE oraBindsUncompress[] =
	{
		{ 0, SQLT_STR, ":directory",             pDirectory,            ORA_IDENTIFIER_SIZE + 1 },
		{ 0, SQLT_STR, ":original_filename"  ,   pOriginalFileName,     MAX_FMT_SIZE            },
		{ 0, SQLT_STR, ":uncompressed_filename", pUncompressedFileName, MAX_FMT_SIZE            }
	};

	struct ORACLESTATEMENT oraStmtUncompress = { "\
DECLARE\
	f_handle UTL_FILE.FILE_TYPE;\
	c_handle BINARY_INTEGER;\
	raw_buffer RAW(32767);\
	blob_buffer BLOB; \
BEGIN\
	DBMS_LOB.CREATETEMPORARY(blob_buffer, TRUE, DBMS_LOB.CALL);\
	f_handle := UTL_FILE.FOPEN(:directory, :original_filename, 'rb');\
	LOOP\
		BEGIN\
			UTL_FILE.GET_RAW(f_handle, raw_buffer, 16384);\
			DBMS_LOB.WRITEAPPEND(blob_buffer, UTL_RAW.LENGTH(raw_buffer), raw_buffer);\
		EXCEPTION\
			WHEN NO_DATA_FOUND THEN\
				EXIT;\
		END;\
	END LOOP;\
	UTL_FILE.FCLOSE(f_handle);\
\
	c_handle := UTL_COMPRESS.LZ_UNCOMPRESS_OPEN(blob_buffer);\
	f_handle := UTL_FILE.FOPEN(:directory, :uncompressed_filename, 'wb');\
	LOOP\
		BEGIN\
			UTL_COMPRESS.LZ_UNCOMPRESS_EXTRACT(c_handle, raw_buffer);\
			UTL_FILE.PUT_RAW(f_handle, raw_buffer);\
		EXCEPTION\
			WHEN NO_DATA_FOUND THEN\
				EXIT;\
		END;\
	END LOOP;\
	UTL_FILE.FCLOSE(f_handle);\
	UTL_COMPRESS.LZ_UNCOMPRESS_CLOSE(c_handle);\
	DBMS_LOB.FREETEMPORARY(blob_buffer);\
END;\
",
	       0, oraBindsUncompress, sizeof(oraBindsUncompress)/sizeof(struct BINDVARIABLE), 0, 0 };

	PrepareStmtAndBind(oraAllInOne, &oraStmtUncompress);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to uncompress file in oracle directory\n");

	ReleaseStmt(oraAllInOne);	
}

void DownloadFileWithCompression(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, int compressionLevel, char* pRemoteFile, char* pLocalFile)
{
	FILE *fp;
	sword result;
	ub4 vCompressionLevel;
	char blobBuffer[ORA_BLOB_BUFFER_SIZE];
	oraub8 vSize;
	int zRet;
	z_stream zStrm;
	unsigned char zOut[ORA_BLOB_BUFFER_SIZE];
	int showProgress;
	off_t cnt;
	off_t sourceSize;

	struct BINDVARIABLE oraBindsDownload[] =
	{
		{ 0, SQLT_STR,  ":directory",         pDirectory,         ORA_IDENTIFIER_SIZE + 1   },
		{ 0, SQLT_INT,  ":compression_level", &vCompressionLevel, sizeof(vCompressionLevel) },
		{ 0, SQLT_STR,  ":filename",          pRemoteFile,        MAX_FMT_SIZE              },
		{ 0, SQLT_BLOB, ":blob",              &oraAllInOne->blob, sizeof(oraAllInOne->blob) }
	};

	struct ORACLESTATEMENT oraStmtDownload = { "\
DECLARE\
	f_handle UTL_FILE.FILE_TYPE;\
	c_handle BINARY_INTEGER;\
	raw_buffer RAW(32767);\
BEGIN\
	f_handle := UTL_FILE.FOPEN(:directory, :filename, 'rb');\
	DBMS_LOB.CREATETEMPORARY(:blob, TRUE, DBMS_LOB.CALL);\
	IF :compression_level > 0 THEN\
		c_handle := UTL_COMPRESS.LZ_COMPRESS_OPEN(:blob, :compression_level);\
	ELSE\
		c_handle := UTL_COMPRESS.LZ_COMPRESS_OPEN(:blob);\
	END IF;\
	LOOP\
		BEGIN\
			UTL_FILE.GET_RAW(f_handle, raw_buffer, 16384);\
			UTL_COMPRESS.LZ_COMPRESS_ADD(c_handle, :blob, raw_buffer);\
		EXCEPTION\
			WHEN NO_DATA_FOUND THEN\
				EXIT;\
		END;\
	END LOOP;\
	UTL_COMPRESS.LZ_COMPRESS_CLOSE(c_handle, :blob);\
	UTL_FILE.FCLOSE(f_handle);\
END;\
",
	       0, oraBindsDownload, sizeof(oraBindsDownload)/sizeof(struct BINDVARIABLE), 0, 0 };

	vCompressionLevel = compressionLevel;

	if (OCIDescriptorAlloc(oraAllInOne->envhp, (void**)&oraAllInOne->blob, OCI_DTYPE_LOB, 0, 0))
	{
		ExitWithError(oraAllInOne, 4, ERROR_NONE, "Failed to allocate BLOB\n");
	}

	PrepareStmtAndBind(oraAllInOne, &oraStmtDownload);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to compress file in oracle directory\n");

	showProgress = 1;
	if (!isatty(STDOUT_FILENO))
		showProgress = 0;
	cnt = 0;
	if (showProgress)
	{
		if (OCILobGetLength2(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob, (oraub8*)&sourceSize))
			ExitWithError(oraAllInOne, 4, ERROR_OCI, "Error getting BLOB length\n");
		start_progress_meter(pRemoteFile, sourceSize, &cnt);
	}

	if ((fp = fopen(pLocalFile, "wb")) == NULL)
	{
		ExitWithError(oraAllInOne, 4, ERROR_OS, "Error opening a local file for writing\n");
	}

	zStrm.zalloc = Z_NULL;
    zStrm.zfree = Z_NULL;
    zStrm.opaque = Z_NULL;
    zStrm.avail_in = 0;
    zStrm.next_in = Z_NULL;
	zRet = inflateInit2(&zStrm, 16+MAX_WBITS);
	if (zRet != Z_OK)
	{
		fclose(fp);
		ExitWithError(oraAllInOne, 5, ERROR_NONE, "ZLIB initialization failed\n");
	}

	vSize = 0;
	result = OCILobRead2(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob, &vSize, 0, 1, blobBuffer, ORA_BLOB_BUFFER_SIZE, OCI_FIRST_PIECE, 0, 0, 0, 0);
	while (result == OCI_NEED_DATA || result == OCI_SUCCESS)
	{
		cnt += vSize;
		zStrm.avail_in = vSize;
		zStrm.next_in = blobBuffer;
		do
		{
			zStrm.avail_out = ORA_BLOB_BUFFER_SIZE;
			zStrm.next_out = zOut;
			zRet = inflate(&zStrm, Z_NO_FLUSH);
			switch (zRet)
			{
            case Z_STREAM_ERROR:
			case Z_NEED_DICT:
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&zStrm);
				fclose(fp);
				ExitWithError(oraAllInOne, 5, ERROR_NONE, "ZLIB inflate failed: %d, size %d\n", zRet, vSize);
			}

			fwrite(zOut, sizeof(unsigned char), ORA_BLOB_BUFFER_SIZE - zStrm.avail_out, fp);
			if (ferror(fp))
			{
				(void)inflateEnd(&zStrm);
				fclose(fp);
				/* TODO: remove partially downloaded file */
				ExitWithError(oraAllInOne, 4, ERROR_OS, "Error writing to a local file\n");
			}
		}
		while (zStrm.avail_out == 0);

		if (result == OCI_SUCCESS)
			break;
		result = OCILobRead2(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob, &vSize, 0, 1, blobBuffer, ORA_BLOB_BUFFER_SIZE, OCI_NEXT_PIECE, 0, 0, 0, 0);
	}

	if (showProgress)
		stop_progress_meter();
	inflateEnd(&zStrm);
	fclose(fp);

	if (result != OCI_SUCCESS)
	{
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Error reading BLOB\n");
	}

	ReleaseStmt(oraAllInOne);

	OCILobFreeTemporary(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob);

	if (OCIDescriptorFree(oraAllInOne->blob, OCI_DTYPE_LOB))
	{
		ExitWithError(oraAllInOne, 4, ERROR_NONE, "Failed to free BLOB\n");
	}
	oraAllInOne->blob = 0;
}

void UploadFileWithCompression(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, int compressionLevel, char* pRemoteFile, char* pLocalFile)
{
	FILE *fp;
	sword result;
	char blobBuffer[ORA_BLOB_BUFFER_SIZE];
	oraub8 vSize;
	ub1 piece;
	int zRet, zFlush;
	z_stream zStrm;
	unsigned char zIn[ORA_BLOB_BUFFER_SIZE];
	int showProgress;
	off_t cnt;
	off_t sourceSize;
	struct stat fileStat;

	struct BINDVARIABLE oraBindsUpload[] =
	{
		{ 0, SQLT_STR,  ":directory",         pDirectory,         ORA_IDENTIFIER_SIZE + 1   },
		{ 0, SQLT_STR,  ":filename",          pRemoteFile,        MAX_FMT_SIZE              },
		{ 0, SQLT_BLOB, ":blob",              &oraAllInOne->blob, sizeof(oraAllInOne->blob) }
	};

	struct ORACLESTATEMENT oraStmtUpload = { "\
DECLARE\
	f_handle UTL_FILE.FILE_TYPE;\
	c_handle BINARY_INTEGER;\
	raw_buffer RAW(32767);\
BEGIN\
	c_handle := UTL_COMPRESS.LZ_UNCOMPRESS_OPEN(:blob);\
	f_handle := UTL_FILE.FOPEN(:directory, :filename, 'wb');\
	LOOP\
		BEGIN\
			UTL_COMPRESS.LZ_UNCOMPRESS_EXTRACT(c_handle, raw_buffer);\
			UTL_FILE.PUT_RAW(f_handle, raw_buffer);\
		EXCEPTION\
			WHEN NO_DATA_FOUND THEN\
				EXIT;\
		END;\
	END LOOP;\
	UTL_FILE.FCLOSE(f_handle);\
	UTL_COMPRESS.LZ_UNCOMPRESS_CLOSE(c_handle);\
END;\
",
	       0, oraBindsUpload, sizeof(oraBindsUpload)/sizeof(struct BINDVARIABLE), 0, 0 };

	if (OCIDescriptorAlloc(oraAllInOne->envhp, (void**)&oraAllInOne->blob, OCI_DTYPE_LOB, 0, 0))
	{
		ExitWithError(oraAllInOne, 4, ERROR_NONE, "Failed to allocate BLOB\n");
	}

	if (OCILobCreateTemporary(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob, OCI_DEFAULT, 0, OCI_TEMP_BLOB, TRUE/*cache*/, OCI_DURATION_SESSION))
	{
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to create temporary BLOB\n");
	}
	/*if (OCILobOpen(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob, OCI_LOB_READWRITE))
	{
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to open temporary BLOB for write\n");
	}*/
	piece = OCI_FIRST_PIECE;

	showProgress = 1;
	if (!isatty(STDOUT_FILENO))
		showProgress = 0;
	cnt = 0;
	if (showProgress)
	{
		stat(pLocalFile, &fileStat);
		sourceSize = fileStat.st_size;
		start_progress_meter(pLocalFile, sourceSize, &cnt);
	}


	if ((fp = fopen(pLocalFile, "rb")) == NULL)
	{
		ExitWithError(oraAllInOne, 4, ERROR_OS, "Error opening a local file for reading\n");
	}

	zStrm.zalloc = Z_NULL;
    zStrm.zfree = Z_NULL;
    zStrm.opaque = Z_NULL;

	zRet = deflateInit2(&zStrm, compressionLevel ? compressionLevel : Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16+MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
	if (zRet != Z_OK)
	{
		fclose(fp);
		ExitWithError(oraAllInOne, 5, ERROR_NONE, "ZLIB initialization failed\n");
	}

	while (zStrm.avail_in = fread(zIn, sizeof(unsigned char), sizeof(zIn), fp))
	{
		cnt += zStrm.avail_in;
		if (ferror(fp))
		{
			(void)deflateEnd(&zStrm);
			fclose(fp);
			ExitWithError(oraAllInOne, 4, ERROR_OS, "Error reading from a local file\n");
		}

		zFlush = feof(fp) ? Z_FINISH : Z_NO_FLUSH;
		zStrm.next_in = zIn;

		do
		{
			zStrm.avail_out = ORA_BLOB_BUFFER_SIZE;
			zStrm.next_out = blobBuffer;
			zRet = deflate(&zStrm, zFlush);
			if (zRet != Z_OK && zRet != Z_STREAM_END && zRet != Z_BUF_ERROR)
			{
				(void)deflateEnd(&zStrm);
				fclose(fp);
				ExitWithError(oraAllInOne, 5, ERROR_NONE, "ZLIB deflate failed: %d, size %d\n", zRet, zStrm.avail_in);
			}

			if (zRet == Z_STREAM_END)
				piece = OCI_LAST_PIECE;
			vSize = 0;
			if (zStrm.avail_out == ORA_BLOB_BUFFER_SIZE)
				continue;
			result = OCILobWrite2(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob, &vSize, 0, 1, blobBuffer, ORA_BLOB_BUFFER_SIZE - zStrm.avail_out, piece, 0, 0, 0, 0);
			if (result != OCI_NEED_DATA && result)
			{
				(void)deflateEnd(&zStrm);
				fclose(fp);
				/* TODO: remove partially downloaded file */
				ExitWithError(oraAllInOne, 4, ERROR_OCI, "Error writing to BLOB\n");
			}
			piece = OCI_NEXT_PIECE;
		}
		while (zStrm.avail_out == 0);
	}

	if (showProgress)
		stop_progress_meter();
	deflateEnd(&zStrm);
	fclose(fp);

	/*vSize = 0;
	if (OCILobWrite2(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob, &vSize, 0, 1, blobBuffer, 0, OCI_LAST_PIECE, 0, 0, 0, 0))
	{
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Error writing last piece to BLOB\n");
	}
	if (OCILobClose(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob))
	{
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to close temporary BLOB\n");
	}*/

	PrepareStmtAndBind(oraAllInOne, &oraStmtUpload);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to decompress file in oracle directory\n");

	ReleaseStmt(oraAllInOne);

	OCILobFreeTemporary(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob);

	if (OCIDescriptorFree(oraAllInOne->blob, OCI_DTYPE_LOB))
	{
		ExitWithError(oraAllInOne, 4, ERROR_NONE, "Failed to free BLOB\n");
	}
	oraAllInOne->blob = 0;
}

struct PROGRAM_OPTIONS
{
	enum PROGRAM_ACTION programAction;
	const char* lsDirectoryName;
	int compressionLevel;
	int isBackground;
	const char* connectionString;
};

void ExitWithUsage(poptContext* poptcon)
{
	poptPrintUsage(*poptcon, stderr, 0);
	exit(1);
	/* 1 - Error in command line arguments */
}

void SplitToDirectoryAndFileName(poptContext *poptcon, char* pDirectory, char* pFileName)
{
	const char* remoteArg;
	const char* fileNamePtr;

	remoteArg = poptGetArg(*poptcon);
	if (!remoteArg)
	{
		fprintf(stderr, "Missing filename\n");
		ExitWithUsage(poptcon);
	}

	fileNamePtr = strchr(remoteArg, ':');
	if (!fileNamePtr)
	{
		fprintf(stderr, "Missing a colon ':' which separates directory name and file name\n");
		ExitWithUsage(poptcon);
	}

	if (fileNamePtr - remoteArg > ORA_IDENTIFIER_SIZE)
	{
		fprintf(stderr, "Oracle Directory name is too long\n");
		ExitWithUsage(poptcon);
	}

	strncpy(pDirectory, remoteArg, fileNamePtr - remoteArg);
	pDirectory[fileNamePtr - remoteArg] = '\0';
	strncpy(pFileName, fileNamePtr + 1, MAX_FMT_SIZE);
	if (pFileName[MAX_FMT_SIZE - 1])
	{
		fprintf(stderr, "File name is too long\n");
		ExitWithUsage(poptcon);
	}
}

int main(int argc, const char *argv[])
{
	char connectionString[MAX_FMT_SIZE];
	char *pwdptr, *dbconptr;
	char vDirectory[ORA_IDENTIFIER_SIZE + 1];
	char vLocalFile[MAX_FMT_SIZE];
	char vRemoteFile[MAX_FMT_SIZE];
	const char* fileNamePtr;
	const char *localArg, *remoteArg;
	char* sqlLsPtr;
	char sqlLs[10000] = "\
SELECT t.file_name,\
       t.bytes,\
       t.last_modified\
  FROM all_directories d,\
       TABLE(f_ocp_dir_list(d.directory_path)) t\
 WHERE d.directory_name = :directory";
	struct ORACLEALLINONE oraAllInOne = { 0, 0, 0, 0 };
	struct PROGRAM_OPTIONS programOptions;
	poptContext poptcon;
	int rc;

	struct poptOption compressionOptions[] =
	{
		{ "gzip", '\0', POPT_ARG_NONE, 0, ACTION_GZIP, "Compress file in Oracle directory" },
		{ "gunzip", '\0', POPT_ARG_NONE, 0, ACTION_GUNZIP, "Decompress file in Oracle directory" },
		{ "fast", '1', POPT_ARG_NONE, 0, 0x81, "Fastest compression method" },
		{ NULL, '2', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, 0, 0x82 },
		{ NULL, '3', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, 0, 0x83 },
		{ NULL, '4', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, 0, 0x84 },
		{ NULL, '5', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, 0, 0x85 },
		{ NULL, '6', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, 0, 0x86 },
		{ NULL, '7', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, 0, 0x87 },
		{ NULL, '8', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, 0, 0x88 },
		{ "best", '9', POPT_ARG_NONE, 0, 0x89, "Best compression method" },
		{ "background", 'b', POPT_ARG_VAL, &programOptions.isBackground, 1, "Submit Oracle Scheduler job and exit immediately" },
		POPT_TABLEEND
	};

	struct poptOption objOptions[] =
	{
		{ "install", '\0', POPT_ARG_NONE, 0, ACTION_INSTALL, "Install objects" },
		{ "deinstall", '\0', POPT_ARG_NONE, 0, ACTION_DEINSTALL, "Deinstall objects" },
		POPT_TABLEEND
	};

	struct poptOption options[] =
	{
		{ "list-directories", '\0', POPT_ARG_NONE, 0, ACTION_LSDIR, "List Oracle directories" },
		{ "ls", '\0', POPT_ARG_STRING, &programOptions.lsDirectoryName, ACTION_LS, "List files in Oracle directory", "DIRECTORY" },
		{ "rm", '\0', POPT_ARG_NONE, 0, ACTION_RM, "Remove file from Oracle directory" },
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, compressionOptions, 0, "Compression options:" },
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, objOptions, 0, "Database objects for --ls support:" },
		POPT_AUTOHELP
		POPT_TABLEEND
	};

	programOptions.programAction = ACTION_READ;
	programOptions.lsDirectoryName = 0;
	programOptions.compressionLevel = 0;
	programOptions.isBackground = 0;
	programOptions.connectionString = 0;

	poptcon = poptGetContext(NULL, argc, argv, options, 0);
	while ((rc = poptGetNextOpt(poptcon)) >= 0)
	{
		switch (rc)
		{
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87:
		case 0x88:
		case 0x89:
			if (programOptions.compressionLevel)
			{
				fprintf(stderr, "Mutually exclusive compression levels specified\n");
				ExitWithUsage(&poptcon);
			}
			programOptions.compressionLevel = rc - 0x80;
			break;
		default:
			if (programOptions.programAction)
			{
				fprintf(stderr, "Mutually exclusive options specified\n");
				ExitWithUsage(&poptcon);
			}
			programOptions.programAction = rc;
			break;
		}
	}

	if (rc < -1)
	{
		fprintf(stderr, "Error with option [%s]: %s\n",
			poptBadOption(poptcon, POPT_BADOPTION_NOALIAS),
			poptStrerror(rc));
		ExitWithUsage(&poptcon);
	}

	if (programOptions.compressionLevel && programOptions.programAction != ACTION_READ &&
		programOptions.programAction != ACTION_WRITE && programOptions.programAction != ACTION_GZIP)
	{
		fprintf(stderr, "Compression level can only be specified for transfer or gzip mode\n");
		ExitWithUsage(&poptcon);
	}

	if (programOptions.isBackground && programOptions.programAction != ACTION_GZIP && programOptions.programAction != ACTION_GUNZIP)
	{
		fprintf(stderr, "Background mode can only be specified for gzip/gunzip mode\n");
		ExitWithUsage(&poptcon);
	}

	programOptions.connectionString = poptGetArg(poptcon);
	if (!programOptions.connectionString)
	{
		fprintf(stderr, "No Oracle connection string specified\n");
		ExitWithUsage(&poptcon);
	}

	strncpy(connectionString, programOptions.connectionString, sizeof(connectionString));	
	if (connectionString[sizeof(connectionString) - 1])
	{
		fprintf(stderr, "Oracle connection string is too long\n");
		ExitWithUsage(&poptcon);
	}

	pwdptr = strchr(connectionString, '/');
	if (pwdptr)
		*pwdptr++ = '\0';
	dbconptr = strchr(pwdptr, '@');
	if (dbconptr)
		*dbconptr++ = '\0';
	else
	{
		fprintf(stderr, "Invalid connection string format\n");
		ExitWithUsage(&poptcon);
	}

#ifdef DEBUG
	printf("Database connection: %s@%s\n", connectionString, dbconptr);
#endif

	switch (programOptions.programAction)
	{
	case ACTION_READ:
	case ACTION_WRITE:
		remoteArg = poptGetArg(poptcon);
		if (!remoteArg)
		{
			fprintf(stderr, "Missing two arguments for source and destination files\n");
			ExitWithUsage(&poptcon);
		}
		localArg = poptGetArg(poptcon);
		if (!localArg)
		{
			fprintf(stderr, "Missing destination file name\n");
			ExitWithUsage(&poptcon);
		}

		fileNamePtr = strchr(remoteArg, ':');
		if (!fileNamePtr)
		{
			programOptions.programAction = ACTION_WRITE;
			fileNamePtr = remoteArg; remoteArg = localArg; localArg = fileNamePtr;

			fileNamePtr = strchr(remoteArg, ':');
			if (!fileNamePtr)
			{
				fprintf(stderr, "Missing a colon ':' in one of arguments to specify a remote (Oracle) site\n");
				ExitWithUsage(&poptcon);
			}
		}

		if (fileNamePtr - remoteArg >= sizeof(vDirectory))
		{
			fprintf(stderr, "Oracle Directory name is too long\n");
			ExitWithUsage(&poptcon);
		}

		strncpy(vDirectory, remoteArg, fileNamePtr - remoteArg);
		vDirectory[fileNamePtr - remoteArg] = '\0';

		strncpy(vLocalFile, localArg, sizeof(vLocalFile));
		if (vLocalFile[sizeof(vLocalFile) - 1])
		{
			fprintf(stderr, "Local file name is too long\n");
			ExitWithUsage(&poptcon);
		}
		strncpy(vRemoteFile, fileNamePtr + 1, sizeof(vRemoteFile));
		if (vRemoteFile[sizeof(vRemoteFile) - 1])
		{
			fprintf(stderr, "Remote file name is too long\n");
			ExitWithUsage(&poptcon);
		}

#ifdef DEBUG
		printf("Copying %s Oracle. Local file %s, remote file %s, directory %s\n",
			   programOptions.programAction == ACTION_READ ? "FROM" : "TO", vLocalFile, vRemoteFile, vDirectory);
		if (programOptions.compressionLevel)
			printf("On-the-fly compression is on, level %d...\n", programOptions.compressionLevel);
#endif
		break;
#ifdef DEBUG
	case ACTION_LSDIR:
		printf("Listing directories...\n");
		break;
#endif
	case ACTION_LS:
		strncpy(vDirectory, programOptions.lsDirectoryName, sizeof(vDirectory));	
		if (vDirectory[sizeof(vDirectory) - 1])
		{
			fprintf(stderr, "Oracle Directory name is too long\n");
			ExitWithUsage(&poptcon);
		}

		if (poptPeekArg(poptcon))
		{
			strcat(sqlLs, " AND (");
			while (poptPeekArg(poptcon))
			{
				if (strlen(sqlLs) + 25/*approx*/ + strlen(poptPeekArg(poptcon)) >= 1000)
				{
					fprintf(stderr, "File list is too long\n");
					ExitWithUsage(&poptcon);
				}
				strcat(sqlLs, "t.file_name like '");
				sqlLsPtr = sqlLs + strlen(sqlLs);
				strcpy(sqlLsPtr, poptGetArg(poptcon));
				while (*sqlLsPtr)
				{
					if (*sqlLsPtr == '*')
						*sqlLsPtr = '%';
					else if (*sqlLsPtr == '?')
						*sqlLsPtr = '_';
					sqlLsPtr++;
				}
				strcat(sqlLs, "' OR ");				
			}
			sqlLs[strlen(sqlLs)-4] = ')';
			sqlLs[strlen(sqlLs)-3] = '\0';
		}

#ifdef DEBUG
		printf("Listing files in directory %s...\n", vDirectory);
		printf("SQL: %s\n", sqlLs);
#endif
		break;
	case ACTION_RM:
		SplitToDirectoryAndFileName(&poptcon, vDirectory, vRemoteFile);

#ifdef DEBUG
		printf("Removing file %s from directory %s...\n", vRemoteFile, vDirectory);
#endif
		break;
	case ACTION_GZIP:
		SplitToDirectoryAndFileName(&poptcon, vDirectory, vRemoteFile);
		if (strlen(vRemoteFile) + 3 >= MAX_FMT_SIZE)
		{
			fprintf(stderr, "Compressed file name is too long\n");
			ExitWithUsage(&poptcon);
		}
		strcpy(vLocalFile, vRemoteFile);
		strcat(vLocalFile, ".gz");
#ifdef DEBUG
		printf("Compressing file, compression level %d%s...\n",
			programOptions.compressionLevel,
			programOptions.isBackground ? ", in background" : "");
#endif
		break;
	case ACTION_GUNZIP:
		SplitToDirectoryAndFileName(&poptcon, vDirectory, vRemoteFile);
		if (strlen(vRemoteFile) < 4 || strcmp(".gz", vRemoteFile + strlen(vRemoteFile) - 3))
		{
			fprintf(stderr, "Compressed file name does not end with .gz\n");
			ExitWithUsage(&poptcon);
		}
		strcpy(vLocalFile, vRemoteFile);
		vLocalFile[strlen(vRemoteFile) - 3] = '\0';
#ifdef DEBUG
		printf("Decompressing file%s...\n",
			programOptions.isBackground ? ", in background" : "");
#endif
		break;
#ifdef DEBUG
	case ACTION_INSTALL:
		printf("Installing objects...\n");
		break;
	case ACTION_DEINSTALL:
		printf("Deinstalling objects...\n");
		break;
#endif
	}

	if (poptGetArg(poptcon))
	{
		fprintf(stderr, "Extra arguments found\n");
		ExitWithUsage(&poptcon);
	}

	poptFreeContext(poptcon);

	OracleLogon(&oraAllInOne, connectionString, pwdptr, dbconptr);

	switch (programOptions.programAction)
	{
	case ACTION_READ:
		if (programOptions.compressionLevel > 0)
			DownloadFileWithCompression(&oraAllInOne, vDirectory, programOptions.compressionLevel, vRemoteFile, vLocalFile);
		else
			TransferFile(&oraAllInOne, 1, vDirectory, vRemoteFile, vLocalFile);
		break;
	case ACTION_WRITE:
		if (programOptions.compressionLevel > 0)
			UploadFileWithCompression(&oraAllInOne, vDirectory, programOptions.compressionLevel, vRemoteFile, vLocalFile);
		else
			TransferFile(&oraAllInOne, 0, vDirectory, vRemoteFile, vLocalFile);
		break;
	case ACTION_LSDIR:
		LsDir(&oraAllInOne);
		break;
	case ACTION_LS:
		Ls(&oraAllInOne, vDirectory, sqlLs);
		break;
	case ACTION_RM:
		Rm(&oraAllInOne, vDirectory, vRemoteFile);
		break;
	case ACTION_GZIP:
		Compress(&oraAllInOne, vDirectory, programOptions.compressionLevel, vRemoteFile, vLocalFile);
		break;
	case ACTION_GUNZIP:
		Uncompress(&oraAllInOne, vDirectory, vRemoteFile, vLocalFile);
		break;
	default:
		/* TODO: remove when everything is implemented */
		ExitWithError(&oraAllInOne, 5, ERROR_NONE, "Not implemented yet\n");
	}

	ExitWithError(&oraAllInOne, 0, ERROR_NONE, 0);
}
