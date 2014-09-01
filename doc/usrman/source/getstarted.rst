Getting Started
---------------

We start with an example to show how MiniSSF works. We also discuss the process of building and running a simulation model.

The HelloWorld Example
======================

The program is a modified version of "Hello, world!" program that makes use of MiniSSF to synchronize multiple processes printing out the greetings one by one. The program is located at the ``examples/helloworld`` directory in the source tree.

The Main Function
*****************

We first list the main function, which creates the model and starts the simulation on distributed-memory machines::

   #include <stdio.h>

   #include "ssf.h"
   using namespace minissf;

   ...

   int main(int argc, char* argv[])
   {
     ssf_init(argc, argv);
   
     int n = ssf_num_machines();
     int p = ssf_machine_index();
   
     MyEntity* ent = new MyEntity(p);
     char icname[128]; sprintf(icname, "IN_%d", (p+1)%n);
     ent->oc->mapto(icname, 1);
   
     ssf_start(n+1);
   
     ssf_finalize();
     return 0;
   }

To use MiniSSF functions, one needs only to include the header file ``ssf.h``. All functions are defined in the namespace ``minissf`` to avoid name collisions.

Before any other MiniSSF functions can be called, the main function must call the function ``ssf_init`` and pass along the command-line arguments, ``argc`` and ``argv``. This function must be called *before* the user processes the command-line arguments. ``ssf_init`` will initialize the simulation environment (such as setting up MPI and pthreads), and then parse and filter out the command-line arguments meant for MiniSSF. Similarly, the main function is expected to call ``ssf_finalize`` to clean up all internal data structures and simulation states when the simulation finishes.

Like MPI, the main function will run on all machines that participate in the execution of the simulation program. MiniSSF adopts the *Single Program Multiple Data (SPMD)* paradigm: a single program is run (although possibly following different branches) on all machines. The function ``ssf_num_machines`` returns the total number of machines (i.e., MPI tasks) running the simulation. The function ``ssf_machine_index`` returns the rank of the current machine (or MPI task). These functions behave similarly to the ``MPI_Comm_size`` and ``MPI_Comm_rank`` functions in MPI.

In this example, the main function creates an entity on each machine, which is an instance of ``MyEntity`` class, using the rank of the current machine as its parameter. Since the main function is expected to be called on all machines running the simulation, there will be as many entities as there are machines in the simulation. We will describe ``MyEntity`` class momentarily; for now, it suffices to know that the main function maps from an output channel of the entity, named ``oc``, to the an input channel of another entity with a specific name defined in ``icname``. The entities will use the channels for communication amongst themselves in simulation.

After the entity has been created and the channels mapped, the main function calls ``ssf_start``. This function starts the simulation from time zero and runs the simulation until the time specified by the argument. Once the simulation finishes, the function will return. 

It is important to note that MiniSSF encapsulates the real simulation work inside the entities. The main function only creates the entities and map them. Detailed logic of the entities is defined in the entity class, described next.


The MyEntity Class
******************

To define the entity, the user needs to create a derived ``Entity`` class. An entity is a container to keep all the state variables. For example, a router can be modeled as an entity in network simulation. So is a disk in the file system simulation. An entity is also a container for simulation processes, input channels, and output channels. In our example, we define only one type of entity, called ``MyEntity``, which is derived from the ``Entity`` class. It only contains one integer identifier, and pointers to an input and an output channel of the entity. The class contains a constructor and a method named ``printout``, which will be used to print out the greetings::

   class MyEntity : public Entity {
   public:
      int id;
      inChannel* ic;
      outChannel* oc;
      MyEntity(int id);
      void printout(Process* p);
   };

The definition of the entity constructor is shown below::

   MyEntity(int i) : id(i) {
      char icname[128]; sprintf(icname, "IN_%d", id);
      ic = new inChannel(this, icname); 
      oc = new outChannel(this);
      new MyProcess(this);

      if(id == 0) {
         Event* evt = new Event();
         oc->write(evt);
      }
   }

The constructor creates an input channel ``ic`` with a specific name. The input channel is the receiving end of a link between entities. An entity needs to use the input channel to receive messages sent from other entities. The name of the input channel must be unique; in this case, we uniquely name the input channel by appending the entity's id. 

The constructor also creates an output channel. The output channel is the sending end of a link between entities. The entity can write messages to the output channel and the simulator will deliver the messages to all input channels that are mapped to the output channel in due time. The messages sent from an output channel experience a pre-specified delay in simulation time before they are delivered to the mapped input channel. 

