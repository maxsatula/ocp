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

	struct ORACLESTATEMENT oraStmtCompress = {
#include "compress.text"
,
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

	struct ORACLESTATEMENT oraStmtUncompress = {
#include "uncompress.text"
,
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

	struct ORACLESTATEMENT oraStmtCompress = {
#include "compress_bg.text"
,
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

	struct ORACLESTATEMENT oraStmtUncompress = {
#include "uncompress_bg.text"
,
	       0, oraBindsUncompress, NO_ORACLE_DEFINES };

	PrepareStmtAndBind(oraAllInOne, &oraStmtUncompress);

	if (ExecuteStmt(oraAllInOne))
		ExitWithError(oraAllInOne, 4, ERROR_OCI, "Failed to submit a decompression job\n");

	printf("Submitted a job %s\n", vJobName);
	ReleaseStmt(oraAllInOne);
}
