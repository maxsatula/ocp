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
#include <oci.h>
#include "oracle.h"
#ifndef _WIN32
# include "longopsmeter.h"
#endif
#include "ocp.h"


void Compress(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, int compressionLevel, int isKeep,
              char* pOriginalFileName, char* pCompressedFileName)
{
	ub4 vCompressionLevel;
	sword ociResult;
#ifndef _WIN32
	int showProgress;
#endif

	struct BINDVARIABLE oraBindsCompress[] =
	{
		{ 0, SQLT_STR, ":directory",           pDirectory,          ORA_IDENTIFIER_SIZE + 1   },
		{ 0, SQLT_INT, ":compression_level",   &vCompressionLevel,  sizeof(vCompressionLevel) },
		{ 0, SQLT_INT, ":keep",                &isKeep,             sizeof(isKeep)            },
		{ 0, SQLT_STR, ":original_filename"  , pOriginalFileName,   MAX_FMT_SIZE              },
		{ 0, SQLT_STR, ":compressed_filename", pCompressedFileName, MAX_FMT_SIZE              },
		{ 0 }
	};

	struct ORACLESTATEMENT oraStmtCompress = { "\
DECLARE\
	f_handle UTL_FILE.FILE_TYPE;\
	c_handle BINARY_INTEGER;\
	raw_buffer RAW(32767);\
	blob_buffer BLOB;\
	actual_size NUMBER;\
	pos BINARY_INTEGER;\
	blobsize BINARY_INTEGER;\
	rindex BINARY_INTEGER;\
	slno BINARY_INTEGER;\
	target NUMBER;\
	exists_ BOOLEAN;\
	file_length NUMBER;\
	blocksize NUMBER; \
BEGIN\
	utl_file.fgetattr(:directory, :original_filename, exists_, file_length, blocksize);\
	rindex := dbms_application_info.set_session_longops_nohint;\
	select object_id\
	  into target\
	  from all_objects\
	 where object_type = 'DIRECTORY'\
	       and object_name = :directory;\
	f_handle := UTL_FILE.FOPEN(:directory, :original_filename, 'rb');\
	pos := 0;\
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
			pos := pos + utl_raw.length(raw_buffer);\
			dbms_application_info.set_session_longops(rindex, slno, 'GZIP', target, 0, pos, file_length, :directory || ':' || :original_filename, 'bytes');\
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
	rindex := dbms_application_info.set_session_longops_nohint;\
	WHILE pos < blobsize LOOP\
		actual_size := LEAST(blobsize - pos, 16384);\
		DBMS_LOB.READ(blob_buffer, actual_size, pos + 1, raw_buffer);\
		UTL_FILE.PUT_RAW(f_handle, raw_buffer);\
		pos := pos + actual_size;\
		dbms_application_info.set_session_longops(rindex, slno, 'WRITE LOB TO FILE', target, 0, pos, blobsize, :directory || ':' || :original_filename, 'bytes');\
	END LOOP;\
	DBMS_LOB.FREETEMPORARY(blob_buffer);\
	UTL_FILE.FCLOSE(f_handle);\
	IF :keep = 0 THEN\
		UTL_FILE.FREMOVE(:directory, :original_filename);\
	END IF;\
END;\
",
		0, oraBindsCompress, NO_ORACLE_DEFINES };

	vCompressionLevel = compressionLevel;

	SetSessionAction(oraAllInOne, "GZIP");

#ifndef _WIN32
	showProgress = 1;
	if (!isatty(STDOUT_FILENO))
		showProgress = 0;

	if (showProgress)
		start_longops_meter(oraAllInOne, 0, 1);
#endif
	PrepareStmtAndBind(oraAllInOne, &oraStmtCompress);
	ociResult = ExecuteStmt(oraAllInOne);
#ifndef _WIN32
	if (showProgress)
		stop_longops_meter();
#endif

	if (ociResult)
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to compress file in oracle directory\n");

	ReleaseStmt(oraAllInOne);
	SetSessionAction(oraAllInOne, 0);
}

