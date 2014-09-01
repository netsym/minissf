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
AUTOXFLAGS = -DAUTOXLATE_REQUIRED=no -DSSFCMDDEBUG=yes

# custom compiler/linker flags
INCLUDES = -I. -I../../kernel/metis
CXXFLAGS =
LDFLAGS =
LIBS = #-Lmetis -lmetis

HEADERS = quenet.h graph.h
SOURCES = quenet.cc graph.cc
OBJECTS = $(SOURCES:.cc=.o)
XFILES = $(SOURCES:.cc=.x)

.PHONY:	#metis metis-clean

all:	quenet

quenet:	$(OBJECTS) #metis
	$(SSFLD) $(AUTOXFLAGS) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS)

quenet.o:	pre_quenet.cc plist.x
	$(SSFCXX) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) pre_quenet.cc -x plist.x -o $@
graph.o:	pre_graph.cc plist.x
	$(SSFCXX) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) pre_graph.cc -x plist.x -o $@

pre_quenet.cc quenet.x: quenet.cc $(HEADERS)
	$(SSFCPP) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) $< -o pre_quenet.cc -x quenet.x
pre_graph.cc graph.x: graph.cc $(HEADERS)
	$(SSFCPP) $(AUTOXFLAGS) $(INCLUDES) $(CXXFLAGS) $< -o pre_graph.cc -x graph.x

plist.x: $(XFILES)
	$(SSFCKPR) $(AUTOXFLAGS) $(XFILES) -o $@

clean:	brite-clean #metis-clean 
	$(RM) quenet $(OBJECTS) core
	$(RM) $(SSFCLEAN) 
	$(RM) pre_*.cc *.x

#metis:
#	$(MAKE) -C metis
#
#metis-clean:
#	$(MAKE) -C metis clean

brite:
	$(MAKE) -C brite

brite-clean:
	$(MAKE) -C brite clean
