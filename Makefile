CC     = gcc
CFLAGS = -O3 -g -MMD -MP

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
	@bin/toc programs/helloworld.bf > /tmp/bfi_toc_test.c \
	    && $(CC) -O2 -o /tmp/bfi_toc_test /tmp/bfi_toc_test.c \
	    && if /tmp/bfi_toc_test | grep -q "Hello World!"; then \
	           echo "PASS: toc"; \
	       else \
	           echo "FAIL: toc"; exit 1; \
	       fi

.PHONY: all clean check
