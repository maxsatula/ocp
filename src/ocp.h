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

#ifndef _OCP_H_
#define _OCP_H_

#define ORA_RAW_BUFFER_SIZE 0x4000
#define ORA_BLOB_BUFFER_SIZE 0x10000

struct ORACLEFILEATTR
{
	sb1 bExists;
	ub8 length;
};

enum HASH_ALGORITHM { HASH_NONE, HASH_MD5, HASH_SHA1 };

void GetOracleFileAttr(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, char* pFileName, struct ORACLEFILEATTR *oraFileAttr);
void TryDirectory(struct ORACLEALLINONE *oraAllInOne, char* pDirectory);

void TransferFile(struct ORACLEALLINONE *oraAllInOne, int readingDirection,
                  char* pDirectory, char* pRemoteFile, char* pLocalFile,
                  int isKeepPartial, int isResume);

void Compress(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, int compressionLevel, int isKeep,
              char* pOriginalFileName, char* pCompressedFileName);
void Uncompress(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, int isKeep,
                char* pOriginalFileName, char* pUncompressedFileName);
void SubmitCompressJob(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, int compressionLevel, int isKeep,
                       char* pOriginalFileName, char* pCompressedFileName);
void SubmitUncompressJob(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, int isKeep,
                         char* pOriginalFileName, char* pUncompressedFileName);

void DownloadFileWithCompression(struct ORACLEALLINONE *oraAllInOne, char* pDirectory,
                                 int compressionLevel, char* pRemoteFile, char* pLocalFile,
                                 int isKeepPartial, int isResume);
void UploadFileWithCompression(struct ORACLEALLINONE *oraAllInOne, char* pDirectory,
                               int compressionLevel, char* pRemoteFile, char* pLocalFile,
                               int isKeepPartial, int isResume);

void LsDir(struct ORACLEALLINONE *oraAllInOne);
void Ls(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, char* patterns, int patternLength, enum HASH_ALGORITHM hashAlgorithm);
void Rm(struct ORACLEALLINONE *oraAllInOne, char* pDirectory, char* pFileName);

void InstallObjects(struct ORACLEALLINONE* oraAllInOne);
void DeinstallObjects(struct ORACLEALLINONE* oraAllInOne);

#endif
