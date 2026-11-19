/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFAction.h>
#import <PDFKit/PDFActionGoTo.h>
#import <PDFKit/PDFActionURL.h>
#import <PDFKit/PDFActionNamed.h>
#import <PDFKit/PDFDestination.h>

@implementation PDFAction

- (NSString *)type {
    return @"";
}

- (id)copyWithZone:(NSZone *)zone {
    return [[[self class] allocWithZone:zone] init];
}

@end

@implementation PDFActionGoTo

- (instancetype)initWithDestination:(PDFDestination *)destination {
    if ((self = [super init]))
        _destination = destination;
    return self;
}

- (NSString *)type {
    return @"GoTo";
}

- (id)copyWithZone:(NSZone *)zone {
    return [[PDFActionGoTo allocWithZone:zone] initWithDestination:[_destination copy]];
}

@end

@implementation PDFActionURL

- (instancetype)initWithURL:(NSURL *)url {
    if ((self = [super init]))
        _URL = [url copy];
    return self;
}

- (NSString *)type {
    return @"URI";
}

- (id)copyWithZone:(NSZone *)zone {
    return [[PDFActionURL allocWithZone:zone] initWithURL:_URL];
}

@end

@implementation PDFActionNamed

- (instancetype)initWithName:(PDFActionNamedName)name {
    if ((self = [super init]))
        _name = name;
    return self;
}

- (NSString *)type {
    return @"Named";
}

- (id)copyWithZone:(NSZone *)zone {
    return [[PDFActionNamed allocWithZone:zone] initWithName:_name];
}

@end
