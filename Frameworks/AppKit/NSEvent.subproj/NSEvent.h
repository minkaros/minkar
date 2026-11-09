/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
                 2010 Markus Hitter <mah@jump-ing.de>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/Foundation.h>
#import <AppKit/AppKitExport.h>
#import <CoreGraphics/CGEvent.h>

@class NSWindow, NSGraphicsContext, NSTrackingArea;

/* The event types, numbered as the API has them. */
typedef NS_ENUM(NSUInteger, NSEventType) {
    NSEventTypeLeftMouseDown = 1,
    NSEventTypeLeftMouseUp = 2,
    NSEventTypeRightMouseDown = 3,
    NSEventTypeRightMouseUp = 4,
    NSEventTypeMouseMoved = 5,
    NSEventTypeLeftMouseDragged = 6,
    NSEventTypeRightMouseDragged = 7,
    NSEventTypeMouseEntered = 8,
    NSEventTypeMouseExited = 9,
    NSEventTypeKeyDown = 10,
    NSEventTypeKeyUp = 11,
    NSEventTypeFlagsChanged = 12,
    NSEventTypeAppKitDefined = 13,
    NSEventTypeSystemDefined = 14,
    NSEventTypeApplicationDefined = 15,
    NSEventTypePeriodic = 16,
    NSEventTypeCursorUpdate = 17,
    NSEventTypeRotate = 18,
    NSEventTypeBeginGesture = 19,
    NSEventTypeEndGesture = 20,
    NSEventTypeScrollWheel = 22,
    NSEventTypeTabletPoint = 23,
    NSEventTypeTabletProximity = 24,
    NSEventTypeOtherMouseDown = 25,
    NSEventTypeOtherMouseUp = 26,
    NSEventTypeOtherMouseDragged = 27,
    NSEventTypeGesture = 29,
    NSEventTypeMagnify = 30,
    NSEventTypeSwipe = 31,
    NSEventTypeSmartMagnify = 32,
    NSEventTypeQuickLook = 33,
    NSEventTypePressure = 34,
    NSEventTypeDirectTouch = 37,
    NSEventTypeChangeMode = 38,

    NSLeftMouseDown = NSEventTypeLeftMouseDown,
    NSLeftMouseUp = NSEventTypeLeftMouseUp,
    NSRightMouseDown = NSEventTypeRightMouseDown,
    NSRightMouseUp = NSEventTypeRightMouseUp,
    NSMouseMoved = NSEventTypeMouseMoved,
    NSLeftMouseDragged = NSEventTypeLeftMouseDragged,
    NSRightMouseDragged = NSEventTypeRightMouseDragged,
    NSMouseEntered = NSEventTypeMouseEntered,
    NSMouseExited = NSEventTypeMouseExited,
    NSKeyDown = NSEventTypeKeyDown,
    NSKeyUp = NSEventTypeKeyUp,
    NSFlagsChanged = NSEventTypeFlagsChanged,
    NSAppKitDefined = NSEventTypeAppKitDefined,
    NSSystemDefined = NSEventTypeSystemDefined,
    NSApplicationDefined = NSEventTypeApplicationDefined,
    NSPeriodic = NSEventTypePeriodic,
    NSCursorUpdate = NSEventTypeCursorUpdate,
    NSScrollWheel = NSEventTypeScrollWheel,
    NSTabletPoint = NSEventTypeTabletPoint,
    NSTabletProximity = NSEventTypeTabletProximity,
    NSOtherMouseDown = NSEventTypeOtherMouseDown,
    NSOtherMouseUp = NSEventTypeOtherMouseUp,
    NSOtherMouseDragged = NSEventTypeOtherMouseDragged,

    /* AppKit's own, never seen by apps: a platform event, a display
       event, and the wakeup that ends a wait for events. */
    NSPlatformSpecific = 40,
    NSPlatformSpecificDisplayEvent = 41,
    NSAppKitSystem = 42,
};

