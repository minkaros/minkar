/* Copyright (c) 2006-2007 Christopher J. W. Lloyd <cjwl@objc.net>
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSWindow.h>
#import <Foundation/NSUserActivity.h>
#import <objc/runtime.h>
#import <AppKit/NSResponder.h>
#import <AppKit/NSTextInputContext.h>
#import <AppKit/NSAlert.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSKeyboardBindingManager.h>
#import <AppKit/NSKeyboardBinding.h>
#import <AppKit/NSApplication.h>
#import <Foundation/NSKeyedArchiver.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSRaise.h>
#import <objc/message.h>

@implementation NSResponder

-(void)encodeWithCoder:(NSCoder *)encoder {
}

-initWithCoder:(NSCoder *)coder {
   if([coder allowsKeyedCoding]){
    // NSKeyedUnarchiver *keyed=(NSKeyedUnarchiver *)coder;
    
    //; _nextResponder=[keyed decodeObjectForKey:@"NSNextResponder"]; 
   }
   return self;
}

-(NSResponder *)nextResponder {
   return _nextResponder;
}

-(NSMenu *)menu {
   NSInvalidAbstractInvocation();
   return nil;
}

-(NSUndoManager *)undoManager {
    return [_nextResponder performSelector:_cmd];
}

-(void)setNextResponder:(NSResponder *)responder {
   _nextResponder=responder;
}

-(void)setMenu:(NSMenu *)menu {
   NSInvalidAbstractInvocation();
}

-validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType {
   return [_nextResponder validRequestorForSendType:sendType returnType:returnType];
}

-(void)doCommandBySelector:(SEL)selector {

    if([self respondsToSelector:selector]) {
        [self performSelector:selector withObject:nil];
    }
    else
        [_nextResponder doCommandBySelector:selector];
}

-(void)interpretKeyEvents:(NSArray *)events {
   int i,icount=[events count];

   for(i=0;i<icount;i++){
    NSEvent      *event=[events objectAtIndex:i];
    NSString     *string=[event charactersIgnoringModifiers];

    /* Minkar: a view that is a text input client gets its keys through its
       input context (insertText:replacementRange:, doCommandBySelector:),
       as on the reference platform; the older text calls are for others. */
    if([self conformsToProtocol:@protocol(NSTextInputClient)] && [self respondsToSelector:@selector(inputContext)]){
     NSTextInputContext *context=[(id)self inputContext];

     if(context!=nil && [context handleEvent:event])
      continue;
    }

    NSKeyboardBinding *keyBinding=[[NSKeyboardBindingManager defaultKeyBindingManager] keyBindingWithString:string modifierFlags:[event modifierFlags]];
    NSArray      *selectorNames=[keyBinding selectorNames];
    if(selectorNames!=nil){
     int j = 0, jcount = [selectorNames count];

     for (j = 0; j < jcount; ++j) {
      //  NSLog(@"doing %@ for %@", [selectorNames objectAtIndex:j], keyBinding);
      [self doCommandBySelector:NSSelectorFromString([selectorNames objectAtIndex:j])];
     }
    }
    else if([self respondsToSelector:@selector(insertText:)]){
     string=[event characters];
 
     if([string length]>0){ // FIX THIS IN APPKIT shouldnt get 0 length 

      unsigned j,length=[string length];
      unichar  buffer[length];
		
      [string getCharacters:buffer];
      for(j=0;j<length;j++){
       unichar check=buffer[j];

        // Filter non char codes - Apple functions keys and ctrl chars
       if(check>=NSUpArrowFunctionKey && check<=NSModeSwitchFunctionKey)
        check=' ';
       else if(check<' ')
        check=' ';

       buffer[j]=check;
      }
      string=[NSString stringWithCharacters:buffer length:length];
      [self insertText:string];
     }
    }
   }
}

-(BOOL)performKeyEquivalent:(NSEvent *)event {
   return NO;
}

-(BOOL)tryToPerform:(SEL)action with:object {
   if([self respondsToSelector:action]){
    [self performSelector:action withObject:object];
    return YES;
   }

   return [_nextResponder tryToPerform:action with:object];
}

-(void)noResponderFor:(SEL)action {
   if(sel_isEqual(action,@selector(keyDown:)))
    NSBeep();
}

-(BOOL)acceptsFirstResponder {
   return NO;
}

-(BOOL)becomeFirstResponder {
   return YES;
}

-(BOOL)resignFirstResponder {
   return YES;
}

-(NSError *)willPresentError:(NSError *)error {
	// do nothing
	return error;
}

/* What NSApp shows: the error as its delegate changes it, without the
 * recovery options nobody can act on. canRecover says whether the
 * attempter can take the chosen option (with a sheet, its asynchronous
 * method). */
