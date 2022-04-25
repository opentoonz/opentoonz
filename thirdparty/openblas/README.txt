
This library was apparently intended for UNIX architectures, meaning that most of the trouble
is for Windows systems. The other problem is the CPU architecture we'll compile against.


INSTALLATION


WINDOWS 32/64

We've managed to compile a STATIC library (.lib) in both 32 and 64 bit. We didn't manage to
build a dynamic library (lapack errors), but there is no need.

The built version is WITHOUT parallel support - Toonz already deals internally with thread
management, so we'd have deactivated multithreading anyways.

As a side note, the problem with threads is apparently related to missing symbols, probably
due to the absence of pthread implementation in the Windows system. We've not investigated
further.


So, follow this procedure:

1. Download both MINGW and MSYS.
    EDIT: Maybe MSYS2 could be enough. You could try that first.

    1.1  There are many version online, not all suitable. The ones we need has 64 bit compilers,
         suggested at the following address:

            http://www.gaia-gis.it/spatialite-3.0.0-BETA/mingw64_how_to.html

         I'm also mentioning the single addresses below:

            MINGW: http://sourceforge.net/projects/mingw-w64/ (look for the most downloaded, non the
                                                               most recent)
            MSYS:  http://sourceforge.net/apps/trac/mingw-w64/wiki/MSYS

         Make sure that in the MinGW folder all compiler executables in the "bin/" folder all have
         the prefix "x86_64-w64-mingw32-".

    1.2  In the MSYS version we've used, it was necessary to create the "fstab" file in "/etc",
         with the line  "c:/MINGW		/mingw", to specify the mingw folder to msys.

         Once done, it should be possible to do "cd /mingw". Watch out that for some reason using
         ls from "/" does not list the folder...

2. Decide the target CPU architecture. If this step is skipped, make will automatically search for
   the compiling system's CPU. The version we built is based on the "nehalem" architecture. It was
   the one automatically generated on our compiling computer.

   To select a specific CPU, add the suffix "TARGET=<your target CPU>" in the following command
   lines. The list of supported CPUS is specified in "TargetList.txt" inside the OpenBLAS folder.

3. Compile using the following commands:

        32-bit: make BINARY=32 USE_THREAD=0 CC=x86_64-w64-mingw32-gcc FC=x86_64-w64-mingw32-gfortran RANLIB=x86_64-w64-mingw32-ranlib
        64-bit: make BINARY=64 USE_THREAD=0 CC=x86_64-w64-mingw32-gcc FC=x86_64-w64-mingw32-gfortran RANLIB=x86_64-w64-mingw32-ranlib

   The only difference is in "BINARY=32/64"; "USE_THREAD=0" indicates multithread support deactivation,
   and we had to specify RANLIB since for some reason the 64 bit version was automatically looking
   "ranlib" at 32 bit.

4. Wait until completion. It should complain that no dll could be created, but the .lib files should
   have been built correctly.

----------
Change Log

Apr. 8, 2022 Removed ./xianyi-OpenBLAS-e6e87a2/lapack-3.4.1.tgz as it contains unredistributable files (see the issue #3928). The package is still available in http://www.netlib.org/lapack/ just in case.
