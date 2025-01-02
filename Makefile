BIN := build

TARGET := $(BIN)/parsec

CFLAGS = -g -O0 -Wall -Wextra -Wpedantic -std=c23 -Isrc -DDEBUG
CFLAGS += -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter

LFLAGS = -framework CoreFoundation -framework Cocoa -framework Metal -framework MetalKit

SRCS := src/darwin.m
OBJS := $(SRCS:%=$(BIN)/%.o)
SHADERS_SRCS := shaders/shaders.metal
SHADERS := $(SHADERS_SRCS:%.metal=$(BIN)/%.metallib)

$(TARGET): $(SHADERS) $(OBJS)
	@clang $(CFLAGS) $(LFLAGS) $(OBJS) -o $@
	@echo == compiled parsec ==

$(BIN)/src/%.m.o: src/%.m
	@echo $<...
	@mkdir -p $(@D)
	@clang $< $(CFLAGS) -fobjc-arc -c -o $@

$(BIN)/shaders/%.metallib: $(BIN)/shaders/%.ir
	@xcrun -sdk macosx metallib -o $@ $<
	@echo == compiled mtl shaders ==

$(BIN)/shaders/%.ir: shaders/%.metal
	@echo $<...
	@mkdir -p $(@D)
	@xcrun -sdk macosx metal -c -o $@ $<
	
.PHONY: clean
clean:
	rm -rf $(BIN)
