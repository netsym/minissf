SSFDIR = ../../..
include $(SSFDIR)/Makefile.include

CXXFLAGS = -Wall -O2 -DQUENET
INCLUDES = -I.

HDRFILES = \
  ASBarabasiAlbertModel-1.h \
  ASBarabasiAlbertModel-2.h \
  ASBarabasiAlbertModel.h \
  ASGLPModel.h \
  ASModel.h \
  ASWaxmanModel.h \
  BottomUpHierModel.h \
  Brite.h \
  Edge.h \
  Graph.h \
  ImportedFileModel.h \
  Model.h \
  Node.h \
  Parser.h \
  RouterBarabasiAlbertModel-1.h \
  RouterBarabasiAlbertModel-2.h \
  RouterBarabasiAlbertModel.h \
  RouterGLPModel.h \
  RouterModel.h \
  RouterWaxmanModel.h \
  TopDownHierModel.h \
  Topology.h \
  Util.h

SRCFILES = \
  ASBarabasiAlbertModel-1.cc \
  ASBarabasiAlbertModel-2.cc \
  ASBarabasiAlbertModel.cc \
  ASGLPModel.cc \
  ASModel.cc \
  ASWaxmanModel.cc \
  BottomUpHierModel.cc \
  BriteMain.cc \
  Edge.cc \
  Graph.cc \
  ImportedFileModel.cc \
  Model.cc \
  Node.cc \
  Parser.cc \
  RouterBarabasiAlbertModel-1.cc \
  RouterBarabasiAlbertModel-2.cc \
  RouterBarabasiAlbertModel.cc \
  RouterGLPModel.cc \
  RouterModel.cc \
  RouterWaxmanModel.cc \
  TopDownHierModel.cc \
  Topology.cc \
  Util.cc

OBJFILES = $(SRCFILES:.cc=.o)

brite:  $(OBJFILES)
	$(CXX) $(CXXFLAGS) -o brite $(OBJFILES) -lm

.cc.o:
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJFILES)
	$(RM) model.quenet brite core *~
