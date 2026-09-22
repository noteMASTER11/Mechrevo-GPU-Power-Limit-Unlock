CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror

.PHONY: test clean

test:
	$(CC) $(CFLAGS) -Iinclude core/semantic_resolver.c tests/test_semantic_resolver.c -o /tmp/semantic-resolver-test
	/tmp/semantic-resolver-test

clean:
	rm -f /tmp/semantic-resolver-test
