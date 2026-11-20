/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSViewAnimation.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSValue.h>

NSString * const NSViewAnimationStartFrameKey=@"NSViewAnimationStartFrameKey";
NSString * const NSViewAnimationEndFrameKey=@"NSViewAnimationEndFrameKey";
NSString * const NSViewAnimationTargetKey=@"NSViewAnimationTargetKey";
NSString * const NSViewAnimationEffectKey=@"NSViewAnimationEffectKey";

NSString * const NSViewAnimationFadeInEffect=@"NSViewAnimationFadeInEffect";
NSString * const NSViewAnimationFadeOutEffect=@"NSViewAnimationFadeOutEffect";

@implementation NSViewAnimation

-initWithViewAnimations:(NSArray *)animations {
// default is 0.5
   [super initWithDuration:0.5 animationCurve:NSAnimationEaseInOut];
   _animations=[animations retain];
   return self;
}

-(void)dealloc {
   [_animations release];
   [_startFrames release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   NSViewAnimation *result=[super copyWithZone:zone];
   
   result->_animations=[_animations copy];
   result->_startFrames=nil;

   return result;
}

-(NSArray *)viewAnimations {
   return _animations;
}

/* Each run starts from where its targets are then. */
-(void)startAnimation {
   [_startFrames release];
   _startFrames=nil;
   [super startAnimation];
}

-(void)setViewAnimations:(NSArray *)animations {
   animations=[animations retain];
   [_animations release];
   _animations=animations;
}

-(void)setCurrentProgress:(NSAnimationProgress)progress {
   [super setCurrentProgress:progress];
   BOOL isFinalFrame=(progress>=1.0)?YES:NO;
   
   NSInteger i,count=[_animations count];
   for(i=0;i<count;i++){
    NSDictionary *info=[_animations objectAtIndex:i];
    id            target=[info objectForKey:NSViewAnimationTargetKey];
    NSValue      *startFrame=[info objectForKey:NSViewAnimationStartFrameKey];

    /* Minkar: without a start frame, the target's frame as the animation
       began, as on the reference; each frame is moved along the way. */
    if(startFrame==nil && [info objectForKey:NSViewAnimationEndFrameKey]!=nil){
     if(_startFrames==nil)
      _startFrames=[NSMutableDictionary new];
     NSNumber *key=[NSNumber numberWithInteger:i];
     if([_startFrames objectForKey:key]==nil && ([target isKindOfClass:[NSView class]] || [target isKindOfClass:[NSWindow class]]))
      [_startFrames setObject:[NSValue valueWithRect:[target frame]] forKey:key];
     startFrame=[_startFrames objectForKey:key];
    }
    NSValue      *endFrame=[info objectForKey:NSViewAnimationEndFrameKey];
    NSString     *effect=[info objectForKey:NSViewAnimationEffectKey];
    
    if(startFrame!=nil && endFrame!=nil){
     NSRect startRect=[startFrame rectValue];
     NSRect endRect=[endFrame rectValue];
     NSRect currentRect;
     
     currentRect.origin.x=startRect.origin.x+(endRect.origin.x-startRect.origin.x)*progress;
     currentRect.origin.y=startRect.origin.y+(endRect.origin.y-startRect.origin.y)*progress;
     currentRect.size.width=startRect.size.width+(endRect.size.width-startRect.size.width)*progress;
     currentRect.size.height=startRect.size.height+(endRect.size.height-startRect.size.height)*progress;
    
     if([target isKindOfClass:[NSView class]])
      [target setFrame:currentRect];
     else if([target isKindOfClass:[NSWindow class]]){
      [target setFrame:currentRect display:YES];
     }
    }
    
    if(effect!=nil){
     if([effect isEqualToString:NSViewAnimationFadeInEffect]){
     
      if([target isKindOfClass:[NSView class]]){
       if(progress>0)
        [target setHidden:NO];
      }
      else if([target isKindOfClass:[NSWindow class]]){
       [target setAlphaValue:progress];
       if(progress>0)
        [target orderFront:nil];
      }
     }
     if([effect isEqualToString:NSViewAnimationFadeOutEffect]){
     
      if([target isKindOfClass:[NSView class]]){
       if(progress>0)
        [target setHidden:YES];
      }
      else if([target isKindOfClass:[NSWindow class]]){
       [target setAlphaValue:1.0-progress];
       
       if(isFinalFrame)
        [target orderOut:nil];
      }
     }
    }
    if([target isKindOfClass:[NSView class]])
     [[target superview] setNeedsDisplay:YES];
   }
}


@end
