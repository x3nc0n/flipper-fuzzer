# test/Makefile — FluckFlock host-side test suite
#
# Compiles and runs ONLY the pure-C generator sources (no Flipper SDK, no
# radio/ files).  Requires a C11-capable host compiler (gcc or clang).
#
# Usage (from repo root or test/ directory):
#
#   cd test && make test         # build + run all tests
#   cd test && make              # build only
#   cd test && make clean        # remove binaries
#
# On Windows run inside WSL or MinGW/MSYS2.
#
# To use an explicit compiler:
#   make CC=gcc test
#   make CC=clang test

CC      ?= cc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic \
           -I../flipperzero/fluckflock/generators

# Pure generator sources — the only files compiled here.
# No Flipper SDK, no radio/ files.
GEN_SRCS := ../flipperzero/fluckflock/generators/prng.c \
            ../flipperzero/fluckflock/generators/identifiers.c

PRNG_BIN   := test_prng
IDENTS_BIN := test_identifiers

.PHONY: all test clean

all: $(PRNG_BIN) $(IDENTS_BIN)

$(PRNG_BIN): test_prng.c $(GEN_SRCS) test_util.h
	$(CC) $(CFLAGS) -o $@ test_prng.c \
		../flipperzero/fluckflock/generators/prng.c

$(IDENTS_BIN): test_identifiers.c $(GEN_SRCS) test_util.h
	$(CC) $(CFLAGS) -o $@ test_identifiers.c \
		../flipperzero/fluckflock/generators/prng.c \
		../flipperzero/fluckflock/generators/identifiers.c

test: all
	@echo ""
	@echo "--- Running $(PRNG_BIN) ---"
	./$(PRNG_BIN)
	@echo ""
	@echo "--- Running $(IDENTS_BIN) ---"
	./$(IDENTS_BIN)
	@echo ""
	@echo "=============================="
	@echo " All test suites PASSED."
	@echo "=============================="

clean:
	rm -f $(PRNG_BIN) $(IDENTS_BIN)
