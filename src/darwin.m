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
  Mouse mouse;
  Renderer renderer;
  GapBuffer text;
} WindowState;

// MARK FontSystem impls

void TextSystem_update_atlas(id<MTLTexture> atlas) {
  if (!FontSystem_is_dirty())
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
                          Quad *quads, size_t num_quads, size_t *offset);

void Renderer_paint_sprites(Renderer *self,
                            id<MTLRenderCommandEncoder> command_encoder,
                            Sprite *sprites, size_t num_sprites,
                            size_t *offset);

void Renderer_encode_commands(Renderer *self, MTKView *view,
                              id<MTLRenderCommandEncoder> command_encoder,
                              Clay_RenderCommandArray commands);

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

    Renderer_encode_commands(self, view, command_encoder, commands);

    [command_encoder endEncoding];

    id<CAMetalDrawable> drawable = [view currentDrawable];
    [command_buffer presentDrawable:drawable];

    [command_buffer commit];
  }
}

void Renderer_paint_quads(Renderer *self,
                          id<MTLRenderCommandEncoder> command_encoder,
                          Quad *quads, size_t num_quads, size_t *offset) {
  if (num_quads == 0)
    return;

  size_t byte_size = sizeof(Quad) * num_quads;
  // TODO we should double the buffer size and try again on the next draw call
  assert(byte_size <= INSTANCE_BUFFER_SIZE);

  char *instance_buffer = [self->instance_buffer contents];
  memcpy(instance_buffer + *offset, quads, sizeof(Quad) * num_quads);
  [self->instance_buffer
      didModifyRange:(NSRange){(NSUInteger)offset, byte_size}];

  [command_encoder setRenderPipelineState:self->quad_pipeline_state];
  [command_encoder setVertexBuffer:self->instance_buffer
                            offset:*offset
                           atIndex:1];
  [command_encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                      vertexStart:0
                      vertexCount:4
                    instanceCount:num_quads];

  *offset += byte_size;
}

void Renderer_paint_sprites(Renderer *self,
                            id<MTLRenderCommandEncoder> command_encoder,
                            Sprite *sprites, size_t num_sprites,
                            size_t *offset) {
  if (num_sprites == 0)
    return;

  size_t byte_size = sizeof(Sprite) * num_sprites;
  // TODO we should double the buffer size and try again on the next draw call
  assert(byte_size + *offset <= INSTANCE_BUFFER_SIZE);

  char *contents = [self->instance_buffer contents];
  memcpy(contents + *offset, sprites, byte_size);
  [self->instance_buffer
      didModifyRange:(NSRange){(NSUInteger)offset, byte_size}];

  [command_encoder setRenderPipelineState:self->sprite_pipeline_state];
  [command_encoder setVertexBuffer:self->instance_buffer
                            offset:*offset
                           atIndex:1];
  [command_encoder setFragmentTexture:self->font_atlas atIndex:0];
  [command_encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                      vertexStart:0
                      vertexCount:4
                    instanceCount:num_sprites];

  *offset += byte_size;
}

