include ../../Makefile.include
INCLUDES = -I.

all:	cmtstrip

cmtstrip:	cmtstrip.o
	$(CC) $(LDFLAGS) $< -o $@ $(LEXLIB)

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

ifeq ("x$(HAVE_FLEX)", "xyes")
cmtstrip.c:	cmtstrip.l
	$(LEX) $(LFLAGS) cmtstrip.l
	mv lex.yy.c cmtstrip.c
else
cmtstrip.c:	pre-cmtstrip.c
	cp -f pre-cmtstrip.c cmtstrip.c
endif

clean:
	$(RM) lex.yy.c cmtstrip.c cmtstrip.o cmtstrip
