/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * osascript: runs AppleScript, as the reference's command does, with our
 * own interpreter of the language's commonly used part (the maintainer's
 * choice, 2026-11-23; it grows as applications and scripts ask):
 *
 *   values      numbers, text, booleans, lists {1, "a"}, records
 *               {name:"x"}, missing value, the constants return, tab,
 *               space, linefeed, quote, pi
 *   statements  set/copy, return, if (one line or a block, else if,
 *               else), repeat (forever, n times, while, until, with i from
 *               a to b [by c], with x in list), exit repeat, try / on
 *               error, error, handlers (on name(params), on run argv) and
 *               their calls, tell application "X" (to ... or a block),
 *               log
 *   expressions & + - * / div mod ^, = is ≠ "is not" < > ≤ ≥ "is less
 *               than" "is greater than" contains "does not contain"
 *               "starts with" "ends with" "is in", and or not, as text /
 *               integer / real / number / list / boolean, item / character
 *               / word / paragraph / text i thru j of, first / last / some
 *               item of, count / length / number of, items / characters /
 *               words / paragraphs of, current date, the clipboard, POSIX
 *               path of, path to (home folder, desktop, documents folder,
 *               applications folder, temporary items)
 *   commands    do shell script [with administrator privileges]
 *               [without altering line endings], display dialog (default
 *               answer, buttons, default button, cancel button, with title,
 *               with icon, giving up after), display notification (with
 *               title, subtitle), say, beep, delay, get volume settings,
 *               set volume (n, output volume n, output muted b), set the
 *               clipboard to, activate / quit / launch / open / name /
 *               version / id / running of an application, application "X"
 *               is running
 *
 * Options: -e lines (each a line of the script), -l AppleScript (no other
 * language), -s with h (readable, the default) or s (recompilable), o (errors
 * to standard output), e (errors to standard error, the default); a script
 * file, or standard input; the arguments after it go to "on run argv".
 * Errors print as the reference's: "0:7: syntax error: message (-2740)",
 * "execution error: message (number)", and the status is 1.
 */

#import <AppKit/AppKit.h>
#import <CoreAudio/AudioHardware.h>
#import <Security/Authorization.h>
#include <math.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

#pragma mark - Values

/* A record: names in order, and their values. */
@interface ASRecord : NSObject
@property (nonatomic, strong) NSMutableArray<NSString *> *keys;
@property (nonatomic, strong) NSMutableDictionary<NSString *, id> *values;
@end

@implementation ASRecord
- (instancetype)init
{
    if ((self = [super init]) != nil) {
        _keys = [NSMutableArray array];
        _values = [NSMutableDictionary dictionary];
    }
    return self;
}
- (void)setValue:(id)value forName:(NSString *)name
{
    if (_values[name] == nil)
        [_keys addObject:name];
    _values[name] = value;
}
@end

/* A file (path to, POSIX file): its POSIX path. */
@interface ASFile : NSObject
@property (nonatomic, copy) NSString *path;
@end
@implementation ASFile
@end

/* An application named in a tell block or an expression. */
@interface ASApplication : NSObject
@property (nonatomic, copy) NSString *name;
@end
@implementation ASApplication
@end

static BOOL isBoolean(id v) { return [v isKindOfClass:[NSNumber class]] && CFGetTypeID((__bridge CFTypeRef)v) == CFBooleanGetTypeID(); }
static BOOL isNumber(id v) { return [v isKindOfClass:[NSNumber class]] && !isBoolean(v); }
static BOOL isReal(id v)
{
    if (!isNumber(v))
        return NO;
    const char *type = [v objCType];
    return strcmp(type, @encode(double)) == 0 || strcmp(type, @encode(float)) == 0;
}

#pragma mark - Errors

@interface ASError : NSException
@property (nonatomic) NSInteger number;
@property (nonatomic) NSUInteger start, end;
@property (nonatomic) BOOL syntax;
@end
@implementation ASError
@end

static void fail(NSInteger number, NSString *format, ...) __attribute__((format(__NSString__, 2, 3)));
static void fail(NSInteger number, NSString *format, ...)
{
    va_list ap;
    va_start(ap, format);
    NSString *message = [[NSString alloc] initWithFormat:format arguments:ap];
    va_end(ap);
    ASError *error = [[ASError alloc] initWithName:@"ASError" reason:message userInfo:nil];
    error.number = number;
    @throw error;
}

#pragma mark - Words

typedef NS_ENUM(NSInteger, TokenKind) { TWord, TNumber, TString, TSymbol, TNewline, TEnd };

@interface Token : NSObject
@property (nonatomic) TokenKind kind;
@property (nonatomic, copy) NSString *text;   /* a word lower-cased; a symbol; a string's contents */
@property (nonatomic, copy) NSString *original;
@property (nonatomic) double number;
@property (nonatomic) BOOL isInteger;
@property (nonatomic) NSUInteger start, end;
@end
@implementation Token
@end

static NSArray<Token *> *tokenize(NSString *source)
{
    NSMutableArray *tokens = [NSMutableArray array];
    NSUInteger i = 0, n = [source length];

    while (i < n) {
        unichar c = [source characterAtIndex:i];
        Token *t = [Token new];
        t.start = i;
        if (c == ' ' || c == '\t') {
            i++;
            continue;
        }
        /* A line continues after ¬. */
        if (c == 0x00AC) {
            i++;
            while (i < n && [source characterAtIndex:i] != '\n' && [source characterAtIndex:i] != '\r')
                i++;
            if (i < n)
                i++;
            continue;
        }
        if (c == '-' && i + 1 < n && [source characterAtIndex:i + 1] == '-') {
            while (i < n && [source characterAtIndex:i] != '\n' && [source characterAtIndex:i] != '\r')
                i++;
            continue;
        }
        if (c == '#') {
            while (i < n && [source characterAtIndex:i] != '\n' && [source characterAtIndex:i] != '\r')
                i++;
            continue;
        }
        if (c == '(' && i + 1 < n && [source characterAtIndex:i + 1] == '*') {
            int depth = 0;
            while (i + 1 < n) {
                unichar a = [source characterAtIndex:i], b = [source characterAtIndex:i + 1];
                if (a == '(' && b == '*') { depth++; i += 2; continue; }
                if (a == '*' && b == ')') { depth--; i += 2; if (depth == 0) break; continue; }
                i++;
            }
            continue;
        }
        if (c == '\n' || c == '\r') {
            t.kind = TNewline;
            t.text = @"\n";
            i++;
        } else if (c == '"' || c == 0x201C) {
            NSMutableString *s = [NSMutableString string];
            unichar close = c == '"' ? '"' : 0x201D;
            i++;
            while (i < n && [source characterAtIndex:i] != close) {
                unichar d = [source characterAtIndex:i];
                if (d == '\\' && i + 1 < n) {
                    unichar e = [source characterAtIndex:++i];
                    d = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e;
                }
                [s appendFormat:@"%C", d];
                i++;
            }
            if (i >= n) {
                ASError *error = [[ASError alloc] initWithName:@"ASError" reason:@"Expected “\"” but found end of script." userInfo:nil];
                error.number = -2741; error.syntax = YES; error.start = t.start; error.end = n;
                @throw error;
            }
            i++;
            t.kind = TString;
            t.text = s;
        } else if (c == '|') {
            NSUInteger start = ++i;
            while (i < n && [source characterAtIndex:i] != '|')
                i++;
            t.kind = TWord;
            t.original = [source substringWithRange:NSMakeRange(start, i - start)];
            t.text = [@"|" stringByAppendingString:t.original];
            if (i < n)
                i++;
        } else if ((c >= '0' && c <= '9') || (c == '.' && i + 1 < n && isdigit([source characterAtIndex:i + 1]))) {
            NSUInteger start = i;
            BOOL real = NO;
            while (i < n && (isdigit([source characterAtIndex:i]) || [source characterAtIndex:i] == '.')) {
                if ([source characterAtIndex:i] == '.') {
                    if (i + 1 >= n || !isdigit([source characterAtIndex:i + 1]))
                        break;
                    real = YES;
                }
                i++;
            }
            if (i < n && ([source characterAtIndex:i] == 'e' || [source characterAtIndex:i] == 'E') && i + 1 < n &&
                (isdigit([source characterAtIndex:i + 1]) || [source characterAtIndex:i + 1] == '-' || [source characterAtIndex:i + 1] == '+')) {
                real = YES;
                i += 2;
                while (i < n && isdigit([source characterAtIndex:i]))
                    i++;
            }
            t.kind = TNumber;
            t.text = [source substringWithRange:NSMakeRange(start, i - start)];
            t.number = [t.text doubleValue];
            t.isInteger = !real && t.number < 536870912;
        } else if ([[NSCharacterSet letterCharacterSet] characterIsMember:c] || c == '_') {
            NSUInteger start = i;
            while (i < n && ([[NSCharacterSet alphanumericCharacterSet] characterIsMember:[source characterAtIndex:i]] ||
                             [source characterAtIndex:i] == '_'))
                i++;
            t.kind = TWord;
            t.original = [source substringWithRange:NSMakeRange(start, i - start)];
            t.text = [t.original lowercaseString];
        } else {
            NSString *two = i + 1 < n ? [source substringWithRange:NSMakeRange(i, 2)] : @"";
            t.kind = TSymbol;
            if ([two isEqualToString:@"<="] || [two isEqualToString:@">="] || [two isEqualToString:@"/="] ||
                [two isEqualToString:@"<>"] || [two isEqualToString:@"!="]) {
                t.text = [two isEqualToString:@"<="] ? @"≤" : [two isEqualToString:@">="] ? @"≥" : @"≠";
                i += 2;
            } else {
                t.text = [NSString stringWithFormat:@"%C", c];
                i++;
            }
        }
        t.end = i;
        [tokens addObject:t];
    }
    Token *end = [Token new];
    end.kind = TEnd;
    end.text = @"";
    end.start = end.end = n;
    [tokens addObject:end];
    return tokens;
}

#pragma mark - The tree

typedef NS_ENUM(NSInteger, NodeKind) {
    NLiteral, NVariable, NList, NRecord, NBinary, NUnary, NAs, NElement, NRange, NCount, NEvery,
    NCall, NCommand, NAppRef, NProperty, NIsRunning, NPathTo, NPosixPath, NPosixFile, NCurrentDate, NClipboard,
    /* statements */
    SSet, SReturn, SIf, SRepeat, SExitRepeat, STry, SError, STell, SExpression, SHandler, SBlock, SLog
};

@interface Node : NSObject
@property (nonatomic) NodeKind kind;
@property (nonatomic, copy) NSString *name;          /* variable, operator, command, element kind */
@property (nonatomic, strong) id value;               /* literal */
@property (nonatomic, strong) NSMutableArray *items;  /* list items, args, block statements, record keys */
@property (nonatomic, strong) NSMutableDictionary *options; /* command parameters */
@property (nonatomic, strong) Node *a, *b, *c, *d;
@property (nonatomic, strong) NSMutableArray *elseIfs; /* if: pairs condition, block */
@property (nonatomic, copy) NSString *repeatKind;
@property (nonatomic, strong) NSArray *params;
@property (nonatomic) NSUInteger start, end;
@end
@implementation Node
- (instancetype)init
{
    if ((self = [super init]) != nil)
        _items = [NSMutableArray array];
    return self;
}
@end

static Node *node(NodeKind kind)
{
    Node *n = [Node new];
    n.kind = kind;
    return n;
}

#pragma mark - The parser

@interface Parser : NSObject {
    NSArray<Token *> *_tokens;
    NSUInteger _at;
    NSString *_source;
    NSUInteger _statementStart;
}
- (instancetype)initWithSource:(NSString *)source;
- (Node *)parseScript;
@end

@implementation Parser

- (instancetype)initWithSource:(NSString *)source
{
    if ((self = [super init]) != nil) {
        _source = source;
        _tokens = tokenize(source);
    }
    return self;
}

