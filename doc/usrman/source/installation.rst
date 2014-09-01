Installation
------------

Supported Platforms
*******************

MiniSSF is portable across a wide variety of parallel architectures. We expect it able to run on most UNIX-based machines. (If it's indeed not the case, please let us know. There is a good chance that with only minimum effort we can make MiniSSF to run on your machine.)


Software Requirement
********************

You may need the following software packages before building MiniSSF. All of these packages, however, are optional.

* **MPI** *(optional)*. MiniSSF uses Message Passing Interface (MPI) for communication among distributed-memory machines for parallel execution. If you don't have MPI, MiniSSF will only work with shared memory.

* **LLVM/Clang** *(optional)*. MiniSSF uses `llvm/clang <http://llvm.org/>`_ for automatic source-code transformation for process-oriented simulation. You can get the lastest source code release of llvm and clang from http://llvm.org/releases/download.html. MiniSSF requires version 2.9 or later. (Note that building llvm/clang may take a long time on some machines.)   If you don't have llvm/clang, MiniSSF would need the user to annotate the source code with special comments so that the source code can be instrumented with the code to support multithreading.  In the following example, we assume that we have already downloaded llvm and clang (version 2.9) from the web site. The following commands will build them together::

   % tar xvzf llvm-2.9.tgz
   % tar xvzf clang-2.9.tgz
   % mv clang-2.9 ./llvm-2.9/tools/clang
   % cd llvm-2.9
   % ./configure
   % make


* **Sphinx** *(optional)*. MiniSSF uses `sphinx <http://sphinx.pocoo.org/>`_ to create the user's manual. You don't need it if you don't have to manually generate this document (it's available online).

* **Doxygen** *(optional)*. MiniSSF uses `doxygen <http://www.stack.nl/~dimitri/doxygen/index.html>`_ to generate the reference manual. You don't need it if you do't have to manually generate the reference manual (it's available online).


Download
*********************

MiniSSF is open source. There are two ways to obtain the source code. You can get it as a zipped tarball from the web site: http://www.primessf.net/minissf/. Once downloaded, it can be decompressed using::

   % tar xvzf minissf-<version>.tar.gz

Alternatively, You can use svn to check out the source code from the repository (using ``guest`` both as the username and password)::

   % svn co https://svn.primessf.net/repos/minissf/tags/release-<version> minissf

If you dare, you can also get the latest source code from the trunk https://svn.primessf.net/repos/minissf/trunk.

In any case, a new directory, called ``minissf``, will be created under the current directory.


Configuration
*************

MiniSSF uses autoconf tools to customize the software before building it. The simplest way to configure MiniSSF is::

   % ./configure

However, in this case, MiniSSF would assume that you don't have llvm/clang installed. That is, your source code has to be manually annotated for multithreading. To have fully automated source-code transformation, you need to tell MiniSSF where you installed llvm/clang::

   % ./configure --with-llvm-path=<LLVM_INSTALL_DIR>

where LLVM_INSTALL_DIR is the root directory of the llvm/clang source tree. 

In some cases, you may want to change the compiler flags. For example, if you want to enable the debugging information or turn on compiler warnings, you can add the following options::

   % ./configure --with-llvm-path=<LLVM_INSTALL_DIR> \
      CXXFLAGS="-O0 -g -Wall" CFLAGS="-O0 -g -Wall"

The configure script is smart enough to locate all necessary information. However, in some special cases, you may need to tell the script where it is. In the following example, we want to use a particular MPI version located at ``/opt/usr/bin/``: we use ``--with-mpicxx`` to specify the MPI C++ compiler and use ``--with-mpicc`` to specify the MPI C compiler::

   % ./configure --with-llvm-path=<LLVM_INSTALL_DIR> \
      --with-mpicxx="/opt/usr/bin/mpicxx" --with-mpicc="/opt/usr/bin/mpicc"


Compilation
*********************

After MiniSSF has been properly configured, you can compile the source code simply by::

   % make

The end result is a static library (``libssf.a``). You can then use it to build your model.

If you want to clean up the source tree and remove the library, you can::

   % make clean

At any point, you can restore the entire source code directory using::

   % make distclean

This command not only deletes all the library and object files created during compilation, but also removes all files from configuration.
