ocp for Windows
===============

Runtime
-------

Here are dependencies required to run pre-compiled `ocp.exe`

### Oracle Client

Download and unpack Oracle **Instant Client Package - Basic Lite** for Microsoft Windows (32-bit) version 11.2.0.4.0 from http://www.oracle.com/technetwork/topics/winsoft-085727.html
`instantclient-basiclite-nt-11.2.0.4.0.zip` (20,258,449 bytes)

Full Oracle Client (or Server) installations should also work, as well as newer versions (12c). Use 32-bit installations only.

As usual, make sure Oracle Instant Client directory is in `PATH` environment variable. If using full installation, then `%ORACLE_HOME%\bin` should be in `PATH`.

### GnuWin32 Libraries

Download and unpack archives, and put files to a directory with `ocp.exe` or to any directory from `PATH` variable:

| URL                                                        | Archive name       | File(s)       |
|------------------------------------------------------------|--------------------|---------------|
| http://gnuwin32.sourceforge.net/downlinks/popt-bin-zip.php | popt-1.8-1-bin.zip | bin\popt1.dll |
| http://gnuwin32.sourceforge.net/downlinks/popt-dep-zip.php | popt-1.8-1-dep.zip | bin\binlibintl-2.dll, bin\libiconv-2.dll |
| http://gnuwin32.sourceforge.net/downlinks/zlib-bin-zip.php | zlib-1.2.3-bin.zip | bin\zlib1.dll |

### Visual C++ Redistributable Package for Visual Studio 2013  

Download and install `vcredist_x86.exe` from http://www.microsoft.com/en-US/download/details.aspx?id=40784

Build
-----

Here are dependencies required to compile `ocp.exe` from source. Please follow instructions in **Runtime** section above (except **Visual C++ Redistributable Package**, that one is automatically supplied by **Visual Studio**  installation), then proceed with steps below.

### Oracle Client

If using Instant Client, download and unpack to the same directory Oracle **Instant Client Package - SDK** for Microsoft Windows (32-bit) version 11.2.0.4.0 from http://www.oracle.com/technetwork/topics/winsoft-085727.html
`instantclient-sdk-nt-11.2.0.4.0.zip` (1,114,946 bytes)

### GnuWin32 Libraries

Download and unpack archives to `win` subdirectory of ocp source tree:

| URL                                                        | Archive name       |
|------------------------------------------------------------|--------------------|
| http://gnuwin32.sourceforge.net/downlinks/popt-lib-zip.php | popt-1.8-1-lib.zip |
| http://gnuwin32.sourceforge.net/downlinks/zlib-lib-zip.php | zlib-1.2.3-lib.zip |

### Visual Studio Express 2013 for Windows Desktop

Downloand and install from https://www.visualstudio.com/en-us/products/visual-studio-express-vs.aspx

### Run Compilation

Then review `win\build.cmd`, edit if necessary, and execute:
```
cd win
build.cmd
```
