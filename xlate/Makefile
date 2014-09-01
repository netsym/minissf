include ../Makefile.include

ifeq ($(LLVM_CLANG), yes)
DIRS = clangit p4
else 
DIRS = p4
endif

all clean:
	@ for dir in $(DIRS); do \
	  ($(MAKE) -C $$dir $@) || exit 1; \
	done