- (Token *)peek { return _tokens[_at]; }
- (Token *)peekAt:(NSUInteger)k { return _tokens[MIN(_at + k, [_tokens count] - 1)]; }
- (Token *)next { Token *t = _tokens[_at]; if (_at + 1 < [_tokens count]) _at++; return t; }

- (void)syntaxError:(NSString *)message at:(Token *)t
{
    ASError *error = [[ASError alloc] initWithName:@"ASError" reason:message userInfo:nil];
    error.number = -2740;
    error.syntax = YES;
    error.start = t.start;
    error.end = t.end;
    @throw error;
}

- (BOOL)isWord:(NSString *)w { Token *t = [self peek]; return t.kind == TWord && [t.text isEqualToString:w]; }
- (BOOL)isWord:(NSString *)w at:(NSUInteger)k { Token *t = [self peekAt:k]; return t.kind == TWord && [t.text isEqualToString:w]; }
- (BOOL)isSymbol:(NSString *)s { Token *t = [self peek]; return t.kind == TSymbol && [t.text isEqualToString:s]; }

/* The words given, in a row: taken. */
- (BOOL)accept:(NSString *)phrase
{
    NSArray *words = [phrase componentsSeparatedByString:@" "];
    for (NSUInteger k = 0; k < [words count]; k++)
        if (![self isWord:words[k] at:k])
            return NO;
    for (NSUInteger k = 0; k < [words count]; k++)
        [self next];
    return YES;
}

- (BOOL)acceptSymbol:(NSString *)s
{
    if (![self isSymbol:s])
        return NO;
    [self next];
    return YES;
}

- (void)expect:(NSString *)phrase
{
    if (![self accept:phrase]) {
        Token *t = [self peek];
        [self syntaxError:[NSString stringWithFormat:@"Expected “%@” but found %@.", phrase, [self describe:t]] at:t];
    }
}

- (void)expectSymbol:(NSString *)s
{
    if (![self acceptSymbol:s]) {
        Token *t = [self peek];
        [self syntaxError:[NSString stringWithFormat:@"Expected “%@” but found %@.", s, [self describe:t]] at:t];
    }
}

- (NSString *)describe:(Token *)t
{
    switch (t.kind) {
    case TEnd: return @"end of script";
    case TNewline: return @"end of line";
    case TString: return @"“\"”";
    case TNumber: return @"number";
    case TWord: return [NSString stringWithFormat:@"identifier"];
    default: return [NSString stringWithFormat:@"“%@”", t.text];
    }
}

- (void)skipNewlines
{
    while ([self peek].kind == TNewline)
        [self next];
}

- (BOOL)atLineEnd
{
    TokenKind k = [self peek].kind;
    return k == TNewline || k == TEnd;
}

- (void)endLine
{
    if (![self atLineEnd]) {
        Token *t = [self peek];
        Token *prev = _at > 0 ? _tokens[_at - 1] : t;
        /* As the reference reports it: from the statement's start. */
        Token *from = [Token new];
        from.start = _statementStart;
        from.end = t.end;
        [self syntaxError:[NSString stringWithFormat:@"A %@ can’t go after this %@.",
                                                     t.kind == TWord ? @"identifier" : [self describe:t],
                                                     prev.kind == TWord ? @"identifier" : [self describe:prev]] at:from];
    }
}

- (Node *)parseScript
{
    Node *script = node(SBlock);
    [self skipNewlines];
    while ([self peek].kind != TEnd) {
        [script.items addObject:[self parseStatement]];
        [self endLine];
        [self skipNewlines];
    }
    return script;
}

/* Statements up to one of the words that end the block. */
- (Node *)parseBlockUntil:(NSArray *)enders
{
    Node *block = node(SBlock);
    [self skipNewlines];
    for (;;) {
        Token *t = [self peek];
        if (t.kind == TEnd)
            [self syntaxError:[NSString stringWithFormat:@"Expected “end” but found end of script."] at:t];
        for (NSString *e in enders)
            if ([self accept:e]) {
                block.name = e;
                return block;
            }
        [block.items addObject:[self parseStatement]];
        [self endLine];
        [self skipNewlines];
    }
}

- (void)acceptEnd:(NSString *)what
{
    /* "end if" or just "end". */
    [self accept:what];
}

- (NSString *)identifier
{
    Token *t = [self peek];
    if (t.kind != TWord)
        [self syntaxError:[NSString stringWithFormat:@"Expected variable name but found %@.", [self describe:t]] at:t];
    [self next];
    return t.text;
}

- (Node *)parseStatement
{
    Token *t = [self peek];
    Node *s;

    _statementStart = t.start;

    if ([self accept:@"property"]) {
        s = node(SSet);
        Node *target = node(NVariable);
        target.name = [self identifier];
        s.a = target;
        if (![self acceptSymbol:@":"])
            [self expect:@"to"];
        s.b = [self parseExpression];
        s.start = t.start;
        return s;
    }
    if ([self accept:@"global"] || [self accept:@"local"]) {
        /* Names only: every variable is the script's or the handler's. */
        while ([self peek].kind == TWord || [self isSymbol:@","])
            [self next];
        s = node(SBlock);
        s.start = t.start;
        return s;
    }
    for (NSString *wrapper in @[ @"ignoring", @"considering", @"with timeout", @"using terms from", @"with transaction" ]) {
        if ([self accept:wrapper]) {
            /* Their conditions change nothing here: the block runs. */
            while (![self atLineEnd])
                [self next];
            NSString *last = [[wrapper componentsSeparatedByString:@" "] lastObject];
            if ([wrapper isEqualToString:@"with timeout"]) last = @"timeout";
            if ([wrapper isEqualToString:@"using terms from"]) last = @"using terms from";
            if ([wrapper isEqualToString:@"with transaction"]) last = @"transaction";
            s = [self parseBlockUntil:@[ [@"end " stringByAppendingString:last], @"end" ]];
            s.kind = SBlock;
            s.start = t.start;
            return s;
        }
    }
    if ([self accept:@"set"]) {
        /* set volume …, set the clipboard to … */
        if ([self isWord:@"volume"]) {
            [self next];
            return [self parseVolumeCommand];
        }
        s = node(SSet);
        s.a = [self parseTarget];
        [self expect:@"to"];
        s.b = [self parseExpression];
    } else if ([self accept:@"copy"]) {
        s = node(SSet);
        s.b = [self parseExpression];
        [self expect:@"to"];
        s.a = [self parseTarget];
    } else if ([self accept:@"return"]) {
        s = node(SReturn);
        if (![self atLineEnd])
            s.a = [self parseExpression];
    } else if ([self accept:@"log"]) {
        s = node(SLog);
        s.a = [self parseExpression];
    } else if ([self accept:@"if"]) {
        s = [self parseIf];
    } else if ([self accept:@"repeat"]) {
        s = [self parseRepeat];
    } else if ([self accept:@"exit repeat"] || [self accept:@"exit"]) {
        s = node(SExitRepeat);
    } else if ([self accept:@"try"]) {
        s = node(STry);
        Node *body = [self parseBlockUntil:@[ @"on error", @"end try", @"end" ]];
        s.a = body;
        if ([body.name isEqualToString:@"on error"]) {
            if (![self atLineEnd]) {
                Token *v = [self peek];
                if (v.kind == TWord && ![v.text isEqualToString:@"number"])
                    s.name = [self identifier];
                if ([self accept:@"number"])
                    s.repeatKind = [self identifier];
            }
            s.b = [self parseBlockUntil:@[ @"end try", @"end" ]];
        }
    } else if ([self accept:@"error"]) {
        s = node(SError);
        if (![self atLineEnd] && ![self isWord:@"number"])
            s.a = [self parseExpression];
        if ([self accept:@"number"])
            s.b = [self parseExpression];
    } else if ([self accept:@"tell"]) {
        s = node(STell);
        s.a = [self parsePrimary];
        if ([self accept:@"to"]) {
            Node *block = node(SBlock);
            [block.items addObject:[self parseStatement]];
            s.b = block;
        } else {
            s.b = [self parseBlockUntil:@[ @"end tell", @"end" ]];
        }
    } else if ([self isWord:@"on"] || [self isWord:@"to"]) {
        [self next];
        s = node(SHandler);
        s.name = [self identifier];
        NSMutableArray *params = [NSMutableArray array];
        if ([self acceptSymbol:@"("]) {
            while (![self isSymbol:@")"]) {
                [params addObject:[self identifier]];
                if (![self acceptSymbol:@","])
                    break;
            }
            [self expectSymbol:@")"];
        } else if ([self acceptSymbol:@"{"]) {
            while (![self isSymbol:@"}"]) {
                [params addObject:[self identifier]];
                if (![self acceptSymbol:@","])
                    break;
            }
            [self expectSymbol:@"}"];
            s.repeatKind = @"list";
        } else if (![self atLineEnd]) {
            [params addObject:[self identifier]];
        }
        s.params = params;
        Node *body = [self parseBlockUntil:@[ [@"end " stringByAppendingString:s.name], @"end" ]];
        s.a = body;
    } else {
        s = node(SExpression);
        s.a = [self parseCommandOrExpression];
    }
    s.start = t.start;
    return s;
}

/* What set assigns to: a variable, an item of a list, a property. */
- (Node *)parseTarget
{
    if ([self accept:@"the clipboard"] || [self accept:@"clipboard"])
        return node(NClipboard);
    if ([self isSymbol:@"{"]) {
        /* set {a, b} to {1, 2} */
        Node *list = [self parsePrimary];
        return list;
    }
    return [self parseUnary];
}

- (Node *)parseVolumeCommand
{
    Node *c = node(NCommand);
    c.name = @"set volume";
    c.options = [NSMutableDictionary dictionary];
    for (;;) {
        if ([self accept:@"output volume"])
            c.options[@"output"] = [self parseExpression];
        else if ([self accept:@"input volume"])
            c.options[@"input"] = [self parseExpression];
        else if ([self accept:@"alert volume"])
            c.options[@"alert"] = [self parseExpression];
        else if ([self accept:@"output muted"])
            c.options[@"muted"] = [self parseExpression];
        else if ([self accept:@"with output muted"])
            c.options[@"muted"] = [self literal:@YES];
        else if ([self accept:@"without output muted"])
            c.options[@"muted"] = [self literal:@NO];
        else if (![self atLineEnd] && c.options[@"level"] == nil && [c.options count] == 0)
            c.options[@"level"] = [self parseExpression];
        else
            break;
    }
    Node *s = node(SExpression);
    s.a = c;
    return s;
}

- (Node *)literal:(id)value
{
    Node *n = node(NLiteral);
    n.value = value;
    return n;
}

- (Node *)parseIf
{
    Node *s = node(SIf);
    s.a = [self parseExpression];
    [self accept:@"then"];
    if (![self atLineEnd]) {
        /* if x then statement */
        Node *block = node(SBlock);
        [block.items addObject:[self parseStatement]];
        s.b = block;
        return s;
    }
    s.elseIfs = [NSMutableArray array];
    Node *body = [self parseBlockUntil:@[ @"else if", @"else", @"end if", @"end" ]];
    s.b = body;
    NSString *ended = body.name;
    while ([ended isEqualToString:@"else if"]) {
        Node *condition = [self parseExpression];
        [self accept:@"then"];
        Node *block = [self parseBlockUntil:@[ @"else if", @"else", @"end if", @"end" ]];
        [s.elseIfs addObject:@[ condition, block ]];
        ended = block.name;
    }
    if ([ended isEqualToString:@"else"]) {
        if (![self atLineEnd]) {
            Node *block = node(SBlock);
            [block.items addObject:[self parseStatement]];
            s.c = block;
        } else
            s.c = [self parseBlockUntil:@[ @"end if", @"end" ]];
    }
    return s;
}

