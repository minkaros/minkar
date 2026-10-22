/*
 * Copyright (C) 2022-2024 Zoe Knox <zoe@pixin.net>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <mach/mach.h>
#import "common.h"
#import "message.h"
#import "WSInput.h"

double clipTo(double val, double min, double max) {
    if(val < min) val = min;
    else if(val > max) val = max;
    return val;
}

@implementation WSInput

-init {
    self = [super init];
    return self;
}

-(void)run:(NSObject *)target {
    /* No device backend yet. */
}

-(void)setLogLevel:(int)level {
    logLevel = level;
}

-(void)setDebugLevel:(int)level {
    logLevel = level;
}

// Call this whenever the cursor moves to a new screen
-(void)setGeometry:(NSRect)geom {
    geometry = geom;
}

-(uint32_t)dragOrMoveCode {
    if(buttonDown[WSInputButtonLeft])
        return NSLeftMouseDragged;
    if(buttonDown[WSInputButtonRight])
        return NSRightMouseDragged;
    return NSMouseMoved;
}

-(void)pointerMovedByX:(double)dx y:(double)dy target:(NSObject *)target {
    struct mach_event me;
    memset(&me, 0, sizeof(me));
    me.code = [self dragOrMoveCode];
    me.dx = dx;
    me.dy = dy;
    me.mods = modifierFlags;
    pointerX = clipTo(pointerX + dx, geometry.origin.x, geometry.size.width);
    pointerY = clipTo(pointerY + dy, geometry.origin.y, geometry.size.height);
    me.x = pointerX;
    me.y = pointerY;
    if(logLevel >= WS_INFO)
        NSLog(@"Input event: type=MOTION dx=%.2f dy=%.2f pos=%.2f,%.2f", dx, dy,
                pointerX, pointerY);
    [target sendEventToApp:&me];
}

-(void)pointerMovedToX:(double)x y:(double)y target:(NSObject *)target {
    struct mach_event me;
    memset(&me, 0, sizeof(me));
    me.code = [self dragOrMoveCode];
    me.mods = modifierFlags;
    me.dx = x - pointerX;
    me.dy = y - pointerY;
    pointerX = clipTo(x, geometry.origin.x, geometry.size.width);
    pointerY = clipTo(y, geometry.origin.y, geometry.size.height);
    me.x = pointerX;
    me.y = pointerY;
    if(logLevel >= WS_INFO)
        NSLog(@"Input event: type=MOTION x=%.2f y=%.2f", pointerX, pointerY);
    [target sendEventToApp:&me];
}

-(void)button:(int)button pressed:(BOOL)pressed target:(NSObject *)target {
    struct mach_event me;
    memset(&me, 0, sizeof(me));
    switch(button) {
        case WSInputButtonLeft:
            me.code = pressed ? NSLeftMouseDown : NSLeftMouseUp;
            break;
        case WSInputButtonRight:
            me.code = pressed ? NSRightMouseDown : NSRightMouseUp;
            break;
        default:
            return;
    }
    if(logLevel >= WS_INFO)
        NSLog(@"Input event: type=BUTTON button=%d state=%d", button, pressed);
    buttonDown[button] = pressed;
    me.mods = modifierFlags;
    me.x = pointerX;
    me.y = pointerY;
    [target sendEventToApp:&me];
}

-(NSPoint)pointerPos {
    return NSMakePoint(pointerX, pointerY);
}

-(NSPoint)setPointerPos:(NSPoint)pos {
    pointerX = pos.x;
    pointerY = pos.y;
    return pos;
}

-(int)fileDescriptor {
    return -1;
}

@end
