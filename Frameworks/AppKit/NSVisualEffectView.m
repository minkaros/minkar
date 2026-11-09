/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSVisualEffectView.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSGraphicsContext.h>

@implementation NSVisualEffectView

-initWithFrame:(NSRect)frame {
   self=[super initWithFrame:frame];
   if(self!=nil){
    _material=NSVisualEffectMaterialAppearanceBased;
    _blendingMode=NSVisualEffectBlendingModeBehindWindow;
   }
   return self;
}

-initWithCoder:(NSCoder *)coder {
   self=[super initWithCoder:coder];
   if(self!=nil && [coder allowsKeyedCoding]){
    _material=[coder decodeIntegerForKey:@"NSMaterial"];
    _blendingMode=[coder decodeIntegerForKey:@"NSBlendingMode"];
    _state=[coder decodeIntegerForKey:@"NSVisualEffectState"];
   }
   return self;
}

-(void)dealloc {
   [_maskImage release];
   [super dealloc];
}

-(BOOL)isOpaque {
   return NO;
}

-(NSVisualEffectMaterial)material { return _material; }
-(void)setMaterial:(NSVisualEffectMaterial)material { _material=material; [self setNeedsDisplay:YES]; }
-(NSVisualEffectBlendingMode)blendingMode { return _blendingMode; }
-(void)setBlendingMode:(NSVisualEffectBlendingMode)mode { _blendingMode=mode; [self setNeedsDisplay:YES]; }
-(NSVisualEffectState)state { return _state; }
-(void)setState:(NSVisualEffectState)state { _state=state; [self setNeedsDisplay:YES]; }
-(BOOL)isEmphasized { return _emphasized; }
-(void)setEmphasized:(BOOL)flag { _emphasized=flag; [self setNeedsDisplay:YES]; }
-(NSImage *)maskImage { return _maskImage; }

-(void)setMaskImage:(NSImage *)image {
   image=[image retain];
   [_maskImage release];
   _maskImage=image;
   [self setNeedsDisplay:YES];
}

/* The dark materials carry light text. */
-(NSBackgroundStyle)interiorBackgroundStyle {
   switch(_material){
    case NSVisualEffectMaterialDark:
    case NSVisualEffectMaterialUltraDark:
    case NSVisualEffectMaterialHUDWindow:
     return NSBackgroundStyleDark;
    case NSVisualEffectMaterialSelection:
     return _emphasized?NSBackgroundStyleEmphasized:NSBackgroundStyleNormal;
    default:
     return NSBackgroundStyleNormal;
   }
}

/* Each material's surface, translucent where Atlas draws it so. */
-(NSColor *)_materialColor {
   BOOL active=(_state==NSVisualEffectStateActive) || (_state==NSVisualEffectStateFollowsWindowActiveState && [[self window] isKeyWindow]);

   switch(_material){
    case NSVisualEffectMaterialSidebar:
     return [NSColor colorWithCalibratedWhite:0.91 alpha:0.96];
    case NSVisualEffectMaterialMenu:
     return [NSColor colorWithCalibratedWhite:0.97 alpha:0.96];
    case NSVisualEffectMaterialPopover:
     return [NSColor colorWithCalibratedWhite:0.96 alpha:0.98];
    case NSVisualEffectMaterialTitlebar:
    case NSVisualEffectMaterialHeaderView:
     return [NSColor colorWithCalibratedWhite:active?0.91:0.95 alpha:1];
    case NSVisualEffectMaterialSelection:
     return (_emphasized && active)?[NSColor selectedContentBackgroundColor]:[NSColor unemphasizedSelectedContentBackgroundColor];
    case NSVisualEffectMaterialToolTip:
     return [NSColor colorWithCalibratedWhite:0.98 alpha:0.97];
    case NSVisualEffectMaterialHUDWindow:
    case NSVisualEffectMaterialDark:
    case NSVisualEffectMaterialUltraDark:
     return [NSColor colorWithCalibratedWhite:0.12 alpha:0.85];
    case NSVisualEffectMaterialContentBackground:
     return [NSColor controlBackgroundColor];
    case NSVisualEffectMaterialUnderPageBackground:
     return [NSColor underPageBackgroundColor];
    default:
     return [NSColor windowBackgroundColor];
   }
}

-(void)drawRect:(NSRect)rect {
   NSRect bounds=[self bounds];

   [[self _materialColor] setFill];
   if(_maskImage!=nil){
    [NSGraphicsContext saveGraphicsState];
    [_maskImage drawInRect:bounds fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0];
    NSRectFillUsingOperation(NSIntersectionRect(rect,bounds),NSCompositeSourceIn);
    [NSGraphicsContext restoreGraphicsState];
   }
   else
    NSRectFillUsingOperation(NSIntersectionRect(rect,bounds),NSCompositeCopy);
}

-(void)viewDidMoveToWindow {
   [super viewDidMoveToWindow];
   [self setNeedsDisplay:YES];
}

@end
