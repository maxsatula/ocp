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
| http://gnuwin32.sourceforge.net/downlinks/popt-dep-zip.php | popt-1.8-1-dep.zip | bin\binlibintl-2.dll, bin\libiconv-2.dll |
| http://gnuwin32.sourceforge.net/downlinks/zlib-bin-zip.php | zlib-1.2.3-bin.zip | bin\zlib1.dll |

### Microsoft Visual C++ 2010 SP1 Redistributable Package (x86)

Download and install `vcredist_x86.exe` from https://www.microsoft.com/en-us/download/details.aspx?id=8328

This dependency has nothing to do with `ocp` itself, it makes Oracle Instant Client 12c work.

### Visual C++ Redistributable for Visual Studio 2015

Download and install `vc_redist.x86.exe` from https://www.microsoft.com/en-us/download/details.aspx?id=48145

Build
-----

Here are dependencies required to compile `ocp.exe` from source. Please follow instructions in **Runtime** section above (except **Visual C++ Redistributable for Visual Studio 2015**, that one is automatically supplied by **Microsoft Visual C++ Build Tools 2015**  installation), then proceed with the steps below.

### Oracle Client

If using Instant Client, download and unpack to the same directory Oracle **Instant Client Package - SDK** for Microsoft Windows (32-bit) version 12.1.0.2.0 from http://www.oracle.com/technetwork/topics/winsoft-085727.html
`instantclient-sdk-nt-12.1.0.2.0.zip` (1,951,770 bytes)

### GnuWin32 Libraries

Download and unpack archives to `win` subdirectory of ocp source tree:

| URL                                                        | Archive name       |
|------------------------------------------------------------|--------------------|
| http://gnuwin32.sourceforge.net/downlinks/popt-lib-zip.php | popt-1.8-1-lib.zip |
| http://gnuwin32.sourceforge.net/downlinks/zlib-lib-zip.php | zlib-1.2.3-lib.zip |

### Microsoft .NET Framework 4.6.1

This is a requirement for successful installation of **Microsoft Visual C++ Build Tools 2015**, nothing to do with `ocp` itself.

Skip this step if Windows 10, do not skip if installation of **Microsoft Visual C++ Build Tools 2015** aborts because of missing .NET Framework.

Download and install from https://www.microsoft.com/en-us/download/details.aspx?id=49982

### Microsoft Visual C++ Build Tools 2015

Downloand and install from https://www.visualstudio.com/downloads/download-visual-studio-vs

Installation selection of *Typical*.

### Run Compilation

Run **VS2015 x86 Native Tools Command Prompt**

Then review `win\build.cmd`, edit if necessary, and execute:
```
cd win
build.cmd
```