typedef NS_OPTIONS(unsigned long long, NSEventMask) {
    NSEventMaskLeftMouseDown = 1ULL << NSEventTypeLeftMouseDown,
    NSEventMaskLeftMouseUp = 1ULL << NSEventTypeLeftMouseUp,
    NSEventMaskRightMouseDown = 1ULL << NSEventTypeRightMouseDown,
    NSEventMaskRightMouseUp = 1ULL << NSEventTypeRightMouseUp,
    NSEventMaskMouseMoved = 1ULL << NSEventTypeMouseMoved,
    NSEventMaskLeftMouseDragged = 1ULL << NSEventTypeLeftMouseDragged,
    NSEventMaskRightMouseDragged = 1ULL << NSEventTypeRightMouseDragged,
    NSEventMaskMouseEntered = 1ULL << NSEventTypeMouseEntered,
    NSEventMaskMouseExited = 1ULL << NSEventTypeMouseExited,
    NSEventMaskKeyDown = 1ULL << NSEventTypeKeyDown,
    NSEventMaskKeyUp = 1ULL << NSEventTypeKeyUp,
    NSEventMaskFlagsChanged = 1ULL << NSEventTypeFlagsChanged,
    NSEventMaskAppKitDefined = 1ULL << NSEventTypeAppKitDefined,
    NSEventMaskSystemDefined = 1ULL << NSEventTypeSystemDefined,
    NSEventMaskApplicationDefined = 1ULL << NSEventTypeApplicationDefined,
    NSEventMaskPeriodic = 1ULL << NSEventTypePeriodic,
    NSEventMaskCursorUpdate = 1ULL << NSEventTypeCursorUpdate,
    NSEventMaskScrollWheel = 1ULL << NSEventTypeScrollWheel,
    NSEventMaskTabletPoint = 1ULL << NSEventTypeTabletPoint,
    NSEventMaskTabletProximity = 1ULL << NSEventTypeTabletProximity,
    NSEventMaskOtherMouseDown = 1ULL << NSEventTypeOtherMouseDown,
    NSEventMaskOtherMouseUp = 1ULL << NSEventTypeOtherMouseUp,
    NSEventMaskOtherMouseDragged = 1ULL << NSEventTypeOtherMouseDragged,
    NSEventMaskGesture = 1ULL << NSEventTypeGesture,
    NSEventMaskMagnify = 1ULL << NSEventTypeMagnify,
    NSEventMaskSwipe = 1ULL << NSEventTypeSwipe,
    NSEventMaskRotate = 1ULL << NSEventTypeRotate,
    NSEventMaskBeginGesture = 1ULL << NSEventTypeBeginGesture,
    NSEventMaskEndGesture = 1ULL << NSEventTypeEndGesture,
    NSEventMaskSmartMagnify = 1ULL << NSEventTypeSmartMagnify,
    NSEventMaskPressure = 1ULL << NSEventTypePressure,
    NSEventMaskDirectTouch = 1ULL << NSEventTypeDirectTouch,
    NSEventMaskChangeMode = 1ULL << NSEventTypeChangeMode,
    NSEventMaskAny = NSUIntegerMax,

    NSLeftMouseDownMask = NSEventMaskLeftMouseDown,
    NSLeftMouseUpMask = NSEventMaskLeftMouseUp,
    NSRightMouseDownMask = NSEventMaskRightMouseDown,
    NSRightMouseUpMask = NSEventMaskRightMouseUp,
    NSMouseMovedMask = NSEventMaskMouseMoved,
    NSLeftMouseDraggedMask = NSEventMaskLeftMouseDragged,
    NSRightMouseDraggedMask = NSEventMaskRightMouseDragged,
    NSMouseEnteredMask = NSEventMaskMouseEntered,
    NSMouseExitedMask = NSEventMaskMouseExited,
    NSKeyDownMask = NSEventMaskKeyDown,
    NSKeyUpMask = NSEventMaskKeyUp,
    NSFlagsChangedMask = NSEventMaskFlagsChanged,
    NSAppKitDefinedMask = NSEventMaskAppKitDefined,
    NSSystemDefinedMask = NSEventMaskSystemDefined,
    NSApplicationDefinedMask = NSEventMaskApplicationDefined,
    NSPeriodicMask = NSEventMaskPeriodic,
    NSCursorUpdateMask = NSEventMaskCursorUpdate,
    NSScrollWheelMask = NSEventMaskScrollWheel,
    NSTabletPointMask = NSEventMaskTabletPoint,
    NSTabletProximityMask = NSEventMaskTabletProximity,
    NSOtherMouseDownMask = NSEventMaskOtherMouseDown,
    NSOtherMouseUpMask = NSEventMaskOtherMouseUp,
    NSOtherMouseDraggedMask = NSEventMaskOtherMouseDragged,
    NSAnyEventMask = NSEventMaskAny,

    NSPlatformSpecificDisplayMask = 1ULL << NSPlatformSpecificDisplayEvent,
};