Then, the constructor creates a simulation process. A simulation process is part of an entity and is used to specify the changes in the entity's state. Each simulation process is an independent thread of execution: a process can wait for a message to arrive at an input channel, or wait for some specified simulation time to pass. We also define one type of simulation process in the entity: the ``MyProcess`` class is derived from the ``Process`` class:: 

   class MyProcess : public Process {
   public:
      MyProcess(Entity* ent) : Process(ent) {}
      virtual void action();
   };

A process starts by executing the ``action`` method. In this example, the ``action`` method simply calls the ``printout`` method of its owner entity::

   void MyProcess::action() {
     ((MyEntity*)owner())->printout(this);
   }

Finally, to start printing the  "Hello, world!" message at each entity, entity 0 writes an event to its output channel. This becomes clear if we look at the definition of the ``printout`` method::

   void printout(Process* p) {
      p->waitOn(ic);
      printf("Entity %d says \"Hello, world!\"\n", id);
      if(id > 0) {
         Event* evt = new Event();
         oc->write(evt);
      }
   }

The process first waits for the arrival of a message at the input channel. The process is blocked until an event appears at the input channel. It then prints out the "Hello, world!" message, and then, if this entity is not entity 0, the process will generate another event and send it to the output channel to inform the next entity to print out its greetings.

From the main function, we can see that the output channel of one entity is mapped to the input channel of the next entity. The entities are connected in a circular fashion: entity 0 is connected to entity 1; entity 1 is connected to entity 2; and so on. The last entity is connected back to entity 0. The total number of entities is the same as the number of machines participating in the simulation.

When entity 0 is created, it sends an event to its output channel. The event will be delivered to the input channel of entity 1. The process at entity 1 (which is waiting on the input channel inside ``printout``) will be unblocked and print out "Hello, world!" before it sends another event out from its output channel. The event will be sent to entity 2, which will print out its "Hello, world!". This will go on until the entity at the last machine sends an event back to entity 0. After entity 0 prints out the last "Hello, world!" message, there will be no more event to forward and the simulation terminates.


Running Simulation
******************

To run the example, we first compile the program using ``make``::

   % cd examples/helloworld
   % make

An executable file called ``helloworld`` will be created in the directory ``examples/helloworld``. The program is a regular MPI program. Different MPI implementations may have different ways to run MPI programs. Here we assume we use ``mpirun`` to start the run on five machines::

   % mpirun -np 5 ./helloworld

The results may look like::

   Entity 1 says "Hello, world!"
   Entity 2 says "Hello, world!"
   Entity 3 says "Hello, world!"
   Entity 4 says "Hello, world!"
   Entity 0 says "Hello, world!"

We know entity 1 prints out the greetings before entity 2, entity 2 before entity 3, entity 3 before entity 4, and entity 4 before entity 0. However, since the entities are run on separate machines, the printout may get interleaved with one another, and the order may not be preserved when shown on your screen.

The Makefile
============

MiniSSF need to do source-code transformation before the model is compiled and linked---either automatically using llvm/clang or manually with annotations in source code (we describe this later). To hide the complexity, it is recommended that the user organize the source code using a ``Makefile`` fashioned after any of the example. 

Let's first start with the Makefile, which we use to build helloworld:: 

   include <minissf-install-dir>/Makefile.include
   AUTOXFLAGS = -DAUTOXLATE_REQUIRED=no -DSSFCMDDEBUG=yes

   INCLUDES = -I.
   CXXFLAGS =
   LDFLAGS =
   LIBS =

   all: helloworld

   helloworld: helloworld.o
        $(SSFLD) $(AUTOXFLAGS) $(LDFLAGS) $< -o $@ $(LIBS)

   helloworld.o: helloworld.cc
        $(SSFCPPCXX) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) $< -o $@

   clean:
        $(RM) helloworld helloworld.o core
        $(RM) $(SSFCLEAN) 


An important thing to note is that the ``Makefile`` needs to include another file called ``Makefile.include``, which is located in the root directory of MiniSSF source tree *(you need to replace ``<minissf-install-dir>`` with the directory path)*.  ``Makefile.include``  contains important definitions, macros, and utility programs to correctly compile the source code. In the least, it contains the following definitions:

