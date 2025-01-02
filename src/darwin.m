#include "fontstash/fontstash.h"
#include "parsec.h"

#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>

// MARK defines

#define NS_NEW(X) [[X alloc] init]

static const char SHADERS[] = {
#embed "../build/shaders/shaders.metallib"
};

// MARK primitive decls

typedef struct Color {
  float r, g, b, a;
} Color;

typedef struct Globals {
  Vec2 viewport_size;
} Globals;

typedef struct Quad {
  Vec2 origin, size;
  Color background_color;
} Quad;

// MARK Renderer decls

typedef struct Renderer {
  id<MTLTexture> font_atlas;
  id<MTLBuffer> instance_buffer;
  id<MTLRenderPipelineState> sprite_pipeline_state;
  id<MTLRenderPipelineState> quad_pipeline_state;
  id<MTLCommandQueue> queue;
} Renderer;

// MARK WindowState decls

typedef struct WindowState {
  Renderer renderer;
} WindowState;

// MARK TextSystem impls

void TextSystem_update_atlas(id<MTLTexture> atlas) {
  if (!TextSystem_is_dirty())
    return;

  int width, height;
  const unsigned char *data = fonsGetTextureData(f_ctx, &width, &height);
  MTLRegion region = {{0, 0, 0}, {width, height, 1}};
  [atlas replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:width];
}

// MARK Renderer impls

#define INSTANCE_BUFFER_SIZE 1024 * 1024 * 2

void Renderer_paint_quads(Renderer *self,
                          id<MTLRenderCommandEncoder> command_encoder,
                          size_t *offset, Quad *quads, size_t num_quads);

void Renderer_paint_sprites(Renderer *self,
                            id<MTLRenderCommandEncoder> command_encoder,
                            size_t *offset, Sprite *sprites,
                            size_t num_sprites);

void Renderer_encode_commands(MTKView *view, Clay_RenderCommandArray commands,
                              Quad (*quads)[], size_t *num_quads,
                              Sprite (*sprites)[], size_t *num_sprites);

id<MTLRenderPipelineState> mk_pipeline_state(id<MTLDevice> device,
                                             id<MTLLibrary> library,
                                             NSString *vertex_name,
                                             NSString *fragment_name,
                                             NSError **err);

void Renderer_init(Renderer *self, id<MTLDevice> device) {
  @autoreleasepool {
    int width, height;
    fonsGetTextureData(f_ctx, &width, &height);
    MTLTextureDescriptor *desc = NS_NEW(MTLTextureDescriptor);
    [desc setWidth:width];
    [desc setHeight:height];
    [desc setPixelFormat:MTLPixelFormatA8Unorm];

    self->font_atlas = [device newTextureWithDescriptor:desc];

    self->instance_buffer =
        [device newBufferWithLength:INSTANCE_BUFFER_SIZE
                            options:MTLResourceStorageModeManaged];

    dispatch_data_t data =
        dispatch_data_create(SHADERS, sizeof(SHADERS),
                             dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0),
                             DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    NSError *err = nil;
    id<MTLLibrary> library = [device newLibraryWithData:data error:&err];
    assert(err == nil);

    self->sprite_pipeline_state = mk_pipeline_state(
        device, library, @"sprite_vertex", @"sprite_fragment", &err);
    assert(err == nil);
    self->quad_pipeline_state = mk_pipeline_state(
        device, library, @"quad_vertex", @"quad_fragment", &err);
    assert(err == nil);

    self->queue = [device newCommandQueue];
  }
}

void Renderer_destroy(Renderer *self) {
  self->font_atlas = nil;
  self->instance_buffer = nil;
  self->quad_pipeline_state = nil;
  self->sprite_pipeline_state = nil;
  self->queue = nil;
}

