BIN := build

TARGET := $(BIN)/parsec

CFLAGS = -g -O0 -Wall -Wextra -Wpedantic -std=c23
CFLAGS += -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter
CFLAGS += -Isrc
CFLAGS += -DDEBUG

LFLAGS = -framework CoreFoundation -framework Cocoa -framework Metal -framework MetalKit

SRCS = src/darwin.m
OBJS = $(SRCS:%=$(BIN)/%.o)

$(TARGET): $(OBJS)
	@clang $(CFLAGS) $(LFLAGS) $^ -o $@
	@echo == compiled parsec ==

$(BIN)/src/%.m.o: src/%.m
	@echo $<...
	@mkdir -p $(@D)
	@clang $< $(CFLAGS) -fobjc-arc -c -o $@

.PHONY: clean
clean:
	rm -rf $(BIN)
