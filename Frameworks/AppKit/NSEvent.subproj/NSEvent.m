/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
                 2010 Markus Hitter <mah@jump-ing.de>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSEvent.h>
#import <AppKit/NSEvent_mouse.h>
#import <AppKit/NSEvent_keyboard.h>
#import <AppKit/NSEvent_periodic.h>
#import <AppKit/NSEvent_other.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSDisplay.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSException.h>
#import <ApplicationServices/ApplicationServices.h>
#import <objc/runtime.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSUserDefaults.h>

/* A monitor: its mask and block. */
@interface NSEventMonitor : NSObject {
@public
   NSEventMask _mask;
   id _block;
}
@end

@implementation NSEventMonitor
-(void)dealloc {
   [_block release];
   [super dealloc];
}
@end

@implementation NSEvent

+(NSPoint)mouseLocation {
   return [[NSDisplay currentDisplay] mouseLocation];
}

+(NSEventModifierFlags)modifierFlags {
   return [[NSDisplay currentDisplay] currentModifierFlags];
}

static NSUInteger pressedButtons;

/* NSApplication tells of each event it sends, for pressedMouseButtons. */
void NSEventNoteMouseButtons(NSEvent *event) {
   NSUInteger bit;

   switch([event type]){
    case NSEventTypeLeftMouseDown: case NSEventTypeLeftMouseUp:   bit=1<<0; break;
    case NSEventTypeRightMouseDown: case NSEventTypeRightMouseUp: bit=1<<1; break;
    case NSEventTypeOtherMouseDown: case NSEventTypeOtherMouseUp: bit=1<<2; break;
    default: return;
   }
   switch([event type]){
    case NSEventTypeLeftMouseDown: case NSEventTypeRightMouseDown: case NSEventTypeOtherMouseDown:
     pressedButtons|=bit;
     break;
    default:
     pressedButtons&=~bit;
     break;
   }
}

+(NSUInteger)pressedMouseButtons {
   return pressedButtons;
}

/* The user's settings (the reference's keys), in seconds; the key repeat
   ones are kept in 15 ms units. */
+(NSTimeInterval)doubleClickInterval {
   id value=[[NSUserDefaults standardUserDefaults] objectForKey:@"com.apple.mouse.doubleClickThreshold"];

   return (value!=nil && [value doubleValue]>0)?[value doubleValue]:0.5;
}

+(NSTimeInterval)keyRepeatDelay {
   id value=[[NSUserDefaults standardUserDefaults] objectForKey:@"InitialKeyRepeat"];

   return ((value!=nil && [value doubleValue]>0)?[value doubleValue]:25)*0.015;
}

+(NSTimeInterval)keyRepeatInterval {
   id value=[[NSUserDefaults standardUserDefaults] objectForKey:@"KeyRepeat"];

   return ((value!=nil && [value doubleValue]>0)?[value doubleValue]:6)*0.015;
}

static BOOL mouseCoalescing=YES;

+(BOOL)isMouseCoalescingEnabled {
   return mouseCoalescing;
}

+(void)setMouseCoalescingEnabled:(BOOL)flag {
   mouseCoalescing=flag;
}

/* No device scrolls with phases yet. */
+(BOOL)isSwipeTrackingFromScrollEventsEnabled {
   return NO;
}

static NSMutableArray *localMonitors;

+(id)addLocalMonitorForEventsMatchingMask:(NSEventMask)mask handler:(NSEvent *(^)(NSEvent *event))block {
   NSEventMonitor *monitor=[[[NSEventMonitor alloc] init] autorelease];

   monitor->_mask=mask;
   monitor->_block=[block copy];
   @synchronized([NSEvent class]){
    if(localMonitors==nil)
     localMonitors=[[NSMutableArray alloc] init];
    [localMonitors addObject:monitor];
   }
   return monitor;
}