void Renderer_paint(Renderer *self, MTKView *view,
                    Clay_RenderCommandArray commands) {
  @autoreleasepool {
    MTLRenderPassDescriptor *desc = [view currentRenderPassDescriptor];

    id<MTLCommandBuffer> command_buffer = [self->queue commandBuffer];
    id<MTLRenderCommandEncoder> command_encoder =
        [command_buffer renderCommandEncoderWithDescriptor:desc];

    CGSize drawable_size = [view drawableSize];
    Globals globals = (Globals){{drawable_size.width, drawable_size.height}};
    [command_encoder setVertexBytes:&globals length:sizeof(Globals) atIndex:0];

    // TODO these fixed arrays will definitely not scale, these should by
    // vectors. For testing it's fine
    Quad quads[512];
    Sprite sprites[4000];
    size_t num_quads = 0;
    size_t num_sprites = 0;
    Renderer_encode_commands(view, commands, &quads, &num_quads, &sprites,
                             &num_sprites);
    // TextSystem_layout("foo, bar", 8, (Vec2){20, 120}, sprites);
    TextSystem_update_atlas(self->font_atlas);

    size_t instance_offset = 0;
    Renderer_paint_quads(self, command_encoder, &instance_offset, quads,
                         num_quads);
    Renderer_paint_sprites(self, command_encoder, &instance_offset, sprites,
                           num_sprites);

    [command_encoder endEncoding];

    id<CAMetalDrawable> drawable = [view currentDrawable];
    [command_buffer presentDrawable:drawable];

    [command_buffer commit];
  }
}

void Renderer_paint_quads(Renderer *self,
                          id<MTLRenderCommandEncoder> command_encoder,
                          size_t *offset, Quad *quads, size_t num_quads) {
  if (num_quads == 0)
    return;

  size_t bytes_len = sizeof(Quad) * num_quads;
  // TODO we should double the buffer size and try again on the next draw call
  assert(bytes_len + *offset < INSTANCE_BUFFER_SIZE);

  char *contents = [self->instance_buffer contents];
  memcpy(contents + *offset, quads, bytes_len);
  [self->instance_buffer
      didModifyRange:(NSRange){(NSUInteger)offset, bytes_len}];

  [command_encoder setRenderPipelineState:self->quad_pipeline_state];
  [command_encoder setVertexBuffer:self->instance_buffer
                            offset:*offset
                           atIndex:1];
  [command_encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                      vertexStart:0
                      vertexCount:4
                    instanceCount:num_quads];

  *offset += bytes_len;
}

void Renderer_paint_sprites(Renderer *self,
                            id<MTLRenderCommandEncoder> command_encoder,
                            size_t *offset, Sprite *sprites,
                            size_t num_sprites) {
  if (num_sprites == 0)
    return;

  size_t bytes_len = sizeof(Quad) * num_sprites;
  // TODO we should double the buffer size and try again on the next draw call
  assert(bytes_len + *offset < INSTANCE_BUFFER_SIZE);

  char *contents = [self->instance_buffer contents];
  memcpy(contents + *offset, sprites, bytes_len);
  [self->instance_buffer
      didModifyRange:(NSRange){(NSUInteger)offset, bytes_len}];

  [command_encoder setRenderPipelineState:self->sprite_pipeline_state];
  [command_encoder setVertexBuffer:self->instance_buffer
                            offset:*offset
                           atIndex:1];
  [command_encoder setFragmentTexture:self->font_atlas atIndex:0];
  [command_encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                      vertexStart:0
                      vertexCount:4
                    instanceCount:num_sprites];

  *offset += bytes_len;
}

void Renderer_encode_commands(MTKView *view, Clay_RenderCommandArray commands,
                              Quad (*quads)[], size_t *num_quads,
                              Sprite (*sprites)[], size_t *num_sprites) {
  // TODO this system is not correct if there are multiple layers to render.
  // Clay returns commands sorted from back to front and shufflign this order to
  // fit into instances loses this information
  for (unsigned int i = 0; i < commands.length; i++) {
    Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
    Clay_BoundingBox bounding_box = command->boundingBox;

    // TODO exhaust this switch
    switch (command->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_NONE:
      break;
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      Clay_RectangleElementConfig *config =
          command->config.rectangleElementConfig;
      (*quads)[*num_quads] = (Quad){
          {bounding_box.x, bounding_box.y},
          {bounding_box.width, bounding_box.height},
          {config->color.r / 255, config->color.g / 255, config->color.b / 255,
           config->color.a / 255},
      };
      (*num_quads)++;
      break;
    }
    // TODO this switch does not account for any config like font size
    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
      Clay_TextElementConfig *config = command->config.textElementConfig;
      Sprite *offset = (*sprites) + *num_sprites;
      TextSystem_layout(command->text.chars, command->text.length,
                        (Vec2){bounding_box.x, bounding_box.y}, config, offset);
      *num_sprites += command->text.length;
      break;
    }
    }
  }
}

