Core API
--------

In this section, we discuss the core MiniSSF classes and functions. We first introduce the three phases the simulation would go through. We discuss the virtual time class and then focus on the five SSF core classes that one can use to build a simulation model. 

Simulation Phases
*****************

We mentioned in the previous section, MiniSSF, like MPI, adopts the Single Program Multiple Data (SPMD) paradigm. The simulation program will be executed simultaneouly on all machines that participate in the simulation run. Each may execute following a different branch of the program depending on the rank and the total number of machines. Normally, an entire simulation would collectively run through three phases:

* **The init phase** starts on each machine at the beginning of the main function when ``ssf_init`` is called. The user can create entities, processes, input and output channels, initialize the state of the state, align the entities, and map the channels. Entities can be created in the main function or inside the entities' ``init`` methods, which are called by the simulator immediately upon the creation of the entities. 

* **The run phase** starts when ``ssf_start`` is called from the main function. There is an implicit barrier by MiniSSF to wait for ``ssf_start`` to be called on all machines. At this time, the simulation model has been created---the entities have been created and properly aligned on each machine, and the channels are mapped as intended. The simulation starts from time zero and runs until the simulator reaches the designated simulation time or when the system finishes processing all simulation events (i.e., the simulator runs out of work). Once the simulation is done, the control will return from ``ssf_start`` on each machine. 

* **The final phase** starts on each machine when ``ssf_start`` returns; the user can use the opportunity to gather statistics and print results before ``ssf_finalize`` is called. The simulator waits until ``ssf_finalize`` is called on all machines before it proceeds to clean up all simulation states and internal data structures, including entities, processes, input and output channels, and all remaining events.

The prototypes of the related functions are listed below::

   void ssf_init(int& argc, char**& argv);
   void ssf_finalize();
   void ssf_start(VirtualTime endtime, double speedup = 0);
   void ssf_abort(int errorcode);
   void ssf_print_options(FILE* fptr);

The function ``ssf_init`` is expected to be called at the beginning of the main function. It will initialize the parallel execution environment for MiniSSF (such as setting up MPI and pthreads). It will also parse and filter out the command-line options that are meant for MiniSSF. That's why it's a good idea that the user should call this function before processing the command-line arguments.

The function ``ssf_finalize`` is expected to be called before exiting from the main function. This function wil clean up all data structures and states. Unless there has been a call to ``ssf_abort``, it is important that ``ssf_finalize`` is called before returning from the main function, or the simulation may not finish correctly. Note that, after this function is returned, the user must not call any MiniSSF functions or accessing any MiniSSF objects (entities, processes, input and output channels, and events). For example, if you wish to print out the statistics about the simulation collected at the entities, you need to do that before ``ssf_finalize`` is called.

The function ``ssf_start`` starts simulation that runs for the amount of time given as the first argument ``endtime``. The function ``ssf_start`` will not return until the whole simulation has finished. The second argument ``speedup`` is used to indicate the emulation speed. This is for pacing the simulation with respect to real time. The emulation speed is defined by the simulation time divided by the wall-clock time. It is set to zero by default, which means that the emulation speed is infinite. That is, we run simulation independent from the wall-clock time and the simulation will be run as fast as possible. If we set the speedup to be 5, for example, the simulation time will progress five times faster than real time. That is, one real second will correspond to 5 simulated seconds. Setting the speedup to be 1 means that we want to run the simulation in real time. We discuss support for emulation in the Emulation Support section. 

The function ``ssf_abort`` terminates the simulation run in case of an error condition. The functiton will clean up the execution environment before exiting the program with the given error code.

The function ``ssf_print_options`` outputs the list of command-line options expected by MiniSSF. This function would be useful for the user program to print out helpful information about all acceptable arguments of the program.

The user program needs to be aware of the underlying platform on which the simulation runs. For instance, the program needs to know the current machine rank and the total number of machines that run the simulation in order to prepare the model. MiniSSF provides a list of functions about the runtime environment. These functions can be called everywhere in the program::

   int ssf_num_machines();
   int ssf_machine_index();
   int ssf_num_processors();
   int ssf_num_processors(int machidx);
   int ssf_total_num_processors();
   int ssf_processor_index();
   int ssf_total_processor_index();
   int ssf_processor_range(int& startidx, int& endidx);

