/* Copyright (c) 2006-2007 Christopher J. W. Lloyd <cjwl@objc.net>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSDraggingManager.h>
#import <AppKit/NSDisplay.h>
#import <AppKit/NSRaise.h>

#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow-Drag.h>
#import <AppKit/NSImageView.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSPasteboard.h>
#import <AppKit/NSDraggingItem.h>
#import <AppKit/NSColor.h>
#include <SkyLight/SkyLight.h>
#include <CoreGraphics/CoreGraphics.h>
#include <unistd.h>

extern SLSWindowID _NSSLSWindowForWindowNumber(NSInteger number);
extern NSWindow *_NSWindowForSLSWindow(SLSWindowID window);

/* What a destination is told about the drag in progress. */
@interface NSDraggingInfoImpl : NSObject <NSDraggingInfo> {
@public
   NSPasteboard   *_pasteboard;
   id              _source;
   NSDragOperation _sourceMask;
   NSDragOperation _lastOperation;
   NSImage        *_image;
   NSPoint         _imageLocation;     /* screen */
   NSPoint         _screenLocation;
   NSWindow       *_destinationWindow;
   NSInteger       _sequenceNumber;
   NSInteger       _formation;
   BOOL            _animatesToDestination;
   NSInteger       _validItems;
   NSArray        *_items;
}
@end

@implementation NSDraggingInfoImpl

-(void)dealloc {
   [_pasteboard release];
   [_image release];
   [_items release];
   [super dealloc];
}

-(NSPasteboard *)draggingPasteboard { return _pasteboard; }
-(NSDragOperation)draggingSourceOperationMask { return _sourceMask; }

/* In the destination window's coordinates. */
-(NSPoint)draggingLocation {
   return (_destinationWindow!=nil)?[_destinationWindow convertScreenToBase:_screenLocation]:_screenLocation;
}

-(NSImage *)draggedImage { return _image; }

-(NSPoint)draggedImageLocation {
   return (_destinationWindow!=nil)?[_destinationWindow convertScreenToBase:_imageLocation]:_imageLocation;
}

-(NSWindow *)draggingDestinationWindow { return _destinationWindow; }
-(id)draggingSource { return _source; }
-(void)slideDraggedImageTo:(NSPoint)point { }
-(NSInteger)draggingSequenceNumber { return _sequenceNumber; }
-(NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)destination { return nil; }
-(NSInteger)draggingFormation { return _formation; }
-(void)setDraggingFormation:(NSInteger)value { _formation=value; }
-(BOOL)animatesToDestination { return _animatesToDestination; }
-(void)setAnimatesToDestination:(BOOL)flag { _animatesToDestination=flag; }
-(NSInteger)numberOfValidItemsForDrop { return _validItems; }
-(void)setNumberOfValidItemsForDrop:(NSInteger)value { _validItems=value; }
-(NSInteger)springLoadingHighlight { return 0; }
-(void)resetSpringLoading { }

-(void)enumerateDraggingItemsWithOptions:(NSUInteger)enumOpts forView:(NSView *)view classes:(NSArray *)classArray searchOptions:(NSDictionary *)searchOptions usingBlock:(void (^)(id draggingItem, NSInteger idx, BOOL *stop))block {
   NSArray  *items=_items;
   BOOL      stop=NO;
   NSInteger index=0;

   /* Without session items, one per object the pasteboard reads. */
   if(items==nil){
    NSMutableArray *made=[NSMutableArray array];

    for(id object in [_pasteboard readObjectsForClasses:classArray options:searchOptions])
     [made addObject:[[[NSDraggingItem alloc] initWithPasteboardWriter:object] autorelease]];
    items=made;
   }
   for(NSDraggingItem *item in items){
    block(item,index++,&stop);
    if(stop)
     break;
   }
}

@end

static NSInteger dragSequence;
static NSDraggingManager *sharedManager;

@implementation NSDraggingManager

