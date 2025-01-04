#define FONTSTASH_IMPLEMENTATION
#define CLAY_IMPLEMENTATION
#include "parsec.h"

// MARK Arenas

Arena *BumpArena_create(char *memory, size_t capacity) {
  BumpArena *self = malloc(sizeof(BumpArena));
  self->capacity = capacity;
  self->offset = 0;
  self->memory = memory;

  Arena *arena = malloc(sizeof(Arena));
  arena->arena = self;
  arena->release = BumpArena_release;
  arena->alloc = BumpArena_alloc;
  arena->free = BumpArena_free;

  return arena;
}

void BumpArena_release(Arena *arena) {
  free(arena->arena);
  free(arena);
}

void *BumpArena_alloc(void *_self, size_t size) {
  BumpArena *self = _self;

  assert(self->offset + size < self->capacity);

  void *alloced = self->memory + self->offset;
  self->offset += size;
  return alloced;
}

void BumpArena_free(void *_self, void *ptr) {
  // noop
}

// MARK TextSystem

void GapBuffer_init(GapBuffer *self) {
  self->text = calloc(GAP_SIZE, sizeof(char));
  self->cursor = self->text;
  self->gap_start = self->text;
  self->gap_end = self->text + GAP_SIZE;
  self->text_end = self->gap_end;
}

void GapBuffer_destroy(GapBuffer *self) { free(self->text); }

size_t GapBuffer_full_length(GapBuffer *self) {
  return self->text_end - self->text;
}

size_t GapBuffer_gap_length(GapBuffer *self) {
  return self->gap_start - self->gap_end;
}

void GapBuffer_move_chars(GapBuffer *self, char *dest, char *src, size_t len) {
  if (dest == src || len == 0)
    return;

  if (src > dest) {
    if (src + len >= self->text_end)
      return;
    for (; len > 0; len--) {
      *(dest++) = *(src++);
    }
  } else {
    src += len;
    dest += len;
    for (; len > 0; len--) {
      *(--dest) = *(--src);
    }
  }
}

void GapBuffer_move_gap_to_point(GapBuffer *self) {
  if (self->cursor == self->gap_start)
    return;

  if (self->cursor == self->gap_end) {
    self->cursor = self->gap_start;
    return;
  }

  if (self->cursor < self->gap_start) {
    GapBuffer_move_chars(self, self->cursor + (self->gap_end - self->gap_start),
                         self->cursor, self->gap_start - self->cursor);
    self->gap_end -= self->gap_start - self->cursor;
    self->gap_start = self->cursor;
  } else {
    GapBuffer_move_chars(self, self->gap_start, self->gap_end,
                         self->cursor - self->gap_end);
    self->gap_start += self->cursor - self->gap_end;
    self->gap_end = self->cursor;
    self->cursor = self->gap_start;
  }
}

void GapBuffer_extend_buffer(GapBuffer *self, size_t size) {
  char *orig = self->text;
  size_t new_size = GapBuffer_full_length(self) + size;
  self->text = realloc(self->text, new_size);

  size_t mem_offset = self->text - orig;

  self->cursor += mem_offset;
  self->text_end += mem_offset;
  self->gap_start += mem_offset;
  self->gap_end += mem_offset;
}

void GapBuffer_extend_gap(GapBuffer *self) {
  if (self->gap_end - self->gap_start >= GAP_SIZE)
    return;

  GapBuffer_extend_buffer(self, GAP_SIZE);
  GapBuffer_move_chars(self, self->gap_end + GAP_SIZE, self->gap_end,
                       self->text_end - self->gap_end);

  self->gap_end += GAP_SIZE;
  self->text_end += GAP_SIZE;
}

void GapBuffer_insert_char(GapBuffer *self, char c) {
  if (self->cursor != self->gap_start)
    GapBuffer_move_gap_to_point(self);

  if (self->gap_start == self->gap_end)
    GapBuffer_extend_gap(self);

  *(self->gap_start++) = c;
}

void GapBuffer_put_char(GapBuffer *self, char c) {
  GapBuffer_insert_char(self, c);
  self->cursor++;
}

void GapBuffer_delete_char(GapBuffer *self) {
  if (self->cursor == self->text)
    return;

  if (self->cursor != self->gap_start)
    GapBuffer_move_gap_to_point(self);

  self->cursor--;
  *(--self->gap_start) = 0;
}

