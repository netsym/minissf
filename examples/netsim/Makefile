# The Makefile must first include the following file so that we have
# the necessary definitions (such as SSFCPPCXX, SSFLD, SSFCLEAN)
include ../../Makefile.include

# If you have instrumented source code (i.e., adding "//! SSF ..."
# comments), you can set AUTOXLATE_REQUIRED to no, which means that
# minissf doesn't need llvm/clang for source code translation (you do
# that); otherwise, set AUTOXLATE_REQUIRED to yes so that minissf will
# do all automatic translations (which would require llvm/clang in
# place). Include -DSSFCMDDEBUG=yes if you want detailed information
# how your code is compiled (for expert only).
AUTOXFLAGS = -DAUTOXLATE_REQUIRED=yes #-DSSFCMDDEBUG=yes

# custom compiler/linker flags
INCLUDES = -I.
CXXFLAGS =
LDFLAGS =
LIBS =

HEADERS = netsim.h ippacket.h host.h tcpsess.h router.h
SOURCES = netsim.cc host.cc tcpsess.cc router.cc
OBJECTS = $(SOURCES:.cc=.o)
XFILES = $(SOURCES:.cc=.x)

all:	netsim

netsim:	$(OBJECTS)
	$(SSFLD) $(AUTOXFLAGS) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS)

netsim.o:	pre_netsim.cc plist.x
	$(SSFCXX) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) pre_netsim.cc -x plist.x -o $@
host.o:	pre_host.cc plist.x
	$(SSFCXX) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) pre_host.cc -x plist.x -o $@
tcpsess.o:	pre_tcpsess.cc plist.x
	$(SSFCXX) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) pre_tcpsess.cc -x plist.x -o $@
router.o:	pre_router.cc plist.x
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