+(NSDraggingManager *)_sharedManager {
   if(sharedManager==nil)
    sharedManager=[[NSDraggingManager alloc] init];
   return sharedManager;
}


+(NSDraggingManager *)draggingManager {
   return [[NSDisplay currentDisplay] draggingManager];
}

/* Windows keep their own dragged types; nothing to register. */
-(void)registerWindow:(NSWindow *)window dragTypes:(NSArray *)types {
}

-(void)unregisterWindow:(NSWindow *)window {
}

static id localSource;

/* ---- Drags between applications (through the window server) ---- */

/* The source's view of a drag over another application's window: the
   operation its last reply chose, and whether the drop was answered. */
static NSDragOperation remoteOperation;
static NSInteger       remoteSequence, performSequence;
static BOOL            performAnswered;
static NSDragOperation performOperation;

/* The destination's: the drag over one of its windows from elsewhere. */
static NSDraggingInfoImpl *remoteInfo;
static id                  remoteDestination;

/* The topmost window at a screen point below the drag image, of any
   application: its server window and its owner. */
static SLSWindowID serverWindowAtPoint(NSPoint point,NSWindow *dragWindow,pid_t *owner){
   CFArrayRef  list=CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly,kCGNullWindowID);
   CGFloat     height=CGDisplayBounds(CGMainDisplayID()).size.height;
   CGPoint     top=CGPointMake(point.x,height-point.y);
   SLSWindowID drag=_NSSLSWindowForWindowNumber([dragWindow windowNumber]), found=0;

   *owner=0;
   if(list==NULL)
    return 0;
   for(CFIndex i=0;i<CFArrayGetCount(list) && found==0;i++){
    CFDictionaryRef entry=CFArrayGetValueAtIndex(list,i);
    CFNumberRef     number=CFDictionaryGetValue(entry,kCGWindowNumber);
    CFNumberRef     pid=CFDictionaryGetValue(entry,kCGWindowOwnerPID);
    CFDictionaryRef boundsDict=CFDictionaryGetValue(entry,kCGWindowBounds);
    int32_t         windowNumber=0,ownerPID=0;
    CGRect          bounds;

    if(number==NULL || pid==NULL || boundsDict==NULL || !CGRectMakeWithDictionaryRepresentation(boundsDict,&bounds))
     continue;
    CFNumberGetValue(number,kCFNumberSInt32Type,&windowNumber);
    CFNumberGetValue(pid,kCFNumberSInt32Type,&ownerPID);
    if((SLSWindowID)windowNumber==drag || !CGRectContainsPoint(bounds,top))
     continue;
    found=(SLSWindowID)windowNumber;
    *owner=ownerPID;
   }
   CFRelease(list);
   return found;
}

static void postDragEvent(SLSWindowID to,uint32_t phase,NSInteger sequence,NSDragOperation operation,
                          NSPoint screen,SLSWindowID from){
   SLSEvent e;
   CGFloat  height=CGDisplayBounds(CGMainDisplayID()).size.height;

   memset(&e,0,sizeof(e));
   e.type=kSLSEventDrag;
   e.clickCount=(int32_t)phase;
   e.buttonNumber=(uint32_t)sequence;
   e.flags=(uint32_t)operation;
   e.x=screen.x;
   e.y=height-screen.y;
   e.dx=from;
   SLSPostDragEvent(to,&e);
}

-(id)localDraggingSource {
   return localSource;
}

/* The screen point of an event (its window's base coordinates). */
static NSPoint screenPointOfEvent(NSEvent *event){
   NSWindow *window=[event window];
   NSPoint   point=[event locationInWindow];

   return (window!=nil)?[window convertBaseToScreen:point]:point;
}

/* This application's window under the point, below the drag image. Drops
   on other applications' windows wait for the window server. */