void Uncompress(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, int isKeep,
                char* pOriginalFileName, char* pUncompressedFileName)
{
	sword ociResult;
#ifndef _WIN32
	int showProgress;
#endif

	struct BINDVARIABLE oraBindsUncompress[] =
	{
		{ 0, SQLT_STR, ":directory",             pDirectory,            ORA_IDENTIFIER_SIZE + 1 },
		{ 0, SQLT_INT, ":keep",                  &isKeep,               sizeof(isKeep)          },
		{ 0, SQLT_STR, ":original_filename"  ,   pOriginalFileName,     MAX_FMT_SIZE            },
		{ 0, SQLT_STR, ":uncompressed_filename", pUncompressedFileName, MAX_FMT_SIZE            },
		{ 0 }
	};

	struct ORACLESTATEMENT oraStmtUncompress = { "\
DECLARE\
	f_handle UTL_FILE.FILE_TYPE;\
	c_handle BINARY_INTEGER;\
	raw_buffer RAW(32767);\
	blob_buffer BLOB;\
	pos BINARY_INTEGER;\
	rindex BINARY_INTEGER;\
	slno BINARY_INTEGER;\
	target NUMBER;\
	exists_ BOOLEAN;\
	file_length NUMBER;\
	blocksize NUMBER; \
BEGIN\
	utl_file.fgetattr(:directory, :original_filename, exists_, file_length, blocksize);\
	rindex := dbms_application_info.set_session_longops_nohint;\
	select object_id\
	  into target\
	  from all_objects\
	 where object_type = 'DIRECTORY'\
	       and object_name = :directory;\
	DBMS_LOB.CREATETEMPORARY(blob_buffer, TRUE, DBMS_LOB.CALL);\
	f_handle := UTL_FILE.FOPEN(:directory, :original_filename, 'rb');\
	pos := 0;\
	LOOP\
		BEGIN\
			UTL_FILE.GET_RAW(f_handle, raw_buffer, 16384);\
			DBMS_LOB.WRITEAPPEND(blob_buffer, UTL_RAW.LENGTH(raw_buffer), raw_buffer);\
			pos := pos + utl_raw.length(raw_buffer);\
			dbms_application_info.set_session_longops(rindex, slno, 'READ LOB FROM FILE', target, 0, pos, file_length, :directory || ':' || :original_filename, 'bytes');\
		EXCEPTION\
			WHEN NO_DATA_FOUND THEN\
				EXIT;\
		END;\
	END LOOP;\
	UTL_FILE.FCLOSE(f_handle);\
\
	rindex := dbms_application_info.set_session_longops_nohint;\
	c_handle := UTL_COMPRESS.LZ_UNCOMPRESS_OPEN(blob_buffer);\
	f_handle := UTL_FILE.FOPEN(:directory, :uncompressed_filename, 'wb');\
	pos := 0;\
	LOOP\
		BEGIN\
			UTL_COMPRESS.LZ_UNCOMPRESS_EXTRACT(c_handle, raw_buffer);\
			UTL_FILE.PUT_RAW(f_handle, raw_buffer);\
			pos := pos + utl_raw.length(raw_buffer);\
			dbms_application_info.set_session_longops(rindex, slno, 'GUNZIP', target, 0, pos, file_length, :directory || ':' || :original_filename, 'bytes');\
		EXCEPTION\
			WHEN NO_DATA_FOUND THEN\
				EXIT;\
		END;\
	END LOOP;\
	UTL_FILE.FCLOSE(f_handle);\
	UTL_COMPRESS.LZ_UNCOMPRESS_CLOSE(c_handle);\
	DBMS_LOB.FREETEMPORARY(blob_buffer);\
	IF :keep = 0 THEN\
		UTL_FILE.FREMOVE(:directory, :original_filename);\
	END IF;\
END;\
",
		0, oraBindsUncompress, NO_ORACLE_DEFINES };

	SetSessionAction(oraAllInOne, "GUNZIP");

#ifndef _WIN32
	showProgress = 1;
	if (!isatty(STDOUT_FILENO))
		showProgress = 0;

	if (showProgress)
		start_longops_meter(oraAllInOne, 0, 1);
#endif
	PrepareStmtAndBind(oraAllInOne, &oraStmtUncompress);
	ociResult = ExecuteStmt(oraAllInOne);
#ifndef _WIN32
	if (showProgress)
		stop_longops_meter();
#endif

	if (ociResult)
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to uncompress file in oracle directory\n");

	ReleaseStmt(oraAllInOne);	
	SetSessionAction(oraAllInOne, 0);
}

