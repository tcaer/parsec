#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>
#include <assert.h>

// MARK defines

#define NS_NEW(X) [[X alloc] init]

const char SHADERS[] = {
#embed "../build/shaders/shaders.metallib"
};

// MARK Renderer decls

typedef struct Renderer {
  id<MTLRenderPipelineState> quad_pipeline_state;
  id<MTLCommandQueue> queue;
} Renderer;

// MARK WindowState decls

typedef struct WindowState {
  Renderer renderer;
} WindowState;

// MARK Renderer impls

id<MTLRenderPipelineState> mk_pipeline_state(id<MTLDevice> device,
                                             id<MTLLibrary> library,
                                             NSString *vertex_name,
                                             NSString *fragment_name,
                                             NSError **err);

void Renderer_init(Renderer *self, id<MTLDevice> device) {
  @autoreleasepool {
    self->queue = [device newCommandQueue];

    dispatch_data_t data =
        dispatch_data_create(SHADERS, sizeof(SHADERS),
                             dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0),
                             DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    NSError *err = nil;
    id<MTLLibrary> library = [device newLibraryWithData:data error:&err];
    assert(err == nil);

    self->quad_pipeline_state = mk_pipeline_state(
        device, library, @"quad_vertex", @"quad_fragment", &err);
    assert(err == nil);
  }
}

void Renderer_destroy(Renderer *self) {
  self->quad_pipeline_state = nil;
  self->queue = nil;
}

void Renderer_paint(Renderer *self, MTKView *view) {
  @autoreleasepool {
    MTLRenderPassDescriptor *desc = [view currentRenderPassDescriptor];

    id<MTLCommandBuffer> command_buffer = [self->queue commandBuffer];
    id<MTLRenderCommandEncoder> command_encoder =
        [command_buffer renderCommandEncoderWithDescriptor:desc];

    [command_encoder setRenderPipelineState:self->quad_pipeline_state];
    [command_encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                        vertexStart:0
                        vertexCount:4
                      instanceCount:1];

    [command_encoder endEncoding];

    id<CAMetalDrawable> drawable = [view currentDrawable];
    [command_buffer presentDrawable:drawable];

    [command_buffer commit];
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
  Renderer_paint(&state->renderer, view);
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
@end

int main() {
  NSApplication *app = [NSApplication sharedApplication];

  ParsecAppDelegate *del = NS_NEW(ParsecAppDelegate);
  [app setDelegate:del];

  [app run];
}
