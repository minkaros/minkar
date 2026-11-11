/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/Foundation.h>
#import <AppKit/NSEvent.h>

@class NSMenu, NSUndoManager;

@interface NSResponder : NSObject <NSCoding> {
    id _nextResponder;
}

- (NSResponder *)nextResponder;

- (NSMenu *)menu;
- (NSUndoManager *)undoManager;

- (void)setNextResponder:(NSResponder *)responder;
- (void)setMenu:(NSMenu *)menu;

- validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType;

- (void)interpretKeyEvents:(NSArray *)events;
- (BOOL)performKeyEquivalent:(NSEvent *)event;
- (BOOL)tryToPerform:(SEL)action with:object;
- (void)noResponderFor:(SEL)action;

- (BOOL)acceptsFirstResponder;
- (BOOL)becomeFirstResponder;
- (BOOL)resignFirstResponder;

- (void)flagsChanged:(NSEvent *)event;

- (NSError *)willPresentError:(NSError *)error;
- (BOOL)presentError:(NSError *)error;
- (void)presentError:(NSError *)error modalForWindow:(NSWindow *)window delegate:delegate didPresentSelector:(SEL)selector contextInfo:(void *)info;

- (void)keyUp:(NSEvent *)event;
- (void)keyDown:(NSEvent *)event;

- (void)cursorUpdate:(NSEvent *)event;
- (void)scrollWheel:(NSEvent *)event;

- (void)mouseUp:(NSEvent *)event;
- (void)mouseDown:(NSEvent *)event;
- (void)mouseMoved:(NSEvent *)event;
- (void)mouseEntered:(NSEvent *)event;
- (void)mouseExited:(NSEvent *)event;
- (void)mouseDragged:(NSEvent *)event;

- (void)rightMouseUp:(NSEvent *)event;
- (void)rightMouseDown:(NSEvent *)event;
- (void)rightMouseDragged:(NSEvent *)event;

@end

/* Minkar: the responder actions key bindings send (API 10.11), as a
   protocol; NSResponder implements the ones it handles. */
@protocol NSStandardKeyBindingResponding <NSObject>
@optional
- (void)insertText:(id)insertString;
- (void)doCommandBySelector:(SEL)selector;
- (void)moveForward:(id)sender;
- (void)moveRight:(id)sender;
- (void)moveBackward:(id)sender;
- (void)moveLeft:(id)sender;
- (void)moveUp:(id)sender;
- (void)moveDown:(id)sender;
- (void)moveWordForward:(id)sender;
- (void)moveWordBackward:(id)sender;
- (void)moveToBeginningOfLine:(id)sender;
- (void)moveToEndOfLine:(id)sender;
- (void)moveToBeginningOfParagraph:(id)sender;
- (void)moveToEndOfParagraph:(id)sender;
- (void)moveToEndOfDocument:(id)sender;
- (void)moveToBeginningOfDocument:(id)sender;
- (void)pageDown:(id)sender;
- (void)pageUp:(id)sender;
- (void)centerSelectionInVisibleArea:(id)sender;
- (void)moveBackwardAndModifySelection:(id)sender;
- (void)moveForwardAndModifySelection:(id)sender;
- (void)moveWordForwardAndModifySelection:(id)sender;
- (void)moveWordBackwardAndModifySelection:(id)sender;
- (void)moveUpAndModifySelection:(id)sender;
- (void)moveDownAndModifySelection:(id)sender;
- (void)moveToBeginningOfLineAndModifySelection:(id)sender;
- (void)moveToEndOfLineAndModifySelection:(id)sender;
- (void)moveToBeginningOfParagraphAndModifySelection:(id)sender;
- (void)moveToEndOfParagraphAndModifySelection:(id)sender;
- (void)moveToEndOfDocumentAndModifySelection:(id)sender;
- (void)moveToBeginningOfDocumentAndModifySelection:(id)sender;
- (void)pageDownAndModifySelection:(id)sender;
- (void)pageUpAndModifySelection:(id)sender;
- (void)moveParagraphForwardAndModifySelection:(id)sender;
- (void)moveParagraphBackwardAndModifySelection:(id)sender;
- (void)moveWordRight:(id)sender;
- (void)moveWordLeft:(id)sender;
- (void)moveRightAndModifySelection:(id)sender;
- (void)moveLeftAndModifySelection:(id)sender;
- (void)moveWordRightAndModifySelection:(id)sender;
- (void)moveWordLeftAndModifySelection:(id)sender;
- (void)moveToLeftEndOfLine:(id)sender;
- (void)moveToRightEndOfLine:(id)sender;
- (void)moveToLeftEndOfLineAndModifySelection:(id)sender;
- (void)moveToRightEndOfLineAndModifySelection:(id)sender;
- (void)scrollPageUp:(id)sender;
- (void)scrollPageDown:(id)sender;
- (void)scrollLineUp:(id)sender;
- (void)scrollLineDown:(id)sender;
- (void)scrollToBeginningOfDocument:(id)sender;
- (void)scrollToEndOfDocument:(id)sender;
- (void)transpose:(id)sender;
- (void)transposeWords:(id)sender;
- (void)selectAll:(id)sender;
- (void)selectParagraph:(id)sender;
- (void)selectLine:(id)sender;
- (void)selectWord:(id)sender;
- (void)indent:(id)sender;
- (void)insertTab:(id)sender;
- (void)insertBacktab:(id)sender;
- (void)insertNewline:(id)sender;
- (void)insertParagraphSeparator:(id)sender;
- (void)insertNewlineIgnoringFieldEditor:(id)sender;
- (void)insertTabIgnoringFieldEditor:(id)sender;
- (void)insertLineBreak:(id)sender;
- (void)insertContainerBreak:(id)sender;
- (void)insertSingleQuoteIgnoringSubstitution:(id)sender;
- (void)insertDoubleQuoteIgnoringSubstitution:(id)sender;
- (void)changeCaseOfLetter:(id)sender;
- (void)uppercaseWord:(id)sender;
- (void)lowercaseWord:(id)sender;
- (void)capitalizeWord:(id)sender;
- (void)deleteForward:(id)sender;
- (void)deleteBackward:(id)sender;
- (void)deleteBackwardByDecomposingPreviousCharacter:(id)sender;
- (void)deleteWordForward:(id)sender;
- (void)deleteWordBackward:(id)sender;
- (void)deleteToBeginningOfLine:(id)sender;
- (void)deleteToEndOfLine:(id)sender;
- (void)deleteToBeginningOfParagraph:(id)sender;
- (void)deleteToEndOfParagraph:(id)sender;
- (void)yank:(id)sender;
- (void)complete:(id)sender;
- (void)setMark:(id)sender;
- (void)deleteToMark:(id)sender;
- (void)selectToMark:(id)sender;
- (void)swapWithMark:(id)sender;
- (void)cancelOperation:(id)sender;
- (void)makeBaseWritingDirectionNatural:(id)sender;
- (void)makeBaseWritingDirectionLeftToRight:(id)sender;
- (void)makeBaseWritingDirectionRightToLeft:(id)sender;
- (void)makeTextWritingDirectionNatural:(id)sender;
- (void)makeTextWritingDirectionLeftToRight:(id)sender;
- (void)makeTextWritingDirectionRightToLeft:(id)sender;
- (void)quickLookPreviewItems:(id)sender;
@end


