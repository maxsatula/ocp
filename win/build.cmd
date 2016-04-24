rem *** Before running build.cmd:
rem
rem 1. Review and edit include\config.h (update version information) if necessary
rem 2. Adjust the two variables (%INSTANT_CLIENT% and %OCP_ROOT%) if necessary

set INSTANT_CLIENT=c:\instantclient_12_1
set OCP_ROOT=..

set ICINCHOME=%INSTANT_CLIENT%\sdk\include
set ICLIBHOME=%INSTANT_CLIENT%\sdk\lib\msvc
set SRC=%OCP_ROOT%\src
cl -I%ICINCHOME% -I%OCP_ROOT%\yesno -Iinclude -I. -DHAVE_CONFIG_H -D_CRT_SECURE_NO_DEPRECATE -D_DLL -D_MT %SRC%/main.c %SRC%/oracle.c %SRC%/compress.c %SRC%/install.c %SRC%/ls.c %SRC%/lsdir.c %SRC%/orafileattr.c %SRC%/trydir.c %SRC%/rm.c %SRC%/transfer.c %SRC%/transfercompr.c %OCP_ROOT%/yesno/yesno.c /link /LIBPATH:%ICLIBHOME% lib/libpopt.lib lib/zlib.lib oci.lib msvcrt.lib Shlwapi.lib /nod:libc /nod:libcmt /out:ocp.exe