void SubmitCompressJob(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, int compressionLevel, int isKeep,
                       char* pOriginalFileName, char* pCompressedFileName)
{
	ub4 vCompressionLevel;
	char vJobName[ORA_IDENTIFIER_SIZE + 1];

	struct BINDVARIABLE oraBindsCompress[] =
	{
		{ 0, SQLT_STR, ":job_name",            vJobName,            ORA_IDENTIFIER_SIZE + 1   },
		{ 0, SQLT_STR, ":directory",           pDirectory,          ORA_IDENTIFIER_SIZE + 1   },
		{ 0, SQLT_INT, ":compression_level",   &vCompressionLevel,  sizeof(vCompressionLevel) },
		{ 0, SQLT_INT, ":keep",                &isKeep,             sizeof(isKeep)            },
		{ 0, SQLT_STR, ":original_filename",   pOriginalFileName,   MAX_FMT_SIZE              },
		{ 0, SQLT_STR, ":compressed_filename", pCompressedFileName, MAX_FMT_SIZE              },
		{ 0 }
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
	blobsize BINARY_INTEGER;\
	rindex BINARY_INTEGER;\
	slno BINARY_INTEGER;\
	target NUMBER;\
	exists_ BOOLEAN;\
	file_length NUMBER;\
	blocksize NUMBER; \
BEGIN\
	utl_file.fgetattr(' || safe_directory_ || ', ' || safe_original_filename_ || ', exists_, file_length, blocksize);\
	rindex := dbms_application_info.set_session_longops_nohint;\
	select object_id\
	  into target\
	  from all_objects\
	 where object_type = ''DIRECTORY''\
	       and object_name = ' || safe_directory_ || ';\
	f_handle := UTL_FILE.FOPEN(' || safe_directory_ || ', ' || safe_original_filename_ || ', ''rb'');\
	pos := 0;\
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
			pos := pos + utl_raw.length(raw_buffer);\
			dbms_application_info.set_session_longops(rindex, slno, ''GZIP'', target, 0, pos, file_length, ' || safe_directory_ || ' || '':'' || ' || safe_original_filename_ || ', ''bytes'');\
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
	rindex := dbms_application_info.set_session_longops_nohint;\
	WHILE pos < blobsize LOOP\
		actual_size := LEAST(blobsize - pos, 16384);\
		DBMS_LOB.READ(blob_buffer, actual_size, pos + 1, raw_buffer);\
		UTL_FILE.PUT_RAW(f_handle, raw_buffer);\
		pos := pos + actual_size;\
		dbms_application_info.set_session_longops(rindex, slno, ''WRITE LOB TO FILE'', target, 0, pos, blobsize, ' || safe_directory_ || ' || '':'' || ' || safe_original_filename_ || ', ''bytes'');\
	END LOOP;\
	DBMS_LOB.FREETEMPORARY(blob_buffer);\
	UTL_FILE.FCLOSE(f_handle);\
	' || case when :keep = 0 then \
		'UTL_FILE.FREMOVE(' || safe_directory_ || ', ' || safe_original_filename_ || ');'\
	end || '\
END;\
');\
END;",
	       0, oraBindsCompress, NO_ORACLE_DEFINES };

	vCompressionLevel = compressionLevel;

	PrepareStmtAndBind(oraAllInOne, &oraStmtCompress);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to submit a compression job\n");

	printf("Submitted a job %s\n", vJobName);
	ReleaseStmt(oraAllInOne);
}