typedef NS_OPTIONS(NSUInteger, NSEventModifierFlags) {
    NSEventModifierFlagCapsLock = 1 << 16,
    NSEventModifierFlagShift = 1 << 17,
    NSEventModifierFlagControl = 1 << 18,
    NSEventModifierFlagOption = 1 << 19,
    NSEventModifierFlagCommand = 1 << 20,
    NSEventModifierFlagNumericPad = 1 << 21,
    NSEventModifierFlagHelp = 1 << 22,
    NSEventModifierFlagFunction = 1 << 23,
    NSEventModifierFlagDeviceIndependentFlagsMask = 0xffff0000UL,

    NSAlphaShiftKeyMask = NSEventModifierFlagCapsLock,
    NSShiftKeyMask = NSEventModifierFlagShift,
    NSControlKeyMask = NSEventModifierFlagControl,
    NSAlternateKeyMask = NSEventModifierFlagOption,
    NSCommandKeyMask = NSEventModifierFlagCommand,
    NSNumericPadKeyMask = NSEventModifierFlagNumericPad,
    NSHelpKeyMask = NSEventModifierFlagHelp,
    NSFunctionKeyMask = NSEventModifierFlagFunction,
    NSDeviceIndependentModifierFlagsMask = NSEventModifierFlagDeviceIndependentFlagsMask,
};

/* Where a gesture or a scroll is: trackpads give phases; a mouse wheel
   none. */
typedef NS_OPTIONS(NSUInteger, NSEventPhase) {
    NSEventPhaseNone = 0,
    NSEventPhaseBegan = 1 << 0,
    NSEventPhaseStationary = 1 << 1,
    NSEventPhaseChanged = 1 << 2,
    NSEventPhaseEnded = 1 << 3,
    NSEventPhaseCancelled = 1 << 4,
    NSEventPhaseMayBegin = 1 << 5,
};

typedef NS_ENUM(NSInteger, NSEventGestureAxis) {
    NSEventGestureAxisNone = 0,
    NSEventGestureAxisHorizontal,
    NSEventGestureAxisVertical,
};

typedef NS_OPTIONS(NSUInteger, NSEventSwipeTrackingOptions) {
    NSEventSwipeTrackingLockDirection = 0x1 << 0,
    NSEventSwipeTrackingClampGestureAmount = 0x1 << 1,
};

typedef NS_ENUM(NSUInteger, NSPointingDeviceType) {
    NSPointingDeviceTypeUnknown = 0,
    NSPointingDeviceTypePen = 1,
    NSPointingDeviceTypeCursor = 2,
    NSPointingDeviceTypeEraser = 3,
};

/* A key's character when it is a function key (arrows, F1...), as
   specialKey gives it. */
typedef NSUInteger NSEventSpecialKey;