* **SSFCPP**: this is the preprocessor for source-code transformation (similar to the c preprocessor).
* **SSFCXX**: this is the C/C++ compiler that converts the *preprocessed* source code to an object file (similar to ``gcc`` or ``g++``).
* **SSFCPPCXX**: this is one command that does preprocessing (SSFCPP) and compilation (SSFCXX) together.
* **SSFCKPR**: this is a utility program that finds out all procedures in the source code (we'll describe this later).
* **SSFLD**: this is the linker that combines the object files and libraries and produces the final executable file (similar to ``ld``).
* **SSFCLEAN**: this is a macro that contains the names of all generated intermediate files during the process, so that one can remove these files after compilation.

In the Makefile above, we use ``SSFCPPCXX`` to preprocess and compile ``helloworld.cc``, which creates the object file ``helloworld.o``. And then we use ``SSFLD`` to create the final executable file ``helloworld``. 

The macro ``AUTOXLATE_REQUIRED`` is used for error checking. If it is set to "no", as in this case, it means that the source code has also been manually annotated (we'll discuss manual source-code annotations). Manual annotation would not be necessary if llvm/clang has been properly installed, in which case MiniSSF does automatic source-code transformation. One must understand, if he did not install llvm/clang, using manual annotation will be the only way. In most cases, one does not bother with manual translation; that is, ``AUTOXLATE_REQUIRED`` should be set to "yes". That is, if MiniSSF finds out that llvm/clang is not installed, an error will be prompted. 

If the definition ``SSFCMDDEBUG`` is set to "yes", the system will prompt exactly what commands are used for the compilation. This is just for clarification.

In cases where several source files are needed to build the final program, we cannot use ``SSFCPPCXX``. We need to separate preprocessing and compiling. Here we use the Makefile in the netsim example (which is a simple network simulator in the examples directory) to show how this can be done::

   include ../../Makefile.include
   AUTOXFLAGS = -DAUTOXLATE_REQUIRED=yes

   INCLUDES = -I.
   CXXFLAGS =
   LDFLAGS =
   LIBS =

   HEADERS = netsim.h ippacket.h host.h tcpsess.h router.h
   SOURCES = netsim.cc host.cc tcpsess.cc router.cc
   OBJECTS = $(SOURCES:.cc=.o)
   XFILES = $(SOURCES:.cc=.x)

   all:    netsim

   netsim: $(OBJECTS)
        $(SSFLD) $(AUTOXFLAGS) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS)

   netsim.o: pre_netsim.cc plist.x
        $(SSFCXX) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) pre_netsim.cc -x plist.x -o $@
   host.o: pre_host.cc plist.x
        $(SSFCXX) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) pre_host.cc -x plist.x -o $@
   tcpsess.o: pre_tcpsess.cc plist.x
        $(SSFCXX) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) pre_tcpsess.cc -x plist.x -o $@
   router.o: pre_router.cc plist.x
        $(SSFCXX) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) pre_router.cc -x plist.x -o $@

   pre_netsim.cc netsim.x: netsim.cc $(HEADERS)
        $(SSFCPP) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) $< -o pre_netsim.cc -x netsim.x
   pre_host.cc host.x: host.cc $(HEADERS)
        $(SSFCPP) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) $< -o pre_host.cc -x host.x
   pre_tcpsess.cc tcpsess.x: tcpsess.cc $(HEADERS)
        $(SSFCPP) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) $< -o pre_tcpsess.cc -x tcpsess.x
   pre_router.cc router.x: router.cc $(HEADERS)
        $(SSFCPP) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) $< -o pre_router.cc -x router.x

   plist.x: $(XFILES)
        $(SSFCKPR) $(AUTOXFLAGS) $(XFILES) -o $@

   clean:
        $(RM) netsim $(OBJECTS) core
        $(RM) $(SSFCLEAN) 
        $(RM) pre_*.cc *.x

The program contains four source files and five header files. The entire compilation takes four steps. In the first step, each source file is preprocessed using ``SSFCPP``. The ``-o`` option specifies the name of the output file to be generated after preprocessing; the ``-x`` option specifies the name of an auxiliary file that contains necessary information for later processing. After all source files are preprocessed, the second step uses ``SSFCKPR`` to combine all auxiliary files and generates another file. It's called ``plist.x`` in the example. In the third step, each preprocessed source file is compiled using ``SSFCXX``, which generates the corresponding object file; the combined auxiliary file ``plist.x`` must be presented to the compiler here using the ``-x`` option. Finally, in the last step, all the object files (and libraries) are linked using ``SSFLD`` to create the final executable.