The function ``ssf_num_machines`` returns the number of machines that run the simulation. The machines are indexed or ranked from zero to the total number of machines minus one. The function ``ssf_machine_index`` returns the rank of the machine. The function ``ssf_num_processors`` can tell the total number of processors on the current machine. If the method is called with an integer argument, the function returns the number of processors on the machine of the given rank. The processors are indexed both globally for the entire runtime environment and locally on each machine. The function ``ssf_processor_index`` returns the index of the processor on the local machine, while the function ``ssf_total_processor_index`` returns the global index of this processor. The function ``ssf_processor_range`` gets the global index range of the processors on the local machine; the function returns the global processor index of the running processor.

The user may want to use MPI to send and receive data between the machines. For example, after ``ssf_start`` returns, the user may want to collect the statistics of the simulation run. In this case, the user may get the MPI communicator using::

    MPI_Comm ssf_machine_communicator();

In order to prevent the user communications from interfering with MiniSSF internal functions that also use MPI, the user should limit performing such communications only during the init and final phases.



Virtual Time
************

The simulation time in MiniSSF is represented by the ``VirtualTime`` class. The class provides the necessary functions of converting between time units, such as hours, minutes, seconds, milliseconds, microseconds, and nanoseconds. Internally, the simulation time is defined as a 64-bit integer and it is "pegged" against the simulation clock ticks since the start of the run. By default, a clock tick is set to be a nanosecond. In this case, a second is 10^9 ticks, and a millisecond is 10^6 ticks. 

The ``VirtualTime`` class defines a set of static variables for commonly used time units, such as *DAY*, *HOUR*, *MINUTE*, *SECOND*, *MILLISECOND*, *MICROSECOND*, and *NANOSECOND*.  The class defines a set of constructors to create a simulation time from different primitive data types (such as int and double). The constructors all have an optional second argument, which can be used to specify the unit of time (the default is nanosecond). For example, we use::

   VirtualTime(3.6, VirtualTime::SECOND)
   VirtualTime(120, VirtualTime::MICROSECOND)
   VirtualTime(16)

to represent 3.6 seconds, 120 microseconds, and 16 nanoseconds of simulation time, respectively. The ``VirtualTime`` class also defines another constant *INFINITY* to indicate infinity in simulation time. 

In rare cases, the user may want to customize the granularity of a clock tick using the ``setPrecision`` method. This method is expected to be called *at most* once and at the very beginning before any virtual time has ever been used. In most simulation scenarios, there's no need to change the time precision, unless the simulation needs to deal with a time scale either smaller than a nanosecond or much larger so that using the default nanosecond clock ticks could cause the 64-bit integer to overflow. For example, if one only needs to deal with the hourly changes of the arrival rate at a facility and no less,
the time precision can be set at the hour level::

   setPrecision(VirtualTime::HOUR)

Note that if you do so, you can't go below the hour precision. That is, ``VirtualTime(1.2, VirtualTime::HOUR)`` would end up be the same as ``VirtualTime(1, VirtualTime::HOUR)``.

The ``second``, ``millisecond``, ``microsecond``, and ``nanosecond`` methods are used to return the virtual time in different time units. The return value is a double precision floating point number. 

It is important to note that casting (explicitly or implicitly) from a ``VirtualTime`` value to a primitive data type, say float, returns the number of simulation clock ticks used to represent the virtual time. For example, if ``(double)VirtualTime(1.2, VirtualTime::MILLISECOND)`` would give ``1.3e6``.

The ``VirtualTime`` class also defines common arithmetic, comparison, and assignment operators so that it can be used together with the primitive types in an expression. In the following example we add 150 milliseconds to the original delay of 1 second and then multiply the result by a factor of 10. The final delay in microseconds is assigned to a double precision floating point variable. Also, the final delay is used in a wait statement (``waitFor``) to block a simulation process for the specific time::

    VirtualTime delay(1, VirtualTime::SECOND); // 1 second
    delay += VirtualTime(150, VirtualTime::MILLISECOND); // 1.15 seconds
    delay *= 10; // 11.5 seconds
    double var_us = delay.microsecond(); // 1.15e10
    waitFor(delay); // process pauses for 11.5 seconds in simulation time


