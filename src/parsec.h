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

typedef struct Globals {
  Vec2 viewport_size;
} Globals;

typedef struct Sprite {
  Vec2 origin, size;
  Vec2 uv_origin, uv_size;
  Color color;
} Sprite;

typedef struct Quad {
  Vec2 origin, size;
  Color background_color;
} Quad;

typedef struct Mouse {
  Vec2 pos;
  Vec2 d_scroll;
  bool pressed;
} Mouse;

// MARK UI decls
typedef struct StyledTextSpan {
  Color backgroundColor;
  Color textColor;
} StyledTextSpan;

// MARK Allocator decls

typedef struct Allocator {
  void *arena;

  void (*release)(struct Allocator *arena);
  void* (*alloc)(void *self, size_t size);
  void (*free)(void *self, void *ptr);
} Allocator;

#define Allocator_new(A, M, C) A##_create(M, C)

#define Allocator_release(A) A->release(A)

#define Allocator_alloc(A, T, N) A->alloc(A->arena, sizeof(T) * N)

#define Allocator_free(A, P) A->free(A->arena, P)

// An arena where all memory is expected to share the same lifetime. This 
// arena simply keeps track of the current offset to allocate the next block memory 
typedef struct Arena {
  size_t capacity;
  size_t offset;
  char *memory;
} BumpArena;

Allocator *Arena_create(char *memory, size_t capacity);

void Arena_release(Allocator *arena);

void *Arena_alloc(void *_self, size_t size);

void Arena_free(void *_self, void *ptr);

// MARK GapBuffer decls

typedef struct GapBuffer {
  char *buffer;
  char *buffer_end;
  char *gap_start;
  char *gap_end;
} GapBuffer;

void GapBuffer_init(GapBuffer *self);

void GapBuffer_destroy(GapBuffer *self);

// MARK TextEditor decls

typedef struct Selection {
  // anchor is the start of the selection, head is where the selection is extended to.
  // head can be extended to be before or after the anchor
  size_t anchor, head;
} Selection;

typedef struct TextEditor {
  GapBuffer buffer;
  Selection selection;
} TextEditor;

void TextEditor_init(TextEditor *self);

// TODO doesn't handle utf-8 or modifiers
void TextEditor_handle_key(TextEditor *self, char c);

// MARK TextEditor iter decls

// Iterates over each line in a text editor
typedef struct TextEditorLineIterItem {
  char *line;
  size_t len;
  size_t idx;
  float cursor_offset;
} TextEditorLineIterItem;

typedef struct TextEditorLineIter {
  char *curr;
  TextEditorLineIterItem item;
} TextEditorLineIter;

void TextEditorLineIter_init(TextEditorLineIter *self, TextEditor *editor);

TextEditorLineIterItem *TextEditorLineIter_next(TextEditorLineIter *self,
                                                TextEditor *editor);

// Iterates over a line to generate blocks groups of styles for spans of chars
// in the line
typedef struct TextEditorSpanIterItem {
  char *span;
  size_t len;
  StyledTextSpan styles;
  size_t idx;
} TextEditorSpanIterItem;

typedef struct TextEditorSpanIter {
  char *line;
  char *curr;
  size_t len;
  TextEditorSpanIterItem item;
} TextEditorSpanIter;

void TextEditorSpanIter_init(TextEditorSpanIter *self, char *line, size_t len);

TextEditorSpanIterItem *TextEditorSpanIter_next(TextEditorSpanIter *self,
                                                TextEditor *editor);

// MARK FontSystem

#define DEFAULT_FONT_SIZE 14

void FontSystem_init();

void FontSystem_destroy();

void FontSystem_layout(const char* text, size_t len, Vec2 origin, 
                       Clay_TextElementConfig *config, Sprite *sprites, 
                       size_t *num_sprites);

Clay_Dimensions FontSystem_measure_text(Clay_String *text,
                                        Clay_TextElementConfig *config);

bool FontSystem_is_dirty();

const unsigned char* FontSystem_get_texture_data(int* width, int* height);

// MARK UI

typedef struct UIContext {
  Allocator *arena;
  TextEditor *editor;
} UIContext;

void UI_init();

void UI_set_state(float dt, Vec2 viewport_size, Mouse *mouse);

Clay_RenderCommandArray EditorView_render(UIContext *ctx);