void Renderer_encode_commands(Renderer *self, MTKView *view,
                              id<MTLRenderCommandEncoder> command_encoder,
                              Clay_RenderCommandArray commands) {
  size_t instance_offset = 0;

  for (unsigned int i = 0; i < commands.length; i++) {
    Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
    Clay_BoundingBox bounding_box = command->boundingBox;

    switch (command->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_NONE:
      break;
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      Clay_RectangleElementConfig *config =
          command->config.rectangleElementConfig;

      Quad quad = (Quad){
          {bounding_box.x, bounding_box.y},
          {bounding_box.width, bounding_box.height},
          {config->color.r / 255, config->color.g / 255, config->color.b / 255,
           config->color.a / 255},
      };

      Renderer_paint_quads(self, command_encoder, &quad, 1, &instance_offset);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_BORDER:
      printf("border command unimplemented\n");
      break;
    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
      Clay_TextElementConfig *config = command->config.textElementConfig;

      size_t num_sprites = 0;
      Sprite sprites[4000];
      FontSystem_layout(command->text.chars, command->text.length,
                        (Vec2){bounding_box.x, bounding_box.y}, config, sprites,
                        &num_sprites);

      TextSystem_update_atlas(self->font_atlas);
      Renderer_paint_sprites(self, command_encoder, sprites, num_sprites,
                             &instance_offset);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
      [command_encoder
          setScissorRect:(MTLScissorRect){bounding_box.x, bounding_box.y,
                                          bounding_box.width,
                                          bounding_box.height}];
      break;
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
      CGSize drawable_size = [view drawableSize];
      [command_encoder
          setScissorRect:(MTLScissorRect){0, 0, drawable_size.width,
                                          drawable_size.height}];
      break;
    case CLAY_RENDER_COMMAND_TYPE_IMAGE:
      printf("image command unimplemented");
      break;
    case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
      printf("custom command unimplemented\n");
      break;
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
  CFAbsoluteTime last_frame_time;
}

- (void)setState:(WindowState *)new_state;
@end

@implementation ParsecView
- (void)setState:(WindowState *)new_state {
  state = new_state;
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
}

- (void)drawInMTKView:(MTKView *)view {
  CFAbsoluteTime current_time = CACurrentMediaTime();
  CFAbsoluteTime dt = current_time - last_frame_time;
  last_frame_time = current_time;

  CGSize viewport_size = [view drawableSize];
  UI_set_state(dt, (Vec2){viewport_size.width, viewport_size.height},
               &state->mouse);

  char memory[MEGABYTE * 2];
  Arena *arena = BumpArena_create(memory, sizeof(memory));

  UIContext ctx = {arena, &state->text};
  Clay_RenderCommandArray commands = EditorView_render(&ctx);
  Renderer_paint(&state->renderer, view, commands);

  Arena_release(arena);
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)mouseMoved:(NSEvent *)event {
  CGSize viewport_size = [self drawableSize];
  float scale = [[self layer] contentsScale];

  NSPoint location = [event locationInWindow];
  float new_x = location.x * scale;
  float new_y = viewport_size.height - (location.y * scale);

  state->mouse.pos = (Vec2){new_x, new_y};
}

- (void)mouseExited:(NSEvent *)event {
  state->mouse.pos = (Vec2){-10, -10};
}

- (void)scrollWheel:(NSEvent *)event {
  float dx = [event scrollingDeltaX];
  float dy = [event scrollingDeltaY];

  state->mouse.d_scroll = (Vec2){dx, dy};
}

- (void)mouseDown:(NSEvent *)event {
  state->mouse.pressed = true;
}

- (void)mouseUp:(NSEvent *)event {
  state->mouse.pressed = false;
}

- (void)keyDown:(NSEvent *)event {
  switch ([event keyCode]) {
  // (Tino) TODO: standardize this and make it platform agnostic
  case 51:
    GapBuffer_delete_char(&state->text);
    break;
  default:
    NSString *chars = [event charactersIgnoringModifiers];
    if ([chars length] == 0)
      return;

    // TODO this doesn't handle utf-8
    char c = [chars characterAtIndex:0];
    GapBuffer_put_char(&state->text, c);
  }
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
  GapBuffer_destroy(&state->text);

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
  NSTrackingArea *t_area = [[NSTrackingArea alloc]
      initWithRect:rect
           options:NSTrackingMouseMoved | NSTrackingActiveInKeyWindow |
                   NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited
             owner:view
          userInfo:nil];
  [view addTrackingArea:t_area];

  [win setContentView:view];
  [win setInitialFirstResponder:view];

  WindowState *state = calloc(1, sizeof(WindowState));
  state->mouse.pos = (Vec2){-10, -10};
  Renderer_init(&state->renderer, device);
  GapBuffer_init(&state->text);

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
  // TODO if there are multiple devices, we should prefer a low-power one
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
  FontSystem_destroy();
}
@end

int main() {
  FontSystem_init();
  UI_init();

  NSApplication *app = [NSApplication sharedApplication];

  ParsecAppDelegate *del = NS_NEW(ParsecAppDelegate);
  [app setDelegate:del];

  [app run];
}