Entity
******

An entity is a container for state variables. For example, a router or a host in a network model can be represented as an entity. Similarly, the CPU, memory, or disk in a computer systems simulation can also be represented as an entity. An entity is also a container for processes, input and output channels (which we describe later). A user-defined entity must be derived from this base entity class. We list the major functions of the ``Entity`` class below::

    class Entity : public ProcedureContainer {
       Entity(bool emulated = false, VirtualTime responsiveness = VirtualTime::INFINITY);
       virtual ~Entity();

       virtual void init();
       virtual void wrapup();

       void alignto(Entity* entity);
       VirtualTime now() const;

       const set<Entity*>& coalignedEntities();
       const set<Process*>& getProcesses();
       const set<inChannel*>& getInChannels();
       const set<outChannel*>& getOutChannels();
    };

The ``Entity`` is derived from the ``ProcedureContainer`` class, which means it can contain methods used as procedures. We describe this in more detail in the next section.

Entities are static objects. That is, the user can only create entities at the simulation init phase before ``ssf_start`` is called.  Once created, the entities should not be reclaimed by the user. They will be reclaimed only by the simulator once the simulation is finalized (i.e., after ``ssf_finalize`` is called).

When creating an entity, the user can declare whether this entity should be treated as an *emulated* entity. An emulated entity means that all activities associated with this entity will be *pinned down* to real time. By default, i.e., without arguments, the entity is not emulated. We discuss support for emulation in detail in the Emulation Support section. 

The ``init`` method is called by the simulator immediately after the entity is created. Similarly, the ``wrapup`` method is called by the simulation before the entity is about to be reclaim (after the simulation has finished). The two methods are virtual and they do nothing by default; the user may want to override them in the derived class if necessary.

Each entity has a timeline. When an entity is created, it is independent and maintains its own timeline. A timeline is implemented with its own event list and simulation clock that can advance independently fom other timelines. An entity should not directly access the state of another entity of a different timeline, because the state of the entities may very well be at a different simulation time. The correct way to communicate with other entities is to send or receive events through the channels.  

This is true unless the entities share the same timeline. These entities sharing the same timeline are said to be *co-aligned*, in which case the entities will advance in simulation time synchronously, annd they can directly access each other's state variables. This is certainly convenient. The downside is that the simulator will not be able to exploit the potential parallelism betwteen the co-aligned entities. Co-aligned entities will be assigned onto the same processor and all activities associated with the co-aligned entities will be sequentialized to maintain strict timestamp ordering. 

Initially each entity is independent and has its own timeline. The user can use the ``alignto`` method to align them up.  Entity alignment can only take place during the simulation init phase. That is, you should not call this function once ``ssf_start`` has been called. Alignment is cumulative, associative, and transitive. Suppose entity A is aligned to entity B, and entity C is aligned to entity D, and if the user then align B to C, all four entities will be co-aligned.

The ``now`` method returns the current simulation time. Note that there is no global simulation clock in parallel simulation. Only co-aligned entities can share the same timeline and therefore the same simulation clock. Entities not co-aligned may experience different simulation time. Therefore, it is incorrect for an entity to access the state variables (including processes, input channels and output channels) of another entity on a different timeline. 

The remaining four methods, ``coalignedEntities``, ``getProcesses``, ``getInChannels``, and ``getOutChannels``, return a list of co-aligned entities, processes, input channels, and output channels, respectively. The return value is a reference to a set, which is a constant and cannot be modified.


Process
*******

A simulation process in MiniSSF is represented by the ``Process`` class. A process specifies the state evolution of the logical component represented by its owner entity. The simulation process starts as soon as the ``Process`` object is created and it starts by executing the starting procedure, which is the ``action`` method. During its execution, a process may be blocked waiting for a message to arrive on a specified input channel or on a set of input channels. It may also be blocked waiting for a specified period of simulation time to pass. 

