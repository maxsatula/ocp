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
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>
#include <oci.h>
#include "oracle.h"
#include "ocp.h"
#include "progressmeter.h"

void DownloadFileWithCompression(struct ORACLEALLINONE *oraAllInOne, char* pDirectory,
                                 int compressionLevel, char* pRemoteFile, char* pLocalFile,
                                 int isKeepPartial, int isResume)
{
	FILE *fp;
	sword result;
	ub4 vCompressionLevel;
	ub8 vSkipBytes;
	char blobBuffer[ORA_BLOB_BUFFER_SIZE];
	oraub8 vSize;
	int zRet;
	z_stream zStrm;
	unsigned char zOut[ORA_BLOB_BUFFER_SIZE];
	int showProgress;
	int isStdUsed;
	off_t cnt;
	off_t sourceSize;
        struct stat fileStat;

	struct BINDVARIABLE oraBindsDownload[] =
	{
		{ 0, SQLT_STR,  ":directory",         pDirectory,         ORA_IDENTIFIER_SIZE + 1   },
		{ 0, SQLT_INT,  ":compression_level", &vCompressionLevel, sizeof(vCompressionLevel) },
		{ 0, SQLT_STR,  ":filename",          pRemoteFile,        MAX_FMT_SIZE              },
		{ 0, SQLT_BLOB, ":blob",              &oraAllInOne->blob, sizeof(oraAllInOne->blob) },
		{ 0, SQLT_INT,  ":skipbytes",         &vSkipBytes,        sizeof(vSkipBytes)        },
		{ 0 }
	};

	struct ORACLESTATEMENT oraStmtDownload = { "\
DECLARE\
	f_handle UTL_FILE.FILE_TYPE;\
	c_handle BINARY_INTEGER;\
	raw_buffer RAW(32767);\
BEGIN\
	f_handle := UTL_FILE.FOPEN(:directory, :filename, 'rb');\
	if :skipbytes > 0 then \
		declare \
			leftToSkip_ number := :skipbytes; \
			size_ number; \
		begin \
			while leftToSkip_ > 0 loop \
				size_ := least(leftToSkip_, 16384); \
				utl_file.get_raw(f_handle, raw_buffer, size_); \
				leftToSkip_ := leftToSkip_ - size_; \
			end loop; \
		end; \
	end if; \
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
	       0, oraBindsDownload, NO_ORACLE_DEFINES };

	vCompressionLevel = compressionLevel;
	isStdUsed = !strcmp(pLocalFile, "-");
	if (isStdUsed)
	{
		isResume = 0;
		isKeepPartial = 1;
	}

	SetSessionAction(oraAllInOne, "GZIP_AND_DOWNLOAD: GZIP");
	if (OCIDescriptorAlloc(oraAllInOne->envhp, (void**)&oraAllInOne->blob, OCI_DTYPE_LOB, 0, 0))
	{
		ExitWithError(oraAllInOne, 4, ERROR_NONE, "Failed to allocate BLOB\n");
	}

	cnt = 0;
	vSkipBytes = 0;
	if (isResume)
	{
		if (!stat(pLocalFile, &fileStat))
			vSkipBytes = cnt = fileStat.st_size;
		if (!cnt)
			isResume = 0;
	}

	PrepareStmtAndBind(oraAllInOne, &oraStmtDownload);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to compress file in oracle directory\n");

	showProgress = 1;
	if (!isatty(STDOUT_FILENO) || isStdUsed)
		showProgress = 0;
	if (showProgress)
	{
		if (OCILobGetLength2(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob, (oraub8*)&sourceSize))
			ExitWithError(oraAllInOne, 4, ERROR_OCI, "Error getting BLOB length\n");
		start_progress_meter(pRemoteFile, sourceSize, &cnt);
	}

	SetSessionAction(oraAllInOne, "GZIP_AND_DOWNLOAD: DOWNLOAD");
	if (!isStdUsed && (fp = fopen(pLocalFile, isResume ? "ab" : "wb")) == NULL)
	{
		ExitWithError(oraAllInOne, 4, ERROR_OS, "Error opening a local file for writing\n");
	}
	if (isStdUsed)
		fp = stdout;

	zStrm.zalloc = Z_NULL;
	zStrm.zfree = Z_NULL;
	zStrm.opaque = Z_NULL;
	zStrm.avail_in = 0;
	zStrm.next_in = Z_NULL;
	zRet = inflateInit2(&zStrm, 16+MAX_WBITS);
	if (zRet != Z_OK)
	{
		if (!isStdUsed)
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
				if (!isStdUsed)
					fclose(fp);
				ExitWithError(oraAllInOne, 5, ERROR_NONE, "ZLIB inflate failed: %d, size %d\n", zRet, vSize);
			}

			fwrite(zOut, sizeof(unsigned char), ORA_BLOB_BUFFER_SIZE - zStrm.avail_out, fp);
			if (ferror(fp))
			{
				(void)inflateEnd(&zStrm);
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
		}
		while (zStrm.avail_out == 0);

		if (result == OCI_SUCCESS)
			break;
		result = OCILobRead2(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob, &vSize, 0, 1, blobBuffer, ORA_BLOB_BUFFER_SIZE, OCI_NEXT_PIECE, 0, 0, 0, 0);
	}

	if (showProgress)
		stop_progress_meter();
	inflateEnd(&zStrm);
	if (!isStdUsed)
		fclose(fp);

	if (result != OCI_SUCCESS)
	{
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Error reading BLOB\n");
	}

	ReleaseStmt(oraAllInOne);
	SetSessionAction(oraAllInOne, 0);

	OCILobFreeTemporary(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob);

	if (OCIDescriptorFree(oraAllInOne->blob, OCI_DTYPE_LOB))
	{
		ExitWithError(oraAllInOne, 4, ERROR_NONE, "Failed to free BLOB\n");
	}
	oraAllInOne->blob = 0;
}

