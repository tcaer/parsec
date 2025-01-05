#define FONTSTASH_IMPLEMENTATION
#include "parsec.h"

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

const unsigned char *FontSystem_get_texture_data(int *width, int *height) {
  return fonsGetTextureData(f_ctx, width, height);
}
