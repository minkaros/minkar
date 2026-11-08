/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "TMSession.h"
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>

#define SCROLLBACK 10000

static NSString *defaultShell;

@implementation TMSession {
    VTerm *_vt;
    VTermScreen *_screen;
    int _master;
    dispatch_source_t _readSource, _exitSource;
    NSMutableArray<NSData *> *_scrollback;      /* each a line's VTermScreenCell array */
    NSMutableString *_titleBuffer;
}

/* ---- libvterm's callbacks ---- */

static int damage(VTermRect rect, void *user)
{
    TMSession *self = (__bridge TMSession *)user;
    [self.delegate session:self didDamageRect:rect];
    return 1;
}

static int movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user)
{
    TMSession *self = (__bridge TMSession *)user;
    self->_cursor = pos;
    self->_cursorVisible = visible;
    [self.delegate sessionDidMoveCursor:self];
    return 1;
}

static int settermprop(VTermProp prop, VTermValue *val, void *user)
{
    TMSession *self = (__bridge TMSession *)user;

    switch (prop) {
    case VTERM_PROP_TITLE:
        if (val->string.initial)
            self->_titleBuffer = [NSMutableString string];
        [self->_titleBuffer appendString:[[NSString alloc] initWithBytes:val->string.str
            length:val->string.len encoding:NSUTF8StringEncoding] ?: @""];
        if (val->string.final) {
            self->_title = [self->_titleBuffer copy];
            [self.delegate session:self didChangeTitle:self->_title];
        }
        break;
    case VTERM_PROP_CURSORVISIBLE:
        self->_cursorVisible = val->boolean;
        [self.delegate sessionDidMoveCursor:self];
        break;
    case VTERM_PROP_ALTSCREEN:
        self->_alternateScreen = val->boolean;
        break;
    default:
        break;
    }
    return 1;
}

static int bell(void *user)
{
    TMSession *self = (__bridge TMSession *)user;
    [self.delegate sessionDidRing:self];
    return 1;
}

static int sb_pushline(int cols, const VTermScreenCell *cells, void *user)
{
    TMSession *self = (__bridge TMSession *)user;

    [self->_scrollback addObject:[NSData dataWithBytes:cells length:sizeof(VTermScreenCell) * cols]];
    if ([self->_scrollback count] > self->_scrollbackLimit)
        [self->_scrollback removeObjectsInRange:NSMakeRange(0, [self->_scrollback count] - self->_scrollbackLimit)];
    [self.delegate sessionScrollbackDidChange:self];
    return 1;
}

/* A larger screen takes lines back from the scrollback. */
static int sb_popline(int cols, VTermScreenCell *cells, void *user)
{
    TMSession *self = (__bridge TMSession *)user;
    NSData *line = [self->_scrollback lastObject];
    NSUInteger have;

    if (line == nil)
        return 0;
    have = [line length] / sizeof(VTermScreenCell);
    for (int c = 0; c < cols; c++) {
        if ((NSUInteger)c < have)
            cells[c] = ((const VTermScreenCell *)[line bytes])[c];
        else {
            memset(&cells[c], 0, sizeof(VTermScreenCell));
            cells[c].width = 1;
            cells[c].fg = ((const VTermScreenCell *)[line bytes])[0].fg;
            cells[c].bg = ((const VTermScreenCell *)[line bytes])[0].bg;
        }
    }
    [self->_scrollback removeLastObject];
    [self.delegate sessionScrollbackDidChange:self];
    return 1;
}

static int sb_clear(void *user)
{
    TMSession *self = (__bridge TMSession *)user;
    [self clearScrollback];
    return 1;
}

static const VTermScreenCallbacks callbacks = {
    .damage = damage,
    .movecursor = movecursor,
    .settermprop = settermprop,
    .bell = bell,
    .sb_pushline = sb_pushline,
    .sb_popline = sb_popline,
    .sb_clear = sb_clear,
};

/* What libvterm answers or the keyboard makes: to the program. */
static void output(const char *bytes, size_t length, void *user)
{
    TMSession *self = (__bridge TMSession *)user;

    while (length > 0 && self->_master >= 0) {
        ssize_t n = write(self->_master, bytes, length);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return;
        }
        bytes += n;
        length -= (size_t)n;
    }
}

/* ---- The session ---- */

+ (void)setDefaultShell:(NSString *)path
{
    defaultShell = [path copy];
}

+ (NSString *)defaultShell
{
    return defaultShell;
}

static NSString *loginShell(void)
{
    if ([defaultShell length] && access([defaultShell fileSystemRepresentation], X_OK) == 0)
        return defaultShell;

    struct passwd *pw = getpwuid(getuid());
    const char *shell = (pw != NULL && pw->pw_shell != NULL && pw->pw_shell[0]) ? pw->pw_shell : NULL;

    if (shell == NULL || access(shell, X_OK) != 0)
        shell = access("/bin/zsh", X_OK) == 0 ? "/bin/zsh" : "/bin/sh";
    return @(shell);
}

