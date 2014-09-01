Extended API
------------

MiniSSF provides a few extended classes in addition to the standard SSF classes. These extensions are not essential for developing a model, but they are important and convenient nonetheless. All these extensions are included automatically in the header file ``ssf.h``.


Data Serialization
******************

When an instance of a user-defined event class must be sent across memory space boundaries, the simulator must transform the event object into a machine-independent byte stream and send it via the underlying message passing mechanisms. MiniSSF requires that the user take the responsibility of packing the data fields of the derived event class when the event is about to be shipped to a remote machine. 

As we mentioned in previous section, the event class (derived from ``Event`` class) should provide the virtual method ``pack``, within which the user is expected to convert the event object into a byte array. After the object is shipped to the destination, the user must be able to unpack the data into a newly created event object. Both serialization and deserialization can be performed using the ``CompactDataType`` class::

   class CompactDataType {
      CompactDataType();
      virtual ~CompactDataType();
    
      /* methods for packing a single primitive value */
      void add_float(float val); 
      void add_double(double val); 
      void add_long_double(long double val); 
      void add_char(char val); 
      void add_unsigned_char(unsigned char val); 
      void add_short(short val); 
      void add_unsigned_short(unsigned short val); 
      void add_int(int val); 
      void add_unsigned_int(unsigned int val); 
      void add_long(long val); 
      void add_unsigned_long(unsigned long val); 
      void add_long_long(long long val); 
      void add_unsigned_long_long(unsigned long long val); 
      void add_virtual_time(VirtualTime_t val);

      /* methods for packing an array of primitive values */
      void add_float_array(int nitems, float* val_array); 
      void add_double_array(int nitems, double* val_array); 
      void add_long_double_array(int nitems, long double* val_array); 
      void add_char_array(int nitems, char* val_array); 
      void add_unsigned_char_array(int nitems, unsigned char* val_array); 
      void add_short_array(int nitems, short* val_array); 
      void add_unsigned_short_array(int nitems, unsigned short* val_array); 
      void add_int_array(int nitems, int* val_array); 
      void add_unsigned_int_array(int nitems, unsigned int* val_array); 
      void add_long_array(int nitems, long* val_array); 
      void add_unsigned_long_array(int nitems, unsigned long* val_array); 
      void add_long_long_array(int nitems, long long* val_array); 
      void add_unsigned_long_long_array(int nitems, unsigned long long* val_array); 
      void add_virtual_time_array(int nitems, VirtualTime* val_array); 
   
      /* packing and unpacking a (null-terminated) string */
      void add_string(const char* valstr);
      char* get_string();

      /* methods for unpacking an array of primitive values */
      int get_float(float* addr, int nitems = 1); 
      int get_double(double* addr, int nitems = 1); 
      int get_long_double(long double* addr, int nitems = 1); 
      int get_char(char* addr, int nitems = 1); 
      int get_unsigned_char(unsigned char* addr, int nitems = 1); 
      int get_short(short* addr, int nitems = 1); 
      int get_unsigned_short(unsigned short* addr, int nitems = 1); 
      int get_int(int* addr, int nitems = 1); 
      int get_unsigned_int(unsigned* addr, int nitems = 1); 
      int get_long(long* addr, int nitems = 1); 
      int get_unsigned_long(unsigned long* addr, int nitems = 1); 
      int get_long_long(long long* addr, int nitems = 1); 
      int get_unsigned_long_long(unsigned long long* addr, int nitems = 1); 
      int get_virtual_time(VirtualTime* addr, int nitems = 1); 

      // packing and unpacking to and from a byte array
      int pack(char* buf, int bufsiz);
      int pack_and_delete(char* buf, int bufsiz);
      void unpack(char* buf, int siz);

      /* generic static methods for serializing primitive values (of given size) */
       static void serialize(uint8 data,  char* buf, int bufsiz, int* pos = 0);
       static void serialize(uint16 data, char* buf, int bufsiz, int* pos = 0);
       static void serialize(uint32 data, char* buf, int bufsiz, int* pos = 0);
       static void serialize(uint64 data, char* buf, int bufsiz, int* pos = 0);
       static void serialize(int8 data,   char* buf, int bufsiz, int* pos = 0);
       static void serialize(int16 data,  char* buf, int bufsiz, int* pos = 0);
       static void serialize(int32 data,  char* buf, int bufsiz, int* pos = 0);
       static void serialize(int64 data,  char* buf, int bufsiz, int* pos = 0);
       static void serialize(float data,  char* buf, int bufsiz, int* pos = 0);
       static void serialize(double data, char* buf, int bufsiz, int* pos = 0);

      /* generic static method for deserializing primitive values from buffer */
      static void deserialize(uint16& data, char* buf, int bufsiz, int* pos = 0);
      static void deserialize(uint32& data, char* buf, int bufsiz, int* pos = 0);
      static void deserialize(uint64& data, char* buf, int bufsiz, int* pos = 0);
      static void deserialize(int8& data,   char* buf, int bufsiz, int* pos = 0);
      static void deserialize(int16& data,  char* buf, int bufsiz, int* pos = 0);
      static void deserialize(int32& data,  char* buf, int bufsiz, int* pos = 0);
      static void deserialize(int64& data,  char* buf, int bufsiz, int* pos = 0);
      static void deserialize(float& data,  char* buf, int bufsiz, int* pos = 0);
      static void deserialize(double& data, char* buf, int bufsiz, int* pos = 0);
   };

