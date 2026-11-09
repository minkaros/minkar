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

   if(destination!=nil && operation!=NSDragOperationNone){
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

@end