Process suspension happens as a result of executing the wait statement, which is defined as a function call to one of the wait methods defined in the ``Process`` class. We first show the skeleton of the ``Process`` class in the following::

   class Process : public ProcedureContainer {
      Process(Entity* owner);
      virtual ~Process();

      virtual void action() = 0;

      virtual void init();
      virtual void wrapup();

      void waitOn(const set<inChannel*>& icset);
      void waitOn(inChannel* ic);
      void waitOn();
      void waitFor(VirtualTime delay);
      void waitUntil(VirtualTime time);
      bool waitOnFor(const set<inChannel*>& icset, VirtualTime delay);
      bool waitOnFor(inChannel* ic, VirtualTime delay);
      bool waitOnFor(VirtualTime delay);
      bool waitOnUntil(const set<inChannel*>& icset, VirtualTime time);
      bool waitOnUntil(inChannel* ic, VirtualTime time);
      bool waitOnUntil(VirtualTime time);
      void waitForever();
      void suspendForever();

      void waitsOn(const set<inChannel*>& icset);
      void waitsOn(inChannel* ic);

      Entity* owner();
      VirtualTime now();
      inChannel* activeChannel();
   };

Processes and Procedures
========================

A process can be created either at the init phase or during the simulation run. The constructor of the ``Process`` class identifies the owner entity of the process. A process must belong to an entity and the ownership cannot change during the lifetime of the process. (So no process migration between the entities). 

The process, once created, will start executing the starting procedure, which is the ``action`` method of the class. The ``action`` method is pure virtual in the base class, the user needs to create a class that derives from the ``Process`` class and overwrites this method. 

In the beginning, the starting procedure will be at the bottom of the calling stack of the simulation process. The starting procedure may also call other procedures. The call chain is implemented as a stack of procedures maintained for each simulation process. A procedure is defined as a method that can be interrupted and suspended during its execution. A starting procedure is just a procedure. A procedure can call the wait statements (described in the next section), or call another procedures. A procedure may contain arbitrary normal code that does not advance the simulation time when executed. The simulation time may only be advanced when wait statements are encountered. A wait statement can suspend the process either for some specified period of simulation time or for an arrival of an event at an input channel that belongs to the owner entity. 

In MiniSSF, a procedure must be defined as a method of the ``ProcedureContainer`` class or its subclass. Both ``Entity`` and ``Process`` are derived from the ``ProcedureContainer`` class. Therefore, their methods can be used as procedures. If you want to designate a method of a class not derived from ``Entity`` or ``Process`` to be a procedure, you need to make sure that the class is derived from the  ``ProcedureContainer`` class.

The ``init`` method is expected to be called by the simulator after the process object has been created and before the process' starting procedure is called; it is expected to be invoked by the simulator at the same simulation time when the process is created.  The user can override this method in the derived process class. The ``wrapup`` method will be called before the process is terminated and then reclaimed by the simulator. The system reclaims all processes when simulation finishes. It is also possible that the simulator deletes a process once it realizes that the process will remain to be blocked for the rest of the simulation (such as a call to the ``waitForever`` method or to the ``waitOn`` method on a null input channel).

Wait Statements
===============

The wait statements are special functions of the ``Process`` class. A wait statement can block a process until certain condition is met. The condition could be when an event has arrived at an input channel of the owner entity that a process is waiting on. It could also be when a specified period of simulation time has elapsed. There are therefore in principle two types of wait statements: one waiting for an event to arrive at an input channel; the other waiting for a period of simulation time to pass. There are also wait statements that combine both. In any case, a process may suspend its execution until the expected event happens. The wait statements can only be called within the context of a procedure. It is an error to call a wait statement from a regular function or method.

* The ``waitOn`` methods are used to block a process until an event arrives at a specified input channel or a set of input channels. The input channels must be either owned by the owner entity of the calling process or owned by entities co-aligned with the owner entity of the calling process. The ``waitOn`` method without parameters is used to block a process for the event arrivals at a static set of input channels (set by ``waitsOn``, which we describe in the following).

* The ``waitFor`` method is used to block a process for the specified amount of simulation time. Similarly, the ``waitUntil`` method is used to block a process until a specific point in simulation time. 