The non-static methods are divided into methods for packing and unpacking. At first, one should create a new ``CompactDataType`` object. Data can be then added to the object by calling ``add_*`` methods, one at a time. The ``add_*_array`` methods can also be used, in case the user wants to add an array of elements of the same primitive type all at once. In this case, the number of elements needs to be provided as the first parameter. After that, the user will call the ``pack`` method, or the ``pack\_and\_delete`` method to convert the byte stream into the buffer, which the simulator will use to send to the remote machine. The latter is for convenience; it does the pack and then deletes the ``CompactDataType`` object in the same call.

At the destination when the user needs to retrieve the data in the event factory method. The user will first call the ``unpack`` method to convert the data from the byte array. The user then calls the ``get_*`` methods to retrieve data from the object one by one, in the same order as the data was added. That is, the first value added to the object needs to be be retrieved first. The ``CompactDataType`` class assumes the user understands the exact packing sequence of the data inside the object. Trying to retrieve the data of an unmatched type will produce an error exception.

The static methods in the ``CompactDataType`` class are used to serialize/pack a variable of a particular type into the byte array and deserialize/unpack the data from the byte array into a variable of a particular type. These methods provide a way for the user to translate between different data representations without having to create the ``CompactDataType`` object.
 

Random Number Generators
************************

Having a good random number generator is important to stochastic simulation. MiniSSF provides quite a few different random number generators and approximately a dozen random variate generators for common probability distributions. The random generators are ported from three different sources. All random number generators are implemented as derived classes of ``Random``. 

For easy and fast random number generation, we recommend using the Lehmer random number generator, implemented as the ``LehmerRandom`` class. The generator is a linear congruential pseudo random number generator that uses a multiplier of 48271 and a modulus of 2^31-1. The generator has 256 random streams (with a jump multiplier being 22925). For details, consult the book "*Discrete-Event Simulation: a First Course*", by Lawrence Leemis and Steve Park, published by Prentice Hall in 2005. The constructor of a Lehmer random number generator is as follows::

   LehmerRandom(int myseed, int streamidx=0, int nstreams=1)

One must provide a seed to start with and select the type of the random number generator. If the seed is zero, the seed will be picked based on current machine time. Also, one can create separate random streams using the same seed and the same total number of random streams ``nstreams``, but vary the stream index ``streamidx``. 

To go for extremely high quality random number generation, one can use the Mersenne Twister random number generator, which is implemented as the ``MersenneTwisterRandom`` class. Mersenne Twister has a large state size, but renders a period of 2^19937-1 and a sequence that is 623-dimensionally equidistributed. The implementation is modified from that by Richard J. Wagner, which is based on the code written by Makoto Matsumoto, Takuji Nishimura, and Shawn Cokus. For more information, go to the inventors' web page at http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html

Mersenne Twister random number generator provides two constructors::

 	MersenneTwisterRandom (const uint32 one_seed)
	MersenneTwisterRandom (uint32 *const big_seed, const uint32 seed_len=624)

The first constructor uses a simple unsigned integer as the random seed. The second constructor use an array of integers as seed. 

The third option is using the random number generators ported from the SPRNG library (version 2.0), developed by Michael Mascagni et al. at Florida State University. For detailed information, we encourage the user to consult the SPRNG 2.0 User's Guide at http://sprng.cs.fsu.edu/. We implement the generators in the ``SPRNGRandom`` class, which features 5 classes of random number generators:

* **Combined Multiple Recursive Generator (CMRG)**: the period of this generator is around 2^219; the number of distinct streams available is over 10^8.

* **48-Bit Linear Congruential Generator with Prime Addend (LCG)**: the period of this generator is 2^48; the number of distinct streams available is of the order of 2^19.

* **64-Bit Linear Congruential Generator with Prime Addend (LCG64)**: the period of this generator is 2^64; the number of distinct streams available is over 10^8.

* **Modified Lagged Fibonacci Generator (LFG)**: the period of this generator is 2^31*(2^k-1), where k is the lag; the number of distinct streams available is 2^(31(k-1)-1).

* **Multiplicative Lagged Fibonacci Generator (MLFG)**: the period of this generator is 2^61*(2^k-1), where k is the lag; the number of distinct streams available is 2^(63(k-1)-1).

The constructor of the ``SPRNGRandom`` class is declared as follows::

   SPRNGRandom(int seed, int type = SPRNG_TYPE_CMRG, int streamidx = 0, int nstreams = 1);

Like ``LehmerRandom``, one must provide a seed to start with and select the type of the random number generator. If the seed is zero, the seed will be picked based on current machine time. Also, one can create separate random streams using the same seed and the same total number of random streams ``nstreams``, but vary the stream index ``streamidx``. The second argument is the type of the random number generator. The following list all available types of random number generators:

:SPRNG_TYPE_CMRG_LECU1:	Combined multiple recursive generator (CMRG), a = 0x27BB2EE687B0B0FD.
:SPRNG_TYPE_CMRG_LECU2: Combined multiple recursive generator(CMRG), a = 0x2C6FE96EE78B6955.
:SPRNG_TYPE_CMRG_LECU3: Combined multiple recursive generator(CMRG), a = 0x369DEA0F31A53F85.
:SPRNG_TYPE_LCG_CRAYLCG: 48-bit linear congruential generator with prime addend (LCG), a = 0x2875A2E7B175.
:SPRNG_TYPE_LCG_DRAND48: 48-bit linear congruential generator with prime addend (LCG), a = 0x5DEECE66D.
:SPRNG_TYPE_LCG_FISH1:	48-bit linear congruential generator with prime addend (LCG), a = 0x3EAC44605265.
:SPRNG_TYPE_LCG_FISH2:	48-bit linear congruential generator with prime addend (LCG), a = 0x1EE1429CC9F5.
:SPRNG_TYPE_LCG_FISH3: 	48-bit linear congruential generator with prime addend (LCG), a = 0x275B38EB4BBD.
:SPRNG_TYPE_LCG_FISH4:	48-bit linear congruential generator with prime addend (LCG), a = 0x739A9CB08605.
:SPRNG_TYPE_LCG_FISH5:	48-bit linear congruential generator with prime addend (LCG), a = 0x3228D7CC25F5.
:SPRNG_TYPE_LCG64_LECU1: 64-bit linear congruential generator with prime addend (LCG64), a = 0x27BB2EE687B0B0FD.
:SPRNG_TYPE_LCG64_LECU2: 64-bit linear congruential generator with prime addend (LCG64), a = 0x2C6FE96EE78B6955.
:SPRNG_TYPE_LCG64_LECU3: 64-bit linear congruential generator with prime addend (LCG64), a = 0x369DEA0F31A53F85.
:SPRNG_TYPE_LFG_LAG1279: Modified lagged fibonacci generator (LFG), l = 1279, k = 861.
:SPRNG_TYPE_LFG_LAG17:	Modified lagged fibonacci generator (LFG), l = 17, k = 5.
:SPRNG_TYPE_LFG_LAG31: 	Modified lagged fibonacci generator (LFG), l = 31, k = 6.
:SPRNG_TYPE_LFG_LAG55: 	Modified lagged fibonacci generator (LFG), l = 55, k = 24.
:SPRNG_TYPE_LFG_LAG63: 	Modified lagged fibonacci generator (LFG), l = 63, k = 31.
:SPRNG_TYPE_LFG_LAG127:	Modified lagged fibonacci generator (LFG), l = 127, k = 97.
:SPRNG_TYPE_LFG_LAG521:	Modified lagged fibonacci generator (LFG), l = 521, k = 353.
:SPRNG_TYPE_LFG_LAG521B: Modified lagged fibonacci generator (LFG), l = 521, k = 168.
:SPRNG_TYPE_LFG_LAG607:	Modified lagged fibonacci generator (LFG), l = 607, k = 334.
:SPRNG_TYPE_LFG_LAG607B: Modified lagged fibonacci generator (LFG), l = 607, k = 273.
:SPRNG_TYPE_LFG_LAG1279B: Modified lagged fibonacci generator (LFG), l = 1279, k = 419.
:SPRNG_TYPE_MLFG_LAG1279: Multiplicative lagged fibonacci generator (MLFG), l = 1279, k = 861.
:SPRNG_TYPE_MLFG_LAG1279B: Multiplicative lagged fibonacci generator (MLFG), l = 1279, k = 419.
:SPRNG_TYPE_MLFG_LAG17:	Multiplicative lagged fibonacci generator (MLFG), l = 17, k = 5.
:SPRNG_TYPE_MLFG_LAG31:	Multiplicative lagged fibonacci generator (MLFG), l = 31, k = 6.
:SPRNG_TYPE_MLFG_LAG55:	Multiplicative lagged fibonacci generator (MLFG), l = 55, k = 24.
:SPRNG_TYPE_MLFG_LAG63:	Multiplicative lagged fibonacci generator (MLFG), l = 63, k = 31.
:SPRNG_TYPE_MLFG_LAG127: Multiplicative lagged fibonacci generator (MLFG), l = 127, k = 97.
:SPRNG_TYPE_MLFG_LAG521: Multiplicative lagged fibonacci generator (MLFG), l = 521, k = 353.
:SPRNG_TYPE_MLFG_LAG521B: Multiplicative lagged fibonacci generator (MLFG), l = 521, k = 168.
:SPRNG_TYPE_MLFG_LAG607: Multiplicative lagged fibonacci generator (MLFG), l = 607, k = 334.
:SPRNG_TYPE_MLFG_LAG607B: Multiplicative lagged fibonacci generator (MLFG), l = 607, k = 273.
:SPRNG_TYPE_CMRG:	Default combined multiple recursive generator.
:SPRNG_TYPE_LCG: 	Default 48-bit linear congruential generator with prime addend (LCG).
:SPRNG_TYPE_LCG64: 	Default 64-bit linear congruential generator with prime addend (LCG64).
:SPRNG_TYPE_LFG:	Default modified lagged fibonacci generator (LFG).
:SPRNG_TYPE_MLFG: 	Default multiplicative lagged fibonacci generator (MLFG).