- (Node *)parseRepeat
{
    Node *s = node(SRepeat);
    if ([self accept:@"while"]) {
        s.repeatKind = @"while";
        s.a = [self parseExpression];
    } else if ([self accept:@"until"]) {
        s.repeatKind = @"until";
        s.a = [self parseExpression];
    } else if ([self accept:@"with"]) {
        s.name = [self identifier];
        if ([self accept:@"from"]) {
            s.repeatKind = @"from";
            s.a = [self parseExpression];
            [self expect:@"to"];
            s.b = [self parseExpression];
            if ([self accept:@"by"])
                s.c = [self parseExpression];
        } else {
            [self expect:@"in"];
            s.repeatKind = @"in";
            s.a = [self parseExpression];
        }
    } else if (![self atLineEnd]) {
        s.repeatKind = @"times";
        s.a = [self parseExpression];
        [self accept:@"times"];
    } else
        s.repeatKind = @"forever";
    s.d = [self parseBlockUntil:@[ @"end repeat", @"end" ]];
    return s;
}

/* A command with its parameters, else an expression. */
- (Node *)parseCommandOrExpression
{
    Token *t = [self peek];
    Node *c = node(NCommand);
    c.options = [NSMutableDictionary dictionary];
    c.start = t.start;

    if ([self accept:@"do shell script"]) {
        c.name = @"do shell script";
        c.a = [self parseExpression];
        for (;;) {
            if ([self accept:@"with administrator privileges"] || [self accept:@"administrator privileges"])
                c.options[@"admin"] = [self literal:@YES];
            else if ([self accept:@"without altering line endings"])
                c.options[@"raw"] = [self literal:@YES];
            else if ([self accept:@"altering line endings"])
                c.options[@"raw"] = [self parseExpression];
            else if ([self accept:@"user name"] || [self accept:@"password"])
                (void)[self parseExpression];
            else if ([self acceptSymbol:@"and"] || [self accept:@"and"])
                continue;
            else
                break;
        }
        return c;
    }
    if ([self accept:@"display dialog"]) {
        c.name = @"display dialog";
        c.a = [self parseExpression];
        for (;;) {
            if ([self accept:@"default answer"]) c.options[@"answer"] = [self parseExpression];
            else if ([self accept:@"buttons"]) c.options[@"buttons"] = [self parseExpression];
            else if ([self accept:@"default button"]) c.options[@"default"] = [self parseExpression];
            else if ([self accept:@"cancel button"]) c.options[@"cancel"] = [self parseExpression];
            else if ([self accept:@"with title"]) c.options[@"title"] = [self parseExpression];
            else if ([self accept:@"with icon"]) c.options[@"icon"] = [self parseExpression];
            else if ([self accept:@"giving up after"]) c.options[@"giveup"] = [self parseExpression];
            else if ([self accept:@"with hidden answer"]) c.options[@"hidden"] = [self literal:@YES];
            else break;
        }
        return c;
    }
    if ([self accept:@"display alert"]) {
        c.name = @"display dialog";
        c.a = [self parseExpression];
        if ([self accept:@"message"]) c.options[@"message"] = [self parseExpression];
        for (;;) {
            if ([self accept:@"buttons"]) c.options[@"buttons"] = [self parseExpression];
            else if ([self accept:@"default button"]) c.options[@"default"] = [self parseExpression];
            else if ([self accept:@"cancel button"]) c.options[@"cancel"] = [self parseExpression];
            else if ([self accept:@"as"]) (void)[self identifier];
            else if ([self accept:@"giving up after"]) c.options[@"giveup"] = [self parseExpression];
            else break;
        }
        if (c.options[@"buttons"] == nil)
            c.options[@"buttons"] = [self literal:@[ @"OK" ]];
        return c;
    }
    if ([self accept:@"display notification"]) {
        c.name = @"display notification";
        if (![self isWord:@"with"] && ![self atLineEnd])
            c.a = [self parseExpression];
        for (;;) {
            if ([self accept:@"with title"]) c.options[@"title"] = [self parseExpression];
            else if ([self accept:@"subtitle"]) c.options[@"subtitle"] = [self parseExpression];
            else if ([self accept:@"sound name"]) (void)[self parseExpression];
            else break;
        }
        return c;
    }
    if ([self accept:@"say"]) {
        c.name = @"say";
        c.a = [self parseExpression];
        while ([self accept:@"using"] || [self accept:@"speaking rate"] || [self accept:@"saving to"])
            (void)[self parseExpression];
        return c;
    }
    if ([self accept:@"delay"]) {
        c.name = @"delay";
        if (![self atLineEnd])
            c.a = [self parseExpression];
        return c;
    }
    if ([self accept:@"beep"]) {
        c.name = @"beep";
        if (![self atLineEnd])
            c.a = [self parseExpression];
        return c;
    }
    if ([self accept:@"get volume settings"]) {
        c.name = @"get volume settings";
        return c;
    }
    for (NSString *verb in @[ @"activate", @"quit", @"launch", @"reopen", @"run" ])
        if ([self accept:verb]) {
            c.name = verb;
            if (![self atLineEnd] && ![self isWord:@"saving"])
                c.a = [self parsePrimary];
            while ([self accept:@"saving"])
                (void)[self parseExpression];
            return c;
        }
    if ([self accept:@"open"]) {
        c.name = @"open";
        c.a = [self parseExpression];
        return c;
    }
    if ([self accept:@"get"])
        return [self parseExpression];
    return [self parseExpression];
}

#pragma mark Expressions

- (Node *)parseExpression { return [self parseOr]; }

- (Node *)binary:(NSString *)op left:(Node *)a right:(Node *)b
{
    Node *n = node(NBinary);
    n.name = op;
    n.a = a;
    n.b = b;
    return n;
}

- (Node *)parseOr
{
    Node *a = [self parseAnd];
    while ([self accept:@"or"])
        a = [self binary:@"or" left:a right:[self parseAnd]];
    return a;
}

- (Node *)parseAnd
{
    Node *a = [self parseNot];
    while ([self accept:@"and"])
        a = [self binary:@"and" left:a right:[self parseNot]];
    return a;
}

- (Node *)parseNot
{
    if ([self accept:@"not"]) {
        Node *n = node(NUnary);
        n.name = @"not";
        n.a = [self parseNot];
        return n;
    }
    return [self parseComparison];
}

- (Node *)parseComparison
{
    Node *a = [self parseConcatenation];
    for (;;) {
        NSString *op = nil;
        if ([self acceptSymbol:@"="] || [self accept:@"is equal to"] || [self accept:@"equals"] || [self accept:@"is equal"])
            op = @"=";
        else if ([self acceptSymbol:@"≠"] || [self accept:@"is not equal to"] || [self accept:@"is not"] ||
                 [self accept:@"isn't"] || [self accept:@"does not equal"])
            op = @"≠";
        else if ([self acceptSymbol:@"≤"] || [self accept:@"is less than or equal to"])
            op = @"≤";
        else if ([self acceptSymbol:@"≥"] || [self accept:@"is greater than or equal to"])
            op = @"≥";
        else if ([self acceptSymbol:@"<"] || [self accept:@"is less than"] || [self accept:@"comes before"])
            op = @"<";
        else if ([self acceptSymbol:@">"] || [self accept:@"is greater than"] || [self accept:@"comes after"])
            op = @">";
        else if ([self accept:@"does not contain"] || [self accept:@"doesn't contain"])
            op = @"!contains";
        else if ([self accept:@"contains"] || [self accept:@"contain"])
            op = @"contains";
        else if ([self accept:@"starts with"] || [self accept:@"begins with"] || [self accept:@"start with"])
            op = @"starts";
        else if ([self accept:@"ends with"] || [self accept:@"end with"])
            op = @"ends";
        else if ([self accept:@"is not in"] || [self accept:@"is not contained by"])
            op = @"!in";
        else if ([self accept:@"is in"] || [self accept:@"is contained by"])
            op = @"in";
        else if ([self accept:@"is"])
            op = @"=";
        if (op == nil)
            return a;
        a = [self binary:op left:a right:[self parseConcatenation]];
    }
}

- (Node *)parseConcatenation
{
    Node *a = [self parseAdditive];
    while ([self acceptSymbol:@"&"])
        a = [self binary:@"&" left:a right:[self parseAdditive]];
    return a;
}

- (Node *)parseAdditive
{
    Node *a = [self parseMultiplicative];
    for (;;) {
        if ([self acceptSymbol:@"+"])
            a = [self binary:@"+" left:a right:[self parseMultiplicative]];
        else if ([self acceptSymbol:@"-"])
            a = [self binary:@"-" left:a right:[self parseMultiplicative]];
        else
            return a;
    }
}

- (Node *)parseMultiplicative
{
    Node *a = [self parsePower];
    for (;;) {
        if ([self acceptSymbol:@"*"])
            a = [self binary:@"*" left:a right:[self parsePower]];
        else if ([self acceptSymbol:@"/"] || [self acceptSymbol:@"÷"])
            a = [self binary:@"/" left:a right:[self parsePower]];
        else if ([self accept:@"div"])
            a = [self binary:@"div" left:a right:[self parsePower]];
        else if ([self accept:@"mod"])
            a = [self binary:@"mod" left:a right:[self parsePower]];
        else
            return a;
    }
}

- (Node *)parsePower
{
    Node *a = [self parseUnary];
    if ([self acceptSymbol:@"^"])
        a = [self binary:@"^" left:a right:[self parsePower]];
    return a;
}

- (Node *)parseUnary
{
    if ([self acceptSymbol:@"-"]) {
        Node *n = node(NUnary);
        n.name = @"-";
        n.a = [self parseUnary];
        return n;
    }
    if ([self acceptSymbol:@"+"])
        return [self parseUnary];
    Node *a = [self parseOf];
    while ([self accept:@"as"]) {
        Node *n = node(NAs);
        n.a = a;
        NSString *type = [self identifier];
        if ([type isEqualToString:@"unicode"] || [type isEqualToString:@"«class"])
            type = [self identifier];
        n.name = type;
        a = n;
    }
    return a;
}

static NSSet *elementWords(void)
{
    static NSSet *words;
    if (words == nil)
        words = [NSSet setWithArray:@[ @"item", @"items", @"character", @"characters", @"word", @"words",
                                       @"paragraph", @"paragraphs", @"text", @"line", @"lines" ]];
    return words;
}