void UploadFileWithCompression(struct ORACLEALLINONE *oraAllInOne, char* pDirectory,
                               int compressionLevel, char* pRemoteFile, char* pLocalFile,
                               int isKeepPartial, int isResume)
{
	FILE *fp;
	sword result;
	char blobBuffer[ORA_BLOB_BUFFER_SIZE];
	oraub8 vSize;
	char vOpenMode[3];
	ub1 piece;
	int zRet, zFlush;
	z_stream zStrm;
	unsigned char zIn[ORA_BLOB_BUFFER_SIZE];
	int showProgress;
	int isStdUsed;
	off_t cnt;
	off_t sourceSize;
	struct stat fileStat;
	int isError;
	struct ORACLEFILEATTR oracleFileAttr;

	struct BINDVARIABLE oraBindsUpload[] =
	{
		{ 0, SQLT_STR,  ":directory", pDirectory,         ORA_IDENTIFIER_SIZE + 1   },
		{ 0, SQLT_STR,  ":filename",  pRemoteFile,        MAX_FMT_SIZE              },
		{ 0, SQLT_STR,  ":openmode",  vOpenMode,          sizeof(vOpenMode)         },
		{ 0, SQLT_BLOB, ":blob",      &oraAllInOne->blob, sizeof(oraAllInOne->blob) },
		{ 0 }
	};

	struct ORACLESTATEMENT oraStmtUpload = { "\
DECLARE\
	f_handle UTL_FILE.FILE_TYPE;\
	c_handle BINARY_INTEGER;\
	raw_buffer RAW(32767);\
BEGIN\
	c_handle := UTL_COMPRESS.LZ_UNCOMPRESS_OPEN(:blob);\
	f_handle := UTL_FILE.FOPEN(:directory, :filename, :openmode);\
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
	       0, oraBindsUpload, NO_ORACLE_DEFINES };

	isStdUsed = !strcmp(pLocalFile, "-");
	if (isStdUsed)
		isResume = 0;

	SetSessionAction(oraAllInOne, "UPLOAD_AND_GUNZIP: UPLOAD");
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
	if (!isatty(STDOUT_FILENO) || isStdUsed)
		showProgress = 0;
	cnt = 0;
	if (isResume)
	{
		GetOracleFileAttr(oraAllInOne, pDirectory, pRemoteFile, &oracleFileAttr);
		if (oracleFileAttr.bExists)
			cnt = oracleFileAttr.length;
		if (!cnt)
			isResume = 0;
	}

	if (showProgress)
	{
		stat(pLocalFile, &fileStat);
		sourceSize = fileStat.st_size;
		start_progress_meter(pLocalFile, sourceSize, &cnt);
	}


	if (!isStdUsed && (fp = fopen(pLocalFile, "rb")) == NULL)
	{
		ExitWithError(oraAllInOne, 4, ERROR_OS, "Error opening a local file for reading\n");
	}
	if (isStdUsed)
		fp = stdin;

	if (cnt > 0)
	{
		if (fseek(fp, cnt, SEEK_SET))
		{
			fclose(fp);
			ExitWithError(oraAllInOne, 4, ERROR_OS, "Error setting reading position in a local file\n");
		}
	}

	zStrm.zalloc = Z_NULL;
    zStrm.zfree = Z_NULL;
    zStrm.opaque = Z_NULL;

	zRet = deflateInit2(&zStrm, compressionLevel ? compressionLevel : Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16+MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
	if (zRet != Z_OK)
	{
		if (!isStdUsed)
			fclose(fp);
		ExitWithError(oraAllInOne, 5, ERROR_NONE, "ZLIB initialization failed\n");
	}

	while (!feof(fp))
	{
		zStrm.avail_in = fread(zIn, sizeof(unsigned char), sizeof(zIn), fp);
		cnt += zStrm.avail_in;
		if (ferror(fp))
		{
			(void)deflateEnd(&zStrm);
			if (!isStdUsed)
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
				if (!isStdUsed)
					fclose(fp);
				ExitWithError(oraAllInOne, 5, ERROR_NONE, "ZLIB deflate failed: %d, size %d\n", zRet, zStrm.avail_in);
			}

			if (zRet == Z_STREAM_END)
				piece = (piece == OCI_FIRST_PIECE) ? OCI_ONE_PIECE : OCI_LAST_PIECE;

			vSize = (piece == OCI_ONE_PIECE) ? ORA_BLOB_BUFFER_SIZE - zStrm.avail_out : 0;
			if (zStrm.avail_out == ORA_BLOB_BUFFER_SIZE)
				continue;
			result = OCILobWrite2(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob, &vSize, 0, 1, blobBuffer, ORA_BLOB_BUFFER_SIZE - zStrm.avail_out, piece, 0, 0, 0, 0);
			if (result != OCI_NEED_DATA && result)
			{
				(void)deflateEnd(&zStrm);
				if (!isStdUsed)
					fclose(fp);
				ExitWithError(oraAllInOne, 4, ERROR_OCI, "Error writing to BLOB\n");
			}
			if (piece == OCI_FIRST_PIECE)
				piece = OCI_NEXT_PIECE;
		}
		while (zStrm.avail_out == 0);
	}

	if (showProgress)
		stop_progress_meter();
	deflateEnd(&zStrm);
	if (!isStdUsed)
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

	isError = 0;
        strcpy(vOpenMode, isResume ? "ab" : "wb");
	SetSessionAction(oraAllInOne, "UPLOAD_AND_GUNZIP: GUNZIP");
	PrepareStmtAndBind(oraAllInOne, &oraStmtUpload);

	if (ExecuteStmt(oraAllInOne))
	{
		ExitWithError(oraAllInOne, -1, ERROR_OCI, "Failed to decompress file in oracle directory\n");
		isError = 1;
	}

	ReleaseStmt(oraAllInOne);
	SetSessionAction(oraAllInOne, 0);

	OCILobFreeTemporary(oraAllInOne->svchp, oraAllInOne->errhp, oraAllInOne->blob);

	if (OCIDescriptorFree(oraAllInOne->blob, OCI_DTYPE_LOB))
	{
		ExitWithError(oraAllInOne, -1, ERROR_NONE, "Failed to free BLOB\n");
		isError = 1;
	}
	oraAllInOne->blob = 0;

	if (isError)
	{
		if (!isKeepPartial)
			Rm(oraAllInOne, pDirectory, pRemoteFile);
		ExitWithError(oraAllInOne, 4, ERROR_NONE, 0);
	}
}
