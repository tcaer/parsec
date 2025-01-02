#define FONTSTASH_IMPLEMENTATION
#define CLAY_IMPLEMENTATION
#include "parsec.h"

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

const char LOREM_IPSUM[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin dignissim "
    "eros eu vehicula venenatis. Nullam posuere mollis massa. Maecenas "
    "malesuada magna quis gravida semper. Cras lacinia quis magna quis "
    "iaculis. Quisque quis risus ullamcorper, cursus nisi a, interdum libero. "
    "Nunc a blandit risus. Praesent bibendum lacus eu justo lacinia "
    "vestibulum. Proin ut lectus non quam sodales efficitur. Aenean et quam "
    "vestibulum, finibus felis a, vulputate felis. Suspendisse eu cursus eros. "
    "Phasellus eu lobortis mauris, id facilisis lectus. Aenean ac tellus ut "
    "lacus imperdiet dictum et et metus. Pellentesque condimentum non dui at "
    "porttitor. Donec a erat nec est lobortis tempor gravida nec ligula. "
    "Phasellus pulvinar eros at mi ullamcorper pulvinar.\nQuisque hendrerit "
    "mollis sapien quis blandit. Morbi id massa arcu. Cras mattis justo "
    "hendrerit, interdum tellus sed, congue lacus. Vestibulum non risus nunc. "
    "Sed molestie sem sit amet neque finibus, in consequat dolor feugiat. "
    "Integer in ante sed nisi tincidunt tristique. Fusce pretium nulla at orci "
    "bibendum, a pulvinar nisi pharetra.\nCurabitur sit amet turpis "
    "tincidunt, "
    "posuere ligula eget, semper tellus. In hac habitasse platea dictumst. Sed "
    "a leo ante. Cras ornare hendrerit dui ac pretium. Aliquam suscipit sapien "
    "urna, et fringilla lacus vehicula ac. Pellentesque ultricies porta enim, "
    "sed bibendum felis sagittis non. Quisque consequat vulputate justo, nec "
    "fermentum eros condimentum sed. Aliquam erat volutpat. Phasellus "
    "consequat eleifend ex, id luctus nisl consectetur vitae. Vestibulum ante "
    "ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae; "
    "In ac lorem faucibus, efficitur lacus sollicitudin, venenatis "
    "dolor.\nCurabitur nisl nunc, consectetur tincidunt ligula ut, sodales "
    "interdum mauris. Nam in luctus nisi. Donec sodales accumsan augue, sit "
    "amet malesuada tellus scelerisque sit amet. Vivamus diam augue, tincidunt "
    "in urna ut, luctus fringilla justo. Duis tempor sapien ut nibh vulputate "
    "blandit eu in massa. Aenean eget enim consequat, viverra arcu in, maximus "
    "ligula. Aliquam erat volutpat. Class aptent taciti sociosqu ad litora "
    "torquent per conubia nostra, per inceptos himenaeos. Sed luctus nunc id "
    "nibh semper efficitur lobortis in ipsum. Nam vel erat tempus, mollis nunc "
    "vitae, elementum elit. Duis dignissim fermentum tellus a ullamcorper. "
    "Aenean in ante at ligula eleifend volutpat non consectetur ex.\nSed sit "
    "amet condimentum augue, sit amet interdum orci. Sed aliquet eu ligula et "
    "faucibus. Vestibulum in justo feugiat dolor vestibulum commodo in ac est. "
    "Nam sit amet tincidunt massa, at malesuada nulla. Donec ac imperdiet "
    "nulla. Mauris ante velit, tincidunt eget elit at, ultrices dictum arcu. "
    "Curabitur faucibus vel orci id auctor. Maecenas vestibulum nisi et "
    "tristique imperdiet. Aenean eleifend nisl sit amet massa hendrerit, nec "
    "dictum est dictum. Integer facilisis quis magna a varius. Phasellus "
    "convallis egestas arcu, vel maximus nisl tristique maximus. Vivamus "
    "vestibulum augue enim, vel venenatis urna interdum ut. Aliquam "
    "condimentum, orci nec efficitur finibus, tellus enim lacinia sem, vel "
    "scelerisque tortor nunc volutpat tortor. Duis blandit, odio in "
    "ullamcorper imperdiet, tellus odio cursus tellus, vitae tincidunt tortor "
    "mi a magna. Nam vulputate orci vel justo suscipit, vitae imperdiet lorem "
    "sodales.";

void *clay_memory;

void UI_init() {
  unsigned int memory_size = Clay_MinMemorySize();
  clay_memory = malloc(memory_size);
  Clay_Arena arena =
      Clay_CreateArenaWithCapacityAndMemory(memory_size, clay_memory);
  Clay_SetMeasureTextFunction(TextSystem_measure_text);
  Clay_Initialize(arena, (Clay_Dimensions){2560, 1440});
}

void UI_set_state(Vec2 viewport_size, Vec2 mouse_pos) {
  Clay_SetLayoutDimensions((Clay_Dimensions){viewport_size.x, viewport_size.y});
  Clay_SetPointerState((Clay_Vector2){mouse_pos.x, mouse_pos.y}, false);
}

Clay_RenderCommandArray UI_render_editor() {
  Clay_BeginLayout();

  CLAY(CLAY_ID("EditorContainer"),
       CLAY_LAYOUT({.sizing = {CLAY_SIZING_GROW({}), CLAY_SIZING_GROW({})}}),
       CLAY_RECTANGLE({.color = {251, 241, 199, 255}})) {
    CLAY(CLAY_ID("TestText"), CLAY_LAYOUT({.padding = {200, 200}})) {
      CLAY_TEXT(
          CLAY_STRING(LOREM_IPSUM),
          CLAY_TEXT_CONFIG({.fontSize = 28,
                            .textColor = Clay_Hovered()
                                             ? (Clay_Color){0, 255, 0, 255}
                                             : (Clay_Color){0, 0, 0, 255}}));
    }
  }

  return Clay_EndLayout();
}