/* item 2 of x, characters 1 thru 3 of x, first word of x, count of x... */
- (Node *)parseOf
{
    Token *t = [self peek];

    if ([self accept:@"count of"] || [self accept:@"length of"] || [self accept:@"number of items in"] ||
        [self accept:@"number of"]) {
        Node *n = node(NCount);
        n.a = [self parseUnary];
        return n;
    }
    if ([self accept:@"count"]) {
        Node *n = node(NCount);
        [self accept:@"every item of"];
        n.a = [self parseUnary];
        return n;
    }
    if ([self accept:@"posix path of"]) {
        Node *n = node(NPosixPath);
        n.a = [self parseUnary];
        return n;
    }
    if ([self accept:@"posix file"]) {
        Node *n = node(NPosixFile);
        n.a = [self parseUnary];
        return n;
    }
    if ([self accept:@"path to"]) {
        Node *n = node(NPathTo);
        NSMutableArray *words = [NSMutableArray array];
        while ([self peek].kind == TWord && ![self isWord:@"as"] && ![self isWord:@"from"])
            [words addObject:[self next].text];
        n.name = [words componentsJoinedByString:@" "];
        while ([self accept:@"from"])
            (void)[self identifier];
        return n;
    }

    /* first / last / some / every element, or an index or range. */
    NSString *position = nil;
    for (NSString *p in @[ @"first", @"last", @"some", @"every", @"middle", @"second", @"third" ])
        if ([self isWord:p] && [elementWords() containsObject:[self peekAt:1].text]) {
            position = p;
            [self next];
            break;
        }
    if ((position != nil || [elementWords() containsObject:t.text]) && t.kind == TWord) {
        /* Only an element when "of" follows or an index does. */
        Token *kind = [self peek];
        NSString *elem = kind.text;
        BOOL plural = [elem hasSuffix:@"s"] && ![elem isEqualToString:@"text"];
        Token *after = [self peekAt:1];
        BOOL isElement = position != nil || [after.text isEqualToString:@"of"] || after.kind == TNumber ||
                         after.kind == TWord || (after.kind == TSymbol && [after.text isEqualToString:@"-"]) ||
                         (after.kind == TSymbol && [after.text isEqualToString:@"("]);
        if ([elem isEqualToString:@"text"] && after.kind != TNumber && ![after.text isEqualToString:@"of"] &&
            position == nil)
            isElement = NO;
        if (isElement && !([after.text isEqualToString:@"of"] && position == nil && !plural) ) {
            [self next];
            Node *n;
            NSString *singular = plural ? [elem substringToIndex:[elem length] - 1] : elem;
            if ([singular isEqualToString:@"line"]) singular = @"paragraph";
            if (position != nil) {
                n = node(NElement);
                n.repeatKind = position;
            } else if ([self accept:@"of"]) {
                n = node(NEvery);
            } else {
                Node *index = [self parseOfIndex];
                if ([self accept:@"thru"] || [self accept:@"through"]) {
                    n = node(NRange);
                    n.b = index;
                    n.c = [self parseOfIndex];
                } else {
                    n = node(NElement);
                    n.b = index;
                }
                [self expect:@"of"];
            }
            if (n.kind == NElement && position != nil)
                [self expect:@"of"];
            n.name = singular;
            if (n.kind == NRange && [elem isEqualToString:@"text"])
                n.name = @"text";
            n.a = [self parseUnary];
            return n;
        }
        if (isElement && [after.text isEqualToString:@"of"] && plural) {
            [self next];
            [self next];
            Node *n = node(NEvery);
            n.name = [elem substringToIndex:[elem length] - 1];
            if ([n.name isEqualToString:@"line"]) n.name = @"paragraph";
            n.a = [self parseUnary];
            return n;
        }
    }

    Node *p = [self parsePrimary];
    /* name of application "X", the property of a record */
    while ([self accept:@"of"]) {
        Node *n = node(NProperty);
        n.name = p.kind == NVariable ? p.name : @"";
        n.a = [self parseUnary];
        p = n;
    }
    /* application "X" is running */
    if (p.kind == NAppRef && [self isWord:@"is"] && [self isWord:@"running" at:1]) {
        [self next];
        [self next];
        Node *n = node(NIsRunning);
        n.a = p;
        return n;
    }
    if (p.kind == NAppRef && [self isWord:@"is"] && [self isWord:@"not" at:1] && [self isWord:@"running" at:2]) {
        [self next]; [self next]; [self next];
        Node *n = node(NIsRunning);
        n.a = p;
        Node *notNode = node(NUnary);
        notNode.name = @"not";
        notNode.a = n;
        return notNode;
    }
    return p;
}

- (Node *)parseOfIndex
{
    if ([self acceptSymbol:@"-"]) {
        Node *n = node(NUnary);
        n.name = @"-";
        n.a = [self parsePrimary];
        return n;
    }
    return [self parsePrimary];
}

- (Node *)parsePrimary
{
    Token *t = [self peek];
    Node *n;

    switch (t.kind) {
    case TNumber:
        [self next];
        return [self literal:t.isInteger ? @((long long)t.number) : @(t.number)];
    case TString:
        [self next];
        return [self literal:t.text];
    case TSymbol:
        if ([self acceptSymbol:@"("]) {
            [self skipNewlines];
            n = [self parseExpression];
            [self skipNewlines];
            [self expectSymbol:@")"];
            return n;
        }
        if ([self acceptSymbol:@"{"]) {
            [self skipNewlines];
            /* A record if its first item is name:value. */
            if ([self peek].kind == TWord && [[self peekAt:1].text isEqualToString:@":"]) {
                n = node(NRecord);
                while (![self isSymbol:@"}"]) {
                    NSString *key = [self next].original ?: @"";
                    [self expectSymbol:@":"];
                    [n.items addObject:@[ key, [self parseExpression] ]];
                    [self skipNewlines];
                    if (![self acceptSymbol:@","])
                        break;
                    [self skipNewlines];
                }
                [self skipNewlines];
                [self expectSymbol:@"}"];
                return n;
            }
            n = node(NList);
            while (![self isSymbol:@"}"]) {
                [n.items addObject:[self parseExpression]];
                [self skipNewlines];
                if (![self acceptSymbol:@","])
                    break;
                [self skipNewlines];
            }
            [self skipNewlines];
            [self expectSymbol:@"}"];
            return n;
        }
        [self syntaxError:[NSString stringWithFormat:@"Expected expression but found %@.", [self describe:t]] at:t];
        return nil;
    case TWord:
        break;
    default:
        [self syntaxError:[NSString stringWithFormat:@"Expected expression but found %@.", [self describe:t]] at:t];
        return nil;
    }

    if ([self accept:@"true"]) return [self literal:@YES];
    if ([self accept:@"false"]) return [self literal:@NO];
    if ([self accept:@"missing value"]) return [self literal:[NSNull null]];
    if ([self accept:@"pi"]) return [self literal:@(M_PI)];
    if ([self accept:@"return"] || [self accept:@"linefeed"]) return [self literal:@"\n"];
    if ([self accept:@"tab"]) return [self literal:@"\t"];
    if ([self accept:@"space"]) return [self literal:@" "];
    if ([self accept:@"quote"]) return [self literal:@"\""];
    if ([self accept:@"current date"]) return node(NCurrentDate);
    if ([self accept:@"the clipboard"] || [self accept:@"clipboard"]) return node(NClipboard);
    if ([self accept:@"application"] || [self accept:@"app"]) {
        n = node(NAppRef);
        n.a = [self parsePrimary];
        return n;
    }
    [self accept:@"the"];
    if ([self isWord:@"my"]) [self next];
    Token *name = [self next];
    if (name.kind != TWord)
        [self syntaxError:[NSString stringWithFormat:@"Expected expression but found %@.", [self describe:name]] at:name];
    /* A handler call: name(args). */
    if ([self isSymbol:@"("] && [self peek].start == name.end) {
        [self next];
        n = node(NCall);
        n.name = name.text;
        while (![self isSymbol:@")"]) {
            [n.items addObject:[self parseExpression]];
            if (![self acceptSymbol:@","])
                break;
        }
        [self expectSymbol:@")"];
        return n;
    }
    /* A property of several words before "of": button returned of x. */
    static NSSet *stop;
    if (stop == nil)
        stop = [NSSet setWithArray:@[ @"of", @"to", @"then", @"and", @"or", @"is", @"as", @"contains", @"in", @"by",
                                      @"from", @"thru", @"through", @"else", @"with", @"without", @"mod", @"div",
                                      @"not", @"times", @"returning", @"given" ]];
    NSUInteger k = 0;
    while ([self peekAt:k].kind == TWord && ![stop containsObject:[self peekAt:k].text])
        k++;
    NSString *full = name.text;
    if (k > 0 && [self isWord:@"of" at:k]) {
        NSMutableArray *words = [NSMutableArray arrayWithObject:name.text];
        for (NSUInteger j = 0; j < k; j++)
            [words addObject:[self next].text];
        full = [words componentsJoinedByString:@" "];
    }
    n = node(NVariable);
    n.name = full;
    n.start = name.start;
    n.end = name.end;
    return n;
}

@end

#pragma mark - Running

@interface Handler : NSObject
@property (nonatomic, strong) Node *node;
@end
@implementation Handler
@end

@interface ReturnSignal : NSException
@property (nonatomic, strong) id value;
@end
@implementation ReturnSignal
@end

@interface ExitRepeat : NSException
@end
@implementation ExitRepeat
@end

@interface Interpreter : NSObject {
    NSMutableArray<NSMutableDictionary *> *_scopes;
    NSMutableDictionary<NSString *, Node *> *_handlers;
    NSMutableArray *_tellTargets;
}
@property (nonatomic, strong) id result;
- (id)runScript:(Node *)script arguments:(NSArray *)arguments;
@end

static NSString *textOf(id value, BOOL recompilable);

@implementation Interpreter

- (instancetype)init
{
    if ((self = [super init]) != nil) {
        _scopes = [NSMutableArray arrayWithObject:[NSMutableDictionary dictionary]];
        _handlers = [NSMutableDictionary dictionary];
        _tellTargets = [NSMutableArray array];
    }
    return self;
}

- (id)lookup:(NSString *)name
{
    if ([name isEqualToString:@"result"])
        return _result ?: [NSNull null];
    for (NSMutableDictionary *scope in [_scopes reverseObjectEnumerator]) {
        id value = scope[name];
        if (value != nil)
            return value;
        if (scope == _scopes[0])
            break;
    }
    id global = _scopes[0][name];
    if (global != nil)
        return global;
    fail(-2753, @"The variable %@ is not defined.", [name hasPrefix:@"|"] ? [name substringFromIndex:1] : name);
    return nil;
}

- (void)assign:(id)value to:(NSString *)name
{
    [_scopes lastObject][name] = value;
}

- (id)runScript:(Node *)script arguments:(NSArray *)arguments
{
    Node *run = nil;
    NSMutableArray *top = [NSMutableArray array];

    for (Node *s in script.items) {
        if (s.kind == SHandler) {
            _handlers[s.name] = s;
            if ([s.name isEqualToString:@"run"])
                run = s;
        } else
            [top addObject:s];
    }
    @try {
        if ([top count] > 0) {
            for (Node *s in top)
                [self execute:s];
        }
        if (run != nil) {
            NSMutableDictionary *scope = [NSMutableDictionary dictionary];
            if ([run.params count] > 0) {
                if ([run.repeatKind isEqualToString:@"list"])
                    for (NSUInteger i = 0; i < [run.params count]; i++)
                        scope[run.params[i]] = i < [arguments count] ? arguments[i] : [NSNull null];
                else
                    scope[run.params[0]] = arguments;
            }
            [_scopes addObject:scope];
            [self executeBlock:run.a];
            [_scopes removeLastObject];
        }
    } @catch (ReturnSignal *signal) {
        return signal.value;
    }
    return _result;
}

- (void)executeBlock:(Node *)block
{
    for (Node *s in block.items)
        [self execute:s];
}

- (BOOL)truth:(id)value
{
    if (isBoolean(value) || isNumber(value))
        return [value boolValue];
    if ([value isKindOfClass:[NSString class]]) {
        if ([[value lowercaseString] isEqualToString:@"true"]) return YES;
        if ([[value lowercaseString] isEqualToString:@"false"]) return NO;
    }
    fail(-1700, @"Can’t make %@ into type boolean.", textOf(value, YES));
    return NO;
}

