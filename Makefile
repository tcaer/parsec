BIN := build

TARGET := $(BIN)/parsec

CFLAGS = -g -O0 -Wall -Wextra -Wpedantic -std=c23 -Isrc -Ibuild -DDEBUG -Wc23-extensions
CFLAGS += -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter

LFLAGS := -framework CoreFoundation -framework Cocoa -framework Metal -framework MetalKit

SRCS := src/parsec.c src/darwin.m
OBJS := $(SRCS:%=$(BIN)/%.o)

SHADERS_SRCS := shaders/shaders.metal
SHADERS := $(SHADERS_SRCS:%.metal=$(BIN)/%.metallib)

LIBS := build/fontstash/fontstash.h build/clay/clay.h

$(TARGET): $(LIBS) $(SHADERS) $(OBJS)
	@clang $(CFLAGS) $(LFLAGS) $(OBJS) -o $@
	@echo == compiled parsec ==

$(BIN)/src/%.c.o: src/%.c
	@echo $<...
	@mkdir -p $(@D)
	@clang $< $(CFLAGS) -c -o $@

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

build/fontstash/fontstash.h:
	@mkdir -p $(@D)
	@git clone -q https://github.com/memononen/fontstash
	@mv fontstash/src/fontstash.h build/fontstash
	@mv fontstash/src/stb_truetype.h build/fontstash
	@rm -rf fontstash
	@echo == setup fontstash ==

build/clay/clay.h:
	@mkdir -p $(@D)
# pin clay to v0.12
	@git clone -q https://github.com/nicbarker/clay && cd clay && git checkout -q 2f67b612560cca6aadb145560beffae7a0460534 && cd ..
	@mv clay/clay.h build/clay
	@rm -rf clay
	@echo == setup clay ==

.PHONY: clean
clean:
	@rm -rf $(BIN)
	@echo == cleaned ==