+(id)addGlobalMonitorForEventsMatchingMask:(NSEventMask)mask handler:(void (^)(NSEvent *event))block {
   NSEventMonitor *monitor=[[[NSEventMonitor alloc] init] autorelease];

   monitor->_mask=mask;
   monitor->_block=[block copy];
   return monitor;
}

+(void)removeMonitor:(id)eventMonitor {
   @synchronized([NSEvent class]){
    [localMonitors removeObjectIdenticalTo:eventMonitor];
   }
}

/* Each local monitor that wants the event, in the order they were added;
   the event to send, or nil. */
NSEvent *NSEventRunLocalMonitors(NSEvent *event) {
   NSArray *monitors;

   @synchronized([NSEvent class]){
    monitors=[[localMonitors copy] autorelease];
   }
   for(NSEventMonitor *monitor in monitors){
    if(event==nil)
     break;
    if(monitor->_mask & NSEventMaskFromType([event type]))
     event=((NSEvent *(^)(NSEvent *))monitor->_block)(event);
   }
   return event;
}

/* Mouse, key and scroll events from a CG event, located in the window
   under it (or the screen, without one). */
+(NSEvent *)eventWithCGEvent:(CGEventRef)cgEvent {
   CGEventType          type;
   CGPoint              global;
   NSPoint              location;
   NSEventModifierFlags flags;
   NSWindow            *window;

   if(cgEvent==NULL)
    return nil;
   type=CGEventGetType(cgEvent);
   global=CGEventGetUnflippedLocation(cgEvent);
   flags=(NSEventModifierFlags)CGEventGetFlags(cgEvent)&NSEventModifierFlagDeviceIndependentFlagsMask;
   window=[NSApp windowWithWindowNumber:[NSWindow windowNumberAtPoint:NSMakePoint(global.x,global.y) belowWindowWithWindowNumber:0]];
   location=(window!=nil)?[window convertScreenToBase:NSMakePoint(global.x,global.y)]:NSMakePoint(global.x,global.y);

   switch(type){
    case kCGEventLeftMouseDown: case kCGEventLeftMouseUp:
    case kCGEventRightMouseDown: case kCGEventRightMouseUp:
    case kCGEventOtherMouseDown: case kCGEventOtherMouseUp:
    case kCGEventMouseMoved:
    case kCGEventLeftMouseDragged: case kCGEventRightMouseDragged: case kCGEventOtherMouseDragged:
     return [self mouseEventWithType:(NSEventType)type location:location modifierFlags:flags window:window
        clickCount:CGEventGetIntegerValueField(cgEvent,kCGMouseEventClickState)
        deltaX:CGEventGetDoubleValueField(cgEvent,kCGMouseEventDeltaX) deltaY:CGEventGetDoubleValueField(cgEvent,kCGMouseEventDeltaY)];

    case kCGEventScrollWheel:
     return [self mouseEventWithType:NSEventTypeScrollWheel location:location modifierFlags:flags window:window
        deltaY:CGEventGetIntegerValueField(cgEvent,kCGScrollWheelEventDeltaAxis1)];

    case kCGEventKeyDown: case kCGEventKeyUp: case kCGEventFlagsChanged: {
     UniChar      buffer[16];
     UniCharCount length=0;
     NSString    *characters;

     CGEventKeyboardGetUnicodeString(cgEvent,16,&length,buffer);
     characters=[NSString stringWithCharacters:buffer length:length];
     return [self keyEventWithType:(NSEventType)type location:location modifierFlags:flags
        timestamp:[NSDate timeIntervalSinceReferenceDate] windowNumber:[window windowNumber] context:nil
        characters:characters charactersIgnoringModifiers:characters
        isARepeat:CGEventGetIntegerValueField(cgEvent,kCGKeyboardEventAutorepeat)!=0
        keyCode:(unsigned short)CGEventGetIntegerValueField(cgEvent,kCGKeyboardEventKeycode)];
    }

    default:
     return nil;
   }
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

-initWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)modifierFlags window:(NSWindow *)window {
   _type=type;
   _timestamp=[NSDate timeIntervalSinceReferenceDate];
   _locationInWindow=location;
   _modifierFlags=modifierFlags;
   _windowNumber=[window windowNumber];
   return self;
}