enum {
    NSUpArrowFunctionKey = 0xF700,
    NSDownArrowFunctionKey = 0xF701,
    NSLeftArrowFunctionKey = 0xF702,
    NSRightArrowFunctionKey = 0xF703,
    NSF1FunctionKey = 0xF704,
    NSF2FunctionKey = 0xF705,
    NSF3FunctionKey = 0xF706,
    NSF4FunctionKey = 0xF707,
    NSF5FunctionKey = 0xF708,
    NSF6FunctionKey = 0xF709,
    NSF7FunctionKey = 0xF70A,
    NSF8FunctionKey = 0xF70B,
    NSF9FunctionKey = 0xF70C,
    NSF10FunctionKey = 0xF70D,
    NSF11FunctionKey = 0xF70E,
    NSF12FunctionKey = 0xF70F,
    NSF13FunctionKey = 0xF710,
    NSF14FunctionKey = 0xF711,
    NSF15FunctionKey = 0xF712,
    NSF16FunctionKey = 0xF713,
    NSF17FunctionKey = 0xF714,
    NSF18FunctionKey = 0xF715,
    NSF19FunctionKey = 0xF716,
    NSF20FunctionKey = 0xF717,
    NSF21FunctionKey = 0xF718,
    NSF22FunctionKey = 0xF719,
    NSF23FunctionKey = 0xF71A,
    NSF24FunctionKey = 0xF71B,
    NSF25FunctionKey = 0xF71C,
    NSF26FunctionKey = 0xF71D,
    NSF27FunctionKey = 0xF71E,
    NSF28FunctionKey = 0xF71F,
    NSF29FunctionKey = 0xF720,
    NSF30FunctionKey = 0xF721,
    NSF31FunctionKey = 0xF722,
    NSF32FunctionKey = 0xF723,
    NSF33FunctionKey = 0xF724,
    NSF34FunctionKey = 0xF725,
    NSF35FunctionKey = 0xF726,
    NSInsertFunctionKey = 0xF727,
    NSDeleteFunctionKey = 0xF728,
    NSHomeFunctionKey = 0xF729,
    NSBeginFunctionKey = 0xF72A,
    NSEndFunctionKey = 0xF72B,
    NSPageUpFunctionKey = 0xF72C,
    NSPageDownFunctionKey = 0xF72D,
    NSPrintScreenFunctionKey = 0xF72E,
    NSScrollLockFunctionKey = 0xF72F,
    NSPauseFunctionKey = 0xF730,
    NSSysReqFunctionKey = 0xF731,
    NSBreakFunctionKey = 0xF732,
    NSResetFunctionKey = 0xF733,
    NSStopFunctionKey = 0xF734,
    NSMenuFunctionKey = 0xF735,
    NSUserFunctionKey = 0xF736,
    NSSystemFunctionKey = 0xF737,
    NSPrintFunctionKey = 0xF738,
    NSClearLineFunctionKey = 0xF739,
    NSClearDisplayFunctionKey = 0xF73A,
    NSInsertLineFunctionKey = 0xF73B,
    NSDeleteLineFunctionKey = 0xF73C,
    NSInsertCharFunctionKey = 0xF73D,
    NSDeleteCharFunctionKey = 0xF73E,
    NSPrevFunctionKey = 0xF73F,
    NSNextFunctionKey = 0xF740,
    NSSelectFunctionKey = 0xF741,
    NSExecuteFunctionKey = 0xF742,
    NSUndoFunctionKey = 0xF743,
    NSRedoFunctionKey = 0xF744,
    NSFindFunctionKey = 0xF745,
    NSHelpFunctionKey = 0xF746,
    NSModeSwitchFunctionKey = 0xF747
};

enum {
    NSApplicationActivated = 0,
    NSApplicationDeactivated = 1
};

/* Subtypes of NSAppKitDefined events (the public NSEventSubtype). */
typedef NS_ENUM(short, NSEventSubtype) {
    NSEventSubtypeWindowExposed = 0,
    NSEventSubtypeApplicationActivated = 1,
    NSEventSubtypeApplicationDeactivated = 2,
    NSEventSubtypeWindowMoved = 4,
    NSEventSubtypeScreenChanged = 8,
    NSEventSubtypePowerOff = 1,
    NSEventSubtypeMouseEvent = 0,
    NSEventSubtypeTabletPoint = 1,
    NSEventSubtypeTabletProximity = 2,
    NSEventSubtypeTouch = 3,
    NSWindowExposedEventType = 0,
    NSApplicationActivatedEventType = 1,
    NSApplicationDeactivatedEventType = 2,
    NSWindowMovedEventType = 4,
    NSScreenChangedEventType = 8,
};

@interface NSEvent : NSObject <NSCopying> {
    NSEventType _type;
    NSTimeInterval _timestamp;
    NSPoint _locationInWindow;
    NSUInteger _modifierFlags;
    NSInteger _windowNumber;
}

+ (NSPoint)mouseLocation;
+ (NSEventModifierFlags)modifierFlags;
/* The mouse buttons down now: bit 0 the left one, 1 the right one, 2 and
   up the others. */
+ (NSUInteger)pressedMouseButtons;
+ (NSTimeInterval)doubleClickInterval;
+ (NSTimeInterval)keyRepeatDelay;
+ (NSTimeInterval)keyRepeatInterval;
+ (BOOL)isMouseCoalescingEnabled;
+ (void)setMouseCoalescingEnabled:(BOOL)flag;

/* Blocks that see the application's events before they are sent: a local
   monitor returns the event to send (another, or nil for none). Global
   monitors see other applications' events, which Minkar does not give
   (as the reference does for applications without that permission). */
+ (id)addLocalMonitorForEventsMatchingMask:(NSEventMask)mask handler:(NSEvent *(^)(NSEvent *event))block;
+ (id)addGlobalMonitorForEventsMatchingMask:(NSEventMask)mask handler:(void (^)(NSEvent *event))block;
+ (void)removeMonitor:(id)eventMonitor;