void SubmitUncompressJob(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, int isKeep,
                         char* pOriginalFileName, char* pUncompressedFileName)
{
	char vJobName[ORA_IDENTIFIER_SIZE + 1];

	struct BINDVARIABLE oraBindsUncompress[] =
	{
		{ 0, SQLT_STR, ":job_name",              vJobName,              ORA_IDENTIFIER_SIZE + 1 },
		{ 0, SQLT_STR, ":directory",             pDirectory,            ORA_IDENTIFIER_SIZE + 1 },
		{ 0, SQLT_INT, ":keep",                  &isKeep,               sizeof(isKeep)          },
		{ 0, SQLT_STR, ":original_filename"  ,   pOriginalFileName,     MAX_FMT_SIZE            },
		{ 0, SQLT_STR, ":uncompressed_filename", pUncompressedFileName, MAX_FMT_SIZE            },
		{ 0 }
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
	blob_buffer BLOB;\
	pos BINARY_INTEGER;\
	rindex BINARY_INTEGER;\
	slno BINARY_INTEGER;\
	target NUMBER;\
	exists_ BOOLEAN;\
	file_length NUMBER;\
	blocksize NUMBER; \
BEGIN\
	utl_file.fgetattr(' || safe_directory_ || ', ' || safe_original_filename_ || ', exists_, file_length, blocksize);\
	rindex := dbms_application_info.set_session_longops_nohint;\
	select object_id\
	  into target\
	  from all_objects\
	 where object_type = ''DIRECTORY''\
	       and object_name = ' || safe_directory_ || ';\
	DBMS_LOB.CREATETEMPORARY(blob_buffer, TRUE, DBMS_LOB.CALL);\
	f_handle := UTL_FILE.FOPEN(' || safe_directory_ || ', ' || safe_original_filename_ || ', ''rb'');\
	pos := 0;\
	LOOP\
		BEGIN\
			UTL_FILE.GET_RAW(f_handle, raw_buffer, 16384);\
			DBMS_LOB.WRITEAPPEND(blob_buffer, UTL_RAW.LENGTH(raw_buffer), raw_buffer);\
			pos := pos + utl_raw.length(raw_buffer);\
			dbms_application_info.set_session_longops(rindex, slno, ''READ LOB FROM FILE'', target, 0, pos, file_length, ' || safe_directory_ || ' || '':'' || ' || safe_original_filename_ || ', ''bytes'');\
		EXCEPTION\
			WHEN NO_DATA_FOUND THEN\
				EXIT;\
		END;\
	END LOOP;\
	UTL_FILE.FCLOSE(f_handle);\
\
	rindex := dbms_application_info.set_session_longops_nohint;\
	c_handle := UTL_COMPRESS.LZ_UNCOMPRESS_OPEN(blob_buffer);\
	f_handle := UTL_FILE.FOPEN(' || safe_directory_ || ', ' || safe_uncompressed_filename_ || ', ''wb'');\
	pos := 0;\
	LOOP\
		BEGIN\
			UTL_COMPRESS.LZ_UNCOMPRESS_EXTRACT(c_handle, raw_buffer);\
			UTL_FILE.PUT_RAW(f_handle, raw_buffer);\
			pos := pos + utl_raw.length(raw_buffer);\
			dbms_application_info.set_session_longops(rindex, slno, ''GUNZIP'', target, 0, pos, file_length, ' || safe_directory_ || ' || '':'' || ' || safe_original_filename_ || ', ''bytes'');\
		EXCEPTION\
			WHEN NO_DATA_FOUND THEN\
				EXIT;\
		END;\
	END LOOP;\
	UTL_FILE.FCLOSE(f_handle);\
	UTL_COMPRESS.LZ_UNCOMPRESS_CLOSE(c_handle);\
	DBMS_LOB.FREETEMPORARY(blob_buffer);\
	' || case when :keep = 0 then \
		'UTL_FILE.FREMOVE(' || safe_directory_ || ', ' || safe_original_filename_ || ');'\
	end || '\
END;\
');\
END;",
	       0, oraBindsUncompress, NO_ORACLE_DEFINES };

	PrepareStmtAndBind(oraAllInOne, &oraStmtUncompress);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to submit a decompression job\n");

	printf("Submitted a job %s\n", vJobName);
	ReleaseStmt(oraAllInOne);
}