-(void)dealloc {
   [super dealloc];
}

+(NSEvent *)mouseEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)modifierFlags window:(NSWindow *)window clickCount:(NSInteger)clickCount deltaX:(CGFloat)deltaX deltaY:(CGFloat)deltaY{
   return [[[NSEvent_mouse alloc] initWithType:type location:location modifierFlags:modifierFlags window:window clickCount:clickCount deltaX:deltaX deltaY:deltaY] autorelease];
}

+(NSEvent *)mouseEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)modifierFlags window:(NSWindow *)window deltaY:(CGFloat)deltaY {
   return [[[NSEvent_mouse alloc] initWithType:type location:location modifierFlags:modifierFlags window:window deltaY:deltaY] autorelease];
}

+(NSEvent *)enterExitEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)flags timestamp:(NSTimeInterval)timestamp windowNumber:(NSInteger)windowNumber context:(NSGraphicsContext *)context eventNumber:(NSInteger)eventNumber trackingNumber:(NSInteger)tracking userData:(void *)userData {
 return [[[NSEvent_mouse alloc] initWithType:type location:location modifierFlags:flags timestamp:timestamp windowNumber:windowNumber context:context eventNumber:eventNumber trackingNumber:tracking userData:userData] autorelease];
}

+(NSEvent *)mouseEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)modifierFlags timestamp:(NSTimeInterval)timestamp windowNumber:(NSInteger)windowNumber context:(NSGraphicsContext*)context eventNumber:(NSInteger)eventNumber clickCount:(NSInteger)clickCount pressure:(float)pressure {
   return [[[NSEvent_mouse alloc] initWithType:type location:location modifierFlags:modifierFlags timestamp:timestamp windowNumber:windowNumber context:context eventNumber:eventNumber clickCount:clickCount pressure:pressure] autorelease];
}

+(NSEvent *)keyEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)modifierFlags window:(NSWindow *)window characters:(NSString *)characters charactersIgnoringModifiers:(NSString *)charactersIgnoringModifiers isARepeat:(BOOL)isARepeat keyCode:(unsigned short)keyCode {
   return [[[NSEvent_keyboard alloc] initWithType:type location:location modifierFlags:modifierFlags window:window characters:characters charactersIgnoringModifiers:charactersIgnoringModifiers isARepeat:isARepeat keyCode:keyCode] autorelease];
}

+(NSEvent *)keyEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)modifierFlags timestamp:(NSTimeInterval)timestamp windowNumber:(NSInteger)windowNumber context:(NSGraphicsContext *)context characters:(NSString *)characters charactersIgnoringModifiers:(NSString *)charactersIgnoringModifiers isARepeat:(BOOL)isARepeat keyCode:(unsigned short)keyCode {   
   return [[[NSEvent_keyboard alloc] initWithType:type location:location modifierFlags:modifierFlags window:[NSApp windowWithWindowNumber:windowNumber] characters:characters charactersIgnoringModifiers:charactersIgnoringModifiers isARepeat:isARepeat keyCode:keyCode] autorelease];
}

+(NSEvent *)otherEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSEventModifierFlags)flags timestamp:(NSTimeInterval)timestamp windowNumber:(NSInteger)windowNum context:(NSGraphicsContext *)context subtype:(short)subtype data1:(NSInteger)data1 data2:(NSInteger)data2 {
   return [[[NSEvent_other alloc] initWithType:type location:location modifierFlags:flags timestamp:timestamp windowNumber:windowNum context:context subtype:subtype data1:data1 data2:data2] autorelease];
}


