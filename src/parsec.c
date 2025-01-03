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

// MARK FontSystem impls

#define TEXT_ATLAS_SIZE 512

static unsigned char JETBRAINS_MONO[] = {
#embed "/Library/Fonts/JetBrainsMono-Regular.ttf"
};

FONScontext *f_ctx;

void TextSystem_init() {
  FONSparams params = {0};
  params.width = TEXT_ATLAS_SIZE;
  params.height = TEXT_ATLAS_SIZE;
  params.flags = FONS_ZERO_TOPLEFT;
  f_ctx = fonsCreateInternal(&params);
  fonsSetAlign(f_ctx, FONS_ALIGN_TOP);

  fonsAddFontMem(f_ctx, "JetBrainsMonoRegular", JETBRAINS_MONO,
                 sizeof(JETBRAINS_MONO), 0);
}

void TextSystem_destroy() { fonsDeleteInternal(f_ctx); }

void TextSystem_layout(const char *text, size_t len, Vec2 origin,
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

bool TextSystem_is_dirty() {
  return f_ctx->dirtyRect[0] < f_ctx->dirtyRect[2] &&
         f_ctx->dirtyRect[1] < f_ctx->dirtyRect[3];
}

Clay_Dimensions TextSystem_measure_text(Clay_String *text,
                                        Clay_TextElementConfig *config) {
  FONSstate *state = fons__getState(f_ctx);
  state->size = config->fontSize;
  state->spacing = config->letterSpacing;

  float width;
  // Fontstash isn't great at measuring single spaces, so we have to calculate
  // it manually by adding the xadvance with the width of a space
  if (text->length == 1 && text->chars[0] == ' ') {
    FONSglyph *glyph =
        fons__getGlyph(f_ctx, f_ctx->fonts[0], ' ', state->size, state->blur);
    assert(glyph != NULL);
    width = glyph->xadv + (glyph->x1 - glyph->x0);
  } else {
    float bounds[4];
    fonsTextBounds(f_ctx, 0, 0, text->chars, text->chars + text->length,
                   bounds);
    width = bounds[2] - bounds[0];
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
  Clay_SetMeasureTextFunction(TextSystem_measure_text);
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

void EditorView_render_line(UIContext *ctx, Clay_String text, int idx) {
  char temp[256];
  int length = snprintf(temp, sizeof(temp), "%i", idx);
  char *str = Arena_alloc(ctx->arena, char, length);
  memcpy(str, temp, length);
  Clay_String cl_str = {length, str};

  CLAY(CLAY_IDI("EditorLine", idx),
       CLAY_LAYOUT(
           {.sizing = {.width = CLAY_SIZING_GROW({})}, .childGap = 28})) {
    CLAY(CLAY_IDI("EditorLineNumberGutter", idx),
         CLAY_LAYOUT({.sizing = {.width = CLAY_SIZING_FIXED(28 * 3)},
                      .childAlignment = {.x = CLAY_ALIGN_X_RIGHT}})) {
      CLAY_TEXT(cl_str, CLAY_TEXT_CONFIG({.textColor = {124, 111, 100, 255},
                                          .fontSize = 28}));
    }
    CLAY_TEXT(text,
              CLAY_TEXT_CONFIG({.fontSize = 28, .textColor = {0, 0, 0, 255}}));
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

    CLAY(CLAY_ID("EditorContents"),
         CLAY_LAYOUT({.sizing = {CLAY_SIZING_GROW({}), CLAY_SIZING_GROW({})},
                      .layoutDirection = CLAY_TOP_TO_BOTTOM}),
         CLAY_SCROLL({.vertical = true})) {
      for (int i = 0; i < 100; i++) {
        EditorView_render_line(ctx, CLAY_STRING("// TODO implement this"), i);
      }
    }
  }

  return Clay_EndLayout();
}
