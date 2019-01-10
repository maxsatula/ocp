ocp for Windows
===============

Runtime
-------

Here are dependencies required to run pre-compiled `ocp.exe`

### Oracle Client

Download and unpack Oracle **Basic Light Package** for Microsoft Windows (32-bit) version 18.3.0.0.0 from http://www.oracle.com/technetwork/topics/winsoft-085727.html
`instantclient-basiclite-nt-18.3.0.0.0dbru.zip` (3,544,0728 bytes)

Full Oracle Client (or Server) installations will also work. Use 32-bit installations only.

As usual, make sure Oracle Instant Client directory is in `PATH` environment variable. If using full Oracle Client installation, then `%ORACLE_HOME%\bin` should be in `PATH`.

### GnuWin32 Libraries

Download and unpack archives, and put files to a directory with `ocp.exe` or to any directory from `PATH` variable:

| URL                                                        | Archive name       | File(s)       |
|------------------------------------------------------------|--------------------|---------------|
| http://gnuwin32.sourceforge.net/downlinks/popt-bin-zip.php | popt-1.8-1-bin.zip | bin\popt1.dll |
| http://gnuwin32.sourceforge.net/downlinks/popt-dep-zip.php | popt-1.8-1-dep.zip | bin\libintl-2.dll, bin\libiconv-2.dll |
| http://gnuwin32.sourceforge.net/downlinks/zlib-bin-zip.php | zlib-1.2.3-bin.zip | bin\zlib1.dll |

### Visual C++ Redistributable Packages for Visual Studio 2013 (x86)

Download and install `vcredist_x86.exe` from https://www.microsoft.com/en-us/download/details.aspx?id=40784

Build
-----

Here are dependencies required to compile `ocp.exe` from source. Please follow instructions in **Runtime** section above (except **Visual C++ Redistributable Packages for Visual Studio 2013 (x86)**, that one is automatically supplied by **Visual Studio Community 2013**  installation), then proceed with the steps below.

A version 2013 of Visual C++ (MSVC) was chosen because Oracle Instant Client 18.3.0.0.0 already has **Visual C++ Redistributable Packages for Visual Studio 2013 (x86)** as its runtime dependency (it requires `MSVCR120.dll` file).
Thus, compiling `ocp` with the same MSVC version as Oracle used for their Instant Client will avoid dependencies on more than one version of `MSVCR*.dll`.

### Oracle Client

If using Instant Client, download and unpack to the same directory Oracle **SDK Package** for Microsoft Windows (32-bit) version 18.3.0.0.0 from http://www.oracle.com/technetwork/topics/winsoft-085727.html
`instantclient-sdk-nt-18.3.0.0.0dbru.zip` (1,499,306 bytes)

### GnuWin32 Libraries

Download and unpack archives to `win` subdirectory of ocp source tree:

| URL                                                        | Archive name       |
|------------------------------------------------------------|--------------------|
| http://gnuwin32.sourceforge.net/downlinks/popt-lib-zip.php | popt-1.8-1-lib.zip |
| http://gnuwin32.sourceforge.net/downlinks/zlib-lib-zip.php | zlib-1.2.3-lib.zip |

### Visual Studio Community 2013

Download and install **Visual Studio Community 2013**,
deselect any options during installation, as they are unnecessary.

### Run Compilation

Run **VS2013 x86 Native Tools Command Prompt**

Then review `win\build.cmd`, edit if necessary, and execute:
```
cd win
build.cmd
```