-(NSEventType)type {
   return _type;
}

-(NSTimeInterval)timestamp {
   return _timestamp;
}

-(NSPoint)locationInWindow {
   return _locationInWindow;
}

-(NSEventModifierFlags)modifierFlags {
   return _modifierFlags;
}

-(NSWindow *)window {
   return [NSApp windowWithWindowNumber:[self windowNumber]];
}

-(NSInteger)windowNumber {
    return _windowNumber;
}

-(NSInteger)clickCount {
   return 0;
}

-(CGFloat)deltaX {
   return 0;
}

-(CGFloat)deltaY {
   return 0;
}

-(NSInteger)buttonNumber {
   [NSException raise:NSInternalInconsistencyException format:@"-[%@ %s] not valid for type %lu",[self class],sel_getName(_cmd),(unsigned long)_type];
   return 0;
}

-(NSInteger)eventNumber {
   return 0;
}

-(float)pressure {
   return 0;
}

-(CGFloat)scrollingDeltaX {
   return [self deltaX];
}

-(CGFloat)scrollingDeltaY {
   return [self deltaY];
}

-(BOOL)hasPreciseScrollingDeltas {
   return NO;
}

-(NSEventPhase)phase {
   return NSEventPhaseNone;
}

-(NSEventPhase)momentumPhase {
   return NSEventPhaseNone;
}

-(BOOL)isDirectionInvertedFromDevice {
   return NO;
}

-(CGFloat)magnification {
   return 0;
}

-(float)rotation {
   return 0;
}

-(NSInteger)stage {
   return 0;
}

-(NSPointingDeviceType)pointingDeviceType {
   return NSPointingDeviceTypeUnknown;
}

-(BOOL)isARepeat {
   [NSException raise:NSInternalInconsistencyException format:@"-[%@ %s] not valid for type %lu",[self class],sel_getName(_cmd),(unsigned long)_type];
   return NO;
}

-(NSEventSpecialKey)specialKey {
   NSString *characters;
   unichar   c;

   if(_type!=NSEventTypeKeyDown && _type!=NSEventTypeKeyUp)
    return 0;
   characters=[self charactersIgnoringModifiers];
   if([characters length]==0)
    return 0;
   c=[characters characterAtIndex:0];
   if(c>=0xF700 && c<=0xF8FF)
    return c;
   switch(c){
    case 0x03: case 0x08: case 0x09: case 0x0A: case 0x0D: case 0x19: case 0x7F:
     return c;
    default:
     return 0;
   }
}

/* A CG event with the event's type, place, flags and, for keys, its code
   and characters; NULL for types CG has none for. Valid as long as the
   current autorelease pool. */
-(CGEventRef)CGEvent {
   CGEventRef result=CGEventCreate(NULL);
   NSWindow  *window=[self window];
   NSPoint    point=(window!=nil)?[window convertBaseToScreen:_locationInWindow]:_locationInWindow;
   CGFloat    height=CGDisplayBounds(CGMainDisplayID()).size.height;

   if(result==NULL)
    return NULL;
   CGEventSetType(result,(CGEventType)_type);
   CGEventSetLocation(result,CGPointMake(point.x,height-point.y));
   CGEventSetFlags(result,(CGEventFlags)_modifierFlags);
   if(_type==NSEventTypeKeyDown || _type==NSEventTypeKeyUp){
    NSString *characters=[self characters];
    unichar   buffer[16];
    NSUInteger length=MIN([characters length],(NSUInteger)16);

    [characters getCharacters:buffer range:NSMakeRange(0,length)];
    CGEventKeyboardSetUnicodeString(result,length,buffer);
    CGEventSetIntegerValueField(result,kCGKeyboardEventKeycode,[self keyCode]);
   }
   CFAutorelease(result);
   return result;
}

-(CGFloat)deltaZ {
   return 0;
}

-(NSString *)characters {
   return nil;
}

