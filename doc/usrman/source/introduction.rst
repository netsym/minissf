Introduction
------------

MiniSSF is a parallel discrete-event simulator; it's a streamlined implementation of the Scalable Simulation Framework (SSF).


Scalable Simulation Framework
*****************************

SSF provides a lean API for developing large-scale complex models.  It has a modular design---the target system can be modeled as a collection of interacting entities with explicit communication channels.  The model can be partitioned and run high-performance computing platforms for parallel processing.

MiniSSF Implementation
**********************

MiniSSF is a C++ implementation of SSF. It can run on almost all Unix-based environments, including shared-memory multi-processor multi-core machines, distributed-memory machine clusters, and a combination of both. It's a multi-threaded MPI program.

Two distinguished features of MiniSSF are:

* MiniSSF implements only a subset of the SSF API and extends it for greater usability. MiniSSF adopts the minimalistic approach, keeping only a small number of core functions and classes, that are essential for building large complex models.

* MiniSSF fully supports process-oriented simulation. It does automatic source-code transformation for highly efficient user-space multithreading.


