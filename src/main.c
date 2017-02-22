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
#else
# include <shlwapi.h>
#endif
#include <popt.h>
#include "oracle.h"
#include "ocp.h"
#include "yesno.h"

enum PROGRAM_ACTION { ACTION_READ, ACTION_WRITE, ACTION_LSDIR, ACTION_LS, ACTION_RM,
	ACTION_GZIP, ACTION_GUNZIP, ACTION_INSTALL, ACTION_DEINSTALL };

enum TRANSFER_MODE { TRANSFER_MODE_INTERACTIVE, TRANSFER_MODE_OVERWRITE, TRANSFER_MODE_FAIL, TRANSFER_MODE_RESUME };

struct PROGRAM_OPTIONS
{
	enum PROGRAM_ACTION programAction;
	char* lsDirectoryName;
	int compressionLevel;
	int isBackground;
	int isKeepPartial;
	int isKeepOriginal;
	enum TRANSFER_MODE transferMode;
	const char* connectionString;
	ub4 adminMode;
	enum HASH_ALGORITHM hashAlgorithm;

	int isStdUsed;
	int numberOfOracleSessions;
};

void ExitWithUsage(poptContext* poptcon)
{
#ifndef _WIN32
	poptPrintUsage(*poptcon, stderr, 0);
#else
	fprintf(stderr, "Try to run with --help or --usage option\n");
#endif
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

void ConfirmOverwrite(struct ORACLEALLINONE *oraAllInOne, struct PROGRAM_OPTIONS *programOptions, const char *fileName)
{
	switch (programOptions->transferMode)
	{
	case TRANSFER_MODE_FAIL:
		ExitWithError(oraAllInOne, 1, ERROR_NONE, "File already exists on destination\n");
		break;
	case TRANSFER_MODE_INTERACTIVE:
		/* TODO: if !isatty then just fail w/o asking? */
		fprintf (stderr, "%s: overwrite %s? ",
		         PACKAGE, fileName);
		if (!yesno())
			ExitWithError(oraAllInOne, 0, ERROR_NONE, 0);
		break;
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
	char* filePatternPtr;
	char filePatterns[1000];
	struct stat fileStat;
	struct ORACLEALLINONE oraAllInOne = { 0, 0, 0, 0 };
	struct PROGRAM_OPTIONS programOptions;
	struct ORACLEFILEATTR oracleFileAttr;
	poptContext poptcon;
	int rc;
#ifdef _WIN32
	DWORD mode;
	char passBuffer[50];
#endif

	struct poptOption transferModeOptions[] =
	{
		{ "interactive", 'i', POPT_ARG_VAL, &programOptions.transferMode, TRANSFER_MODE_INTERACTIVE, "prompt before overwrite (overrides previous -f -c options)" },
		{ "force",       'f', POPT_ARG_VAL, &programOptions.transferMode, TRANSFER_MODE_OVERWRITE, "force overwrite an existing file (overrides previous -i -c options)" },
		{ "keep-partial", '\0', POPT_ARG_VAL, &programOptions.isKeepPartial, 1, "if an error occurred, do not delete partially transferred file" },
		{ "continue", 'c', POPT_ARG_VAL, &programOptions.transferMode, TRANSFER_MODE_RESUME, "resume transfer (implies --keep-partial) (overrides previous -f -i options)" },
		POPT_TABLEEND
	};

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
		{ "background", 'b', POPT_ARG_VAL, &programOptions.isBackground, 1, "Submit an Oracle Scheduler job and exit immediately" },
		{ "keep", 'k', POPT_ARG_VAL, &programOptions.isKeepOriginal, 1, "keep (don't delete) input file" },
		POPT_TABLEEND
	};

	struct poptOption lsOptions[] =
	{
		{ "md5", '\0', POPT_ARG_VAL, &programOptions.hashAlgorithm, HASH_MD5, "Calculate MD5 on listed files" },
		{ "sha1", '\0', POPT_ARG_VAL, &programOptions.hashAlgorithm, HASH_SHA1, "Calculate SHA1 on listed files" },
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
		{ "sysdba", '\0', POPT_ARG_VAL, &programOptions.adminMode, OCI_SYSDBA, "Connect as SYSDBA" },
		{ "sysoper", '\0', POPT_ARG_VAL, &programOptions.adminMode, OCI_SYSOPER, "Connect as SYSOPER" },
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, transferModeOptions, 0, "Transfer options:" },
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, compressionOptions, 0, "Compression options:" },
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, lsOptions, 0, "File list options:" },
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, objOptions, 0, "Database objects for --ls support:" },
		POPT_AUTOHELP
		POPT_TABLEEND
	};

	programOptions.programAction = ACTION_READ;
	programOptions.lsDirectoryName = 0;
	programOptions.compressionLevel = 0;
	programOptions.isBackground = 0;
	programOptions.isKeepPartial = 0;
	programOptions.isKeepOriginal = 0;
	programOptions.transferMode = TRANSFER_MODE_FAIL;
	programOptions.connectionString = 0;
	programOptions.isStdUsed = 0;
	programOptions.numberOfOracleSessions = 1;
	programOptions.adminMode = OCI_DEFAULT;
	programOptions.hashAlgorithm = 0;

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

	if (programOptions.isKeepOriginal && programOptions.programAction != ACTION_GZIP && programOptions.programAction != ACTION_GUNZIP)
	{
		fprintf(stderr, "--keep can only be specified for gzip/gunzip mode\n");
		ExitWithUsage(&poptcon);
	}

	if (programOptions.hashAlgorithm && programOptions.programAction != ACTION_LS)
	{
		fprintf(stderr, "Hashing option can only be specified for list mode\n");
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

	dbconptr = strchr(connectionString, '@');
	if (dbconptr)
		*dbconptr++ = '\0';
	else
	{
		fprintf(stderr, "Invalid connection string format\n");
		ExitWithUsage(&poptcon);
	}
	pwdptr = strchr(connectionString, '/');
	if (pwdptr)
		*pwdptr++ = '\0';
	if (programOptions.transferMode == TRANSFER_MODE_RESUME)
		programOptions.isKeepPartial = 1;

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

		if (!strcmp(vLocalFile, "-"))
			programOptions.isStdUsed = 1;

		if (programOptions.programAction == ACTION_READ
		    && !programOptions.isStdUsed
		    && (stat(vLocalFile, &fileStat) == 0)
		    && fileStat.st_mode & S_IFDIR)
		{
			if (strlen(vLocalFile) + 1 + strlen(vRemoteFile) >= sizeof(vLocalFile))
			{
				fprintf(stderr, "Local path is too long\n");
				ExitWithUsage(&poptcon);
			}
			strcat(vLocalFile, "/");
			strcat(vLocalFile, vRemoteFile);
		}

		if (programOptions.programAction == ACTION_WRITE
		    && !programOptions.isStdUsed
		    && (strlen(vRemoteFile) == 0))
		{
#ifndef _WIN32
			strcpy(vRemoteFile, basename(vLocalFile));
#else
			strcpy(vRemoteFile, PathFindFileName(vLocalFile));
#endif
		}

		if (programOptions.compressionLevel > 0)
			programOptions.numberOfOracleSessions = 2;
		break;
	case ACTION_LS:
		strncpy(vDirectory, programOptions.lsDirectoryName, sizeof(vDirectory));	
		if (vDirectory[sizeof(vDirectory) - 1])
		{
			fprintf(stderr, "Oracle Directory name is too long\n");
			ExitWithUsage(&poptcon);
		}

		filePatterns[0] = '\0';
		filePatternPtr = filePatterns;
		while (poptPeekArg(poptcon))
		{
			if (filePatternPtr - filePatterns + strlen(poptPeekArg(poptcon)) + 1 >= sizeof(filePatterns))
			{
				fprintf(stderr, "File list is too long\n");
				ExitWithUsage(&poptcon);
			}
			strcpy(filePatternPtr, poptGetArg(poptcon));
			filePatternPtr += strlen(filePatternPtr) + 1;
		}
		break;
	case ACTION_RM:
		SplitToDirectoryAndFileName(&poptcon, vDirectory, vRemoteFile);
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
		programOptions.numberOfOracleSessions = 2;
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
		programOptions.numberOfOracleSessions = 2;
		break;
	}

	if (poptGetArg(poptcon))
	{
		fprintf(stderr, "Extra arguments found\n");
		ExitWithUsage(&poptcon);
	}

	poptFreeContext(poptcon);