Command-Line Options
====================

Once the executable file of a simulation model is created successfully, the model is ready to run. There are command-line options that one can use to change the behavior of the simulation run. Parsing these command-line options happens when the ``ssf_init`` function is invoked. They are removed from the argument list when the function returns. Of course, the user program can define their own command-line options. 

* ``-n <N>``: set the number of processors or cores to be used on each machine. MiniSSF uses MPI for distributed simulation on a cluster (or a supercomputer).  On each machine, MiniSSF can be configured to run as a multithreaded program to take advantage of the multiple processors or cores (we simply call them processors). They communicate through shared memory. The default is one. The following examples show how to set the runtime environment (assuming that ``myprog`` is the simulation program)::

   # run sequentially
   % ./myprog

   # run on four machines each with one processor
   % mpirun -np 4 ./myprog

   # run on three machines each with four processors
   % mpirun -np 3 ./myprog -n 4

* ``--set-nprocs <M> <N>``: set the number of processors on machine M to be N specifically. This command-line option is useful when the underly machines to run the simulation are not uniform.  This option can also be used together with the ``-n`` option::

   # run on three machines; machine 0 has four processors, the others each have two processors
   % mpirun -np 3 ./myprog -n 2 --set-nprocs 0 4

   # run on eight machines; machine 2 has six processors, the others each have one processor
   % mpirun -np 8 ./myprog --set-nprocs 2 6

* ``-s <S>``: set the global random seed. This is the seed of all seeds used by the random streams in simulation. Simply changing this seed would alter the sequence of all pseudo random number generators used in simulation.  It is guaranteed the simulation generates repeatable results if the same seed is chosen (regardless of the runtime environment).  By default, it is set to be 54321. If the user sets it to zero, the simulator will get the seed from the system clock, in which case every run would result differently. 

* ``-o <F>``: set output file. By default, the simulator uses the standard output. This option tells the simulator to pipe the output to the designated file of the given name. If MiniSSF is running on multiple machines, each machine will generate a separate file::

   # send the output to two files, myfile-0 and myfile-1, one for each machine
   % mpirun -np 2 ./myprog -n 2 -o myfile

* ``-i <I>``: show tje progress of simulation at the given time interval. The option is useful for debugging when a simulation takes a long time to run without any evidence of making progress. If set, the simulator will print out a message showing its progress during the run at every interval I in simulation time to indicate the simulation is running normally. The user can append time units after time interval in the command-line (by default, it is in seconds), such as 10ms (i.e., 10 milliseconds), 5ns (i.e., 5 nanoseconds), and 2d (i.e., 2 days). For example::

   # run simulation and print out a message every 10 seconds
   % ./myprog -i 10

   # run simulation and print out a message every 2 milliseconds
   % ./myprog -i 2ms

* ``-d <D>``: set the debug level. Normally, MiniSSF will print out some brief information about the simulation run. The default debug level is 1. Setting it to 0 would turn off all debug messages. Setting it to 2 would produce a table with the runtime statistics collected by the simulator.

* ``--``: indicate the end of parsing MiniSSF command-line options, after which the user can place their command-line options without worrying about any conflicts. For example, if the user program wants to use -n and -i for his own use, one can do the following::

    # run simulation with my own -n option
   % ./myprog -- -n -i

* ``-a <A>``: set the maximum number of alignments (or logical processes) allowed on each processor. This is a command-line option for performance tuning. Usually, you don't need to tinker with this option at all. By default, ``A=1``. That is, all logical processes assigned to a processor will be merged into one. If one sets it to be zero, it means the simulator will not merge them at all. 

* ``-T <T>`` : manually set the global synchronization threshold. Use -1 to represent infinity. 

 ``-t <t>`` : manually set the local synchronization threshold on each machine. Use -1 to represent infinity. 

 ``--set-local-thresh <M> <t>``: manually set the local synchronization threshold on machine M. Use -1 to represent infinity. 

 ``--set-training-len <L>``: set the minimal training duration used by the simulator to find the optimal synchronization thresholds (the default is 5% of the end simulation time). 

 Unless you need to specifically deal with the MiniSSF's hierarchical composite synchronization algorithm, you don't need to handle these command-line options. These options are for performance tuning.