+ (NSEvent *)eventWithCGEvent:(CGEventRef)cgEvent;

- initWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)modifierFlags window:(NSWindow *)window;

// AppKit's own constructors, for its events.
+ (NSEvent *)mouseEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)modifierFlags window:(NSWindow *)window clickCount:(NSInteger)clickCount deltaX:(CGFloat)deltaX deltaY:(CGFloat)deltaY;
+ (NSEvent *)mouseEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)modifierFlags window:(NSWindow *)window deltaY:(CGFloat)deltaY;

+ (NSEvent *)enterExitEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)flags timestamp:(NSTimeInterval)timestamp windowNumber:(NSInteger)windowNumber context:(NSGraphicsContext *)context eventNumber:(NSInteger)eventNumber trackingNumber:(NSInteger)tracking userData:(void *)userData;

+ (NSEvent *)mouseEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)flags timestamp:(NSTimeInterval)timestamp windowNumber:(NSInteger)windowNumber context:(NSGraphicsContext *)context eventNumber:(NSInteger)eventNumber clickCount:(NSInteger)clickCount pressure:(float)pressure;

+ (NSEvent *)keyEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)modifierFlags timestamp:(NSTimeInterval)timestamp windowNumber:(NSInteger)windowNumber context:(NSGraphicsContext *)context characters:(NSString *)characters charactersIgnoringModifiers:(NSString *)charactersIgnoringModifiers isARepeat:(BOOL)isARepeat keyCode:(unsigned short)keyCode;

+ (NSEvent *)otherEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)flags timestamp:(NSTimeInterval)timestamp windowNumber:(NSInteger)windowNum context:(NSGraphicsContext *)context subtype:(short)subtype data1:(NSInteger)data1 data2:(NSInteger)data2;

@property (readonly) NSEventType type;
@property (readonly) NSTimeInterval timestamp;
@property (readonly) NSPoint locationInWindow;
@property (readonly) NSEventModifierFlags modifierFlags;
@property (readonly, assign) NSWindow *window;
@property (readonly) NSInteger windowNumber;
@property (readonly) CGEventRef CGEvent;

@property (readonly) NSInteger clickCount;
@property (readonly) NSInteger buttonNumber;
@property (readonly) NSInteger eventNumber;
@property (readonly) float pressure;
@property (readonly) CGFloat deltaX;
@property (readonly) CGFloat deltaY;
@property (readonly) CGFloat deltaZ;
/* A scroll's distance: in lines for a wheel (hasPreciseScrollingDeltas
   NO), in points for devices that have them. */
@property (readonly) CGFloat scrollingDeltaX;
@property (readonly) CGFloat scrollingDeltaY;
@property (readonly) BOOL hasPreciseScrollingDeltas;
@property (readonly) NSEventPhase phase;
@property (readonly) NSEventPhase momentumPhase;
@property (readonly, getter=isDirectionInvertedFromDevice) BOOL directionInvertedFromDevice;
@property (readonly) CGFloat magnification;
@property (readonly) float rotation;
@property (readonly) NSInteger stage;
@property (readonly) NSPointingDeviceType pointingDeviceType;

@property (readonly, copy) NSString *characters;
@property (readonly, copy) NSString *charactersIgnoringModifiers;
@property (readonly, getter=isARepeat) BOOL ARepeat;
@property (readonly) unsigned short keyCode;
/* The function key the event is for (NSUpArrowFunctionKey...), or 0. */
@property (readonly) NSEventSpecialKey specialKey;

+ (void)startPeriodicEventsAfterDelay:(NSTimeInterval)delay withPeriod:(NSTimeInterval)period;
+ (void)stopPeriodicEvents;

@property (readonly) NSEventSubtype subtype;
@property (readonly) NSInteger data1;
@property (readonly) NSInteger data2;
@property (readonly) NSInteger trackingNumber;
@property (readonly) NSTrackingArea *trackingArea;
@property (readonly) void *userData;


- (void)trackSwipeEventWithOptions:(NSEventSwipeTrackingOptions)options dampenAmountThresholdMin:(CGFloat)minDampenThreshold max:(CGFloat)maxDampenThreshold usingHandler:(void (^)(CGFloat gestureAmount, NSEventPhase phase, BOOL isComplete, BOOL *stop))trackingHandler;
+ (BOOL)isSwipeTrackingFromScrollEventsEnabled;

@end

APPKIT_EXPORT NSEventMask NSEventMaskFromType(NSEventType type);