- (void)execute:(Node *)s
{
    switch (s.kind) {
    case SSet: {
        id value = [self evaluate:s.b];
        [self store:value into:s.a];
        _result = value;
        break;
    }
    case SReturn: {
        ReturnSignal *signal = [[ReturnSignal alloc] initWithName:@"return" reason:nil userInfo:nil];
        signal.value = s.a != nil ? [self evaluate:s.a] : nil;
        @throw signal;
    }
    case SLog: {
        id value = [self evaluate:s.a];
        fprintf(stderr, "%s\n", [textOf(value, NO) UTF8String]);
        break;
    }
    case SExpression:
        _result = [self evaluate:s.a];
        break;
    case SIf: {
        if ([self truth:[self evaluate:s.a]]) {
            [self executeBlock:s.b];
            break;
        }
        BOOL done = NO;
        for (NSArray *pair in s.elseIfs)
            if ([self truth:[self evaluate:pair[0]]]) {
                [self executeBlock:pair[1]];
                done = YES;
                break;
            }
        if (!done && s.c != nil)
            [self executeBlock:s.c];
        break;
    }
    case SRepeat:
        @try {
            [self repeat:s];
        } @catch (ExitRepeat *exit) {
        }
        break;
    case SExitRepeat:
        @throw [[ExitRepeat alloc] initWithName:@"exit" reason:nil userInfo:nil];
    case STry:
        @try {
            [self executeBlock:s.a];
        } @catch (ASError *error) {
            if (s.b != nil) {
                if (s.name != nil)
                    [self assign:error.reason to:s.name];
                if (s.repeatKind != nil)
                    [self assign:@(error.number) to:s.repeatKind];
                [self executeBlock:s.b];
            }
        }
        break;
    case SError: {
        NSString *message = s.a != nil ? textOf([self evaluate:s.a], NO) : @"An error of type -2700 has occurred.";
        NSInteger number = s.b != nil ? [[self evaluate:s.b] integerValue] : -2700;
        fail(number, @"%@", message);
        break;
    }
    case STell: {
        id target = [self evaluate:s.a];
        [_tellTargets addObject:target ?: [NSNull null]];
        @try {
            [self executeBlock:s.b];
        } @finally {
            [_tellTargets removeLastObject];
        }
        break;
    }
    case SHandler:
        _handlers[s.name] = s;
        break;
    case SBlock:
        [self executeBlock:s];
        break;
    default:
        _result = [self evaluate:s];
    }
}

- (void)store:(id)value into:(Node *)target
{
    if (target.kind == NVariable) {
        [self assign:value to:target.name];
    } else if (target.kind == NList) {
        NSArray *values = [value isKindOfClass:[NSArray class]] ? value : @[ value ];
        for (NSUInteger i = 0; i < [target.items count]; i++)
            [self store:i < [values count] ? values[i] : [NSNull null] into:target.items[i]];
    } else if (target.kind == NClipboard) {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setString:textOf(value, NO) forType:NSPasteboardTypeString];
    } else if (target.kind == NElement && [target.name isEqualToString:@"item"] && target.a.kind == NVariable) {
        NSMutableArray *list = [[self coerce:[self lookup:target.a.name] to:@"list"] mutableCopy];
        NSInteger index = [[self evaluate:target.b] integerValue];
        NSInteger count = (NSInteger)[list count];
        NSInteger at = index < 0 ? count + index : index - 1;
        if (at < 0 || at >= count)
            fail(-1728, @"Can’t set item %ld of %@ to %@.", (long)index, textOf(list, YES), textOf(value, YES));
        list[(NSUInteger)at] = value;
        [self assign:list to:target.a.name];
    } else if (target.kind == NProperty && target.a.kind == NVariable) {
        id record = [self lookup:target.a.name];
        if (![record isKindOfClass:[ASRecord class]])
            fail(-10006, @"Can’t set %@ of %@.", target.name, textOf(record, YES));
        [(ASRecord *)record setValue:value forName:target.name];
    } else
        fail(-10006, @"Can’t set that to %@.", textOf(value, YES));
}

- (void)repeat:(Node *)s
{
    NSString *kind = s.repeatKind;

    if ([kind isEqualToString:@"forever"]) {
        for (;;)
            [self executeBlock:s.d];
    } else if ([kind isEqualToString:@"times"]) {
        long long n = [[self coerce:[self evaluate:s.a] to:@"integer"] longLongValue];
        for (long long i = 0; i < n; i++)
            [self executeBlock:s.d];
    } else if ([kind isEqualToString:@"while"]) {
        while ([self truth:[self evaluate:s.a]])
            [self executeBlock:s.d];
    } else if ([kind isEqualToString:@"until"]) {
        while (![self truth:[self evaluate:s.a]])
            [self executeBlock:s.d];
    } else if ([kind isEqualToString:@"from"]) {
        double from = [[self coerce:[self evaluate:s.a] to:@"number"] doubleValue];
        double to = [[self coerce:[self evaluate:s.b] to:@"number"] doubleValue];
        double by = s.c != nil ? [[self coerce:[self evaluate:s.c] to:@"number"] doubleValue] : 1;
        if (by == 0)
            fail(-2700, @"The step of a repeat can’t be 0.");
        for (double i = from; by > 0 ? i <= to : i >= to; i += by) {
            [self assign:(floor(i) == i ? (id)@((long long)i) : (id)@(i)) to:s.name];
            [self executeBlock:s.d];
        }
    } else {
        id list = [self evaluate:s.a];
        NSArray *items = [list isKindOfClass:[NSArray class]] ? list : [self coerce:list to:@"list"];
        for (id item in [items copy]) {
            [self assign:item to:s.name];
            [self executeBlock:s.d];
        }
    }
}

#pragma mark Values

- (id)coerce:(id)value to:(NSString *)type
{
    if ([type isEqualToString:@"text"] || [type isEqualToString:@"string"]) {
        if ([value isKindOfClass:[NSArray class]]) {
            NSMutableString *s = [NSMutableString string];
            for (id item in value)
                [s appendString:[self coerce:item to:@"text"]];
            return s;
        }
        if ([value isKindOfClass:[ASFile class]])
            return [self hfsPath:[(ASFile *)value path]];
        if ([value isKindOfClass:[NSNull class]])
            return @"missing value";
        if ([value isKindOfClass:[ASRecord class]])
            fail(-1700, @"Can’t make %@ into type text.", textOf(value, YES));
        return textOf(value, NO);
    }
    if ([type isEqualToString:@"integer"] || [type isEqualToString:@"number"] || [type isEqualToString:@"real"]) {
        double d;
        if (isNumber(value))
            d = [value doubleValue];
        else if (isBoolean(value))
            fail(-1700, @"Can’t make %@ into type %@.", textOf(value, YES), type);
        else if ([value isKindOfClass:[NSString class]]) {
            NSScanner *scanner = [NSScanner scannerWithString:value];
            if (![scanner scanDouble:&d] || ![scanner isAtEnd])
                fail(-1700, @"Can’t make %@ into type %@.", textOf(value, YES), type);
        } else if ([value isKindOfClass:[NSArray class]] && [value count] == 1)
            return [self coerce:[value firstObject] to:type];
        else {
            fail(-1700, @"Can’t make %@ into type %@.", textOf(value, YES), type);
            return nil;
        }
        if ([type isEqualToString:@"integer"])
            return @((long long)llround(d));
        if ([type isEqualToString:@"real"])
            return @(d);
        return floor(d) == d && fabs(d) < 536870912 && !isReal(value) ? (id)@((long long)d) : (id)@(d);
    }
    if ([type isEqualToString:@"boolean"])
        return [self truth:value] ? @YES : @NO;
    if ([type isEqualToString:@"list"]) {
        if ([value isKindOfClass:[NSArray class]])
            return value;
        if ([value isKindOfClass:[ASRecord class]]) {
            NSMutableArray *values = [NSMutableArray array];
            for (NSString *k in [(ASRecord *)value keys])
                [values addObject:[(ASRecord *)value values][k]];
            return values;
        }
        return @[ value ];
    }
    if ([type isEqualToString:@"alias"] || [type isEqualToString:@"file"]) {
        ASFile *file = [ASFile new];
        file.path = [value isKindOfClass:[ASFile class]] ? [(ASFile *)value path] : [self posixFromHFS:textOf(value, NO)];
        return file;
    }
    return value;
}

/* The reference's colon paths: the startup volume is "Minkar". */
- (NSString *)hfsPath:(NSString *)posix
{
    NSString *path = [posix stringByReplacingOccurrencesOfString:@":" withString:@"/"];
    NSArray *parts = [[path componentsSeparatedByString:@"/"] filteredArrayUsingPredicate:
                                                           [NSPredicate predicateWithFormat:@"length > 0"]];
    BOOL directory = [posix hasSuffix:@"/"];
    NSString *result = [@"Minkar:" stringByAppendingString:[parts componentsJoinedByString:@":"]];
    return directory && [parts count] > 0 ? [result stringByAppendingString:@":"] : result;
}

- (NSString *)posixFromHFS:(NSString *)hfs
{
    if ([hfs hasPrefix:@"/"])
        return hfs;
    NSMutableArray *parts = [[hfs componentsSeparatedByString:@":"] mutableCopy];
    if ([parts count] > 0)
        [parts removeObjectAtIndex:0];
    return [@"/" stringByAppendingString:[parts componentsJoinedByString:@"/"]];
}

- (NSComparisonResult)compare:(id)a with:(id)b
{
    if ((isNumber(a) || isBoolean(a)) && (isNumber(b) || isBoolean(b)))
        return [a compare:b];
    if (isNumber(a) && [b isKindOfClass:[NSString class]])
        return [a compare:[self coerce:b to:@"number"]];
    if ([a isKindOfClass:[NSDate class]] && [b isKindOfClass:[NSDate class]])
        return [a compare:b];
    return [textOf(a, NO) compare:[self coerce:b to:@"text"] options:NSCaseInsensitiveSearch];
}

- (BOOL)equal:(id)a to:(id)b
{
    if ([a isKindOfClass:[NSString class]] && [b isKindOfClass:[NSString class]])
        return [a compare:b options:NSCaseInsensitiveSearch] == NSOrderedSame;
    if ((isNumber(a) && isNumber(b)) || (isBoolean(a) && isBoolean(b)))
        return [a isEqual:b];
    if ([a isKindOfClass:[NSArray class]] && [b isKindOfClass:[NSArray class]]) {
        if ([a count] != [b count])
            return NO;
        for (NSUInteger i = 0; i < [a count]; i++)
            if (![self equal:a[i] to:b[i]])
                return NO;
        return YES;
    }
    if ([a isKindOfClass:[NSNull class]] || [b isKindOfClass:[NSNull class]])
        return [a isKindOfClass:[NSNull class]] && [b isKindOfClass:[NSNull class]];
    return [a isEqual:b];
}

- (id)arithmetic:(NSString *)op left:(id)a right:(id)b
{
    if ([a isKindOfClass:[NSDate class]] && isNumber(b) && ([op isEqualToString:@"+"] || [op isEqualToString:@"-"]))
        return [a dateByAddingTimeInterval:[op isEqualToString:@"+"] ? [b doubleValue] : -[b doubleValue]];
    id x = [self coerce:a to:@"number"], y = [self coerce:b to:@"number"];
    BOOL integers = !isReal(x) && !isReal(y);
    double p = [x doubleValue], q = [y doubleValue], r;

    if ([op isEqualToString:@"+"]) r = p + q;
    else if ([op isEqualToString:@"-"]) r = p - q;
    else if ([op isEqualToString:@"*"]) r = p * q;
    else if ([op isEqualToString:@"/"]) {
        if (q == 0)
            fail(-2701, @"Can’t divide %@ by zero.", textOf(x, YES));
        return @(p / q);
    } else if ([op isEqualToString:@"div"]) {
        if (q == 0)
            fail(-2701, @"Can’t divide %@ by zero.", textOf(x, YES));
        return @((long long)trunc(p / q));
    } else if ([op isEqualToString:@"mod"]) {
        if (q == 0)
            fail(-2701, @"Can’t divide %@ by zero.", textOf(x, YES));
        r = fmod(p, q);
    } else {
        return @(pow(p, q));
    }
    return integers && fabs(r) < 536870912 ? (id)@((long long)r) : (id)@(r);
}

