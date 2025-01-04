#define FONTSTASH_IMPLEMENTATION
#define CLAY_IMPLEMENTATION
#include "parsec.h"

// MARK defines

#define Color_to_clay(C)                                                       \
  (Clay_Color) { C.r, C.g, C.b, C.a }

// MARK UI decls

typedef struct StyledTextSpan {
  Color backgroundColor;
  Color textColor;
} StyledTextSpan;

// MARK Arenas impls

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

// MARK GapBuffer impls

void GapBuffer_init(GapBuffer *self) {
  self->buffer = calloc(GAP_SIZE, sizeof(char));
  self->gap_start = self->buffer;
  self->gap_end = self->buffer + GAP_SIZE;
  self->buffer_end = self->gap_end;
}

void GapBuffer_destroy(GapBuffer *self) { free(self->buffer); }

static inline size_t GapBuffer_full_length(GapBuffer *self) {
  return self->buffer_end - self->buffer;
}

static inline size_t GapBuffer_gap_length(GapBuffer *self) {
  return self->gap_start - self->gap_end;
}

void GapBuffer_move_chars(GapBuffer *self, char *dest, char *src, size_t len) {
  if (dest == src || len == 0)
    return;

  if (src > dest) {
    if (src + len >= self->buffer_end)
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

void GapBuffer_move_gap_to_selection(GapBuffer *self, Selection *sel) {
  char *head = self->buffer + sel->head;

  // The gap is already at the head
  if (head == self->gap_start)
    return;

  // Shift gap left
  if (head < self->gap_start) {
    size_t len = GapBuffer_gap_length(self);
    GapBuffer_move_chars(self, head + len, head, len);
    self->gap_start = head;
    self->gap_end = head + len;
  } else {
    size_t len = head - self->gap_end;
    GapBuffer_move_chars(self, self->gap_start, self->gap_end,
                         head - self->gap_end);
    self->gap_start += len;
    self->gap_end = head;
  }
}

void GapBuffer_extend_buffer(GapBuffer *self, size_t size) {
  char *orig = self->buffer;
  size_t new_size = GapBuffer_full_length(self) + size;
  self->buffer = realloc(self->buffer, new_size);

  size_t mem_offset = self->buffer - orig;

  self->buffer_end += mem_offset;
  self->gap_start += mem_offset;
  self->gap_end += mem_offset;
}

void GapBuffer_extend_gap(GapBuffer *self) {
  if (self->gap_end - self->gap_start > 0)
    return;

  GapBuffer_extend_buffer(self, GAP_SIZE);
  GapBuffer_move_chars(self, self->gap_end + GAP_SIZE, self->gap_end,
                       self->buffer_end - self->gap_end);

  self->gap_end += GAP_SIZE;
  self->buffer_end += GAP_SIZE;
}

void GapBuffer_put_char(GapBuffer *self, Selection *sel, char c) {
  GapBuffer_move_gap_to_selection(self, sel);
  GapBuffer_extend_gap(self);

  *(self->gap_start++) = c;
}

void GapBuffer_delete_chars(GapBuffer *self, Selection *sel) {
  GapBuffer_move_gap_to_selection(self, sel);

  if (self->gap_start == self->buffer)
    return;

  *(--self->gap_start) = 0;
}

// MARK TextEditor impls

void TextEditor_init(TextEditor *self) {
  memset(&self->selection, 0, sizeof(Selection));
  GapBuffer_init(&self->buffer);
}

void TextEditor_handle_key(TextEditor *self, char c) {
  switch ((int)c) {
  // Backspace
  case 127:
    GapBuffer_delete_chars(&self->buffer, &self->selection);
    self->selection.anchor = --self->selection.head;
    break;
  default:
    GapBuffer_put_char(&self->buffer, &self->selection, c);
    self->selection.anchor = ++self->selection.head;
    break;
  }
}

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

void TextEditorLineIter_init(TextEditorLineIter *self, TextEditor *editor) {
  self->curr = editor->buffer.buffer;
  self->item = (TextEditorLineIterItem){0};
}

static inline bool is_new_line(char c) { return c == '\r' || c == '\n'; }

TextEditorLineIterItem *TextEditorLineIter_next(TextEditorLineIter *self,
                                                TextEditor *editor) {
  if (self->curr > editor->buffer.buffer_end)
    return NULL;

  char *start = self->curr;
  char *end = editor->buffer.buffer_end;
  for (; self->curr <= editor->buffer.buffer_end; self->curr++) {
    if (is_new_line(*self->curr)) {
      end = self->curr;
      self->curr++;
      break;
    }
  }

  TextEditorLineIterItem *item = &self->item;
  item->idx++;
  item->cursor_offset = -1;

  item->len = end - start;
  item->line = start;

  char *head = editor->buffer.buffer + editor->selection.head;
  if (start <= head && head <= end) {
    Clay_TextElementConfig *cfg = CLAY_TEXT_CONFIG({.fontSize = 28});
    Clay_String pre = {head - start, start};
    Clay_Dimensions dims = FontSystem_measure_text(&pre, cfg);
    item->cursor_offset = dims.width;
  }

  return item;
}

StyledTextSpan DEFAULT_SPAN_STYLES = {.backgroundColor = {0, 0, 0, 0},
                                      .textColor = {40, 40, 40, 255}};

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

void TextEditorSpanIter_init(TextEditorSpanIter *self, char *line, size_t len) {
  self->line = line;
  self->curr = self->line;
  self->len = len;
  self->item = (TextEditorSpanIterItem){0};
}

TextEditorSpanIterItem *TextEditorSpanIter_next(TextEditorSpanIter *self,
                                                TextEditor *editor) {
  char *end = self->line + self->len;

  if (self->curr > end)
    return NULL;

  TextEditorSpanIterItem *item = &self->item;

  item->span = self->curr;
  StyledTextSpan styles = DEFAULT_SPAN_STYLES;

  for (; self->curr <= self->line + self->len; self->curr++) {
    // If we encounter the gap, stop the current span, and set the cursor to
    // resume after the gap
    if (self->curr == editor->buffer.gap_start) {
      end = self->curr;
      self->curr = editor->buffer.gap_end + 1;
      break;
    }
  }

  item->len = end - item->span;
  item->styles = styles;

  return item;
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

  CLAY(CLAY_IDI("EditorLineGutter", idx),
       CLAY_LAYOUT({.sizing = {.width = CLAY_SIZING_FIXED(28 * 3)},
                    .childAlignment = {.x = CLAY_ALIGN_X_RIGHT}})) {
    CLAY_TEXT(gutter_str, CLAY_TEXT_CONFIG({.textColor = {124, 111, 100, 255},
                                            .fontSize = 28}));
  }
}

void EditorLine_render_cursor(UIContext *ctx, TextEditorLineIterItem *line) {
  if (line->cursor_offset < 0)
    return;

  CLAY(CLAY_ID("EditorLineCursor"),
       CLAY_FLOATING({.offset = {line->cursor_offset, 0}}),
       CLAY_LAYOUT({.sizing = {CLAY_SIZING_FIXED(2), CLAY_SIZING_FIXED(26)}}),
       CLAY_RECTANGLE({.color = {0, 0, 0, 255}})) {}
}

void EditorLine_render_span(TextEditorSpanIterItem *span) {
  Clay_String span_str = {span->len, span->span};

  CLAY(CLAY_IDI("EditorLineSpan", span->idx),
       CLAY_RECTANGLE({.color = Color_to_clay(span->styles.backgroundColor)})) {
    CLAY_TEXT(
        span_str,
        CLAY_TEXT_CONFIG({.fontSize = 28,
                          .textColor = Color_to_clay(span->styles.textColor)}));
  }
}

void EditorLine_render(UIContext *ctx, TextEditorLineIterItem *line) {
  CLAY(CLAY_IDI("EditorLine", line->idx),
       CLAY_LAYOUT(
           {.sizing = {.width = CLAY_SIZING_GROW({})}, .childGap = 28})) {
    EditorLine_render_gutter(ctx, line->idx);

    CLAY(CLAY_IDI("EditorLineContent", line->idx)) {
      if (line->len > 0) {
        TextEditorSpanIter iter = {0};
        TextEditorSpanIter_init(&iter, line->line, line->len);
        for (TextEditorSpanIterItem *item =
                 TextEditorSpanIter_next(&iter, ctx->editor);
             item != NULL; item = TextEditorSpanIter_next(&iter, ctx->editor)) {
          EditorLine_render_span(item);
        }
      }
      EditorLine_render_cursor(ctx, line);
    }
  }
}

void EditorView_render_editor(UIContext *ctx) {
  CLAY(CLAY_ID("EditorContents"),
       CLAY_LAYOUT({.sizing = {CLAY_SIZING_GROW({}), CLAY_SIZING_GROW({})},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM}),
       CLAY_SCROLL({.vertical = true})) {
    TextEditorLineIter iter = {0};
    TextEditorLineIter_init(&iter, ctx->editor);
    for (TextEditorLineIterItem *item =
             TextEditorLineIter_next(&iter, ctx->editor);
         item != NULL; item = TextEditorLineIter_next(&iter, ctx->editor)) {
      EditorLine_render(ctx, item);
    }
  }
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

    EditorView_render_editor(ctx);
  }

  return Clay_EndLayout();
}