static NSWindow *destinationWindowAtPoint(NSPoint point,NSWindow *dragWindow){
   NSInteger number=[NSWindow windowNumberAtPoint:point belowWindowWithWindowNumber:[dragWindow windowNumber]];

   return (number!=0)?[NSApp windowWithWindowNumber:number]:nil;
}

/* NSView and NSWindow answer draggingUpdated: with the source's mask; a
   destination that only says what it takes in draggingEntered: keeps that
   answer. */
static BOOL overridesDraggingUpdated(id destination){
   SEL sel=@selector(draggingUpdated:);
   IMP imp;

   if(![destination respondsToSelector:sel])
    return NO;
   imp=[destination methodForSelector:sel];
   if([destination isKindOfClass:[NSView class]])
    return imp!=[NSView instanceMethodForSelector:sel];
   if([destination isKindOfClass:[NSWindow class]])
    return imp!=[NSWindow instanceMethodForSelector:sel];
   return YES;
}

-(NSDragOperation)_operationForDestination:(id)destination info:(NSDraggingInfoImpl *)info entered:(BOOL)entered {
   NSDragOperation operation=NSDragOperationNone;

   if(entered && [destination respondsToSelector:@selector(draggingEntered:)])
    operation=[destination draggingEntered:info];
   else if(!entered && overridesDraggingUpdated(destination))
    operation=[destination draggingUpdated:info];
   else if(entered)
    operation=NSDragOperationGeneric;
   else
    /* Without draggingUpdated: the answer to draggingEntered: holds. */
    return info->_lastOperation;
   info->_lastOperation=operation&info->_sourceMask;
   return info->_lastOperation;
}

/* Location is in the window coordinates of the event's window. The image
   follows the mouse in a window of its own; the window under the mouse
   is told of the drag (entered, updated, exited) and, on mouse up, of the
   drop. Without a drop the image slides back. */