- (id)evaluate:(Node *)n
{
    switch (n.kind) {
    case NLiteral:
        return n.value;
    case NVariable: {
        if (![self hasVariable:n.name] && _handlers[n.name] != nil)
            return [self call:n.name arguments:@[]];
        /* Inside tell application: its properties. */
        if (![self hasVariable:n.name] && [[_tellTargets lastObject] isKindOfClass:[ASApplication class]])
            return [self property:n.name ofApplication:[_tellTargets lastObject]];
        return [self lookup:n.name];
    }
    case NList: {
        NSMutableArray *list = [NSMutableArray array];
        for (Node *item in n.items)
            [list addObject:[self evaluate:item]];
        return list;
    }
    case NRecord: {
        ASRecord *record = [ASRecord new];
        for (NSArray *pair in n.items)
            [record setValue:[self evaluate:pair[1]] forName:pair[0]];
        return record;
    }
    case NUnary: {
        id value = [self evaluate:n.a];
        if ([n.name isEqualToString:@"not"])
            return [self truth:value] ? @NO : @YES;
        return [self arithmetic:@"-" left:@0 right:value];
    }
    case NAs:
        return [self coerce:[self evaluate:n.a] to:n.name];
    case NBinary:
        return [self binary:n];
    case NCount: {
        id value = [self evaluate:n.a];
        if ([value isKindOfClass:[NSString class]])
            return @((long long)[value length]);
        if ([value isKindOfClass:[ASRecord class]])
            return @((long long)[[(ASRecord *)value keys] count]);
        return @((long long)[[self coerce:value to:@"list"] count]);
    }
    case NEvery:
        return [self elementsOf:[self evaluate:n.a] kind:n.name];
    case NElement:
        return [self element:n];
    case NRange: {
        id container = [self evaluate:n.a];
        NSArray *elements = [n.name isEqualToString:@"text"] ? [self elementsOf:container kind:@"character"]
                                                             : [self elementsOf:container kind:n.name];
        NSInteger count = (NSInteger)[elements count];
        NSInteger i = [[self coerce:[self evaluate:n.b] to:@"integer"] integerValue];
        NSInteger j = [[self coerce:[self evaluate:n.c] to:@"integer"] integerValue];
        if (i < 0) i = count + i + 1;
        if (j < 0) j = count + j + 1;
        if (i > j) { NSInteger k = i; i = j; j = k; }
        if (i < 1 || j > count)
            fail(-1728, @"Can’t get %@s %ld thru %ld of %@.", n.name, (long)i, (long)j, textOf(container, YES));
        NSArray *slice = [elements subarrayWithRange:NSMakeRange((NSUInteger)(i - 1), (NSUInteger)(j - i + 1))];
        if ([n.name isEqualToString:@"text"])
            return [slice componentsJoinedByString:@""];
        return slice;
    }
    case NProperty: {
        id container = [self evaluate:n.a];
        if ([container isKindOfClass:[ASRecord class]]) {
            id value = [(ASRecord *)container values][n.name];
            if (value == nil)
                fail(-1728, @"Can’t get %@ of %@.", n.name, textOf(container, YES));
            return value;
        }
        if ([container isKindOfClass:[ASApplication class]])
            return [self property:n.name ofApplication:container];
        if ([n.name isEqualToString:@"length"])
            return @((long long)[textOf(container, NO) length]);
        fail(-1728, @"Can’t get %@ of %@.", n.name, textOf(container, YES));
        return nil;
    }
    case NAppRef: {
        ASApplication *app = [ASApplication new];
        app.name = textOf([self evaluate:n.a], NO);
        return app;
    }
    case NIsRunning:
        return [self isRunning:[self evaluate:n.a]] ? @YES : @NO;
    case NCall: {
        NSMutableArray *args = [NSMutableArray array];
        for (Node *a in n.items)
            [args addObject:[self evaluate:a]];
        return [self call:n.name arguments:args];
    }
    case NCommand:
        return [self command:n];
    case NCurrentDate:
        return [NSDate date];
    case NClipboard:
        return [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString] ?: @"";
    case NPosixPath: {
        id value = [self evaluate:n.a];
        if ([value isKindOfClass:[ASFile class]])
            return [(ASFile *)value path];
        return [self posixFromHFS:textOf(value, NO)];
    }
    case NPosixFile: {
        ASFile *file = [ASFile new];
        file.path = textOf([self evaluate:n.a], NO);
        return file;
    }
    case NPathTo:
        return [self pathTo:n.name];
    default:
        fail(-2700, @"Can’t evaluate that here.");
        return nil;
    }
}

- (BOOL)hasVariable:(NSString *)name
{
    if ([name isEqualToString:@"result"])
        return YES;
    return [_scopes lastObject][name] != nil || _scopes[0][name] != nil;
}

- (id)binary:(Node *)n
{
    NSString *op = n.name;

    if ([op isEqualToString:@"and"])
        return [self truth:[self evaluate:n.a]] && [self truth:[self evaluate:n.b]] ? @YES : @NO;
    if ([op isEqualToString:@"or"])
        return [self truth:[self evaluate:n.a]] || [self truth:[self evaluate:n.b]] ? @YES : @NO;
    id a = [self evaluate:n.a], b = [self evaluate:n.b];
    if ([op isEqualToString:@"&"]) {
        if ([a isKindOfClass:[NSArray class]] || [b isKindOfClass:[NSArray class]]) {
            NSMutableArray *list = [[self coerce:a to:@"list"] mutableCopy];
            [list addObjectsFromArray:[self coerce:b to:@"list"]];
            return list;
        }
        if ([a isKindOfClass:[ASRecord class]] && [b isKindOfClass:[ASRecord class]]) {
            ASRecord *merged = [ASRecord new];
            for (ASRecord *r in @[ b, a ])
                for (NSString *k in r.keys)
                    [merged setValue:r.values[k] forName:k];
            return merged;
        }
        if ([a isKindOfClass:[NSString class]])
            return [a stringByAppendingString:[self coerce:b to:@"text"]];
        return @[ a, b ];
    }
    if ([op isEqualToString:@"="]) return [self equal:a to:b] ? @YES : @NO;
    if ([op isEqualToString:@"≠"]) return [self equal:a to:b] ? @NO : @YES;
    if ([op isEqualToString:@"<"]) return [self compare:a with:b] == NSOrderedAscending ? @YES : @NO;
    if ([op isEqualToString:@">"]) return [self compare:a with:b] == NSOrderedDescending ? @YES : @NO;
    if ([op isEqualToString:@"≤"]) return [self compare:a with:b] != NSOrderedDescending ? @YES : @NO;
    if ([op isEqualToString:@"≥"]) return [self compare:a with:b] != NSOrderedAscending ? @YES : @NO;
    if ([op hasSuffix:@"contains"] || [op hasSuffix:@"in"]) {
        BOOL reversed = [op hasSuffix:@"in"];
        id container = reversed ? b : a, item = reversed ? a : b;
        BOOL found;
        if ([container isKindOfClass:[NSArray class]]) {
            found = NO;
            for (id x in container)
                if ([self equal:x to:item])
                    found = YES;
        } else
            found = [textOf(container, NO) rangeOfString:[self coerce:item to:@"text"] options:NSCaseInsensitiveSearch].location != NSNotFound;
        return ([op hasPrefix:@"!"] ? !found : found) ? @YES : @NO;
    }
    if ([op isEqualToString:@"starts"] || [op isEqualToString:@"ends"]) {
        if ([a isKindOfClass:[NSArray class]]) {
            id edge = [op isEqualToString:@"starts"] ? [a firstObject] : [a lastObject];
            return edge != nil && [self equal:edge to:b] ? @YES : @NO;
        }
        NSString *s = [textOf(a, NO) lowercaseString], *t = [[self coerce:b to:@"text"] lowercaseString];
        return ([op isEqualToString:@"starts"] ? [s hasPrefix:t] : [s hasSuffix:t]) ? @YES : @NO;
    }
    return [self arithmetic:op left:a right:b];
}

- (NSArray *)elementsOf:(id)container kind:(NSString *)kind
{
    if ([kind isEqualToString:@"item"])
        return [self coerce:container to:@"list"];
    NSString *text = [self coerce:container to:@"text"];
    NSMutableArray *result = [NSMutableArray array];
    if ([kind isEqualToString:@"character"]) {
        [text enumerateSubstringsInRange:NSMakeRange(0, [text length]) options:NSStringEnumerationByComposedCharacterSequences
                              usingBlock:^(NSString *s, NSRange r, NSRange e, BOOL *stop) { [result addObject:s]; }];
    } else if ([kind isEqualToString:@"word"]) {
        for (NSString *w in [text componentsSeparatedByCharactersInSet:
                                  [[NSCharacterSet alphanumericCharacterSet] invertedSet]])
            if ([w length] > 0)
                [result addObject:w];
    } else if ([kind isEqualToString:@"paragraph"]) {
        NSString *unified = [[text stringByReplacingOccurrencesOfString:@"\r\n" withString:@"\n"]
            stringByReplacingOccurrencesOfString:@"\r" withString:@"\n"];
        [result addObjectsFromArray:[unified componentsSeparatedByString:@"\n"]];
        if ([unified hasSuffix:@"\n"])
            [result removeLastObject];
    } else
        fail(-1728, @"Can’t get every %@ of %@.", kind, textOf(container, YES));
    return result;
}

- (id)element:(Node *)n
{
    id container = [self evaluate:n.a];
    NSArray *elements = [self elementsOf:container kind:[n.name isEqualToString:@"text"] ? @"character" : n.name];
    NSInteger count = (NSInteger)[elements count];
    NSInteger index;

    if ([n.repeatKind isEqualToString:@"first"]) index = 1;
    else if ([n.repeatKind isEqualToString:@"second"]) index = 2;
    else if ([n.repeatKind isEqualToString:@"third"]) index = 3;
    else if ([n.repeatKind isEqualToString:@"last"]) index = -1;
    else if ([n.repeatKind isEqualToString:@"middle"]) index = (count + 1) / 2;
    else if ([n.repeatKind isEqualToString:@"some"]) index = count > 0 ? (NSInteger)arc4random_uniform((uint32_t)count) + 1 : 1;
    else if ([n.repeatKind isEqualToString:@"every"]) return elements;
    else index = [[self coerce:[self evaluate:n.b] to:@"integer"] integerValue];
    NSInteger at = index < 0 ? count + index : index - 1;
    if (at < 0 || at >= count)
        fail(-1728, @"Can’t get %@ %ld of %@.", n.name, (long)index, textOf(container, YES));
    return elements[(NSUInteger)at];
}

- (id)call:(NSString *)name arguments:(NSArray *)args
{
    Node *handler = _handlers[name];
    if (handler == nil)
        fail(-1708, @"«script» doesn’t understand the “%@” message.", name);
    NSMutableDictionary *scope = [NSMutableDictionary dictionary];
    for (NSUInteger i = 0; i < [handler.params count]; i++)
        scope[handler.params[i]] = i < [args count] ? args[i] : [NSNull null];
    [_scopes addObject:scope];
    id value = nil;
    @try {
        [self executeBlock:handler.a];
        value = _result;
    } @catch (ReturnSignal *signal) {
        value = signal.value;
    } @finally {
        [_scopes removeLastObject];
    }
    return value ?: [NSNull null];
}

#pragma mark Applications

- (NSString *)appPath:(NSString *)name
{
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSString *path = [workspace fullPathForApplication:name];
    if (path == nil && [name rangeOfString:@"."].location != NSNotFound)
        path = [workspace absolutePathForAppBundleWithIdentifier:name];
    if (path == nil) {
        for (NSString *dir in @[ @"/Applications", @"/System/Applications", @"/System/Applications/Utilities",
                                 @"/Applications/Utilities", @"/System/Library/CoreServices" ]) {
            NSString *candidate = [[dir stringByAppendingPathComponent:name] stringByAppendingPathExtension:@"app"];
            if ([[NSFileManager defaultManager] fileExistsAtPath:candidate])
                return candidate;
            NSString *squeezed = [[dir stringByAppendingPathComponent:[name stringByReplacingOccurrencesOfString:@" " withString:@""]]
                stringByAppendingPathExtension:@"app"];
            if ([[NSFileManager defaultManager] fileExistsAtPath:squeezed])
                return squeezed;
        }
    }
    return path;
}

