/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A terminal session: a shell (or a command) on a pseudo-terminal, its
 * output read into libvterm's screen, the keyboard written back; the
 * lines scrolled off the top kept as scrollback.
 */

#import <Foundation/Foundation.h>
#include <vterm.h>

@class TMSession;

@protocol TMSessionDelegate <NSObject>
/* Rows and columns of the screen that changed (scrollback lines don't). */
- (void)session:(TMSession *)session didDamageRect:(VTermRect)rect;
- (void)sessionDidMoveCursor:(TMSession *)session;
- (void)session:(TMSession *)session didChangeTitle:(NSString *)title;
- (void)sessionDidRing:(TMSession *)session;
- (void)sessionScrollbackDidChange:(TMSession *)session;
- (void)session:(TMSession *)session didExitWithStatus:(int)status;
@end

@interface TMSession : NSObject
@property (weak) id<TMSessionDelegate> delegate;
@property (readonly) int rows, columns;
@property (readonly) pid_t pid;
/* The program's name ("zsh"). */
@property (readonly, copy) NSString *programName;
@property (readonly, copy) NSString *title;
@property (readonly) BOOL exited;
@property (readonly) VTermPos cursor;
@property (readonly) BOOL cursorVisible;
@property (readonly) BOOL alternateScreen;
/* Lines kept above the screen (10000 unless set). */
@property (nonatomic) NSUInteger scrollbackLimit;

/* The shell a session without arguments runs, as a login shell; nil or
 * empty (or not executable): the user's own. */
+ (void)setDefaultShell:(NSString *)path;
+ (NSString *)defaultShell;

/* The user's login shell, or the arguments (argv[0] the program's path). */
- (instancetype)initWithRows:(int)rows columns:(int)columns arguments:(NSArray<NSString *> *)arguments
    directory:(NSString *)directory;
- (void)resizeToRows:(int)rows columns:(int)columns;

/* A screen cell (row 0 the top of the screen). */
- (BOOL)getCell:(VTermScreenCell *)cell row:(int)row column:(int)column;
/* The scrollback: lines scrolled off the top, oldest first. */
@property (readonly) NSUInteger scrollbackCount;
- (BOOL)getCell:(VTermScreenCell *)cell scrollbackLine:(NSUInteger)line column:(int)column;
- (void)clearScrollback;
/* RGB of a cell's colour (its defaults the view's). */
- (void)getRed:(uint8_t *)r green:(uint8_t *)g blue:(uint8_t *)b ofColor:(VTermColor)color;
- (void)setDefaultForeground:(VTermColor)fg background:(VTermColor)bg;
/* One of the 16 ANSI colors (0–15). */
- (void)setPaletteIndex:(int)index red:(uint8_t)r green:(uint8_t)g blue:(uint8_t)b;

/* The keyboard: text, a special key, a paste. */
- (void)sendCharacter:(uint32_t)character modifiers:(VTermModifier)modifiers;
- (void)sendKey:(VTermKey)key modifiers:(VTermModifier)modifiers;
- (void)paste:(NSString *)text;
- (void)writeData:(NSData *)data;

/* The text of the screen (and scrollback), lines joined with newlines. */
- (NSString *)textFromLine:(NSInteger)firstLine column:(int)firstColumn toLine:(NSInteger)lastLine column:(int)lastColumn;
- (NSString *)screenText;
/* A line of text on the screen, as if the program had written it. */
- (void)showMessage:(NSString *)message;
- (void)terminate;
@end