* The ``waitOnFor`` methods are designed as a combination of the ``waitOn`` and ``waitFor`` methods. The calling process will be suspended until either an event arrives at the specified input channel or channels, or for the specified amount of simulation time, whichever happens first. The method returns true if it is timed out, or false if an event arrives at an input channel. The ``waitOnUntil`` methods behave similarly except that the calling process is blocked until either an event arrives at the specified input channel or channels, or a specific point in simulation time is reached. If both happen at the same time, it is non-deterministic (depending on the arbitration of the simulator handling simultaneous simulation events). If the input channel or the set of input channels are not included in the argument, it is understood that static input channels will be used (set by ``waitsOn``, which we describe in the following).

* The ``waitForever`` method is used to suspend a process forever. Semantically, it is identical to terminating the process. The runtime system reclaims the process immediately. The ``suspendForever`` method also terminates a process. However, the process object will be kept around until the simulation ends.

* The ``waitsOn`` methods are designed to set a static input channel or a set of static channels. If a process needs to wait on one or a set of input channels repeatedly, every time it calls a wait function, without ``waitsOn``, it would have to specify them explicitly as argument. The efficiency can be improved if we can indicate to the simulator that the process will use a static input channel or a set of input channels that the process repeatedly waits on. 


Other Methods
=============

The ``owner`` method returns a pointer to the owner entity of this process. The ``now`` method returns the current simulation time of the entity. This method is simply an alias to the owner entity's ``now`` method.  

The ``activeChannel`` method returns the input channel that contains a newly arrived event that unblocks the process. This method can only be called within a procedure. It is expected to be called after ``waitOn``, ``waitOnFor``, or ``waitOnUntil``; for the latter two, when the wait statement returns false, indicating the calling process is unblocked because of an event arriving at the input channel.


inChannel
*********

The ``inChannel`` class represents the input channel, which is the end point of a communication link between entities. An input channel can be mapped from several output channels. Similarly, an output channel can be mapped to multiple input channels. An event sent from an output channel will be delivered by the simulator to all mapped input channels, one at a time.

A process can be blocked on a set of input channels for messages to arrive at these input channels. An event that arrives at an input channel on which no process is waiting will be discarded by the simulator implicitly. If, on the other hand, the input channel is waited on by several processes, the simulator will unblock each process, which can then receive a copy of the event. The following lists the major methods of the ``inChannel`` class::
 
   class inChannel {
      inChannel(Entity* owner);
      inChannel(Entity* owner, const char* name);
      virtual ~inChannel();

      Entity* owner();
      Event* activeEvent();
   };

The first constructor creates an unnamed input channel. The constructor is called by passing an argument that points to an entity as its owner. An unnamed input channel is unknown outside the current address space and therefore can only be mapped to using a reference to the instance. In particular, it cannot be used to connect entities (i.e., the input and output channels of the entities) belonging to different machines on distributed platforms.
he constructor of a named input channel.

The second constructor has two arguments: the owner entity and the globally unique name of the input channel to be created. A named input channel can be referenced (i.e., mapped) from the outside of the address spaces in a distributed environment (using the ``outChannel::mapto`` method). In either case, all input channels must be statically owned by an entity (i.e., the ownership cannot change during the simulation). The ``owner`` method returns a pointer to the owner entity.

Destroying an input channel means one needs to separate the links between all output channels mapped to this input channel and the input channel itself. The user shall not reclaim input channels directly: similar to entities and output channels, input channels can only be reclaimed by the simulator when it destroys the owner entity of the input channel at the end of the simulation.

The ``activeEvent`` method is expected to be called by the process waiting on the input channel. This method can only be called within a procedure. When an event arrives at the input channel, it will unblock each process waiting on the input channel. The process resumes its execution after returning from the wait statement. If needed, the process should immediately use the ``activeEvent`` method to retrieve the arrival event at the input channel. A newly arrived event can be retrieved at most once by any waiting process. Once the event has been retrieved, when calling this method again, the method will throw an exeception and return null. If multiple processes are waiting on the same inchannel, each waiting process can retrieve a copy of the arrived event. 