#ifndef _WIN32
	if (!pwdptr)
		pwdptr = getpass("Password: ");
#else
	mode = 0;
	if (!pwdptr)
	{
		HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
		GetConsoleMode(hStdin, &mode);
		SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
		fprintf(stderr, "Password: ");
		fgets(passBuffer, 50, stdin);
		SetConsoleMode(hStdin, mode);
		fprintf(stderr, "\n");
		if (strlen(passBuffer) > 0 && passBuffer[strlen(passBuffer) - 1] == '\n')
			passBuffer[strlen(passBuffer) - 1] = '\0';
		pwdptr = passBuffer;
	}
#endif
	if (!pwdptr)
		ExitWithError(&oraAllInOne, 1, ERROR_OS, 0);

	OracleLogon(&oraAllInOne, connectionString, pwdptr, dbconptr, programOptions.adminMode, PACKAGE, programOptions.numberOfOracleSessions);

	switch (programOptions.programAction)
	{
	case ACTION_READ:
		TryDirectory(&oraAllInOne, vDirectory);
#ifndef _WIN32
		if (!programOptions.isStdUsed && access(vLocalFile, F_OK) != -1)
#else
		if (!programOptions.isStdUsed && PathFileExists(vLocalFile))
#endif
			ConfirmOverwrite(&oraAllInOne, &programOptions, vLocalFile);
		if (programOptions.compressionLevel > 0)
			DownloadFileWithCompression(&oraAllInOne, vDirectory, programOptions.compressionLevel, vRemoteFile, vLocalFile, programOptions.isKeepPartial, programOptions.transferMode == TRANSFER_MODE_RESUME);
		else
			TransferFile(&oraAllInOne, 1, vDirectory, vRemoteFile, vLocalFile, programOptions.isKeepPartial, programOptions.transferMode == TRANSFER_MODE_RESUME);
		break;
	case ACTION_WRITE:
		TryDirectory(&oraAllInOne, vDirectory);
		GetOracleFileAttr(&oraAllInOne, vDirectory, vRemoteFile, &oracleFileAttr);
		if (oracleFileAttr.bExists)
			ConfirmOverwrite(&oraAllInOne, &programOptions, vRemoteFile);
		if (programOptions.compressionLevel > 0)
			UploadFileWithCompression(&oraAllInOne, vDirectory, programOptions.compressionLevel, vRemoteFile, vLocalFile, programOptions.isKeepPartial, programOptions.transferMode == TRANSFER_MODE_RESUME);
		else
			TransferFile(&oraAllInOne, 0, vDirectory, vRemoteFile, vLocalFile, programOptions.isKeepPartial, programOptions.transferMode == TRANSFER_MODE_RESUME);
		break;
	case ACTION_LSDIR:
		LsDir(&oraAllInOne);
		break;
	case ACTION_LS:
		TryDirectory(&oraAllInOne, vDirectory);
		Ls(&oraAllInOne, vDirectory, filePatterns, filePatternPtr - filePatterns, programOptions.hashAlgorithm);
		break;
	case ACTION_RM:
		TryDirectory(&oraAllInOne, vDirectory);
		Rm(&oraAllInOne, vDirectory, vRemoteFile);
		break;
	case ACTION_GZIP:
		TryDirectory(&oraAllInOne, vDirectory);
		GetOracleFileAttr(&oraAllInOne, vDirectory, vLocalFile, &oracleFileAttr);
		if (oracleFileAttr.bExists)
			ConfirmOverwrite(&oraAllInOne, &programOptions, vLocalFile);
		if (programOptions.isBackground)
			SubmitCompressJob(&oraAllInOne, vDirectory, programOptions.compressionLevel, programOptions.isKeepOriginal, vRemoteFile, vLocalFile);
		else
			Compress(&oraAllInOne, vDirectory, programOptions.compressionLevel, programOptions.isKeepOriginal, vRemoteFile, vLocalFile);
		break;
	case ACTION_GUNZIP:
		TryDirectory(&oraAllInOne, vDirectory);
		GetOracleFileAttr(&oraAllInOne, vDirectory, vLocalFile, &oracleFileAttr);
		if (oracleFileAttr.bExists)
			ConfirmOverwrite(&oraAllInOne, &programOptions, vLocalFile);
		if (programOptions.isBackground)
			SubmitUncompressJob(&oraAllInOne, vDirectory, programOptions.isKeepOriginal, vRemoteFile, vLocalFile);
		else
			Uncompress(&oraAllInOne, vDirectory, programOptions.isKeepOriginal, vRemoteFile, vLocalFile);
		break;
	case ACTION_INSTALL:
		InstallObjects(&oraAllInOne);
		break;
	case ACTION_DEINSTALL:
		DeinstallObjects(&oraAllInOne);
		break;
	}

	ExitWithError(&oraAllInOne, 0, ERROR_NONE, 0);
}