-(NSString *)charactersIgnoringModifiers {
   return nil;
}

-(unsigned short)keyCode {
   return 0xFFFF;
}

-(NSString *)description {
    return [NSString stringWithFormat:@"<NSEvent: type=%d loc=(%f,%f) time=%f flags=0x%X win=%p winNum=%d",[self type], [self locationInWindow].x, [self locationInWindow].y, [self timestamp], [self modifierFlags], [self window], [self windowNumber]];
}


static NSTimer *_periodicTimer=nil;

+(void)_periodicDelay:(NSTimer *)timer {
   NSTimeInterval period=[[timer userInfo] doubleValue];

   [_periodicTimer invalidate];
   [_periodicTimer release];

   _periodicTimer=[[NSTimer timerWithTimeInterval:period
     target:self selector:@selector(_periodicEvent:) userInfo:nil
     repeats:YES] retain];

   [[NSRunLoop currentRunLoop] addTimer:_periodicTimer forMode:NSEventTrackingRunLoopMode];
}

+(void)_periodicEvent:(NSTimer *)timer {
   NSEvent *event=[[[NSEvent_periodic alloc] initWithType:NSPeriodic location:NSMakePoint(0,0) modifierFlags:0 window:nil] autorelease];

   [[NSDisplay currentDisplay] postEvent:event atStart:NO];
   [[NSDisplay currentDisplay] discardEventsMatchingMask:NSPeriodicMask beforeEvent:event];
}


+(void)startPeriodicEventsAfterDelay:(NSTimeInterval)delay withPeriod:(NSTimeInterval)period {
   NSNumber *userInfo=[NSNumber numberWithDouble:period];

   if(_periodicTimer!=nil)
     [NSException raise:NSInternalInconsistencyException format:@"periodic events already enabled"];

   _periodicTimer=[[NSTimer timerWithTimeInterval:delay
     target:self selector:@selector(_periodicDelay:) userInfo:userInfo
     repeats:NO] retain];

   [[NSRunLoop currentRunLoop] addTimer:_periodicTimer forMode:NSEventTrackingRunLoopMode];
}

+(void)stopPeriodicEvents {
   [_periodicTimer invalidate];
   [_periodicTimer release];
   _periodicTimer=nil;
}

-(NSEventSubtype)subtype {
   [NSException raise:NSInternalInconsistencyException format:@"No event subtype in %@",[self class]];
   return 0;
}

-(NSInteger)data1 {
   [NSException raise:NSInternalInconsistencyException format:@"No event data1 in %@",[self class]];
   return 0;
}

-(NSInteger)data2 {
   [NSException raise:NSInternalInconsistencyException format:@"No event data2 in %@",[self class]];
   return 0;
}

-(void *)userData {
	[NSException raise:NSInternalInconsistencyException format:@"No userData in %@",[self class]];
	return 0;
}

-(NSInteger)trackingNumber {
	[NSException raise:NSInternalInconsistencyException format:@"No trackingNumber in %@",[self class]];
	return 0;
}

-(NSTrackingArea *)trackingArea {
	[NSException raise:NSInternalInconsistencyException format:@"No trackingArea in %@",[self class]];
    return nil;
}


/* There are no swipe gestures from the input devices yet: the handler
   hears the gesture end at once, with no amount. */
-(void)trackSwipeEventWithOptions:(NSEventSwipeTrackingOptions)options dampenAmountThresholdMin:(CGFloat)minDampenThreshold max:(CGFloat)maxDampenThreshold
   usingHandler:(void (^)(CGFloat gestureAmount, NSEventPhase phase, BOOL isComplete, BOOL *stop))trackingHandler {
   BOOL stop=NO;

   if(trackingHandler!=nil)
    trackingHandler(0,NSEventPhaseEnded,YES,&stop);
}


@end

NSEventMask NSEventMaskFromType(NSEventType type){
   return 1ULL<<type;
}

