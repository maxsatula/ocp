ocp for Windows
===============

Runtime
-------

Here are dependencies required to run pre-compiled `ocp.exe`

### Oracle Client

Download and unpack Oracle **Instant Client Package - Basic Lite** for Microsoft Windows (32-bit) version 12.1.0.2.0 from http://www.oracle.com/technetwork/topics/winsoft-085727.html
`instantclient-basiclite-nt-12.1.0.2.0.zip` (33,851,306 bytes)

Full Oracle Client (or Server) installations will also work. Use 32-bit installations only.

As usual, make sure Oracle Instant Client directory is in `PATH` environment variable. If using full installation, then `%ORACLE_HOME%\bin` should be in `PATH`.

### GnuWin32 Libraries

Download and unpack archives, and put files to a directory with `ocp.exe` or to any directory from `PATH` variable:

| URL                                                        | Archive name       | File(s)       |
|------------------------------------------------------------|--------------------|---------------|
| http://gnuwin32.sourceforge.net/downlinks/popt-bin-zip.php | popt-1.8-1-bin.zip | bin\popt1.dll |
| http://gnuwin32.sourceforge.net/downlinks/popt-dep-zip.php | popt-1.8-1-dep.zip | bin\libintl-2.dll, bin\libiconv-2.dll |
| http://gnuwin32.sourceforge.net/downlinks/zlib-bin-zip.php | zlib-1.2.3-bin.zip | bin\zlib1.dll |

### Microsoft Visual C++ 2010 SP1 Redistributable Package (x86)

Download and install `vcredist_x86.exe` from https://www.microsoft.com/en-us/download/details.aspx?id=8328

Build
-----

Here are dependencies required to compile `ocp.exe` from source. Please follow instructions in **Runtime** section above (except **Microsoft Visual C++ 2010 SP1 Redistributable Package (x86)**, that one is automatically supplied by **Microsoft Visual C++ 2010 Express SP1**  installation), then proceed with the steps below.

A version 2010 of Visual C++ (MSVC) was chosen because Oracle Instant Client 12.1.0.2.0 already has **Microsoft Visual C++ 2010 SP1 Redistributable Package (x86)** as its runtime dependency (it requires `MSVCR100.dll` file).
Thus, compiling `ocp` with the same MSVC version as Oracle used for their Instant Client will avoid dependencies on more than one version of `MSVCR*.dll`.

### Oracle Client

If using Instant Client, download and unpack to the same directory Oracle **Instant Client Package - SDK** for Microsoft Windows (32-bit) version 12.1.0.2.0 from http://www.oracle.com/technetwork/topics/winsoft-085727.html
`instantclient-sdk-nt-12.1.0.2.0.zip` (1,951,770 bytes)

### GnuWin32 Libraries

Download and unpack archives to `win` subdirectory of ocp source tree:

| URL                                                        | Archive name       |
|------------------------------------------------------------|--------------------|
| http://gnuwin32.sourceforge.net/downlinks/popt-lib-zip.php | popt-1.8-1-lib.zip |
| http://gnuwin32.sourceforge.net/downlinks/zlib-lib-zip.php | zlib-1.2.3-lib.zip |

### Microsoft Visual C++ 2010 Express SP1

  1. Download and install **Microsoft Visual C++ 2010 Express**,
	 deselect any options during installation, as they are unnecessary.
  2. Download and install **Microsoft Visual Studio 2010 Service Pack 1**

### Run Compilation

Run **Visual Studio Command Prompt (2010)**

Then review `win\build.cmd`, edit if necessary, and execute:
```
cd win
build.cmd
```
