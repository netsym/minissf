DIRS = helloworld muxtree # others must be built one by one
OTHERDIRS = quenet phold netsim echoserver emu-phold
all:
	@ for dir in $(DIRS); do \
	  echo "--- building $$dir ---"; \
	  ($(MAKE) -C $$dir $@) || exit 1; \
	done

clean:
	@ for dir in $(DIRS) $(OTHERDIRS); do \
	  echo "--- cleaning $$dir ---"; \
	  ($(MAKE) -C $$dir $@) || exit 1; \
	done
