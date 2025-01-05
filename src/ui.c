#define CLAY_IMPLEMENTATION
#include "parsec.h"

// MARK defines

#define Color_to_clay(C)                                                       \
  (Clay_Color) { C.r, C.g, C.b, C.a }

// MARK TextEditor iter impls

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

// MARK EditorLine impls

void EditorLine_render_gutter(UIContext *ctx, size_t idx) {
  char temp[256];
  int length = snprintf(temp, sizeof(temp), "%zu", idx);
  char *str = Allocator_alloc(ctx->arena, char, length);
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

// MARK EditorView impls

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
