// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// Built with ARC.

#import "WebPage.h"

@implementation _WebPage {
    NSURLSessionDataTask *_task;
    NSMutableArray *_history;
    NSInteger _historyIndex;
    NSURLRequest *_request;
    NSURL *_URL;
    NSString *_title;
    BOOL _loading;
    double _progress;
    NSUInteger _generation;
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super init])) {
        _scrollView = [[NSScrollView alloc] initWithFrame:frame];
        _scrollView.hasVerticalScroller = YES;
        _scrollView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _textView = [[NSTextView alloc] initWithFrame:_scrollView.contentView.bounds];
        _textView.editable = NO;
        _textView.selectable = YES;
        _textView.textContainerInset = NSMakeSize(8, 8);
        _textView.autoresizingMask = NSViewWidthSizable;
        _textView.delegate = self;
        _scrollView.documentView = _textView;
        _history = [NSMutableArray array];
        _historyIndex = -1;
        _drawsBackground = YES;
    }
    return self;
}

- (NSURL *)URL { return _URL; }
- (NSString *)title { return _title; }
- (NSURLRequest *)request { return _request; }
- (BOOL)isLoading { return _loading; }
- (double)estimatedProgress { return _progress; }
- (NSArray *)history { return [_history copy]; }
- (NSInteger)historyIndex { return _historyIndex; }
- (NSString *)text { return _textView.string; }

- (void)setDrawsBackground:(BOOL)draws {
    _drawsBackground = draws;
    _textView.drawsBackground = draws;
    _scrollView.drawsBackground = draws;
}

- (void)start:(NSURLRequest *)request {
    _request = [request copy];
    _loading = YES;
    _progress = 0.1;
    [self.client pageDidStartLoad:self];
}

- (void)fail:(NSError *)error {
    _loading = NO;
    _progress = 1;
    [self.client page:self didFailWithError:error];
}

/* Shows the content: HTML through the reader, text as it is, images as
   images. */