- (instancetype)initWithRows:(int)rows columns:(int)columns arguments:(NSArray<NSString *> *)arguments
    directory:(NSString *)directory
{
    if ((self = [super init]) == nil)
        return nil;
    _master = -1;
    _rows = rows;
    _columns = columns;
    _scrollback = [NSMutableArray array];
    _scrollbackLimit = SCROLLBACK;
    _cursorVisible = YES;
    _title = @"";

    _vt = vterm_new(rows, columns);
    vterm_set_utf8(_vt, 1);
    vterm_output_set_callback(_vt, output, (__bridge void *)self);
    _screen = vterm_obtain_screen(_vt);
    vterm_screen_set_callbacks(_screen, &callbacks, (__bridge void *)self);
    vterm_screen_set_damage_merge(_screen, VTERM_DAMAGE_SCROLL);
    vterm_screen_enable_altscreen(_screen, 1);
    vterm_screen_enable_reflow(_screen, true);
    vterm_screen_reset(_screen, 1);

    struct winsize size = { .ws_row = (unsigned short)rows, .ws_col = (unsigned short)columns };
    NSString *program = [arguments count] ? arguments[0] : loginShell();
    /* A login shell: "-zsh", as login(1) and the reference's Terminal start it. */
    NSString *zero = [arguments count] ? [program lastPathComponent] :
        [@"-" stringByAppendingString:[program lastPathComponent]];
    _programName = [program lastPathComponent];
    NSArray *rest = [arguments count] > 1 ? [arguments subarrayWithRange:NSMakeRange(1, [arguments count] - 1)] : @[];
    const char *home = getenv("HOME");
    NSString *dir = directory ?: (home ? @(home) : @"/");

    /* Everything the child needs, made before fork. */
    const char *path = strdup([program fileSystemRepresentation]);
    const char *cwd = strdup([dir fileSystemRepresentation]);
    char **argv = calloc([rest count] + 2, sizeof(char *));
    argv[0] = strdup([zero UTF8String]);
    for (NSUInteger i = 0; i < [rest count]; i++)
        argv[i + 1] = strdup([rest[i] UTF8String]);

    pid_t pid = forkpty(&_master, NULL, NULL, &size);
    if (pid < 0)
        return nil;
    if (pid == 0) {
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        setenv("TERM_PROGRAM", "Terminal", 1);
        if (getenv("LANG") == NULL)
            setenv("LANG", "en_US.UTF-8", 1);
        if (chdir(cwd) != 0)
            chdir("/");
        signal(SIGPIPE, SIG_DFL);
        execv(path, argv);
        _exit(127);
    }
    _pid = pid;
    fcntl(_master, F_SETFL, fcntl(_master, F_GETFL) | O_NONBLOCK);

    __weak TMSession *weakSelf = self;
    _readSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)_master, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(_readSource, ^{
        [weakSelf readOutput];
    });
    dispatch_resume(_readSource);
    _exitSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)pid, DISPATCH_PROC_EXIT,
        dispatch_get_main_queue());
    dispatch_source_set_event_handler(_exitSource, ^{
        [weakSelf childExited];
    });
    dispatch_resume(_exitSource);
    return self;
}

- (void)readOutput
{
    char buffer[65536];

    for (;;) {
        ssize_t n = read(_master, buffer, sizeof(buffer));
        if (n > 0) {
            vterm_input_write(_vt, buffer, (size_t)n);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EINTR))
            break;
        /* The other side is closed: the program is gone or going. */
        dispatch_source_cancel(_readSource);
        break;
    }
    vterm_screen_flush_damage(_screen);
}

- (void)childExited
{
    int status = 0;

    if (waitpid(_pid, &status, WNOHANG) >= 0) {
        [self readOutput];
        _exited = YES;
        dispatch_source_cancel(_exitSource);
        [self.delegate session:self didExitWithStatus:status];
    }
}

- (void)resizeToRows:(int)rows columns:(int)columns
{
    if (rows < 2 || columns < 2 || (rows == _rows && columns == _columns))
        return;
    _rows = rows;
    _columns = columns;
    vterm_set_size(_vt, rows, columns);
    vterm_screen_flush_damage(_screen);
    if (_master >= 0) {
        struct winsize size = { .ws_row = (unsigned short)rows, .ws_col = (unsigned short)columns };
        ioctl(_master, TIOCSWINSZ, &size);    /* the kernel sends the program SIGWINCH */
    }
}

- (BOOL)getCell:(VTermScreenCell *)cell row:(int)row column:(int)column
{
    if (row < 0 || row >= _rows || column < 0 || column >= _columns)
        return NO;
    return vterm_screen_get_cell(_screen, (VTermPos){ .row = row, .col = column }, cell) != 0;
}

- (void)setScrollbackLimit:(NSUInteger)limit
{
    _scrollbackLimit = limit;
    if ([_scrollback count] > limit) {
        [_scrollback removeObjectsInRange:NSMakeRange(0, [_scrollback count] - limit)];
        [self.delegate sessionScrollbackDidChange:self];
    }
}

- (NSUInteger)scrollbackCount
{
    return [_scrollback count];
}

