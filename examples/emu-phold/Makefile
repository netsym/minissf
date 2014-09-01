# The Makefile must first include the following file so that we have
# the necessary definitions (such as SSFCPPCXX, SSFLD, SSFCLEAN)
MINISSFDIR = ../..
include $(MINISSFDIR)/Makefile.include

# If you have instrumented source code (i.e., adding "//! SSF ..."
# comments), you can set AUTOXLATE_REQUIRED to no, which means that
# minissf doesn't need llvm/clang for source code translation (you do
# that); otherwise, set AUTOXLATE_REQUIRED to yes so that minissf will
# do all automatic translations (which would require llvm/clang in
# place). Include -DSSFCMDDEBUG=yes if you want detailed information
# how your code is compiled (for expert only).
AUTOXFLAGS = -DAUTOXLATE_REQUIRED=no -DSSFCMDDEBUG=yes

# custom compiler/linker flags
INCLUDES = -I.
CXXFLAGS = -Wall
LDFLAGS =
LIBS =

all:	phold

phold:	phold.o
	$(SSFLD) $(AUTOXFLAGS) $(LDFLAGS) $< -o $@ $(LIBS)

phold.o:	phold.cc
	$(SSFCPPCXX) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) $< -o $@

clean:
	$(RM) phold phold.o core
	$(RM) $(SSFCLEAN) 