// MARK FontSystem impls

#define TEXT_ATLAS_SIZE 512

static unsigned char JETBRAINS_MONO[] = {
#embed "/Library/Fonts/JetBrainsMono-Regular.ttf"
};

FONScontext *f_ctx;

void FontSystem_init() {
  FONSparams params = {0};
  params.width = TEXT_ATLAS_SIZE;
  params.height = TEXT_ATLAS_SIZE;
  params.flags = FONS_ZERO_TOPLEFT;
  f_ctx = fonsCreateInternal(&params);
  fonsSetAlign(f_ctx, FONS_ALIGN_TOP);

  fonsAddFontMem(f_ctx, "JetBrainsMonoRegular", JETBRAINS_MONO,
                 sizeof(JETBRAINS_MONO), 0);
}

void FontSystem_destroy() { fonsDeleteInternal(f_ctx); }

void FontSystem_layout(const char *text, size_t len, Vec2 origin,
                       Clay_TextElementConfig *config, Sprite *sprites,
                       size_t *num_sprites) {
  FONSstate *state = fons__getState(f_ctx);
  state->size = config->fontSize;
  state->spacing = config->letterSpacing;

  FONStextIter iter = {0};
  assert(fonsTextIterInit(f_ctx, &iter, origin.x, origin.y + config->lineHeight,
                          text, text + len));
  FONSquad quad = {0};
  for (; fonsTextIterNext(f_ctx, &iter, &quad); (*num_sprites)++) {
    sprites[*num_sprites] =
        (Sprite){{quad.x0, quad.y0},
                 {quad.x1 - quad.x0, quad.y1 - quad.y0},
                 {quad.s0, quad.t0},
                 {quad.s1 - quad.s0, quad.t1 - quad.t0},
                 {config->textColor.r / 255, config->textColor.g / 255,
                  config->textColor.b / 255, config->textColor.a / 255}};
  }
}

bool FontSystem_is_dirty() {
  return f_ctx->dirtyRect[0] < f_ctx->dirtyRect[2] &&
         f_ctx->dirtyRect[1] < f_ctx->dirtyRect[3];
}

Clay_Dimensions FontSystem_measure_text(Clay_String *text,
                                        Clay_TextElementConfig *config) {
  FONSstate *state = fons__getState(f_ctx);
  state->size = config->fontSize;
  state->spacing = config->letterSpacing;

  float bounds[4];
  fonsTextBounds(f_ctx, 0, 0, text->chars, text->chars + text->length, bounds);
  float width = bounds[2] - bounds[0];

  // If the last char is a space, fons will not have accounted for its xadvance,
  // so we must add it manually
  if (*(text->chars + text->length - 1) == ' ') {
    FONSglyph *glyph =
        fons__getGlyph(f_ctx, f_ctx->fonts[0], ' ', state->size, state->blur);
    assert(glyph != NULL);
    width += glyph->xadv;
  }

  return (Clay_Dimensions){width, config->fontSize};
}

// MARK UI impls

void *clay_memory;

void UI_init() {
  unsigned int memory_size = Clay_MinMemorySize();
  clay_memory = malloc(memory_size);
  Clay_Arena arena =
      Clay_CreateArenaWithCapacityAndMemory(memory_size, clay_memory);
  Clay_SetMeasureTextFunction(FontSystem_measure_text);
  Clay_Initialize(arena, (Clay_Dimensions){2560, 1440});
}

void UI_set_state(float dt, Vec2 viewport_size, Mouse *mouse) {
  Clay_SetLayoutDimensions((Clay_Dimensions){viewport_size.x, viewport_size.y});
  Clay_SetPointerState((Clay_Vector2){mouse->pos.x, mouse->pos.y},
                       mouse->pressed);
  Clay_UpdateScrollContainers(
      false, (Clay_Vector2){mouse->d_scroll.x, mouse->d_scroll.y}, dt);
}

// MARK UI component impls

// MARK EditorView impls

