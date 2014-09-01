include Makefile.include
INCLUDES = -I.

# event list handling
EVTLIST_HEADERS = \
	evtlist/simevent.h \
	evtlist/splaytree.h \
	evtlist/binheap.h \
	evtlist/ladderq.h
EVTLIST_SOURCES = 
EVTLIST_CXXFILES = $(filter %.cc,$(EVTLIST_SOURCES))
EVTLIST_CFILES = $(filter %.c,$(EVTLIST_SOURCES))
EVTLIST_OBJECTS = $(EVTLIST_CXXFILES:.cc=.oo) $(EVTLIST_CFILES:.c=.o)

# random number generators
RANDOM_HEADERS = \
	random/random.h \
	random/lehmer.h \
	random/mersenne_twister.h \
	random/lecuyer.h \
	random/sprng.h \
	random/sprng_cmrg.h \
	random/sprng_int64.h \
	random/sprng_interface.h \
	random/sprng_lcg.h \
	random/sprng_lcg64.h \
	random/sprng_lfg.h \
	random/sprng_mlfg.h \
	random/sprng_multiply.h \
	random/sprng_primelist_32.h \
	random/sprng_primelist_64.h \
	random/sprng_primes_32.h \
	random/sprng_primes_64.h \
	random/sprng_simple_.h \
	random/sprng_sprng.h \
	random/sprng_store.h
RANDOM_SOURCES = \
	random/random.cc \
	random/mersenne_twister.cc \
	random/lecuyer.cc \
	random/sprng.cc \
	random/sprng_checkid.c \
	random/sprng_cmrg.c \
	random/sprng_lcg.c \
	random/sprng_lcg64.c \
	random/sprng_lfg.c \
	random/sprng_makeseed.c \
	random/sprng_mlfg.c \
	random/sprng_primes_32.c \
	random/sprng_primes_64.c \
	random/sprng_sprng.c \
	random/sprng_store.c
RANDOM_CXXFILES = $(filter %.cc,$(RANDOM_SOURCES))
RANDOM_CFILES = $(filter %.c,$(RANDOM_SOURCES))
RANDOM_OBJECTS = $(RANDOM_CXXFILES:.cc=.oo) $(RANDOM_CFILES:.c=.o)

# ssf api (exposed to ssf users)
SSFAPI_HEADERS = \
	ssfapi/virtual_time.h \
	ssfapi/ssf_common.h \
	ssfapi/compact_datatype.h \
	ssfapi/quick_memory.h \
	ssfapi/ssf_timer.h \
	ssfapi/ssf_semaphore.h \
	ssfapi/event.h \
	ssfapi/procedure.h \
	ssfapi/process.h \
	ssfapi/entity.h \
	ssfapi/inchannel.h \
	ssfapi/outchannel.h
SSFAPI_SOURCES = \
	ssfapi/virtual_time.cc \
	ssfapi/compact_datatype.cc \
	ssfapi/quick_memory.cc \
	ssfapi/ssf_timer.cc \
	ssfapi/ssf_semaphore.cc \
	ssfapi/event.cc \
	ssfapi/procedure.cc \
	ssfapi/process.cc \
	ssfapi/entity.cc \
	ssfapi/inchannel.cc \
	ssfapi/outchannel.cc
SSFAPI_CXXFILES = $(filter %.cc,$(SSFAPI_SOURCES))
SSFAPI_CFILES = $(filter %.c,$(SSFAPI_SOURCES))
SSFAPI_OBJECTS = $(SSFAPI_CXXFILES:.cc=.oo) $(SSFAPI_CFILES:.c=.o)

# internal to simulation kernel
KERNEL_HEADERS = \
	kernel/throwable.h \
	kernel/ssfmachine.h \
	kernel/timestamp.h \
	kernel/kernel_event.h \
	kernel/binque.h \
	kernel/timeline.h \
	kernel/stargate.h \
	kernel/universe.h \
	ssf.h
KERNEL_SOURCES = \
	kernel/ssfmachine.cc \
	kernel/kernel_event.cc \
	kernel/binque.cc \
	kernel/timeline.cc \
	kernel/stargate.cc \
	kernel/universe.cc \
	kernel/universe_cmdline.cc \
	kernel/universe_mapping.cc \
	kernel/universe_sched.cc \
	kernel/universe_align.cc \
	kernel/ssf.cc