static NSError *presentableError(NSError *error, BOOL sheet, BOOL *canRecover) {
	id delegate=[NSApp delegate];
	SEL recovery=sheet ? @selector(attemptRecoveryFromError:optionIndex:delegate:didRecoverSelector:contextInfo:) : @selector(attemptRecoveryFromError:optionIndex:);

	*canRecover=NO;
	if([delegate respondsToSelector:@selector(application:willPresentError:)])
		error=[delegate application:NSApp willPresentError:error];
	if([[error localizedRecoveryOptions] count]==0)
		return error;

	id attempter=[error recoveryAttempter];
	if(attempter!=nil && [attempter respondsToSelector:recovery]) {
		*canRecover=YES;
		return error;
	}
	NSLog(@"The recovery options of %@ have no attempter that takes them; showing the error without them.",error);

	NSMutableDictionary *info=[NSMutableDictionary dictionaryWithDictionary:[error userInfo]];
	[info removeObjectForKey:NSLocalizedRecoveryOptionsErrorKey];
	return [NSError errorWithDomain:[error domain] code:[error code] userInfo:info];
}

static NSUInteger optionIndexForResponse(NSModalResponse response) {
	return response>=NSAlertFirstButtonReturn ? (NSUInteger)(response-NSAlertFirstButtonReturn) : 0;
}

/* The error goes up the responder chain (each responder may change it
 * with willPresentError:) and NSApp shows it. */
-(BOOL)presentError:(NSError *)error {
	if(self!=NSApp)
		return [(_nextResponder ? _nextResponder : NSApp) presentError:[self willPresentError:error]];

	BOOL canRecover;
	NSError *shown=presentableError(error,NO,&canRecover);
	NSModalResponse response=[[NSAlert alertWithError:shown] runModal];

	if(!canRecover)
		return NO;
	return [[shown recoveryAttempter] attemptRecoveryFromError:shown optionIndex:optionIndexForResponse(response)];
}

/* As presentError:, in a sheet on the window (an application-modal alert
 * when there is none). When it ends, the attempter takes the chosen option
 * and then the delegate gets
 * -didPresentErrorWithRecovery:(BOOL)recovered contextInfo:(void *)info. */
-(void)presentError:(NSError *)error modalForWindow:(NSWindow *)window delegate:delegate didPresentSelector:(SEL)selector contextInfo:(void *)info {
	if(self!=NSApp) {
		[(_nextResponder ? _nextResponder : NSApp) presentError:[self willPresentError:error] modalForWindow:window delegate:delegate didPresentSelector:selector contextInfo:info];
		return;
	}

	BOOL canRecover;
	NSError *shown=presentableError(error,YES,&canRecover);
	void (^finish)(NSModalResponse)=^(NSModalResponse response) {
		if(canRecover) {
			[[shown recoveryAttempter] attemptRecoveryFromError:shown optionIndex:optionIndexForResponse(response) delegate:delegate didRecoverSelector:selector contextInfo:info];
			return;
		}
		if(delegate!=nil && selector!=NULL)
			((void (*)(id,SEL,BOOL,void *))objc_msgSend)(delegate,selector,NO,info);
	};

	NSAlert *alert=[NSAlert alertWithError:shown];
	if(window==nil)
		finish([alert runModal]);
	else
		[alert beginSheetModalForWindow:window completionHandler:finish];
}

-(void)flagsChanged:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)keyUp:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)keyDown:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)cursorUpdate:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)scrollWheel:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)mouseUp:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)mouseDown:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)mouseMoved:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)mouseEntered:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)mouseExited:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)mouseDragged:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)rightMouseUp:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)rightMouseDown:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)rightMouseDragged:(NSEvent *)event {
   [_nextResponder performSelector:_cmd withObject:event];
}

-(void)noop:sender {
}


static char userActivityKey;

-(NSUserActivity *)userActivity {
   return objc_getAssociatedObject(self,&userActivityKey);
}

-(void)setUserActivity:(NSUserActivity *)activity {
   objc_setAssociatedObject(self,&userActivityKey,activity,OBJC_ASSOCIATION_RETAIN);
   [activity becomeCurrent];
}

-(void)updateUserActivityState:(NSUserActivity *)activity {
}

@end

@implementation NSResponder (MinkarRestorableState)
-(void)encodeRestorableStateWithCoder:(NSCoder *)coder { }
-(void)restoreStateWithCoder:(NSCoder *)coder { }
-(void)invalidateRestorableState { }
+(NSArray *)restorableStateKeyPaths { return [NSArray array]; }
@end

/* Windows' restoration settings: kept, though Minkar does not restore
   windows yet (no state is saved between launches). */
@implementation NSWindow (MinkarRestoration)

static char restorableKey, restorationClassKey;

-(BOOL)isRestorable {
   NSNumber *value=objc_getAssociatedObject(self,&restorableKey);
   return value!=nil?[value boolValue]:YES;
}

-(void)setRestorable:(BOOL)value {
   objc_setAssociatedObject(self,&restorableKey,[NSNumber numberWithBool:value],OBJC_ASSOCIATION_RETAIN);
}

-(Class)restorationClass {
   return objc_getAssociatedObject(self,&restorationClassKey);
}

-(void)setRestorationClass:(Class)value {
   objc_setAssociatedObject(self,&restorationClassKey,value,OBJC_ASSOCIATION_ASSIGN);
}

-(void)disableSnapshotRestoration { }
-(void)enableSnapshotRestoration { }

@end