- (NSRunningApplication *)running:(ASApplication *)app
{
    NSString *path = [self appPath:app.name];
    for (NSRunningApplication *r in [[NSWorkspace sharedWorkspace] runningApplications]) {
        if ([[r localizedName] caseInsensitiveCompare:app.name] == NSOrderedSame ||
            [[r bundleIdentifier] caseInsensitiveCompare:app.name] == NSOrderedSame ||
            (path != nil && [[[r bundleURL] path] isEqualToString:path]))
            return r;
    }
    return nil;
}

- (BOOL)isRunning:(id)app
{
    if (![app isKindOfClass:[ASApplication class]])
        fail(-1700, @"Can’t make %@ into type application.", textOf(app, YES));
    return [self running:app] != nil;
}

- (id)property:(NSString *)name ofApplication:(ASApplication *)app
{
    NSString *path = [self appPath:app.name];
    NSBundle *bundle = path != nil ? [NSBundle bundleWithPath:path] : nil;

    if ([name isEqualToString:@"running"])
        return [self running:app] != nil ? @YES : @NO;
    if (bundle == nil)
        fail(-2700, @"Can’t get application \"%@\".", app.name);
    if ([name isEqualToString:@"name"])
        return [bundle objectForInfoDictionaryKey:@"CFBundleName"] ?: [[path lastPathComponent] stringByDeletingPathExtension];
    if ([name isEqualToString:@"version"])
        return [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"] ?: @"";
    if ([name isEqualToString:@"id"])
        return [bundle bundleIdentifier] ?: [NSNull null];
    if ([name isEqualToString:@"frontmost"])
        return [[self running:app] isActive] ? @YES : @NO;
    fail(-1728, @"Can’t get %@ of application \"%@\".", name, app.name);
    return nil;
}

- (ASApplication *)applicationFor:(Node *)target
{
    id value = target != nil ? [self evaluate:target] : [_tellTargets lastObject];
    if ([value isKindOfClass:[ASApplication class]])
        return value;
    if ([value isKindOfClass:[NSString class]]) {
        ASApplication *app = [ASApplication new];
        app.name = value;
        return app;
    }
    return nil;
}

#pragma mark Commands

- (id)command:(Node *)n
{
    NSString *name = n.name;
    NSDictionary *o = n.options;

    if ([name isEqualToString:@"do shell script"])
        return [self shellScript:textOf([self evaluate:n.a], NO) admin:o[@"admin"] != nil
                             raw:o[@"raw"] != nil && [self truth:[self evaluate:o[@"raw"]]]];
    if ([name isEqualToString:@"display dialog"])
        return [self dialog:n];
    if ([name isEqualToString:@"display notification"]) {
        NSUserNotification *note = [NSUserNotification new];
        note.title = o[@"title"] != nil ? textOf([self evaluate:o[@"title"]], NO) : @"Script";
        if (o[@"subtitle"] != nil)
            note.subtitle = textOf([self evaluate:o[@"subtitle"]], NO);
        note.informativeText = n.a != nil ? textOf([self evaluate:n.a], NO) : @"";
        [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:note];
        return nil;
    }
    if ([name isEqualToString:@"say"]) {
        NSString *text = textOf([self evaluate:n.a], NO);
        if ([[NSFileManager defaultManager] isExecutableFileAtPath:@"/usr/bin/say"]) {
            pid_t pid;
            const char *argv[] = { "say", [text UTF8String], NULL };
            if (posix_spawn(&pid, "/usr/bin/say", NULL, NULL, (char **)argv, environ) == 0)
                waitpid(pid, NULL, 0);
        } else
            fail(-2700, @"Speech is not available on this system yet.");
        return nil;
    }
    if ([name isEqualToString:@"delay"]) {
        double seconds = n.a != nil ? [[self coerce:[self evaluate:n.a] to:@"number"] doubleValue] : 0;
        if (seconds > 0)
            [NSThread sleepForTimeInterval:seconds];
        return nil;
    }
    if ([name isEqualToString:@"beep"]) {
        long long times = n.a != nil ? [[self coerce:[self evaluate:n.a] to:@"integer"] longLongValue] : 1;
        for (long long i = 0; i < times; i++)
            NSBeep();
        return nil;
    }
    if ([name isEqualToString:@"get volume settings"])
        return [self volumeSettings];
    if ([name isEqualToString:@"set volume"]) {
        if (o[@"level"] != nil)
            [self setOutputVolume:[[self coerce:[self evaluate:o[@"level"]] to:@"number"] doubleValue] / 7.0];
        if (o[@"output"] != nil)
            [self setOutputVolume:[[self coerce:[self evaluate:o[@"output"]] to:@"number"] doubleValue] / 100.0];
        if (o[@"muted"] != nil)
            [self setMuted:[self truth:[self evaluate:o[@"muted"]]]];
        return nil;
    }
    if ([name isEqualToString:@"activate"] || [name isEqualToString:@"launch"] || [name isEqualToString:@"reopen"] ||
        [name isEqualToString:@"run"]) {
        ASApplication *app = [self applicationFor:n.a];
        if (app == nil)
            return nil;
        NSString *path = [self appPath:app.name];
        if (path == nil)
            fail(-1728, @"Can’t get application \"%@\".", app.name);
        NSRunningApplication *r = [self running:app];
        if (r == nil) {
            if (![[NSWorkspace sharedWorkspace] launchApplication:path])
                fail(-600, @"Application isn’t running.");
            for (int i = 0; i < 100 && (r = [self running:app]) == nil; i++)
                [NSThread sleepForTimeInterval:0.1];
        }
        if ([name isEqualToString:@"activate"] || [name isEqualToString:@"reopen"])
            [r activateWithOptions:NSApplicationActivateIgnoringOtherApps];
        return nil;
    }
    if ([name isEqualToString:@"quit"]) {
        ASApplication *app = [self applicationFor:n.a];
        NSRunningApplication *r = app != nil ? [self running:app] : nil;
        if (r != nil) {
            [r terminate];
            for (int i = 0; i < 100 && ![r isTerminated] && [self running:app] != nil; i++)
                [NSThread sleepForTimeInterval:0.1];
        }
        return nil;
    }
    if ([name isEqualToString:@"open"]) {
        id what = [self evaluate:n.a];
        ASApplication *app = [self applicationFor:nil];
        NSArray *items = [what isKindOfClass:[NSArray class]] ? what : @[ what ];
        for (id item in items) {
            NSString *path = [item isKindOfClass:[ASFile class]] ? [(ASFile *)item path] : [self posixFromHFS:textOf(item, NO)];
            if (app != nil && [self appPath:app.name] != nil)
                [[NSWorkspace sharedWorkspace] openFile:path withApplication:[self appPath:app.name]];
            else
                [[NSWorkspace sharedWorkspace] openFile:path];
        }
        return nil;
    }
    fail(-1708, @"«script» doesn’t understand the “%@” message.", name);
    return nil;
}

/* The command through /bin/sh: its output, without its last newline and
   with its line endings as returns (unless asked not to); a failure is an
   error with its status and standard error. */
- (id)shellScript:(NSString *)script admin:(BOOL)admin raw:(BOOL)raw
{
    int out[2], err[2];
    pid_t pid;
    posix_spawn_file_actions_t actions;
    const char *argv[] = { "/bin/sh", "-c", [script UTF8String], NULL };
    int status = 0;

    if (admin && geteuid() != 0) {
        /* Through the system's authorization (an administrator's right). */
        AuthorizationRef auth = NULL;
        AuthorizationItem item = { kAuthorizationRightExecute, 0, NULL, 0 };
        AuthorizationRights rights = { 1, &item };
        FILE *pipe = NULL;
        if (AuthorizationCreate(&rights, NULL, kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights, &auth) != errAuthorizationSuccess)
            fail(-128, @"User canceled.");
        char *const args[] = { "-c", (char *)[script UTF8String], NULL };
        OSStatus s = AuthorizationExecuteWithPrivileges(auth, "/bin/sh", kAuthorizationFlagDefaults, args, &pipe);
        AuthorizationFree(auth, kAuthorizationFlagDefaults);
        if (s != errAuthorizationSuccess)
            fail(-128, @"User canceled.");
        NSMutableData *data = [NSMutableData data];
        char buffer[4096];
        size_t got;
        while (pipe != NULL && (got = fread(buffer, 1, sizeof(buffer), pipe)) > 0)
            [data appendBytes:buffer length:got];
        if (pipe != NULL)
            fclose(pipe);
        return [self shellOutput:data raw:raw];
    }
    pipe(out);
    pipe(err);
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, out[1], 1);
    posix_spawn_file_actions_adddup2(&actions, err[1], 2);
    posix_spawn_file_actions_addclose(&actions, out[0]);
    posix_spawn_file_actions_addclose(&actions, err[0]);
    posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    if (posix_spawn(&pid, "/bin/sh", &actions, NULL, (char **)argv, environ) != 0)
        fail(-2700, @"sh: cannot run the shell.");
    posix_spawn_file_actions_destroy(&actions);
    close(out[1]);
    close(err[1]);
    NSData *outData = [[[NSFileHandle alloc] initWithFileDescriptor:out[0] closeOnDealloc:YES] readDataToEndOfFile];
    NSData *errData = [[[NSFileHandle alloc] initWithFileDescriptor:err[0] closeOnDealloc:YES] readDataToEndOfFile];
    waitpid(pid, &status, 0);
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    if (code != 0) {
        NSString *message = [[NSString alloc] initWithData:errData encoding:NSUTF8StringEncoding] ?: @"";
        message = [message stringByTrimmingCharactersInSet:[NSCharacterSet newlineCharacterSet]];
        fail(code, @"%@", [message length] ? message : [NSString stringWithFormat:@"The command exited with a non-zero status."]);
    }
    return [self shellOutput:outData raw:raw];
}

- (NSString *)shellOutput:(NSData *)data raw:(BOOL)raw
{
    NSString *text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] ?:
        [[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding];
    if ([text hasSuffix:@"\n"])
        text = [text substringToIndex:[text length] - 1];
    if (!raw)
        text = [text stringByReplacingOccurrencesOfString:@"\n" withString:@"\r"];
    return text;
}

