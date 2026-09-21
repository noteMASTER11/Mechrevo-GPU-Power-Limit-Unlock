CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror

.PHONY: test core-test driver clean

test: core-test

core-test:
	$(CC) $(CFLAGS) -Iinclude core/profile.c core/resolver.c tests/test_resolver.c -o /tmp/unbound-resolver-test
	/tmp/unbound-resolver-test

driver:
	$(MAKE) -C driver

clean:
	$(MAKE) -C driver clean
	rm -f /tmp/unbound-resolver-test