- (BOOL)getCell:(VTermScreenCell *)cell scrollbackLine:(NSUInteger)line column:(int)column
{
    NSData *data;

    if (line >= [_scrollback count])
        return NO;
    data = _scrollback[line];
    if (column < 0 || (NSUInteger)column >= [data length] / sizeof(VTermScreenCell))
        return NO;
    *cell = ((const VTermScreenCell *)[data bytes])[column];
    return YES;
}

- (void)clearScrollback
{
    [_scrollback removeAllObjects];
    [self.delegate sessionScrollbackDidChange:self];
}

- (void)getRed:(uint8_t *)r green:(uint8_t *)g blue:(uint8_t *)b ofColor:(VTermColor)color
{
    vterm_screen_convert_color_to_rgb(_screen, &color);
    *r = color.rgb.red;
    *g = color.rgb.green;
    *b = color.rgb.blue;
}

- (void)setDefaultForeground:(VTermColor)fg background:(VTermColor)bg
{
    vterm_screen_set_default_colors(_screen, &fg, &bg);
}

- (void)setPaletteIndex:(int)index red:(uint8_t)r green:(uint8_t)g blue:(uint8_t)b
{
    VTermColor color;

    if (index < 0 || index > 15)
        return;
    vterm_color_rgb(&color, r, g, b);
    vterm_state_set_palette_color(vterm_obtain_state(_vt), index, &color);
}

- (void)sendCharacter:(uint32_t)character modifiers:(VTermModifier)modifiers
{
    vterm_keyboard_unichar(_vt, character, modifiers);
}

- (void)sendKey:(VTermKey)key modifiers:(VTermModifier)modifiers
{
    vterm_keyboard_key(_vt, key, modifiers);
}

/* Bracketed when the program asked for it (libvterm knows). */
- (void)paste:(NSString *)text
{
    NSString *lines = [[text stringByReplacingOccurrencesOfString:@"\r\n" withString:@"\r"]
        stringByReplacingOccurrencesOfString:@"\n" withString:@"\r"];

    vterm_keyboard_start_paste(_vt);
    [self writeData:[lines dataUsingEncoding:NSUTF8StringEncoding]];
    vterm_keyboard_end_paste(_vt);
}

- (void)writeData:(NSData *)data
{
    output([data bytes], [data length], (__bridge void *)self);
}

static void appendCells(NSMutableString *text, VTermScreenCell *cells, int count, int from, int to)
{
    NSMutableString *line = [NSMutableString string];

    for (int c = from; c < MIN(to, count); c++) {
        if (cells[c].chars[0] == 0) {
            if (c > 0 && cells[c - 1].width == 2)
                continue;
            [line appendString:@" "];
            continue;
        }
        for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cells[c].chars[i]; i++) {
            uint32_t ch = cells[c].chars[i];
            [line appendString:[[NSString alloc] initWithBytes:&ch length:4 encoding:NSUTF32LittleEndianStringEncoding]];
        }
    }
    while ([line hasSuffix:@" "])
        [line deleteCharactersInRange:NSMakeRange([line length] - 1, 1)];
    [text appendString:line];
}

- (NSString *)textFromLine:(NSInteger)firstLine column:(int)firstColumn toLine:(NSInteger)lastLine column:(int)lastColumn
{
    NSMutableString *text = [NSMutableString string];
    NSInteger total = (NSInteger)[_scrollback count] + _rows;

    firstLine = MAX(0, firstLine);
    lastLine = MIN(total - 1, lastLine);
    for (NSInteger line = firstLine; line <= lastLine; line++) {
        int count = _columns;
        VTermScreenCell cells[count > 0 ? count : 1];

        if (line < (NSInteger)[_scrollback count]) {
            NSData *data = _scrollback[line];
            count = (int)MIN((NSUInteger)_columns, [data length] / sizeof(VTermScreenCell));
            memcpy(cells, [data bytes], sizeof(VTermScreenCell) * count);
        } else
            for (int c = 0; c < count; c++)
                [self getCell:&cells[c] row:(int)(line - [_scrollback count]) column:c];
        appendCells(text, cells, count, line == firstLine ? firstColumn : 0,
            line == lastLine ? lastColumn + 1 : count);
        if (line < lastLine)
            [text appendString:@"\n"];
    }
    return text;
}

- (NSString *)screenText
{
    NSInteger first = (NSInteger)[_scrollback count];
    return [self textFromLine:first column:0 toLine:first + _rows - 1 column:_columns - 1];
}

- (void)showMessage:(NSString *)message
{
    NSString *line = [NSString stringWithFormat:@"\r\n%@\r\n", message];
    NSData *data = [line dataUsingEncoding:NSUTF8StringEncoding];

    vterm_input_write(_vt, [data bytes], [data length]);
    vterm_screen_flush_damage(_screen);
}

- (void)terminate
{
    if (!_exited && _pid > 0)
        kill(_pid, SIGHUP);
}

- (void)dealloc
{
    if (_readSource != nil)
        dispatch_source_cancel(_readSource);
    if (_exitSource != nil)
        dispatch_source_cancel(_exitSource);
    if (_master >= 0)
        close(_master);
    if (_vt != NULL)
        vterm_free(_vt);
}

@end