@interface NSResponder (NSResponder_keyBindings)

- (void)doCommandBySelector:(SEL)selector;

- (void)insertText:(NSString *)text;
- (void)insertNewline:sender;
- (void)insertNewlineIgnoringFieldEditor:sender;
- (void)insertTab:sender;
- (void)insertTabIgnoringFieldEditor:sender;
- (void)insertBacktab:sender;
- (void)insertParagraphSeparator:sender;

- (void)deleteForward:sender;
- (void)deleteBackward:sender;
- (void)deleteWordForward:sender;
- (void)deleteWordBackward:sender;
- (void)deleteToBeginningOfLine:sender;
- (void)deleteToEndOfLine:sender;
- (void)deleteToBeginningOfParagraph:sender;
- (void)deleteToEndOfParagraph:sender;

- (void)transpose:sender;
- (void)transposeWords:sender;

- (void)indent:sender;
- (void)complete:sender;
- (void)yank:sender;

- (void)cancelOperation:(id)sender;
- (void)uppercaseWord:sender;
- (void)lowercaseWord:sender;
- (void)capitalizeWord:sender;
- (void)changeCaseOfLetter:sender;

- (void)setMark:sender;
- (void)deleteToMark:sender;
- (void)selectToMark:sender;
- (void)swapWithMark:sender;

- (void)selectWord:sender;
- (void)selectLine:sender;
- (void)selectParagraph:sender;
- (void)selectAll:sender;
- (void)centerSelectionInVisibleArea:sender;

- (void)moveUpAndModifySelection:sender;
- (void)moveDownAndModifySelection:sender;

- (void)moveForwardAndModifySelection:sender;
- (void)moveBackwardAndModifySelection:sender;
- (void)moveRightAndModifySelection:sender;
- (void)moveLeftAndModifySelection:sender;

- (void)moveWordForwardAndModifySelection:sender;
- (void)moveWordBackwardAndModifySelection:sender;
- (void)moveWordRightAndModifySelection:sender;
- (void)moveWordLeftAndModifySelection:sender;

- (void)moveUp:sender;
- (void)moveDown:sender;
- (void)moveLeft:sender;
- (void)moveRight:sender;
- (void)moveForward:sender;
- (void)moveBackward:sender;

- (void)moveWordForward:sender;
- (void)moveWordBackward:sender;
- (void)moveWordRight:sender;
- (void)moveWordLeft:sender;

- (void)moveToBeginningOfLine:sender;
- (void)moveToEndOfLine:sender;

- (void)moveToBeginningOfParagraph:sender;
- (void)moveToEndOfParagraph:sender;

- (void)moveToBeginningOfDocument:sender;
- (void)moveToEndOfDocument:sender;

- (void)pageUp:sender;
- (void)pageDown:sender;

- (void)scrollPageUp:sender;
- (void)scrollPageDown:sender;
- (void)scrollLineUp:sender;
- (void)scrollLineDown:sender;

- (void)scrollToBeginningOfDocument:(id)sender;
- (void)scrollToEndOfDocument:(id)sender;

- (void)noop:sender;

- (void)moveToEndOfDocumentAndModifySelection:sender; // n.b.:
- (void)moveToBeginningOfDocumentAndModifySelection:sender; // these aren't in OSX (can't hurt)

@end

/* Minkar: state restoration's calls (API 10.7). Windows save their
   responders' state; until Minkar restores it, these keep nothing. */
@interface NSResponder (MinkarRestorableState)
- (void)encodeRestorableStateWithCoder:(NSCoder *)coder;
- (void)restoreStateWithCoder:(NSCoder *)coder;
- (void)invalidateRestorableState;
+ (NSArray<NSString *> *)restorableStateKeyPaths;
@end
