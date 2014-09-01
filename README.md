minissf
=======

minissf is a streamlined implementation of the Scalable Simulation
Framework (SSF). It is a simulator designed for parallel
discrete-event simulation of large-scale complex systems. minissf can
run on most high-performance computing platforms, including
shared-memory multi-processor multi-core machines or distributed
machines.

Follow the following steps to download, build and run minissf.

1. Configure minissf:

% cd minissf
% ./configure

If you need to change the compiler flags, for example, enable the
debugging information for debuggers, like gdb, and turn on all
compiler warnings when you compile minissf, you can configure the code
with the following flags:

% ./configure CXXFLAGS="-O0 -g -Wall" CFLAGS="-O0 -g -Wall"

You can explicitly set the MPI compilers. MPI is required by minissf
to run on distributed memory platforms. If it's not set, the configure
script will try to find their names from the usual suspects, such as
mpicxx, mpiCC, etc. If your MPI program has an unusual name or at a
different location (such as the following example), one should specify
the MPI compilers explicitly:

% ./configure --with-mpicxx="/opt/usr/bin/mpicxx" --with-mpicc="/opt/usr/bin/mpicc"

minissf uses LLVM/Clang for C++ source code translation. You should be
able to download and compile the latest LLVM/Clang source code
tree. Note that building LLVM/Clang may take a long time, possibly
well over half an hour depending on the speed of your machine.
Detailed instructions on how to install LLVM and Clang can be found at
http://llvm.org/. You can get the lastest source code release of
LLVM/Clang from http://llvm.org/releases/download.html. 

In the following example, we use LLVM version 2.9. Follow the commands 
below to install LLVM and Clang (which will be put in the ``llvm-2.9``
directory under the current working directory):

% tar xvzf llvm-2.9.tgz
% tar xvzf clang-2.9.tgz
% mv clang-2.9 ./llvm-2.9/tools/clang
% cd llvm-2.9
% ./configure
% make

After that, when you configure minissf, you need to let minissf know
where LLVM/Clang source code is located:

% cd minissf
% ./configure --with-llvm-path=<LLVM_INSTALL_DIR>


2. Build minissf:

After minissf has been properly configured, you can build it simply using:

% make

This step will create the minissf library (libssf.a) that you would
need to run minissf applications.

At any time, you can restore a clean minissf distribution (say, in
order to reconfigure minissf), using:

% make clean

or,

% make distclean

The latter not only deletes all the library and object files that are
created during compilation, but also remove all files related to the
configuration. It's restores the entire minissf directory to the
factory state (as if you just check out from svn or untar from the
archive file).


3. Generate documents:

If you want to create the user's manual and reference manual, you need
to do this manually:

% cd doc
% make

The user's manual will be generated using Sphinx
(http://sphinx.pocoo.org/), which is a python documentation generator
that uses a markup language, called reStructuredText
(http://docutils.sourceforge.net/rst.html). This means you need to
have sphinx properly installed for minissf to generate the user's
manual for you.

The reference manual has two flavors: one is for the modeler, who uses
minissf to write simulation models, and the other is for minissf
developer, who wants to know how minissf is implemented. Both
reference manuals require doxygen, which is a source code
documentation generator for C++ and Java
(http://www.stack.nl/~dimitri/doxygen/index.html). This means you need
to have doxygen installed for minissf to generate the reference
manuals.

These manuals can be viewed online at
http://www.primessf.net/minissf/.


4. Run tests and examples:

There are examples which you can use to test minissf. There are all
located under the example subdirectory under minissf root
directory. Please refer to the specific README files for detailed
description of the examples. To compile all examples, you simply do
the following:

% cd examples
% make

Good luck!


-----

Copyright (c) 2011-2014 Florida International University.

Permission is hereby granted, free of charge, to any individual or
institution obtaining a copy of this software and associated
documentation files (the "software"), to use, copy, modify, and
distribute without restriction.

The software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties of
merchantability, fitness for a particular purpose and
noninfringement.  In no event shall Florida International
University be liable for any claim, damages or other liability,
whether in an action of contract, tort or otherwise, arising from,
out of or in connection with the software or the use or other
dealings in the software.

This software is developed and maintained by

Jason Liu
Modeling and Networking Systems Research Group
School of Computing and Information Sciences
Florida International University
Miami, Florida 33199, USA

You can find our research at http://www.primessf.net/.