If multiple events arrive at the input channel simultaneously, each event arrival will be treated separately. That is, if the process waits on an input channel in a loop, each iteration will handle only one of the events arrived at the input channel. If the event is not retrieved by any of the waiting processes, it will be reclaimed by the simulator automatically. This is a common case: the user may want to use the event delivery mechanism just to synchronize processes.


outChannel
**********

The ``outChannel`` class represents the starting point of a communication link between entities. An output channel can be mapped to several input channels. An event written to an output channel will appear at all mapped input channels. Major methods of the ``outChannel`` class are listed below::

   class outChannel {
      outChannel(Entity* owner, VirtualTime channel_delay=0);
      virtual ~outChannel();
      Entity* owner();
  
      void mapto(inChannel* ic, VirtualTime map_delay = 0);
      void mapto(const char* icname,  VirtualTime map_delay = 0); 
      virtual void write(Event*& evt, VirtualTime per_write_delay = 0);
   };

The events sent from an output channel experience delays before reaching a mapped input channel. The delay is the sum of the channel delay (specified when the output channel is created), the mapping delay (specified when the output channel is mapped to the input channel), and a per-write delay (specified when an event is written to the output channel). If the channel mapping is from an output channel of an entity to an input channel of another entity, and the two entities are *not* co-aligned (i.e., they do not share the same timeline), it is required that the sum of the channel delay and the mapping delay must be strictly larger than zero. The sum of the delays is used as the lookahead for parallel simulation synchronization in MiniSSF; in general, the larger it is, the better the parallel performance would be.

The constructor of the output channel is called by passing an argument that points to an owner entity. Like input channels, all output channels must be owned by an entity. The ownership must be static, meaning that it cannot change during the simulation. The ``owner`` method returns a pointer to the owner entity. One can specify a channel delay when creating the output channel; the delay must not be negative. All events written to this output channel will experience this delay.

Destroying an output channel means one needs to separate the links between the output channel and all input channels the output channel is mapped to. The user shall not reclaim an output channel directly: similar to entities and input channels, output channels can only be reclaimed by the simulator when the owner entity of the output channel is destroyed at the end of the simulation.

The ``mapto`` methods are used to create a mapping between the output channel and an input channel, specified either by as a pointer to the input channel instance or using its globally unique name. The latter method can be used to map the output channel to an input channel, which is not created in the same address space. A mapping delay can be specified if it is other than zero; the delay must not be negative. All events written to this output channel to be delivered to the input channel through this mapping will experience this delay. The ``mapto`` methods can only be called during simulation initialization.

The ``write`` method is used to send an event from the output channel to the mapped input channels. A reference to the event pointer is passed as the first argument. When the function returns, the reference will become null to indicate that the event has been "given" to the simulator and the user should not change or delete the event afterwards. A per-write delay can be specified as the second argument. The delay must not be negative. The delay will be added to the overall delay experienced by the events written to the output channel to be delivered to all mapped input channels. A process may call the ``write`` method of any output channel as long as the owner entity of the process and that of the output channel are either identical or co-aligned. 


Event
*****

The ``Event`` class is the base class for messages that are passed between entities through channels. The user may also use events as a simple mechanism to coordinate simulation processes. If additional information is needed to be passed between the entities, the user can create a class derived from the ``Event`` class. A skeleton of the ``Event`` class is shown in the following::

   class Event {
      Event();
      virtual ~Event();
    
      Event(const Event&);
      virtual Event* clone();
    
      virtual int pack(char* buf, int siz) { return 0; }
   };

A typical life cycle of an event is as follows. An event is created at an entity and sent through the output channel. A copy of event is delivered to the input channel (after appropriate delays). The process waiting on the input channel wakes up, receives and processes the event, and then reclaims the event. Processing the event may create more events.

Events are messages that may travel across the boundaries between processors via shared memory or between distributed memory machines. MiniSSF needs to copy the events when necessary (e.g., in cases where an event is written to an output channels that are mapped to multiple input channels). 

