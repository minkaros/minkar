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

/*
 * Input events for WindowServer. A device backend (IOKit HID) reports raw
 * pointer and button changes here; WSInput keeps the pointer position
 * and button state and turns them into mach_events for applications.
 *
 * Keys arrive as macOS virtual key codes (kVK_*) and are translated with
 * the keyboard layout through WSKeymap (libxkbcommon).
 *
 * FIXME: the IOKit HID backend is not connected yet, so no events arrive.
 */

#import <Foundation/Foundation.h>
#import <AppKit/NSEvent.h>

double clipTo(double val, double min, double max);

@interface NSObject(WSInput)
-(BOOL)sendEventToApp:(struct mach_event *)event;
-(void)signalQuit;
-(void)switchApp;
@end

enum {
    WSInputButtonLeft, WSInputButtonRight, WSInputButtonMiddle, WSInputButtonCount
};

@interface WSInput : NSObject

-init;
-(void)run:(NSObject *)target; /* deliver pending events to target */
-(void)setLogLevel:(int)level;
-(void)setDebugLevel:(int)level;
-(NSPoint)pointerPos;
-(NSPoint)setPointerPos:(NSPoint)pos;
-(void)setGeometry:(NSRect)geom;
-(int)fileDescriptor; /* readable when events are pending, or -1 */

/* Called by the device backend. dy and y grow upwards. */
-(void)pointerMovedByX:(double)dx y:(double)dy target:(NSObject *)target;
-(void)pointerMovedToX:(double)x y:(double)y target:(NSObject *)target;
-(void)button:(int)button pressed:(BOOL)pressed target:(NSObject *)target;
-(void)key:(uint16_t)keyCode pressed:(BOOL)pressed target:(NSObject *)target;

/* Switch keyboard layout (xkeyboard-config names, e.g. "es", "latam"). */
-(BOOL)setKeyboardLayout:(NSString *)layout variant:(NSString *)variant;

@end
