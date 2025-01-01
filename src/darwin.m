#include <AppKit/AppKit.h>

// MARK defines

#define NS_NEW(X) [[X alloc] init];

// MARK ParsecWindow impls

typedef struct ParsecWindowArgs {
  uint width, height;
} ParsecWindowArgs;

ParsecWindowArgs ParsecWindowArgs_default() {
  return (ParsecWindowArgs){1280, 720};
}

@interface ParsecWindow : NSWindow <NSWindowDelegate> {
}
@end

@implementation ParsecWindow
@end

NSWindowStyleMask DEFAULT_WIN_STY =
    NSWindowStyleMaskClosable | NSWindowStyleMaskResizable |
    NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskTitled |
    NSWindowStyleMaskFullSizeContentView;

void ParsecWindow_open(ParsecWindowArgs args) {
  NSRect rect = {};
  rect.size = (CGSize){args.width, args.height};

  ParsecWindow *win =
      [[ParsecWindow alloc] initWithContentRect:rect
                                      styleMask:DEFAULT_WIN_STY
                                        backing:NSBackingStoreBuffered
                                          defer:NO];

  [win setReleasedWhenClosed:NO];
  [win setTitlebarAppearsTransparent:YES];
  [win setAcceptsMouseMovedEvents:YES];
  [win center];

  [win makeKeyAndOrderFront:nil];
}

// MARK ParsecAppDelegate impls

@interface ParsecAppDelegate : NSObject <NSApplicationDelegate> {
}
@end

@implementation ParsecAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp activateIgnoringOtherApps:YES];

  ParsecWindow_open(ParsecWindowArgs_default());
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
