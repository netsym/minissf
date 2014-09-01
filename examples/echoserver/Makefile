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
AUTOXFLAGS = -DAUTOXLATE_REQUIRED=yes -DSSFCMDDEBUG=yes

# custom compiler/linker flags
INCLUDES = -I.
CXXFLAGS =
LDFLAGS =
LIBS =

all:	echoserver echoclient

echoserver:	echoserver.o
	$(SSFLD) $(AUTOXFLAGS) $(LDFLAGS) $< -o $@ $(LIBS)

echoserver.o:	echoserver.cc
	$(SSFCPPCXX) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) $< -o $@

echoclient:	echoclient.cc
	$(CXX) $< -o $@

clean:
	$(RM) echoserver echoserver.o echoclient core
	$(RM) $(SSFCLEAN) 

