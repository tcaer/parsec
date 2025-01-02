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

void TextSystem_layout(const char* text, size_t len, Vec2 origin, Clay_TextElementConfig *config, Sprite *sprites);

bool TextSystem_is_dirty();

// MARK UI

void UI_init();

void UI_set_state(Vec2 viewport_size);

Clay_RenderCommandArray UI_render_editor();
