ocp for Windows
===============

Runtime
-------

Download Oracle **Instant Client Package - Basic Lite** for Microsoft Windows (32-bit) version 11.2.0.4.0 from http://www.oracle.com/technetwork/topics/winsoft-085727.html
`instantclient-basiclite-nt-11.2.0.4.0.zip` (20,258,449 bytes)

Download and unpack archives, and put files to a directory with `ocp.exe` or to any directory from `PATH` variable:

| URL                                                        | Archive name       | File(s)       |
|------------------------------------------------------------|--------------------|---------------|
| http://gnuwin32.sourceforge.net/downlinks/popt-bin-zip.php | popt-1.8-1-bin.zip | bin\popt1.dll |
| http://gnuwin32.sourceforge.net/downlinks/popt-dep-zip.php | popt-1.8-1-dep.zip | bin\binlibintl-2.dll, bin\libiconv-2.dll |
| http://gnuwin32.sourceforge.net/downlinks/zlib-bin-zip.php | zlib-1.2.3-bin.zip | bin\zlib1.dll |

Build
-----

Download Oracle **Instant Client Package - SDK** for Microsoft Windows (32-bit) version 11.2.0.4.0 from http://www.oracle.com/technetwork/topics/winsoft-085727.html
`instantclient-sdk-nt-11.2.0.4.0.zip` (1,114,946 bytes)

Download and unpack archives to `win` subdirectory of ocp source tree

| URL                                                        | Archive name       |
|------------------------------------------------------------|--------------------|
| http://gnuwin32.sourceforge.net/downlinks/popt-lib-zip.php | popt-1.8-1-lib.zip |
| http://gnuwin32.sourceforge.net/downlinks/zlib-lib-zip.php | zlib-1.2.3-lib.zip |

Then review `win\build.cmd`, edit if necessary, and execute
```
cd win
build.cmd
```