MiniSSF require that the ``Event`` class and its derived classes to provide a copy constructor, in which a deep copy of the data structure is expected. Creating a new copy of the event is actually achieved by the simulator's calling the ``clone`` method, which must also be provided by all event classes. The ``clone`` method is *virtual* and returns a pointer to a newly created event object. The following shows an example of how the ``clone`` method is used in conjunction with the copy constructor defined in the MyEvent class, which derives from the ``Event`` base class::

   class MyEvent : public Event {
    public:
      int x, y, z; // three user-defined integers to be passed around
      MyEvent(int a, int b, int c): x(a), y(b), z(c) {} // the constructor
      MyEvent(const MyEvent& e) : Event(e), x(e.x), y(e.y), z(e.z) {} // the copy constructor
      Event* MyEvent::clone() { return new MyEvent(*this); }
      ...
   };

For delivering events across distributed memory, MiniSSF also needs to serialize them. That is, MiniSSF needs to translate an event into a machine-independent byte stream before it is shipped to another machine. At the destination, the machine needs to reconstruct the user event from the byte stream. If the event is an instance of a derived event class, the user needs to provide a way for the simulator to translate an event instance to and from the byte stream. 

The translation from an user event object to a byte stream is called *packing*. The reverse translation from a byte stream to a newly created user event object is called *unpacking*. For event packing, the system requires that the derived event class must provide a virtual method named ``pack``, which the simulator will invoke at the time when it needs to create the byte stream before shipping it to a remote machine. The ``pack`` method is responsible for packing the necessary data fields of the event so that it can be reconstructed (unpacked) at another machine.  The ``pack`` method takes two arguments: a pointer to the buffer to store the bytes, and the size of the buffer; the method returns the actually number of bytes being written out to the buffer. At the event base class, the method does nothing other than returning zero as a special case since the base event class does not have any data to be serialized. The user may use ``CompactDataType`` provided by MiniSSF for serialization. We describle the details of this class in the next section.

In order for the simulator to recognize the event from a byte stream at the remote machine, the user needs to register the event class and associate the event class with an event factory method, which is responsible for creating a new event object of the corresponding event class and unpacking data from the byte stream. The event factory method should be a callback function defined as a static method of the derived event class::

   typedef Event* (*EventFactory)(char* buf, int siz);

The event factory method takes a pointer to the buffer and the size of the buffer as arguments, and returns the new event.

MiniSSF provides two macros. The first macro, ``SSF_DECLARE_EVENT(classname)``, shall be used for registering the event class ``classname`` during the event class declaration. The second macro, ``SSF_REGISTER_EVENT(classname, eventfactory)``, shall be used to associate the event class ``classname`` with the event factory method ``eventfactory``; the macro is expected to be present along with the event class definition.

The following example describes the use of the event factory method and the two macros for declaring and registering an event subclass. The following code snippet contains the declaration of the event class in the header file (say, ``myevent.h``)::

   class MyEvent : public Event {
    public:
      ...
      virtual int pack(char* buf, int bufsiz); // override the method in base class
      static MyEvent* my_event_factory(char* buf, int bufsiz); // user-defined event factory
      SSF_DECLARE_EVENT(MyEvent); // declare the event
   };

The definition of the event class is contained in the source file (say, in ``myevent.cc``)::

   int MyEvent::pack(char* buf, int bufsiz) {
      CompactDataType* cdata = new CompactDataType; // create a byte stream
      cdata->add_int(x); // add integer x to stream
      cdata->add_int(y); // add integer y to stream
      cdata->add_int(z); // add integer z to stream
      return cdata->pack_and_delete(buf, bufsiz); // pack into buffer and delete it
   }
   MyEvent* MyEvent::my_event_factory(CompactDataType* cdata) {
      int x, y, z
      CompactDataType* cdata = new CompactDataType; // create a byte stream
      cdata->unpack(buf, bufsiz); // unpack from buffer
      cdata->get_int(&x); // deserialize integer x from stream
      cdata->get_int(&y); // deserialize integer y from stream
      cdata->get_int(&z); // deserialize integer z from stream
      delete cdata;
      return new MyEvent(x,y,z); // create and return the new event
   }
   // register event with the event factory method
   SSF_REGISTER_EVENT(MyEvent, MyEvent::my_event_factory);
