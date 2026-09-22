CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror

.PHONY: test core-test driver clean

test: core-test

core-test:
	$(CC) $(CFLAGS) -Iinclude core/profile.c core/resolver.c tests/test_resolver.c -o /tmp/unbound-resolver-test
	/tmp/unbound-resolver-test
	$(CC) $(CFLAGS) -Iinclude core/semantic_resolver.c tests/test_semantic_resolver.c -o /tmp/semantic-resolver-test
	/tmp/semantic-resolver-test
	$(CC) $(CFLAGS) -Isrc/runtime tests/test_base_tgp_wire.c -o /tmp/base-tgp-wire-test
	/tmp/base-tgp-wire-test

driver:
	$(MAKE) -C driver

clean:
	$(MAKE) -C driver clean
	rm -f /tmp/unbound-resolver-test /tmp/semantic-resolver-test /tmp/base-tgp-wire-test
