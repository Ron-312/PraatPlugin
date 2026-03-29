// ─── macOS audio-file drop target ─────────────────────────────────────────────
//
// WKWebView registers for NSPasteboardTypeFileURL drags during its own init,
// intercepting file drops before JUCE's FileDragAndDropTarget ever sees them.
// This is the macOS equivalent of the Windows COM AudioFileDropTarget fix.
//
// Fix: after the page loads, add a transparent NSView (AudioFileDropView) on
// top of the WKWebView within the editor's native NSView hierarchy.  Because
// it sits higher in z-order, macOS's drag system sends draggingEntered: to it
// first.  It accepts audio-file drops and routes them to PraatPluginProcessor
// on the message thread; everything else (mouse clicks) falls through to the
// WKWebView below via hitTest: returning nil.
// ─────────────────────────────────────────────────────────────────────────────

#include "plugin/PraatPluginEditor.h"
#include <AppKit/AppKit.h>

// ─── AudioFileDropView ────────────────────────────────────────────────────────

static bool isPraatAudioExtension (NSString* path)
{
    NSString* ext = [[path pathExtension] lowercaseString];
    return [@[@"wav", @"mp3", @"aif", @"aiff", @"flac"] containsObject: ext];
}

@interface AudioFileDropView : NSView
- (instancetype) initWithProcessor:(PraatPluginProcessor*)proc;
- (void) invalidate;   // call before the processor is destroyed
@end

@implementation AudioFileDropView
{
    PraatPluginProcessor* _processor;
}

- (instancetype) initWithProcessor:(PraatPluginProcessor*)proc
{
    self = [super initWithFrame: NSZeroRect];
    if (self)
    {
        _processor = proc;
        [self registerForDraggedTypes: @[NSPasteboardTypeFileURL]];
    }
    return self;
}

- (void) invalidate { _processor = nullptr; }

// Transparent — lets the WKWebView below paint through.
- (BOOL) isOpaque { return NO; }
- (void) drawRect:(NSRect)rect {}

// Click-through — mouse events reach the WKWebView below.
- (NSView*) hitTest:(NSPoint)point { return nil; }

// ── NSDraggingDestination ─────────────────────────────────────────────────────

- (NSDragOperation) draggingEntered:(id<NSDraggingInfo>)sender
{
    for (NSPasteboardItem* item in [[sender draggingPasteboard] pasteboardItems])
    {
        NSString* urlStr = [item stringForType: NSPasteboardTypeFileURL];
        if (! urlStr) continue;
        NSURL* url = [NSURL URLWithString: urlStr];
        if (url && isPraatAudioExtension ([url path]))
            return NSDragOperationCopy;
    }
    return NSDragOperationNone;
}

- (NSDragOperation) draggingUpdated:(id<NSDraggingInfo>)sender
{
    return [self draggingEntered: sender];
}

- (BOOL) prepareForDragOperation:(id<NSDraggingInfo>)sender { return YES; }

- (BOOL) performDragOperation:(id<NSDraggingInfo>)sender
{
    if (_processor == nullptr)
        return NO;

    for (NSPasteboardItem* item in [[sender draggingPasteboard] pasteboardItems])
    {
        NSString* urlStr = [item stringForType: NSPasteboardTypeFileURL];
        if (! urlStr) continue;
        NSURL* url  = [NSURL URLWithString: urlStr];
        NSString* p = [url path];
        if (! p || ! isPraatAudioExtension (p)) continue;

        // performDragOperation: is called on the message thread — load directly.
        _processor->loadAudioFromFile (juce::File (juce::String::fromUTF8 ([p UTF8String])));
        return YES;
    }
    return NO;
}

@end

// ─── PraatPluginEditor integration ────────────────────────────────────────────

void PraatPluginEditor::registerMacDropTarget()
{
    if (auto* peer = getPeer())
    {
        auto* nsView = static_cast<NSView*> (peer->getNativeHandle());
        auto* overlay = [[AudioFileDropView alloc] initWithProcessor: &praatProcessor_];
        [nsView addSubview: overlay];   // added last → highest z-order → sees drags first
        macDropOverlay_ = static_cast<void*> (overlay);
        updateMacDropTargetBounds();
    }
}

void PraatPluginEditor::updateMacDropTargetBounds()
{
    if (macDropOverlay_ == nullptr)
        return;
    auto* overlay = static_cast<AudioFileDropView*> (macDropOverlay_);
    [overlay setFrame: NSMakeRect (0, 0, getWidth(), getHeight())];
}

void PraatPluginEditor::unregisterMacDropTarget()
{
    if (macDropOverlay_ == nullptr)
        return;
    auto* overlay = static_cast<AudioFileDropView*> (macDropOverlay_);
    [overlay invalidate];
    [overlay removeFromSuperview];
    macDropOverlay_ = nullptr;
}
