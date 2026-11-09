/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSDraggingItem.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSView.h>

NSDraggingImageComponentKey const NSDraggingImageComponentIconKey=@"icon";
NSDraggingImageComponentKey const NSDraggingImageComponentLabelKey=@"label";

@implementation NSDraggingImageComponent

+(instancetype)draggingImageComponentWithKey:(NSDraggingImageComponentKey)key {
   return [[[self alloc] initWithKey:key] autorelease];
}

-initWithKey:(NSDraggingImageComponentKey)key {
   _key=[key copy];
   return self;
}

-(void)dealloc {
   [_key release];
   [_contents release];
   [super dealloc];
}

-(NSDraggingImageComponentKey)key { return _key; }
-(void)setKey:(NSDraggingImageComponentKey)key { key=[key copy]; [_key release]; _key=key; }
-(id)contents { return _contents; }
-(void)setContents:(id)contents { contents=[contents retain]; [_contents release]; _contents=contents; }
-(NSRect)frame { return _frame; }
-(void)setFrame:(NSRect)frame { _frame=frame; }

@end

@implementation NSDraggingItem

-initWithPasteboardWriter:(id<NSPasteboardWriting>)pasteboardWriter {
   _item=[pasteboardWriter retain];
   return self;
}

-(void)dealloc {
   [_item release];
   [_imageComponentsProvider release];
   [super dealloc];
}

-(id)item { return _item; }
-(NSRect)draggingFrame { return _draggingFrame; }
-(void)setDraggingFrame:(NSRect)frame { _draggingFrame=frame; }
-(NSArray *(^)(void))imageComponentsProvider { return _imageComponentsProvider; }

-(void)setImageComponentsProvider:(NSArray *(^)(void))provider {
   provider=[provider copy];
   [_imageComponentsProvider release];
   _imageComponentsProvider=provider;
}

/* One icon component the size of the frame, showing contents. */
-(void)setDraggingFrame:(NSRect)frame contents:(id)contents {
   _draggingFrame=frame;
   [self setImageComponentsProvider:(contents==nil)?nil:^NSArray *{
      NSDraggingImageComponent *component=[NSDraggingImageComponent draggingImageComponentWithKey:NSDraggingImageComponentIconKey];

      [component setContents:contents];
      [component setFrame:NSMakeRect(0,0,frame.size.width,frame.size.height)];
      return [NSArray arrayWithObject:component];
   }];
}

-(NSArray *)imageComponents {
   return (_imageComponentsProvider!=nil)?_imageComponentsProvider():nil;
}

/* The image the components make, frame-sized. */
-(NSImage *)_draggingImage {
   NSArray *components=[self imageComponents];
   NSSize   size=_draggingFrame.size;
   NSImage *image;

   if([components count]==0 || size.width<=0 || size.height<=0)
    return nil;
   image=[[[NSImage alloc] initWithSize:size] autorelease];
   [image lockFocus];
   for(NSDraggingImageComponent *component in components){
    id contents=[component contents];

    if([contents isKindOfClass:[NSImage class]])
     [(NSImage *)contents drawInRect:[component frame] fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:0.8];
   }
   [image unlockFocus];
   return image;
}

@end

@implementation NSDraggingSession

-(void)dealloc {
   [_draggingPasteboard release];
   [_items release];
   [super dealloc];
}

-(void)_setPasteboard:(NSPasteboard *)pasteboard items:(NSArray *)items {
   pasteboard=[pasteboard retain];
   [_draggingPasteboard release];
   _draggingPasteboard=pasteboard;
   items=[items copy];
   [_items release];
   _items=items;
}

-(NSDraggingFormation)draggingFormation { return _draggingFormation; }
-(void)setDraggingFormation:(NSDraggingFormation)value { _draggingFormation=value; }
-(BOOL)animatesToStartingPositionsOnCancelOrFail { return _animatesToStartingPositionsOnCancelOrFail; }
-(void)setAnimatesToStartingPositionsOnCancelOrFail:(BOOL)flag { _animatesToStartingPositionsOnCancelOrFail=flag; }
-(NSInteger)draggingLeaderIndex { return _draggingLeaderIndex; }
-(void)setDraggingLeaderIndex:(NSInteger)value { _draggingLeaderIndex=value; }
-(NSPasteboard *)draggingPasteboard { return _draggingPasteboard; }
-(NSInteger)draggingSequenceNumber { return _draggingSequenceNumber; }
-(NSPoint)draggingLocation { return _draggingLocation; }

-(void)enumerateDraggingItemsWithOptions:(NSDraggingItemEnumerationOptions)enumOpts forView:(NSView *)view classes:(NSArray *)classArray searchOptions:(NSDictionary *)searchOptions usingBlock:(void (^)(NSDraggingItem *draggingItem, NSInteger idx, BOOL *stop))block {
   BOOL      stop=NO;
   NSInteger index=0;

   for(NSDraggingItem *item in _items){
    BOOL matches=([classArray count]==0);

    for(Class cls in classArray)
     if([[item item] isKindOfClass:cls])
      matches=YES;
    if(!matches)
     continue;
    block(item,index++,&stop);
    if(stop)
     break;
   }
}

@end
