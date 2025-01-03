#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// silence annoying library compiler warnings
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wextra-semi"
#include <fontstash/fontstash.h>
#include <clay/clay.h>
#pragma clang diagnostic pop

// MARK primitive decls

#define BYTE sizeof(char)
#define KILOBYTE BYTE * 1024
#define MEGABYTE KILOBYTE * 1024

typedef struct Vec2 {
  float x, y;
} Vec2;

typedef struct Color {
  float r, g, b, a;
} Color;

typedef struct Sprite {
  Vec2 origin, size;
  Vec2 uv_origin, uv_size;
  Color color;
} Sprite;

typedef struct Mouse {
  Vec2 pos;
  Vec2 d_scroll;
  bool pressed;
} Mouse;

// MARK Arenas

typedef struct Arena {
  void *arena;

  void (*release)(struct Arena *arena);
  void* (*alloc)(void *self, size_t size);
  void (*free)(void *self, void *ptr);
} Arena;

#define Arena_release(A) A->release(A)

#define Arena_alloc(A, T, N) A->alloc(A->arena, sizeof(T) * N)

#define Arena_free(A, P) A->free(A->arena, P)

// An arena where all memory is expected to share the same lifetime. This 
// arena simply keeps track of the current offset to allocate the next block memory 
typedef struct BumpArena {
  size_t capacity;
  size_t offset;
  char *memory;
} BumpArena;

Arena *BumpArena_create(char *memory, size_t capacity);

void BumpArena_release(Arena *arena);

void *BumpArena_alloc(void *_self, size_t size);

void BumpArena_free(void *_self, void *ptr);

// MARK TextSystem

#define DEFAULT_FONT_SIZE 14

extern FONScontext *f_ctx;

void TextSystem_init();

void TextSystem_destroy();

void TextSystem_layout(const char* text, size_t len, Vec2 origin, 
                       Clay_TextElementConfig *config, Sprite *sprites, 
                       size_t *num_sprites);

bool TextSystem_is_dirty();

// MARK UI

typedef struct UIContext {
  Arena *arena;
} UIContext;

void UI_init();

void UI_set_state(float dt, Vec2 viewport_size, Mouse *mouse);

Clay_RenderCommandArray EditorView_render(UIContext *ctx);
