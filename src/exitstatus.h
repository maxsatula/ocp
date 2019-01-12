/*****************************************************************************
Copyright (C) 2018  Max Satula

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

#ifndef _EXITSTATUS_H_
#define _EXITSTATUS_H_

/* Program return values */

#define RET_DONOTEXIT -1  /* Print error message but do not exit program */
#define RET_OK         0  /* Success */
#define RET_USAGE      1  /* Error in command line arguments */
#define RET_OCIINIT    2  /* Error in OCI object initialization */
#define RET_LOGIN      3  /* Failed to login to a database */
#define RET_FS         4  /* Local filesystem related error */
#define RET_ORA        5  /* Oracle error */
#define RET_ZLIB       6  /* LIBZ error */
#define RET_LS         7  /* Error listing files in Oracle directory */

#endif
