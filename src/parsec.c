#define FONTSTASH_IMPLEMENTATION
#include "parsec.h"

// MARK FontSystem

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

  fonsAddFontMem(f_ctx, "JetBrainsMonoRegular", JETBRAINS_MONO,
                 sizeof(JETBRAINS_MONO), 0);
}

void TextSystem_destroy() { fonsDeleteInternal(f_ctx); }

void TextSystem_layout(const char *text, size_t len, Vec2 origin,
                       Sprite **sprites) {
  if (len == 0) {
    *sprites = NULL;
    return;
  }

  // TODO verify this is the correct way to initialize the length
  *sprites = malloc(sizeof(Sprite) * len);

  FONStextIter iter = {0};
  assert(fonsTextIterInit(f_ctx, &iter, origin.x, origin.y, text, text + len));
  FONSquad quad = {0};
  for (size_t s = 0; fonsTextIterNext(f_ctx, &iter, &quad); s++) {
    (*sprites)[s] = (Sprite){{quad.x0, quad.y0},
                             {quad.x1 - quad.x0, quad.y1 - quad.y0},
                             {quad.s0, quad.t0},
                             {quad.s1 - quad.s0, quad.t1 - quad.t0}};
  }
}

void TextSystem_update_font_size(unsigned int font_size,
                                 unsigned int scale_factor) {
  fonsSetSize(f_ctx, font_size * scale_factor);
}

bool TextSystem_is_dirty() {
  return f_ctx->dirtyRect[0] < f_ctx->dirtyRect[2] &&
         f_ctx->dirtyRect[1] < f_ctx->dirtyRect[3];
}
