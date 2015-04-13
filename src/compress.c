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
#include <oci.h>
#include "oracle.h"
#include "ocp.h"


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

	SetSessionAction(oraAllInOne, "GZIP");
	PrepareStmtAndBind(oraAllInOne, &oraStmtCompress);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to compress file in oracle directory\n");

	ReleaseStmt(oraAllInOne);
	SetSessionAction(oraAllInOne, 0);
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

	SetSessionAction(oraAllInOne, "GUNZIP");
	PrepareStmtAndBind(oraAllInOne, &oraStmtUncompress);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to uncompress file in oracle directory\n");

	ReleaseStmt(oraAllInOne);	
	SetSessionAction(oraAllInOne, 0);
}

void SubmitCompressJob(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, int compressionLevel,
                       char* pOriginalFileName, char* pCompressedFileName)
{
	ub4 vCompressionLevel;
	char vJobName[ORA_IDENTIFIER_SIZE + 1];

	struct BINDVARIABLE oraBindsCompress[] =
	{
		{ 0, SQLT_STR, ":job_name",            vJobName,            ORA_IDENTIFIER_SIZE + 1   },
		{ 0, SQLT_STR, ":directory",           pDirectory,          ORA_IDENTIFIER_SIZE + 1   },
		{ 0, SQLT_INT, ":compression_level",   &vCompressionLevel,  sizeof(vCompressionLevel) },
		{ 0, SQLT_STR, ":original_filename"  , pOriginalFileName,   MAX_FMT_SIZE              },
		{ 0, SQLT_STR, ":compressed_filename", pCompressedFileName, MAX_FMT_SIZE              }
	};

	struct ORACLESTATEMENT oraStmtCompress = { "\
DECLARE\
	safe_directory_ varchar2(60);\
	safe_compression_level_ varchar2(1);\
	safe_original_filename_ varchar2(512);\
	safe_compressed_filename_ varchar2(512);\
BEGIN\
	safe_directory_ := dbms_assert.enquote_literal(''''||replace(:directory, '''', '''''')||'''');\
	safe_compression_level_ := to_char(:compression_level, 'TM', 'NLS_Numeric_Characters = ''.,''');\
	safe_original_filename_ := dbms_assert.enquote_literal(''''||replace(:original_filename, '''', '''''')||'''');\
	safe_compressed_filename_ := dbms_assert.enquote_literal(''''||replace(:compressed_filename, '''', '''''')||'''');\
\
	:job_name := dbms_scheduler.generate_job_name('OCP_GZIP_');\
	dbms_scheduler.create_job (\
		job_name => :job_name,\
		job_type => 'PLSQL_BLOCK',\
		enabled => TRUE,\
		comments => 'ocp compression job',\
		job_action => '\
DECLARE\
	f_handle UTL_FILE.FILE_TYPE;\
	c_handle BINARY_INTEGER;\
	raw_buffer RAW(32767);\
	blob_buffer BLOB;\
	actual_size NUMBER;\
	pos BINARY_INTEGER;\
	blobsize BINARY_INTEGER; \
BEGIN\
	f_handle := UTL_FILE.FOPEN(' || safe_directory_ || ', ' || safe_original_filename_ || ', ''rb'');\
	DBMS_LOB.CREATETEMPORARY(blob_buffer, TRUE, DBMS_LOB.CALL);\
	' || case when :compression_level > 0 then \
		'c_handle := UTL_COMPRESS.LZ_COMPRESS_OPEN(blob_buffer, ' || safe_compression_level_ || ');'\
	else\
		'c_handle := UTL_COMPRESS.LZ_COMPRESS_OPEN(blob_buffer);'\
	end || '\
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
	f_handle := UTL_FILE.FOPEN(' || safe_directory_ || ', ' || safe_compressed_filename_ || ', ''wb'');\
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
');\
END;",
	       0, oraBindsCompress, sizeof(oraBindsCompress)/sizeof(struct BINDVARIABLE), 0, 0 };

	vCompressionLevel = compressionLevel;

	PrepareStmtAndBind(oraAllInOne, &oraStmtCompress);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to submit a compression job\n");

	printf("Submitted a job %s\n", vJobName);
	ReleaseStmt(oraAllInOne);
}

void SubmitUncompressJob(struct ORACLEALLINONE *oraAllInOne, char* pDirectory,
                         char* pOriginalFileName, char* pUncompressedFileName)
{
	char vJobName[ORA_IDENTIFIER_SIZE + 1];

	struct BINDVARIABLE oraBindsUncompress[] =
	{
		{ 0, SQLT_STR, ":job_name",              vJobName,              ORA_IDENTIFIER_SIZE + 1 },
		{ 0, SQLT_STR, ":directory",             pDirectory,            ORA_IDENTIFIER_SIZE + 1 },
		{ 0, SQLT_STR, ":original_filename"  ,   pOriginalFileName,     MAX_FMT_SIZE            },
		{ 0, SQLT_STR, ":uncompressed_filename", pUncompressedFileName, MAX_FMT_SIZE            }
	};

	struct ORACLESTATEMENT oraStmtUncompress = { "\
DECLARE\
	safe_directory_ varchar2(60);\
	safe_original_filename_ varchar2(512);\
	safe_uncompressed_filename_ varchar2(512);\
BEGIN\
	safe_directory_ := dbms_assert.enquote_literal(''''||replace(:directory, '''', '''''')||'''');\
	safe_original_filename_ := dbms_assert.enquote_literal(''''||replace(:original_filename, '''', '''''')||'''');\
	safe_uncompressed_filename_ := dbms_assert.enquote_literal(''''||replace(:uncompressed_filename, '''', '''''')||'''');\
\
	:job_name := dbms_scheduler.generate_job_name('OCP_GUNZIP_');\
	dbms_scheduler.create_job (\
		job_name => :job_name,\
		job_type => 'PLSQL_BLOCK',\
		enabled => TRUE,\
		comments => 'ocp decompression job',\
		job_action => '\
DECLARE\
	f_handle UTL_FILE.FILE_TYPE;\
	c_handle BINARY_INTEGER;\
	raw_buffer RAW(32767);\
	blob_buffer BLOB; \
BEGIN\
	DBMS_LOB.CREATETEMPORARY(blob_buffer, TRUE, DBMS_LOB.CALL);\
	f_handle := UTL_FILE.FOPEN(' || safe_directory_ || ', ' || safe_original_filename_ || ', ''rb'');\
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
	f_handle := UTL_FILE.FOPEN(' || safe_directory_ || ', ' || safe_uncompressed_filename_ || ', ''wb'');\
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
');\
END;",
	       0, oraBindsUncompress, sizeof(oraBindsUncompress)/sizeof(struct BINDVARIABLE), 0, 0 };

	PrepareStmtAndBind(oraAllInOne, &oraStmtUncompress);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to submit a decompression job\n");

	printf("Submitted a job %s\n", vJobName);
	ReleaseStmt(oraAllInOne);
}