id<MTLRenderPipelineState> mk_pipeline_state(id<MTLDevice> device,
                                             id<MTLLibrary> library,
                                             NSString *vertex_name,
                                             NSString *fragment_name,
                                             NSError **err) {
  id<MTLFunction> vertex_fn = [library newFunctionWithName:vertex_name];
  id<MTLFunction> fragment_fn = [library newFunctionWithName:fragment_name];

  MTLRenderPipelineDescriptor *desc = NS_NEW(MTLRenderPipelineDescriptor);
  [desc setVertexFunction:vertex_fn];
  [desc setFragmentFunction:fragment_fn];

  MTLRenderPipelineColorAttachmentDescriptor *c_desc =
      [desc colorAttachments][0];
  [c_desc setPixelFormat:MTLPixelFormatRGBA8Unorm];
  [c_desc setBlendingEnabled:YES];
  [c_desc setRgbBlendOperation:MTLBlendOperationAdd];
  [c_desc setAlphaBlendOperation:MTLBlendOperationAdd];
  [c_desc setSourceRGBBlendFactor:MTLBlendFactorSourceAlpha];
  [c_desc setSourceAlphaBlendFactor:MTLBlendFactorOne];
  [c_desc setDestinationRGBBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
  [c_desc setDestinationAlphaBlendFactor:MTLBlendFactorOne];

  return [device newRenderPipelineStateWithDescriptor:desc error:err];
}

// MARK ParsecView impls

@interface ParsecView : MTKView <MTKViewDelegate> {
  WindowState *state;
}

- (void)setState:(WindowState *)new_state;
@end

@implementation ParsecView
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
}

- (void)drawInMTKView:(MTKView *)view {
  CGSize viewport_size = [view drawableSize];
  UI_set_state((Vec2){viewport_size.width, viewport_size.height});
  Clay_RenderCommandArray commands = UI_render_editor();

  Renderer_paint(&state->renderer, view, commands);
}

- (void)setState:(WindowState *)new_state {
  state = new_state;
}
@end

// MARK ParsecWindow impls

typedef struct ParsecWindowArgs {
  uint width, height;
} ParsecWindowArgs;

ParsecWindowArgs ParsecWindowArgs_default() {
  return (ParsecWindowArgs){1280, 720};
}

@interface ParsecWindow : NSWindow <NSWindowDelegate> {
  WindowState *state;
}

- (void)setState:(WindowState *)new_state;
@end

@implementation ParsecWindow
- (void)windowWillClose:(NSNotification *)notification {
  Renderer_destroy(&state->renderer);

  free(state);
  state = NULL;
}

- (void)setState:(WindowState *)new_state {
  state = new_state;
}
@end

NSWindowStyleMask DEFAULT_WIN_STY =
    NSWindowStyleMaskClosable | NSWindowStyleMaskResizable |
    NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskTitled |
    NSWindowStyleMaskFullSizeContentView;

void ParsecWindow_open(ParsecWindowArgs args, id<MTLDevice> device) {
  NSRect rect = {};
  rect.size = (CGSize){args.width, args.height};

  ParsecWindow *win =
      [[ParsecWindow alloc] initWithContentRect:rect
                                      styleMask:DEFAULT_WIN_STY
                                        backing:NSBackingStoreBuffered
                                          defer:NO];
  [win setDelegate:win];

  [win setReleasedWhenClosed:NO];
  [win setTitlebarAppearsTransparent:YES];
  [win setAcceptsMouseMovedEvents:YES];
  [win center];

  ParsecView *view = [[ParsecView alloc] initWithFrame:rect];
  [view setDevice:device];
  [view setDelegate:view];

  [win setContentView:view];

  WindowState *state = malloc(sizeof(WindowState));
  Renderer_init(&state->renderer, device);

  [win setState:state];
  [view setState:state];

  [win makeKeyAndOrderFront:nil];
}

// MARK ParsecAppDelegate impls

@interface ParsecAppDelegate : NSObject <NSApplicationDelegate> {
}
@end

@implementation ParsecAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  TextSystem_init();
  UI_init();

  // (Tino) TODO if there are multiple devices, we should prefer a low-power one
  // (eg. an intel CPU w/ metal instead of a dedicated GPU)
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();

  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp activateIgnoringOtherApps:YES];

  ParsecWindow_open(ParsecWindowArgs_default(), device);
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:
    (NSApplication *)sender {
#ifdef DEBUG
  return YES;
#else
  return NO;
#endif
}

- (void)applicationWillTerminate:(NSNotification *)notification {
  TextSystem_destroy();
}
@end

int main() {
  NSApplication *app = [NSApplication sharedApplication];

  ParsecAppDelegate *del = NS_NEW(ParsecAppDelegate);
  [app setDelegate:del];

  [app run];
}