- (void)show:(NSData *)data MIMEType:(NSString *)MIMEType encodingName:(NSString *)encodingName URL:(NSURL *)URL {
    NSString *type = MIMEType.lowercaseString ?: @"";
    NSStringEncoding encoding = NSUTF8StringEncoding;
    NSAttributedString *content = nil;
    NSString *title = nil;

    if (encodingName.length > 0) {
        CFStringEncoding cf = CFStringConvertIANACharSetNameToEncoding((__bridge CFStringRef)encodingName);
        if (cf != kCFStringEncodingInvalidId)
            encoding = CFStringConvertEncodingToNSStringEncoding(cf);
    }
    if (type.length == 0 || [type isEqualToString:@"application/octet-stream"]) {
        NSString *extension = URL.pathExtension.lowercaseString;
        type = ([extension isEqualToString:@"html"] || [extension isEqualToString:@"htm"]) ? @"text/html" :
               ([@[@"png", @"jpg", @"jpeg", @"gif", @"tiff", @"bmp"] containsObject:extension]) ? @"image/*" : @"text/plain";
    }
    if ([type hasPrefix:@"text/html"] || [type isEqualToString:@"application/xhtml+xml"]) {
        NSDictionary *attributes = nil;
        content = [[NSAttributedString alloc] initWithHTML:data baseURL:URL documentAttributes:&attributes];
        title = attributes[NSTitleDocumentAttribute];
    } else if ([type hasPrefix:@"image/"]) {
        NSImage *image = [[NSImage alloc] initWithData:data];
        if (image != nil) {
            NSTextAttachment *attachment = [[NSTextAttachment alloc] init];
            NSTextAttachmentCell *cell = [[NSTextAttachmentCell alloc] initImageCell:image];
            attachment.attachmentCell = cell;
            content = [NSAttributedString attributedStringWithAttachment:attachment];
        }
        title = URL.lastPathComponent;
    }
    if (content == nil) {
        NSString *text = [[NSString alloc] initWithData:data encoding:encoding] ?: [[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding];
        content = [[NSAttributedString alloc] initWithString:text ?: @""
            attributes:@{ NSFontAttributeName: [NSFont userFixedPitchFontOfSize:12] }];
    }
    _URL = [URL copy];
    _title = [title copy];
    [self.client pageDidCommitLoad:self];
    [_textView.textStorage setAttributedString:content];
    [_textView scrollRangeToVisible:NSMakeRange(0, 0)];
    _loading = NO;
    _progress = 1;
    if (_historyIndex >= 0 && _historyIndex < (NSInteger)_history.count)
        _history[_historyIndex] = @{ @"url": _URL ?: [NSURL URLWithString:@"about:blank"], @"title": _title ?: @"" };
    [self.client pageDidFinishLoad:self];
}

- (void)record:(NSURL *)URL {
    if (_historyIndex + 1 < (NSInteger)_history.count)
        [_history removeObjectsInRange:NSMakeRange(_historyIndex + 1, _history.count - _historyIndex - 1)];
    [_history addObject:@{ @"url": URL ?: [NSURL URLWithString:@"about:blank"], @"title": @"" }];
    _historyIndex = (NSInteger)_history.count - 1;
}

- (void)loadRequest:(NSURLRequest *)request recordHistory:(BOOL)record {
    NSURL *URL = request.URL;
    NSUInteger generation = ++_generation;

    [_task cancel];
    _task = nil;
    if (record)
        [self record:URL];
    [self start:request];
    if (URL.isFileURL) {
        NSError *error = nil;
        NSData *data = [NSData dataWithContentsOfURL:URL options:0 error:&error];
        if (data == nil)
            [self fail:error];
        else
            [self show:data MIMEType:nil encodingName:nil URL:URL];
        return;
    }
    if ([URL.scheme isEqualToString:@"about"]) {
        [self show:[NSData data] MIMEType:@"text/html" encodingName:nil URL:URL];
        return;
    }
    _task = [[NSURLSession sharedSession] dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (generation != self->_generation)
                    return;
                self->_task = nil;
                if (error != nil || data == nil)
                    [self fail:error ?: [NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorUnknown userInfo:nil]];
                else
                    [self show:data MIMEType:response.MIMEType encodingName:response.textEncodingName URL:response.URL ?: URL];
            });
        }];
    _progress = 0.3;
    [_task resume];
}

- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType encodingName:(NSString *)encoding baseURL:(NSURL *)baseURL {
    ++_generation;
    [_task cancel];
    _task = nil;
    [self record:baseURL];
    [self start:[NSURLRequest requestWithURL:baseURL ?: [NSURL URLWithString:@"about:blank"]]];
    [self show:data MIMEType:MIMEType encodingName:encoding URL:baseURL];
}

- (void)stopLoading {
    ++_generation;
    [_task cancel];
    _task = nil;
    _loading = NO;
}

- (void)reload {
    if (_request != nil)
        [self loadRequest:_request recordHistory:NO];
}

- (BOOL)goToHistoryIndex:(NSInteger)index {
    if (index < 0 || index >= (NSInteger)_history.count)
        return NO;
    _historyIndex = index;
    [self loadRequest:[NSURLRequest requestWithURL:_history[index][@"url"]] recordHistory:NO];
    return YES;
}

- (BOOL)textView:(NSTextView *)textView clickedOnLink:(id)link atIndex:(NSUInteger)charIndex {
    NSURL *URL = [link isKindOfClass:[NSURL class]] ? link : [NSURL URLWithString:[link description] relativeToURL:_URL];
    if (URL == nil)
        return NO;
    if ([self.client page:self shouldFollowLink:URL.absoluteURL])
        [self loadRequest:[NSURLRequest requestWithURL:URL.absoluteURL] recordHistory:YES];
    return YES;
}
@end
