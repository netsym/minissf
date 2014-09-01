.PHONY:	usrman refman devman

all:	usrman refman devman

usrman:	
	$(MAKE) -C usrman html

refman:	refman.doxyconf
	doxygen refman.doxyconf
#	$(MAKE) -C refman-latex

devman:	devman.doxyconf
	doxygen devman.doxyconf
#	$(MAKE) -C devman-latex

clean:
	$(RM) -r refman-html refman-latex
	$(RM) -r devman-html devman-latex
	$(MAKE) -C usrman clean