void EditorLine_render_gutter(UIContext *ctx, size_t idx) {
  char temp[256];
  int length = snprintf(temp, sizeof(temp), "%zu", idx);
  char *str = Arena_alloc(ctx->arena, char, length);
  memcpy(str, temp, length);
  Clay_String gutter_str = {length, str};

  CLAY(CLAY_IDI("EditorLineNumberGutter", idx),
       CLAY_LAYOUT({.sizing = {.width = CLAY_SIZING_FIXED(28 * 3)},
                    .childAlignment = {.x = CLAY_ALIGN_X_RIGHT}})) {
    CLAY_TEXT(gutter_str, CLAY_TEXT_CONFIG({.textColor = {124, 111, 100, 255},
                                            .fontSize = 28}));
  }
}

void EditorLine_render_cursor(UIContext *ctx, char *start, size_t idx) {
  float offset = 0;
  if (start < ctx->text->cursor) {
    Clay_TextElementConfig *cfg = CLAY_TEXT_CONFIG({.fontSize = 28});
    Clay_String text = {ctx->text->cursor - start, start};
    Clay_Dimensions dims = FontSystem_measure_text(&text, cfg);
    offset = dims.width;
  }

  CLAY(CLAY_IDI("EditorLineCursor", idx),
       CLAY_FLOATING({.offset = {offset, 0}}),
       CLAY_LAYOUT({.sizing = {CLAY_SIZING_FIXED(2), CLAY_SIZING_FIXED(26)}}),
       CLAY_RECTANGLE({.color = {0, 0, 0, 255}})) {}
}

void EditorLine_render(UIContext *ctx, char *start, char *end, size_t idx) {
  Clay_String line_str;
  if (end == start) {
    line_str = (Clay_String){0, ""};
  } else if (start <= ctx->text->gap_start && end >= ctx->text->gap_end) {
    size_t len = ctx->text->gap_start - start + end - ctx->text->gap_end;
    char *text = Arena_alloc(ctx->arena, char, len);
    memcpy(text, start, ctx->text->gap_start - start);
    memcpy(text + (ctx->text->gap_start - start), start,
           end - ctx->text->gap_end);
    line_str = (Clay_String){len, text};
  } else {
    line_str = (Clay_String){end - start, start};
  }

  bool has_cursor = start <= ctx->text->cursor && ctx->text->cursor <= end;

  CLAY(CLAY_IDI("EditorLine", idx),
       CLAY_LAYOUT(
           {.sizing = {.width = CLAY_SIZING_GROW({})}, .childGap = 28})) {
    EditorLine_render_gutter(ctx, idx);
    CLAY(CLAY_IDI("EditorLineText", idx)) {
      CLAY_TEXT(line_str, CLAY_TEXT_CONFIG(
                              {.fontSize = 28, .textColor = {0, 0, 0, 255}}));
      if (has_cursor) {
        EditorLine_render_cursor(ctx, start, idx);
      }
    }
  }
}

void EditorView_render_editor(UIContext *ctx) {
  char *line_start = ctx->text->text;
  char *curr = line_start;
  size_t row_idx = 1;

  for (; curr <= ctx->text->text_end; curr++) {
    if (*curr == '\n' || *curr == '\r') {
      EditorLine_render(ctx, line_start, curr, row_idx++);
      line_start = curr + 1;
    }
  }

  if (line_start < curr)
    EditorLine_render(ctx, line_start, curr - 1, row_idx);
}

Clay_RenderCommandArray EditorView_render(UIContext *ctx) {
  Clay_BeginLayout();

  CLAY(CLAY_ID("EditorContainer"),
       CLAY_LAYOUT({.sizing = {CLAY_SIZING_GROW({}), CLAY_SIZING_GROW({})},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM}),
       CLAY_RECTANGLE({.color = {251, 241, 199, 255}})) {
    CLAY(CLAY_ID("TitleBarFiller"),
         CLAY_LAYOUT(
             {.sizing = {CLAY_SIZING_GROW({}), CLAY_SIZING_FIXED(56)}})) {}

    CLAY(CLAY_ID("EditorContents"),
         CLAY_LAYOUT({.sizing = {CLAY_SIZING_GROW({}), CLAY_SIZING_GROW({})},
                      .layoutDirection = CLAY_TOP_TO_BOTTOM}),
         CLAY_SCROLL({.vertical = true})) {
      EditorView_render_editor(ctx);
    }
  }

  return Clay_EndLayout();
}
