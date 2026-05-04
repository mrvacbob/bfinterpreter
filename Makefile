CC     = gcc
CFLAGS = -O3 -g -Wall -MMD -MP

INTERPS = bin/switch bin/direct bin/indirect bin/toc

all: $(INTERPS)

bin/%: %-interpreter.c skeleton.h instructions.h | bin
	$(CC) $(CFLAGS) -MF $@.d -o $@ $<

bin:
	mkdir -p bin

-include $(wildcard bin/*.d)

clean:
	rm -f $(INTERPS) bin/*.d

# Smoke-test: run each interpreter on helloworld.bf and verify output.
# toc emits C, so compile and run the result.
check: $(INTERPS)
	@for interp in switch direct indirect; do \
	    if bin/$$interp programs/helloworld.bf 2>/dev/null | grep -q "Hello World!"; then \
	        echo "PASS: $$interp"; \
	    else \
	        echo "FAIL: $$interp"; exit 1; \
	    fi; \
	done
	@tmpdir=$$(mktemp -d); \
	    bin/toc programs/helloworld.bf > $$tmpdir/toc_test.c \
	    && $(CC) -O2 -o $$tmpdir/toc_test $$tmpdir/toc_test.c \
	    && if $$tmpdir/toc_test | grep -q "Hello World!"; then \
	           echo "PASS: toc"; \
	       else \
	           echo "FAIL: toc"; \
	       fi; \
	    rm -rf $$tmpdir

# Debug build: unoptimized with UBSan + ASan for catching runtime errors.
# Use a separate bin/debug/ directory so sanitized and release builds don't mix.
debug: CC_SAN = -fsanitize=address,undefined -fno-omit-frame-pointer
debug: CFLAGS = -Og -g -Wall -MMD -MP $(CC_SAN)
debug: $(INTERPS:bin/%=bin/debug/%)

bin/debug/%: %-interpreter.c skeleton.h instructions.h | bin/debug
	$(CC) $(CFLAGS) -MF $@.d -o $@ $<

bin/debug:
	mkdir -p bin/debug

.PHONY: all clean check debug
