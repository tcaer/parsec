#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// stb_truetype has many unused functions, silence this warning (it's annoying af)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include <fontstash/fontstash.h>
#pragma clang diagnostic pop

// MARK primitive decls

typedef struct Vec2 {
  float x, y;
} Vec2;

typedef struct Sprite {
  Vec2 origin, size;
  Vec2 uv_origin, uv_size;
} Sprite;

// MARK TextSystem

#define DEFAULT_FONT_SIZE 14

extern FONScontext *f_ctx;

void TextSystem_init();

void TextSystem_destroy();

void TextSystem_layout(const char* text, size_t len, Vec2 origin, Sprite **sprites);

void TextSystem_update_font_size(unsigned int font_size, unsigned int scale_factor);

bool TextSystem_is_dirty();