- (id)dialog:(Node *)n
{
    NSDictionary *o = n.options;
    NSString *text = textOf([self evaluate:n.a], NO);
    NSArray *buttons = o[@"buttons"] != nil ? [self coerce:[self evaluate:o[@"buttons"]] to:@"list"] : @[ @"Cancel", @"OK" ];
    id defaultButton = o[@"default"] != nil ? [self evaluate:o[@"default"]] : (o[@"buttons"] == nil ? @"OK" : nil);
    id cancelButton = o[@"cancel"] != nil ? [self evaluate:o[@"cancel"]] : (o[@"buttons"] == nil ? @"Cancel" : nil);
    NSString *answer = o[@"answer"] != nil ? textOf([self evaluate:o[@"answer"]], NO) : nil;
    double giveUp = o[@"giveup"] != nil ? [[self coerce:[self evaluate:o[@"giveup"]] to:@"number"] doubleValue] : 0;

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    NSAlert *alert = [NSAlert new];
    if (o[@"message"] != nil) {
        alert.messageText = text;
        alert.informativeText = textOf([self evaluate:o[@"message"]], NO);
    } else {
        alert.messageText = o[@"title"] != nil ? textOf([self evaluate:o[@"title"]], NO) : @"";
        alert.informativeText = text;
        if ([alert.messageText length] == 0) {
            alert.messageText = text;
            alert.informativeText = @"";
        }
    }
    /* NSAlert puts its first button rightmost: the default goes first. */
    NSMutableArray *ordered = [NSMutableArray array];
    for (id b in buttons)
        [ordered addObject:textOf(b, NO)];
    NSString *defaultTitle = nil;
    if (isNumber(defaultButton) && [defaultButton integerValue] >= 1 && [defaultButton integerValue] <= (NSInteger)[ordered count])
        defaultTitle = ordered[[defaultButton integerValue] - 1];
    else if (defaultButton != nil)
        defaultTitle = textOf(defaultButton, NO);
    NSString *cancelTitle = nil;
    if (isNumber(cancelButton) && [cancelButton integerValue] >= 1 && [cancelButton integerValue] <= (NSInteger)[ordered count])
        cancelTitle = ordered[[cancelButton integerValue] - 1];
    else if (cancelButton != nil)
        cancelTitle = textOf(cancelButton, NO);
    NSArray *shown = [[ordered reverseObjectEnumerator] allObjects];
    if (defaultTitle != nil && [shown containsObject:defaultTitle]) {
        NSMutableArray *m = [shown mutableCopy];
        [m removeObject:defaultTitle];
        [m insertObject:defaultTitle atIndex:0];
        shown = m;
    }
    for (NSString *title in shown)
        [alert addButtonWithTitle:title];
    NSTextField *field = nil;
    if (answer != nil) {
        field = o[@"hidden"] != nil ? [[NSSecureTextField alloc] initWithFrame:NSMakeRect(0, 0, 260, 22)]
                                    : [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 260, 22)];
        field.stringValue = answer;
        alert.accessoryView = field;
    }
    __block BOOL gaveUp = NO;
    if (giveUp > 0)
        [NSTimer scheduledTimerWithTimeInterval:giveUp repeats:NO block:^(NSTimer *timer) {
            gaveUp = YES;
            [NSApp abortModal];
        }];
    [NSApp activateIgnoringOtherApps:YES];
    NSModalResponse response = [alert runModal];
    NSInteger index = response - NSAlertFirstButtonReturn;
    NSString *pressed = gaveUp ? @"" : (index >= 0 && index < (NSInteger)[shown count] ? shown[(NSUInteger)index] : @"");
    if (!gaveUp && cancelTitle != nil && [pressed isEqualToString:cancelTitle])
        fail(-128, @"User canceled.");
    ASRecord *record = [ASRecord new];
    [record setValue:pressed forName:@"button returned"];
    if (field != nil)
        [record setValue:field.stringValue forName:@"text returned"];
    if (giveUp > 0)
        [record setValue:gaveUp ? @YES : @NO forName:@"gave up"];
    return record;
}

static AudioObjectID defaultOutput(void)
{
    AudioObjectID device = 0;
    UInt32 size = sizeof(device);
    AudioObjectPropertyAddress a = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal,
                                     kAudioObjectPropertyElementMain };
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, &device) != 0)
        return 0;
    return device;
}

- (id)volumeSettings
{
    AudioObjectID device = defaultOutput();
    Float32 volume = 0;
    UInt32 muted = 0, size = sizeof(volume);
    AudioObjectPropertyAddress a = { kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput,
                                     kAudioObjectPropertyElementMain };
    ASRecord *record = [ASRecord new];
    BOOL hasVolume = device != 0 && AudioObjectGetPropertyData(device, &a, 0, NULL, &size, &volume) == 0;

    [record setValue:hasVolume ? (id)@((long long)lroundf(volume * 100)) : [NSNull null] forName:@"output volume"];
    [record setValue:[NSNull null] forName:@"input volume"];
    [record setValue:@100 forName:@"alert volume"];
    a.mSelector = kAudioDevicePropertyMute;
    size = sizeof(muted);
    BOOL hasMute = device != 0 && AudioObjectGetPropertyData(device, &a, 0, NULL, &size, &muted) == 0;
    [record setValue:hasMute ? (muted ? @YES : @NO) : (id)[NSNull null] forName:@"output muted"];
    return record;
}

- (void)setOutputVolume:(double)level
{
    AudioObjectID device = defaultOutput();
    Float32 volume = (Float32)fmax(0, fmin(1, level));
    AudioObjectPropertyAddress a = { kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput,
                                     kAudioObjectPropertyElementMain };
    if (device == 0 || AudioObjectSetPropertyData(device, &a, 0, NULL, sizeof(volume), &volume) != 0)
        fail(-2700, @"The output volume can’t be set: no sound output.");
}

- (void)setMuted:(BOOL)muted
{
    AudioObjectID device = defaultOutput();
    UInt32 value = muted;
    AudioObjectPropertyAddress a = { kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput,
                                     kAudioObjectPropertyElementMain };
    if (device == 0 || AudioObjectSetPropertyData(device, &a, 0, NULL, sizeof(value), &value) != 0)
        fail(-2700, @"The output can’t be muted: no sound output.");
}

- (id)pathTo:(NSString *)what
{
    NSString *home = NSHomeDirectory(), *path = nil;
    NSDictionary *folders = @{
        @"home folder" : home,
        @"desktop" : [home stringByAppendingPathComponent:@"Desktop"],
        @"desktop folder" : [home stringByAppendingPathComponent:@"Desktop"],
        @"documents folder" : [home stringByAppendingPathComponent:@"Documents"],
        @"downloads folder" : [home stringByAppendingPathComponent:@"Downloads"],
        @"music folder" : [home stringByAppendingPathComponent:@"Music"],
        @"pictures folder" : [home stringByAppendingPathComponent:@"Pictures"],
        @"movies folder" : [home stringByAppendingPathComponent:@"Movies"],
        @"library folder" : [home stringByAppendingPathComponent:@"Library"],
        @"preferences folder" : [home stringByAppendingPathComponent:@"Library/Preferences"],
        @"application support" : [home stringByAppendingPathComponent:@"Library/Application Support"],
        @"applications folder" : @"/Applications",
        @"system folder" : @"/System",
        @"startup disk" : @"/",
        @"temporary items" : NSTemporaryDirectory(),
        @"temporary items folder" : NSTemporaryDirectory(),
    };
    path = folders[what];
    if (path == nil)
        fail(-1728, @"Can’t get path to %@.", what);
    ASFile *file = [ASFile new];
    file.path = [path hasSuffix:@"/"] ? path : [path stringByAppendingString:@"/"];
    return file;
}

@end

#pragma mark - Showing values

static NSString *numberText(NSNumber *n)
{
    if (isReal(n)) {
        double d = [n doubleValue];
        if (floor(d) == d && fabs(d) < 1e15)
            return [NSString stringWithFormat:@"%.1f", d];
        return [NSString stringWithFormat:@"%.15g", d];
    }
    return [n stringValue];
}

/* A value as osascript prints it: readable (-s h), or as source (-s s). */
static NSString *textOf(id value, BOOL recompilable)
{
    if (value == nil || [value isKindOfClass:[NSNull class]])
        return @"missing value";
    if ([value isKindOfClass:[NSString class]]) {
        if (!recompilable)
            return value;
        NSString *escaped = [[value stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"]
            stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
        return [NSString stringWithFormat:@"\"%@\"", escaped];
    }
    if (isBoolean(value))
        return [value boolValue] ? @"true" : @"false";
    if (isNumber(value))
        return numberText(value);
    if ([value isKindOfClass:[NSArray class]]) {
        NSMutableArray *parts = [NSMutableArray array];
        for (id item in value)
            [parts addObject:textOf(item, recompilable)];
        NSString *joined = [parts componentsJoinedByString:@", "];
        return recompilable ? [NSString stringWithFormat:@"{%@}", joined] : joined;
    }
    if ([value isKindOfClass:[ASRecord class]]) {
        NSMutableArray *parts = [NSMutableArray array];
        for (NSString *k in [(ASRecord *)value keys])
            [parts addObject:[NSString stringWithFormat:@"%@:%@", k, textOf([(ASRecord *)value values][k], recompilable)]];
        NSString *joined = [parts componentsJoinedByString:@", "];
        return recompilable ? [NSString stringWithFormat:@"{%@}", joined] : joined;
    }
    if ([value isKindOfClass:[NSDate class]]) {
        NSDateFormatter *f = [NSDateFormatter new];
        f.dateFormat = @"EEEE, MMMM d, yyyy 'at' h:mm:ss a";
        f.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
        NSString *s = [f stringFromDate:value];
        return recompilable ? [NSString stringWithFormat:@"date \"%@\"", s] : [@"date " stringByAppendingString:s];
    }
    if ([value isKindOfClass:[ASFile class]]) {
        Interpreter *helper = [Interpreter new];
        return [NSString stringWithFormat:@"alias %@", recompilable ? textOf([helper hfsPath:[(ASFile *)value path]], YES)
                                                                     : [helper hfsPath:[(ASFile *)value path]]];
    }
    if ([value isKindOfClass:[ASApplication class]])
        return [NSString stringWithFormat:@"application \"%@\"", [(ASApplication *)value name]];
    return [value description];
}

#pragma mark - The command

static void usage(void)
{
    fprintf(stderr, "usage: osascript [-l language] [-e script] [-i] [-s {ehso}] [programfile] [argument ...]\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    @autoreleasepool {
        NSMutableArray *lines = [NSMutableArray array];
        NSString *language = @"AppleScript", *flags = @"";
        int i;

        for (i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-e") == 0 && i + 1 < argc)
                [lines addObject:@(argv[++i])];
            else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc)
                language = @(argv[++i]);
            else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
                flags = @(argv[++i]);
            else if (strcmp(argv[i], "-i") == 0)
                ;
            else if (strcmp(argv[i], "--") == 0) {
                i++;
                break;
            } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
                fprintf(stderr, "osascript: illegal option -- %c\n", argv[i][1]);
                usage();
            } else
                break;
        }
        if ([language caseInsensitiveCompare:@"AppleScript"] != NSOrderedSame) {
            fprintf(stderr, "osascript: %s: no such component.\n", [language UTF8String]);
            return 1;
        }
        NSString *source;
        if ([lines count] > 0)
            source = [lines componentsJoinedByString:@"\n"];
        else if (i < argc && strcmp(argv[i], "-") != 0) {
            NSError *error = nil;
            source = [NSString stringWithContentsOfFile:@(argv[i]) encoding:NSUTF8StringEncoding error:&error];
            if (source == nil) {
                fprintf(stderr, "osascript: %s: No such file or directory\n", argv[i]);
                return 1;
            }
            /* A #! line is a comment already. */
            i++;
        } else {
            NSData *data = [[NSFileHandle fileHandleWithStandardInput] readDataToEndOfFile];
            source = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] ?: @"";
            if (i < argc && strcmp(argv[i], "-") == 0)
                i++;
        }
        NSMutableArray *arguments = [NSMutableArray array];
        for (; i < argc; i++)
            [arguments addObject:@(argv[i])];

        BOOL recompilable = [flags rangeOfString:@"s"].location != NSNotFound;
        FILE *errors = [flags rangeOfString:@"o"].location != NSNotFound ? stdout : stderr;
        @try {
            Node *script = [[[Parser alloc] initWithSource:source] parseScript];
            Interpreter *interpreter = [Interpreter new];
            id result = [interpreter runScript:script arguments:arguments];
            if (result != nil && ![result isKindOfClass:[NSNull class]])
                printf("%s\n", [textOf(result, recompilable) UTF8String]);
            else if ([result isKindOfClass:[NSNull class]])
                printf("missing value\n");
        } @catch (ASError *error) {
            if (error.syntax)
                fprintf(errors, "%lu:%lu: syntax error: %s (%ld)\n", (unsigned long)error.start, (unsigned long)error.end,
                        [error.reason UTF8String], (long)error.number);
            else
                fprintf(errors, "%lu:%lu: execution error: %s (%ld)\n", 0UL, (unsigned long)[source length],
                        [error.reason UTF8String], (long)error.number);
            return 1;
        } @catch (ReturnSignal *signal) {
        }
        return 0;
    }
}