-(void)dragImage:(NSImage *)image at:(NSPoint)location offset:(NSSize)offset event:(NSEvent *)event pasteboard:(NSPasteboard *)pasteboard source:(id)source slideBack:(BOOL)slideBack {
   NSDraggingInfoImpl *info=[[[NSDraggingInfoImpl alloc] init] autorelease];
   NSWindow           *sourceWindow=[event window];
   NSPoint             start=(sourceWindow!=nil)?[sourceWindow convertBaseToScreen:location]:location;
   NSPoint             mouse=screenPointOfEvent(event);
   NSSize              size=(image!=nil)?[image size]:NSMakeSize(1,1);
   NSWindow           *dragWindow;
   NSImageView        *imageView;
   id                  destination=nil;
   NSWindow           *destinationWindow=nil;
   NSDragOperation     operation=NSDragOperationNone;
   NSPoint             grab=NSMakePoint(mouse.x-start.x,mouse.y-start.y);

   info->_pasteboard=[pasteboard retain];
   info->_source=source;
   info->_image=[image retain];
   info->_sequenceNumber=++dragSequence;
   info->_sourceMask=[source respondsToSelector:@selector(draggingSourceOperationMaskForLocal:)]?
      [source draggingSourceOperationMaskForLocal:YES]:NSDragOperationEvery;
   localSource=source;

   dragWindow=[[[NSWindow alloc] initWithContentRect:NSMakeRect(start.x,start.y,size.width,size.height)
      styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO] autorelease];
   [dragWindow setReleasedWhenClosed:NO];
   [dragWindow setOpaque:NO];
   [dragWindow setBackgroundColor:[NSColor clearColor]];
   [dragWindow setIgnoresMouseEvents:YES];
   [dragWindow setLevel:NSPopUpMenuWindowLevel];
   imageView=[[[NSImageView alloc] initWithFrame:NSMakeRect(0,0,size.width,size.height)] autorelease];
   [imageView setImage:image];
   [imageView setImageFrameStyle:NSImageFrameNone];
   [dragWindow setContentView:imageView];
   [dragWindow orderFront:nil];
   if([source respondsToSelector:@selector(draggedImage:beganAt:)])
    [source draggedImage:image beganAt:start];

   SLSWindowID remote=0, dragServerWindow=_NSSLSWindowForWindowNumber([dragWindow windowNumber]);

   remoteOperation=NSDragOperationNone;
   for(;;){
    NSEvent *next=[NSApp nextEventMatchingMask:NSEventMaskLeftMouseDragged|NSEventMaskLeftMouseUp|NSEventMaskMouseMoved|NSEventMaskFlagsChanged
       untilDate:[NSDate distantFuture] inMode:NSEventTrackingRunLoopMode dequeue:YES];
    NSPoint  point=screenPointOfEvent(next);
    NSWindow *window;
    id        receiver;

    info->_screenLocation=point;
    info->_imageLocation=NSMakePoint(point.x-grab.x,point.y-grab.y);
    [dragWindow setFrameOrigin:info->_imageLocation];
    if([source respondsToSelector:@selector(draggedImage:movedTo:)])
     [source draggedImage:image movedTo:info->_imageLocation];

    window=destinationWindowAtPoint(point,dragWindow);

    /* Another application's window: told through the window server; the
       operation is what it last answered. */
    pid_t       owner=0;
    SLSWindowID under=(window==nil)?serverWindowAtPoint(point,dragWindow,&owner):0;
    if(under!=0 && owner==getpid())
     under=0;
    if(under!=remote){
     if(remote!=0)
      postDragEvent(remote,kSLSDragExited,++remoteSequence,info->_sourceMask,point,dragServerWindow);
     remote=under;
     remoteOperation=NSDragOperationNone;
     if(remote!=0)
      postDragEvent(remote,kSLSDragEntered,++remoteSequence,info->_sourceMask,point,dragServerWindow);
    }
    else if(remote!=0)
     postDragEvent(remote,kSLSDragUpdated,++remoteSequence,info->_sourceMask,point,dragServerWindow);

    info->_destinationWindow=window;
    receiver=(window!=nil)?[window _receiverForDragSession:info]:nil;
    if(receiver!=destination){
     if([destination respondsToSelector:@selector(draggingExited:)]){
      info->_destinationWindow=destinationWindow;
      [destination draggingExited:info];
      info->_destinationWindow=window;
     }
     destination=receiver;
     destinationWindow=window;
     operation=(destination!=nil)?[self _operationForDestination:destination info:info entered:YES]:NSDragOperationNone;
    }
    else if(destination!=nil)
     operation=[self _operationForDestination:destination info:info entered:NO];

    if([next type]==NSEventTypeLeftMouseUp)
     break;
   }

   BOOL dropped=NO;

   if(remote!=0){
    /* The drop on another application: asked to perform, its answer
       waited for a moment. */
    NSDate *limit=[NSDate dateWithTimeIntervalSinceNow:2.0];

    performAnswered=NO;
    performOperation=NSDragOperationNone;
    performSequence=++remoteSequence;
    if(remoteOperation!=NSDragOperationNone){
     postDragEvent(remote,kSLSDragPerform,performSequence,info->_sourceMask,info->_screenLocation,dragServerWindow);
     while(!performAnswered && [limit timeIntervalSinceNow]>0)
      [[NSRunLoop currentRunLoop] runMode:NSEventTrackingRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
    else
     postDragEvent(remote,kSLSDragExited,performSequence,info->_sourceMask,info->_screenLocation,dragServerWindow);
    dropped=performAnswered && performOperation!=NSDragOperationNone;
    operation=performOperation;
   }
   else if(destination!=nil && operation!=NSDragOperationNone){
    BOOL prepared=![destination respondsToSelector:@selector(prepareForDragOperation:)] || [destination prepareForDragOperation:info];

    if(prepared && [destination respondsToSelector:@selector(performDragOperation:)] && [destination performDragOperation:info]){
     dropped=YES;
     if([destination respondsToSelector:@selector(concludeDragOperation:)])
      [destination concludeDragOperation:info];
    }
   }
   else if([destination respondsToSelector:@selector(draggingExited:)])
    [destination draggingExited:info];

   if(!dropped && slideBack)
    [dragWindow setFrameOrigin:start];
   [dragWindow orderOut:nil];
   [dragWindow close];
   if([source respondsToSelector:@selector(draggedImage:endedAt:operation:)])
    [source draggedImage:image endedAt:info->_imageLocation operation:dropped?operation:NSDragOperationNone];
   localSource=nil;
}

/* A drag event from the window server: a reply to this application's
   drag, or a drag from elsewhere over one of its windows. */
-(void)_skyLightDragEvent:(NSData *)data {
   SLSEvent e;

   if([data length]!=sizeof(e))
    return;
   memcpy(&e,[data bytes],sizeof(e));

   if(e.clickCount==kSLSDragReply){
    if((NSInteger)e.buttonNumber==performSequence && performSequence!=0){
     performOperation=(NSDragOperation)e.flags;
     performAnswered=YES;
    }
    else
     remoteOperation=(NSDragOperation)e.flags;
    return;
   }

   NSWindow       *window=_NSWindowForSLSWindow(e.window);
   CGFloat         height=CGDisplayBounds(CGMainDisplayID()).size.height;
   NSDragOperation answer=NSDragOperationNone;

   if(remoteInfo==nil || e.clickCount==kSLSDragEntered){
    [remoteInfo release];
    remoteInfo=[[NSDraggingInfoImpl alloc] init];
    remoteInfo->_pasteboard=[[NSPasteboard pasteboardWithName:NSDragPboard] retain];
    remoteInfo->_sequenceNumber=++dragSequence;
    remoteDestination=nil;
   }
   remoteInfo->_sourceMask=(NSDragOperation)e.flags;
   remoteInfo->_screenLocation=NSMakePoint(e.x,height-e.y);
   remoteInfo->_imageLocation=remoteInfo->_screenLocation;
   remoteInfo->_destinationWindow=window;

   id receiver=(window!=nil)?[window _receiverForDragSession:remoteInfo]:nil;

   if(e.clickCount==kSLSDragExited){
    if([remoteDestination respondsToSelector:@selector(draggingExited:)])
     [remoteDestination draggingExited:remoteInfo];
    remoteDestination=nil;
    [remoteInfo release];
    remoteInfo=nil;
    return;
   }
   if(receiver!=remoteDestination){
    if([remoteDestination respondsToSelector:@selector(draggingExited:)])
     [remoteDestination draggingExited:remoteInfo];
    remoteDestination=receiver;
    answer=(receiver!=nil)?[self _operationForDestination:receiver info:remoteInfo entered:YES]:NSDragOperationNone;
   }
   else if(receiver!=nil)
    answer=[self _operationForDestination:receiver info:remoteInfo entered:NO];

   if(e.clickCount==kSLSDragPerform){
    BOOL done=NO;

    if(receiver!=nil && answer!=NSDragOperationNone){
     BOOL prepared=![receiver respondsToSelector:@selector(prepareForDragOperation:)] || [receiver prepareForDragOperation:remoteInfo];

     if(prepared && [receiver respondsToSelector:@selector(performDragOperation:)] && [receiver performDragOperation:remoteInfo]){
      done=YES;
      if([receiver respondsToSelector:@selector(concludeDragOperation:)])
       [receiver concludeDragOperation:remoteInfo];
     }
    }
    else if([receiver respondsToSelector:@selector(draggingExited:)])
     [receiver draggingExited:remoteInfo];
    if(!done)
     answer=NSDragOperationNone;
    remoteDestination=nil;
   }
   postDragEvent((SLSWindowID)e.dx,kSLSDragReply,e.buttonNumber,answer,remoteInfo->_screenLocation,e.window);
   if(e.clickCount==kSLSDragPerform){
    [remoteInfo release];
    remoteInfo=nil;
   }
}

@end