KERNEL_CXXFILES = $(filter %.cc,$(KERNEL_SOURCES))
KERNEL_CFILES = $(filter %.c,$(KERNEL_SOURCES))
KERNEL_OBJECTS = $(KERNEL_CXXFILES:.cc=.oo) $(KERNEL_CFILES:.c=.o)

HEADERS = $(EVTLIST_HEADERS) $(RANDOM_HEADERS) $(SSFAPI_HEADERS) $(KERNEL_HEADERS) 
SOURCES = $(EVTLIST_SOURCES) $(RANDOM_SOURCES) $(SSFAPI_SOURCES) $(KERNEL_SOURCES) 
OBJECTS = $(EVTLIST_OBJECTS) $(RANDOM_OBJECTS) $(SSFAPI_OBJECTS) $(KERNEL_OBJECTS) 

.PHONY:	all examples doc srclist
.PHONY: xlate-clean metis-clean examples-clean doc-clean clean distclean

LIBRARIES = libssf.a

all:	$(LIBRARIES) xlate-touch

Makefile.include:
	@ echo "You need to run ./configure first!!!" && false

libssf.a:	Makefile.include $(OBJECTS) metis-touch
	$(AR) $(ARFLAGS) $@ $(OBJECTS)
	$(RANLIB) $@

xlate-touch:
	@echo "--- building xlate ---"
	$(MAKE) -C xlate
	touch xlate-touch
	@echo "--- done building xlate ---"
xlate-clean:
	@echo "--- cleaning xlate ---"
	$(MAKE) -C xlate clean
	$(RM) xlate-touch

examples:	$(LIBRARIES)
	@echo "--- building examples ---"
	$(MAKE) -C examples
	@echo "--- done building examples ---"
examples-clean:
	@echo "--- cleaning examples ---"
	$(MAKE) -C examples clean
	@echo "--- done cleaning examples ---"

doc:
	@echo "--- building documents ---"
	$(MAKE) -C doc
	@echo "--- done building documents ---"
doc-clean:
	@echo "--- cleaning documents ---"
	$(MAKE) -C doc clean
	@echo "--- done cleaning documents ---"

%.o:	%.c $(HEADERS)
	$(MPICC) -c $(INCLUDES) $(CFLAGS) $< -o $@

%.oo:	%.cc $(HEADERS)
	$(MPICXX) -c $(INCLUDES) $(CXXFLAGS) $< -o $@

kernel/universe_align.oo:	kernel/universe_align.cc $(HEADERS)
	$(MPICXX) -c $(INCLUDES) -Ikernel/metis $(CXXFLAGS) $< -o $@

clean:	xlate-clean examples-clean doc-clean metis-clean
	$(RM) $(OBJECTS) $(LIBRARIES)
	$(RM) core *~

metis-touch:
	@echo "--- building metis ---"
	$(MAKE) -C kernel/metis
	touch metis-touch
	@echo "--- done building metis ---"

metis-clean:
	@echo "--- cleaning metis ---"
	$(MAKE) -C kernel/metis clean
	$(RM) metis-touch
	@echo "--- done cleaning metis ---"

distclean:	clean
	$(RM) -r autom4te.cache
	$(RM) Makefile.include ssf_config.h config.log config.status

srclist:
	@ echo "EVTLIST_HEADERS:" $(EVTLIST_HEADERS)
	@ echo "EVTLIST_SOURCES:" $(EVTLIST_SOURCES)
	@ echo "EVTLIST_OBJECTS:" $(EVTLIST_OBJECTS)
	@ echo "RANDOM_HEADERS:" $(RANDOM_HEADERS)
	@ echo "RANDOM_SOURCES:" $(RANDOM_SOURCES)
	@ echo "RANDOM_OBJECTS:" $(RANDOM_OBJECTS)
	@ echo "SSFAPI_HEADERS:" $(SSFAPI_HEADERS)
	@ echo "SSFAPI_SOURCES:" $(SSFAPI_SOURCES)
	@ echo "SSFAPI_OBJECTS:" $(SSFAPI_OBJECTS)
	@ echo "KERNEL_HEADERS:" $(KERNEL_HEADERS)
	@ echo "KERNEL_SOURCES:" $(KERNEL_SOURCES)
	@ echo "KERNEL_OBJECTS:" $(KERNEL_OBJECTS)
