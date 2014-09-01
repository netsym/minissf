include ../../Makefile.include

LLVM_INCLUDES = -I$(LLVM_ROOT)/../include -I$(LLVM_ROOT)/include

CLANG_ROOT = $(LLVM_ROOT)/../tools/clang
CLANG_INCLUDES = -I$(CLANG_ROOT)/include

PLUGIN_INCLUDES = $(LLVM_INCLUDES) $(CLANG_INCLUDES) -I.
CLANG_DEFINITIONS = -D_DEBUG -D_GNU_SOURCE -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS
COMMON_CXXFLAGS=-fno-exceptions -fno-rtti -fPIC -Woverloaded-virtual -Wcast-qual \
  -fno-strict-aliasing -pedantic -Wno-long-long -Wall -W -Wno-unused-parameter \
  -Wwrite-strings
PLUGIN_CXXFLAGS = $(PLUGIN_INCLUDES) $(CLANG_DEFINITIONS) $(COMMON_CXXFLAGS) $(CXXFLAGS)
#PLUGIN_LDFLAGS = -Wl,-R $(LLVM_LIB_PATHS) -lpthread -ldl -lm -share #-R is not portable on mac
PLUGIN_LDFLAGS = $(LLVM_LIB_PATHS) -lpthread -ldl -lm -shared -Wl,-undefined,dynamic_lookup

TARGETS = libannotate.so libtranslate.so checkproc

.PHONY: all clean

all: $(TARGETS)

clean:
	$(RM) $(TARGETS) annotate.o translate.o checkproc.o

annotate.o:	annotate.cpp
	$(CXX) $(VERSION) $(PLUGIN_CXXFLAGS) -c -o $@ $<

translate.o:	translate.cpp
	$(CXX) $(VERSION) $(PLUGIN_CXXFLAGS) -c -o $@ $<

checkproc.o:	checkproc.cpp
	$(CXX) -c -o $@ $<

libannotate.so:	annotate.o
	$(CXX) $(PLUGIN_LDFLAGS) -o $@ $<

libtranslate.so:	translate.o
	$(CXX) $(PLUGIN_LDFLAGS) -o $@ $< 

checkproc:	checkproc.o
	$(CXX) -o $@ $<