All random generators are derived from the ``Random`` class. All derived classes must implement the following three methods::

   virtual double operator()();
   virtual void setSeed(int seed);
   virtual void spawnStreams(int n, Random** streams);

The parentheses operator random a random number uniformly distributed between 0 and 1. The ``setSeed`` sets the random seed (reset it if it's already been set at the creation of the random number generator); if 0, pick one using the system clock. The ``spawnStreams`` method is used to pawn more random streams from the current one. For a given random number generator, it is expected that multiple random streams provide sufficient separation (i.e., with minimal correlation) between random numbers drawn from separate random streams. The user should check with the particular random number generator for the maximum number of random streams that can be used simultaneous and still generate acceptable result. 
The ``spawnStreams`` method takes the number of random streams to be generated (or spawned) from the existing random stream as the first argument. The second argument returns a list of Random objects each corresponding to a newly generated random stream. It is expected the list shall be reclaimed by user afterwards.

The ``Random`` class also supports random variate generation::

    /* continuous distributions */
    double uniform();
    double uniform(double a, double b);
    double exponential(double x);
    double erlang(long n, double x);
    double pareto(double k, double a);
    double normal(double m, double s);
    double lognormal(double a, double b);
    double chisquare(long n);
    double student(long n);

    /* discrete distributions */
    long bernoulli(double p);
    long equilikely(long a, long b);
    long binomial(long n, double p);
    long geometric(double p);
    long pascal(long n, double p);
    long poisson(double m);

These distributions are described below. Integer parameters (shown below in capital letters) are of type ``long``; floating point parameters (shown in lower-case letters) are of type ``double``.

:uniform():	 Uniform distribution between 0 and 1.
:uniform(a,b):	 Uniform distribution between a and b; the mean of the distribution is (a+b)/2, and the variance is (b-a)*(b-a)/12.
:exponential(r): Exponential distribution with rate r. The mean of the distribution is 1/r, and the variance is 1/r^2.
:erlang(N,r):	 N-stage Erlang distribution is the sum of iid exponential distributed random variables each with rate r. The mean of the distribution is N/r and the variance is N/r^2.
:pareto(k,a):	Pareto distribution has a probability density function f(x) = a*k^a/x^(a+1) for x >= k. The range of the random variable is all real numbers greater than k. The mean of the distribution is a*k/(a-1) (for a>1) and the variance is a*k^2/((a-1)^2*(a-2)) (for a>2).
:normal(m,s):	 Normal distribution with mean m and variance s^2.
:lognormal(a,b): Lognormal distribution is the product of a large number of iid variables (in the same way that a normal distribution is the sum of a large number of iid variables). The logarithm of a log normal random variable has a normal distribution. The mean of the distribution is exp(a+0.5*b^2), and variance is (exp(b^2)-1)*exp(2a+b^2).
:chisquare(N):	 Chi-squire distribution is the sum of independent Normal(0,1) distributions squared. The mean of the distribution is N and the variance is 2N.
:student(N):	 Student distribution with N degrees of freedom. The mean is 0 (if N>1), and the variance is N/(N-2), (if N>2).
:bernoulli(p):	 Bernoulli distribution, where p is the probability of getting a head from a coin toss; the mean of the distribution is p, and the variance is p(1-p).
:equilikely(A,B): Choose a random number equally likely from a set of integers ranging from A to B. The range is {A, A+1, ..., B}. The mean of the distribution is (A+B)/2 and the variance is ((B-A+1)*(B-A+1)-1)/12.
:binomial(N,p):	 Binomial distribution is the sum of N Bernoulli distributions. The range of the random variable is {0, 1, ..., N}. The mean is N*p and the variance is N*p*(1-p).
:geometric(p):	 Geometric distribution is the number of coin tosses before a head shows up. The range is all natural numbers {1, 2, ...}. The mean of the distribution is 1/p and the variance is (1-p)/(p*p).
:pascal(N, p):	 Pascal distribution, also known as negative binomial distribution, counts the total number of tails until n heads show up from coin tosses. That is, the probability of y=k means we give the probability of N-1 heads and k failures in k+N-1 trials, and success on the (k+N)th trial. The range is all natural numbers {1, 2, ...}. The mean is N/p and the variance is N*(1-p)/(p*p). Note that Pascal(1, p) is identical to geometric(p).
:poisson(m):	 Poisson distribution has a range of all non-negative integers {0, 1, ...}. The mean of the distribution is m and the variance is also m.

In addition, the ``Random`` class defines a function that can get a permutation of N numbers (indexed from 0 to N-1)::

    void permute(long N, long* A);

The user must provide a pre-allocated array A of size N that contains a list of numbers. The function permute the numbers randomly as the result.


Semaphore
*********

It is frequently the case that a simulation process needs to signal another process about its progress without passing data. Normally, this can be achieved by using two channels: one output channel and one input channel. The channels are mapped together and events are sent as signals from one process to the other. This situation becomes more complicated when there are more processes involved. In situations where the communicating entities are co-aligned, we can use semaphores. A semaphore must be an entity state (that is, it must be a member variable of the user-defined entity class). Processes belonging to the same entity or co-aligned entities can operate on this semaphore in a way similar to an operating system semaphore for inter-process communications (such as in a typical producer-consumer scenario).

MiniSSF implements semaphores in the ``Semaphore`` class::

   class Semaphore {
      Semaphore(Entity* owner, int initval = 1);
      ~Semaphore();

      void wait();
      void signal();
      int value();
   };

A semaphore must be owned by an entity. A pointer to the entity is passed as the first argument of the constructor. The constructor also establishes the initial value of the semaphore, either passed as the second argument, or if ignore set as 1 by default. Note that a semaphore can only be destroyed during the simulation finalization phase (after ``ssf_finalize`` is called). If the user deletes a semaphore, it should be either in the entity's ``wrapup`` method or in the entity's destructor.

A call to the ``wait`` method decrements the value of the semaphore. The method is expected to be called by a simulation process. If the value of the semaphore becomes zero or negative after the decrement, the calling process will be suspended. The blocking process will be added to the end of the semaphore's list of waiting processes. Since a call to the ``wait`` method could block a process from execution, ``wait`` is actually a wait statement.  A call to the ``signal`` method increments the value of the semaphore. If there are processes on the waiting list, the process at the front of the waiting list will be unblocked.  The ``value`` method returns the current value of the semaphore.


Timer
*****

Timers are used for scheduling actions in the simulated future. A timer, represented by the ``Timer`` class, encapsulates a callback function, which can either be a member function of an entity class, a regular function with a designated signature, or the ``callback`` method of the ``Timer`` class. A timer must be owned by an entity (i.e., a timer is treated as an entity state). The timer can be scheduled to fire off at a future simulation time. It can also be cancelled if necessary before it goes off. If a timer goes off, MiniSSF will call the associated callback function with a pointer to the timer object as the function argument if the callback function is an entity method or a regular function. The user can design a derived class of this class to carry meaningful user data so that when the timer goes off, the callback function will be able to extract the data. A skeleton of the ``Timer`` class is shown below::

   class Timer {
      Timer(Entity* owner, void (Entity::*callback)(Timer*));
      Timer(Entity* owner, void (*callback)(Timer*));
      Timer(Entity* owner);
      virtual ~Timer();

      void schedule(VirtualTime delay);
      void reschedule(VirtualTime delay);
      void cancel();

      Entity* owner() const;
      bool isRunning() const;
      VirtualTime time() const;
      
      virtual void callback() {}
   };

The entity owner and the callback function are both set at the time when a timer is constructed. The first constructor uses an entity method as the callback function; the second constructor uses a regular function as the callback function; the third constructor uses the ``callback`` method defined in the ``Timer`` class as the callback function. In the first two cases, the callback function must take a pointer to the ``Timer`` object as its argument and return void. The the third case, the callback function is the timer's ``callback`` method, which does not take any arguments (since the timer object is ``this`` instance). 

The life cycle of a timer consists of only two states. When the timer is first constructed, it is not running. The user can schedule the timer to fire off in the simulation future by calling the ``schedule`` method with a specified delay. After the call, the timer is running. If the timer is already running, the call to the ``schedule`` method raises an error exception. One can cancel a running timer by calling the ``cancel`` method. If the timer is not running, the call to the ``cancel`` method is simply ignored. A cancelled timer is no longer running and the user can schedule it again. One can also reschedule a running timer by calling the ``reschedule`` method. If the timer is already running, the timer will be cancelled first before scheduling the timer to fire off after the specified delay. If the timer is not running, the ``reschedule`` method behaves the same as the ``schedule`` method.

The ``owner`` method returns the entity owner of this timer. The ``isRunning`` method queries the state of the timer; if the timer is running, that is, if the time has scheduled to fire off at a future simulation time, the method returns true. The ``time`` method returns the time at which the timer is scheduled to go off, if the timer is running. If the timer is not running, the return value will be undefined. To delete a timer, if the timer is running, the destructor will first cancel the timer before reclaiming the timer itself.

Quick Memory
************

Quick memory is a memory management layer that provides fast memory allocation and deallocation services at each processor. The speed comes at the cost of additional memory consumption due to fragmentation. 

MiniSSF provides two ways to use quick memory. The first way is to use quick memory by directly calling the allocation and deallocation functions::

   void* ssf_quickmem_malloc(size_t size);
   void ssf_quickmem_free(void* p);

The ``ssf_quickmem_malloc`` method allocates memory of the given size from quick memory. This function is expected to be faster than ``malloc`` or ``new``. The ``ssf_quickmem_free`` deallocates memory and return it to quick memory. And this function is expected to be faster than ``free`` or ``delete``. It is important to know that memory allocation should be matched: a memory block allocated from quick memory should not be deallocated using ``free`` or ``delete``; similarly, 
the user should use ``ssf_quickmem_free`` to reclaim memory blocks from quick memory only.

The second way to use quick memory is via the ``QuickObject`` class. The class is expected to be used as the base class of all objects that require fast memory allocation and deallocation::

   class QuickObject {
      static void* operator new(size_t size);
      static void operator delete(void* p);

      static void* quick_new(size_t size);
      static void quick_delete(void* p);
   };

MiniSSF overloads the ``new`` and ``delete`` operators in this class so that the user can simply use them to access quick memory. The ``QuickObject`` class also provides two other static functions, ``quick_new`` and ``quick_delete`` for allocating and deallocating quick memory. The user can use them the same way as ``ssf_quickmem_malloc`` and ``ssf_quickmem_free``.
