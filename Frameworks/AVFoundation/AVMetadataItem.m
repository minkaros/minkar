/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Metadata items. An identifier is its key space and its key joined by a
 * slash ("comn" items are "common/..."); items read from a file are
 * loaded already.
 */

#import <AVFoundation/AVMetadataItem.h>

AVMetadataFormat const AVMetadataFormatQuickTimeUserData = @"com.apple.quicktime.udta";
AVMetadataFormat const AVMetadataFormatQuickTimeMetadata = @"com.apple.quicktime.mdta";
AVMetadataFormat const AVMetadataFormatiTunesMetadata = @"com.apple.itunes";
AVMetadataFormat const AVMetadataFormatID3Metadata = @"org.id3";
AVMetadataFormat const AVMetadataFormatISOUserData = @"org.mp4ra";
AVMetadataFormat const AVMetadataFormatHLSMetadata = @"com.apple.quicktime.HLS";
AVMetadataFormat const AVMetadataFormatUnknown = @"com.apple.AVFoundation.metadataformat.unknown";

AVMetadataKeySpace const AVMetadataKeySpaceCommon = @"comn";
AVMetadataKeySpace const AVMetadataKeySpaceQuickTimeUserData = @"udta";
AVMetadataKeySpace const AVMetadataKeySpaceISOUserData = @"uiso";
AVMetadataKeySpace const AVMetadataKeySpaceQuickTimeMetadata = @"mdta";
AVMetadataKeySpace const AVMetadataKeySpaceiTunes = @"itsk";
AVMetadataKeySpace const AVMetadataKeySpaceID3 = @"org.id3";
AVMetadataKeySpace const AVMetadataKeySpaceIcy = @"icy";
AVMetadataKeySpace const AVMetadataKeySpaceHLSDateRange = @"lsdr";

#define COMMON(name, key) \
    AVMetadataKey const AVMetadataCommonKey##name = @key; \
    AVMetadataIdentifier const AVMetadataCommonIdentifier##name = @"common/" key;
COMMON(Title, "title")
COMMON(Creator, "creator")
COMMON(Subject, "subject")
COMMON(Description, "description")
COMMON(Publisher, "publisher")
COMMON(Contributor, "contributor")
COMMON(CreationDate, "creationDate")
COMMON(LastModifiedDate, "lastModifiedDate")
COMMON(Type, "type")
COMMON(Format, "format")
COMMON(Source, "source")
COMMON(Language, "language")
COMMON(Relation, "relation")
COMMON(Location, "location")
COMMON(Copyrights, "copyrights")
COMMON(AlbumName, "albumName")
COMMON(Author, "author")
COMMON(Artist, "artist")
COMMON(Artwork, "artwork")
COMMON(Make, "make")
COMMON(Model, "model")
COMMON(Software, "software")
AVMetadataKey const AVMetadataCommonKeyIdentifier = @"identifier";
AVMetadataIdentifier const AVMetadataCommonIdentifierAssetIdentifier = @"common/identifier";

#define QT(name, key) \
    AVMetadataKey const AVMetadataQuickTimeMetadataKey##name = @"com.apple.quicktime." key;
#define QTID(name, key) \
    QT(name, key) \
    AVMetadataIdentifier const AVMetadataIdentifierQuickTimeMetadata##name = @"mdta/com.apple.quicktime." key;
QTID(Title, "title")
QTID(Artist, "artist")
QTID(Album, "album")
QTID(Description, "description")
QTID(Comment, "comment")
QTID(Copyright, "copyright")
QTID(CreationDate, "creationdate")
QTID(Genre, "genre")
QTID(Software, "software")
QTID(Author, "author")
QT(Keywords, "keywords")
QT(LocationISO6709, "location.ISO6709")

AVMetadataExtraAttributeKey const AVMetadataExtraAttributeValueURIKey = @"valueURI";
AVMetadataExtraAttributeKey const AVMetadataExtraAttributeBaseURIKey = @"baseURI";

/* The common key each format's own key (FFmpeg's names, lowercase, and
   the QuickTime metadata ones) stands for. */
static AVMetadataKey commonKeyFor(id key, AVMetadataKeySpace keySpace) {
    if (![key isKindOfClass:[NSString class]])
        return nil;
    if ([keySpace isEqual:AVMetadataKeySpaceCommon])
        return key;
    NSString *k = [key lowercaseString];
    if ([k hasPrefix:@"com.apple.quicktime."])
        k = [k substringFromIndex:20];
    static NSDictionary *map;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        map = @{
            @"title": AVMetadataCommonKeyTitle, @"artist": AVMetadataCommonKeyArtist,
            @"album": AVMetadataCommonKeyAlbumName, @"album_artist": AVMetadataCommonKeyAuthor,
            @"author": AVMetadataCommonKeyAuthor, @"composer": AVMetadataCommonKeyCreator,
            @"comment": AVMetadataCommonKeyDescription, @"description": AVMetadataCommonKeyDescription,
            @"genre": AVMetadataCommonKeyType, @"copyright": AVMetadataCommonKeyCopyrights,
            @"date": AVMetadataCommonKeyCreationDate, @"creation_time": AVMetadataCommonKeyCreationDate,
            @"creationdate": AVMetadataCommonKeyCreationDate, @"encoder": AVMetadataCommonKeySoftware,
            @"software": AVMetadataCommonKeySoftware, @"publisher": AVMetadataCommonKeyPublisher,
            @"language": AVMetadataCommonKeyLanguage, @"location": AVMetadataCommonKeyLocation,
            @"location.iso6709": AVMetadataCommonKeyLocation, @"make": AVMetadataCommonKeyMake,
            @"model": AVMetadataCommonKeyModel, @"subject": AVMetadataCommonKeySubject,
        };
    });
    return map[k];
}

@implementation AVMetadataItem {
@protected
    AVMetadataIdentifier _identifier;
    NSString *_extendedLanguageTag;
    NSLocale *_locale;
    CMTime _time, _duration;
    NSString *_dataType;
    id<NSObject, NSCopying> _value;
    NSDictionary *_extraAttributes;
    id<NSObject, NSCopying> _key;
    AVMetadataKeySpace _keySpace;
    NSDate *_startDate;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _time = kCMTimeInvalid;
        _duration = kCMTimeInvalid;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    AVMetadataItem *c = [[AVMetadataItem allocWithZone:zone] init];
    [self _copyInto:c];
    return c;
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    AVMutableMetadataItem *c = [[AVMutableMetadataItem allocWithZone:zone] init];
    [self _copyInto:c];
    return c;
}

- (void)_copyInto:(AVMetadataItem *)c {
    c->_identifier = _identifier;
    c->_extendedLanguageTag = _extendedLanguageTag;
    c->_locale = _locale;
    c->_time = _time;
    c->_duration = _duration;
    c->_dataType = _dataType;
    c->_value = _value;
    c->_extraAttributes = _extraAttributes;
    c->_key = _key;
    c->_keySpace = _keySpace;
    c->_startDate = _startDate;
}

- (AVMetadataIdentifier)identifier {
    if (_identifier != nil)
        return _identifier;
    return _key != nil && _keySpace != nil ? [AVMetadataItem identifierForKey:_key keySpace:_keySpace] : nil;
}

- (id<NSObject, NSCopying>)key {
    return _key != nil ? _key : (_identifier != nil ? [AVMetadataItem keyForIdentifier:_identifier] : nil);
}

- (AVMetadataKeySpace)keySpace {
    return _keySpace != nil ? _keySpace : (_identifier != nil ? [AVMetadataItem keySpaceForIdentifier:_identifier] : nil);
}

- (AVMetadataKey)commonKey {
    return commonKeyFor(self.key, self.keySpace);
}

- (NSString *)extendedLanguageTag {
    return _extendedLanguageTag != nil ? _extendedLanguageTag : _locale.localeIdentifier;
}

- (NSLocale *)locale {
    return _locale;
}

- (CMTime)time {
    return _time;
}

- (CMTime)duration {
    return _duration;
}

- (NSString *)dataType {
    if (_dataType != nil)
        return _dataType;
    if ([(id)_value isKindOfClass:[NSString class]])
        return @"com.apple.metadata.datatype.UTF-8";
    if ([(id)_value isKindOfClass:[NSData class]])
        return @"com.apple.metadata.datatype.raw-data";
    if ([(id)_value isKindOfClass:[NSDate class]])
        return @"com.apple.metadata.datatype.ISO-8601";
    if ([(id)_value isKindOfClass:[NSNumber class]])
        return @"com.apple.metadata.datatype.int64";
    return nil;
}

- (id<NSObject, NSCopying>)value {
    return _value;
}

- (NSDictionary *)extraAttributes {
    return _extraAttributes;
}

- (NSDate *)startDate {
    return _startDate;
}

- (NSString *)stringValue {
    id v = _value;
    if ([v isKindOfClass:[NSString class]])
        return v;
    if ([v isKindOfClass:[NSNumber class]])
        return [v stringValue];
    if ([v isKindOfClass:[NSDate class]]) {
        NSISO8601DateFormatter *f = [[NSISO8601DateFormatter alloc] init];
        return [f stringFromDate:v];
    }
    return nil;
}

- (NSNumber *)numberValue {
    id v = _value;
    if ([v isKindOfClass:[NSNumber class]])
        return v;
    if ([v isKindOfClass:[NSString class]]) {
        NSScanner *s = [NSScanner scannerWithString:v];
        double d;
        if ([s scanDouble:&d] && s.isAtEnd)
            return @(d);
    }
    return nil;
}

/* Dates as files write them: ISO 8601 in full, a day, or a year. */
- (NSDate *)dateValue {
    id v = _value;
    if ([v isKindOfClass:[NSDate class]])
        return v;
    if (![v isKindOfClass:[NSString class]])
        return nil;
    NSString *s = [v stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    NSISO8601DateFormatter *f = [[NSISO8601DateFormatter alloc] init];
    f.formatOptions = NSISO8601DateFormatWithInternetDateTime | NSISO8601DateFormatWithFractionalSeconds;
    NSDate *d = [f dateFromString:s];
    if (d == nil) {
        f.formatOptions = NSISO8601DateFormatWithInternetDateTime;
        d = [f dateFromString:s];
    }
    if (d == nil) {
        NSDateFormatter *df = [[NSDateFormatter alloc] init];
        df.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
        df.timeZone = [NSTimeZone timeZoneForSecondsFromGMT:0];
        for (NSString *format in @[ @"yyyy-MM-dd'T'HH:mm:ss", @"yyyy-MM-dd HH:mm:ss", @"yyyy-MM-dd", @"yyyy" ]) {
            df.dateFormat = format;
            if ((d = [df dateFromString:s]) != nil)
                break;
        }
    }
    return d;
}

- (NSData *)dataValue {
    return [(id)_value isKindOfClass:[NSData class]] ? (NSData *)_value : nil;
}

- (AVKeyValueStatus)statusOfValueForKey:(NSString *)key error:(NSError **)outError {
    return AVKeyValueStatusLoaded;
}

- (void)loadValuesAsynchronouslyForKeys:(NSArray<NSString *> *)keys completionHandler:(void (^)(void))handler {
    if (handler != nil)
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), handler);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p identifier=%@ value=%@>", [self class], (void *)self, self.identifier,
                                      _value];
}

+ (AVMetadataIdentifier)identifierForKey:(id)key keySpace:(AVMetadataKeySpace)keySpace {
    if (key == nil || keySpace == nil)
        return nil;
    NSString *k = [key isKindOfClass:[NSString class]] ? key : [key description];
    NSString *space = [keySpace isEqual:AVMetadataKeySpaceCommon] ? @"common" : keySpace;
    return [NSString stringWithFormat:@"%@/%@", space, k];
}

+ (AVMetadataKeySpace)keySpaceForIdentifier:(AVMetadataIdentifier)identifier {
    NSRange slash = [identifier rangeOfString:@"/"];
    if (slash.location == NSNotFound)
        return nil;
    NSString *space = [identifier substringToIndex:slash.location];
    return [space isEqual:@"common"] ? AVMetadataKeySpaceCommon : space;
}

+ (id)keyForIdentifier:(AVMetadataIdentifier)identifier {
    NSRange slash = [identifier rangeOfString:@"/"];
    return slash.location == NSNotFound ? nil : [identifier substringFromIndex:NSMaxRange(slash)];
}

+ (NSArray<AVMetadataItem *> *)metadataItemsFromArray:(NSArray<AVMetadataItem *> *)items withKey:(id)key
                                             keySpace:(AVMetadataKeySpace)keySpace {
    NSMutableArray *r = [NSMutableArray array];
    for (AVMetadataItem *i in items) {
        BOOL keyMatches = key == nil || [i.key isEqual:key] ||
                          ([keySpace isEqual:AVMetadataKeySpaceCommon] && [i.commonKey isEqual:key]);
        BOOL spaceMatches = keySpace == nil || [i.keySpace isEqual:keySpace] ||
                            ([keySpace isEqual:AVMetadataKeySpaceCommon] && i.commonKey != nil);
        if (keyMatches && spaceMatches)
            [r addObject:i];
    }
    return r;
}

+ (NSArray<AVMetadataItem *> *)metadataItemsFromArray:(NSArray<AVMetadataItem *> *)items
                                 filteredByIdentifier:(AVMetadataIdentifier)identifier {
    NSMutableArray *r = [NSMutableArray array];
    for (AVMetadataItem *i in items)
        if ([i.identifier isEqual:identifier])
            [r addObject:i];
    return r;
}

+ (NSArray<AVMetadataItem *> *)metadataItemsFromArray:(NSArray<AVMetadataItem *> *)items withLocale:(NSLocale *)locale {
    NSMutableArray *r = [NSMutableArray array];
    for (AVMetadataItem *i in items)
        if (i.locale == nil || [i.locale.localeIdentifier isEqual:locale.localeIdentifier])
            [r addObject:i];
    return r;
}

/* The items in the first preferred language any has, with those that have
   no language; all of them when none matches. */
+ (NSArray<AVMetadataItem *> *)metadataItemsFromArray:(NSArray<AVMetadataItem *> *)items
             filteredAndSortedAccordingToPreferredLanguages:(NSArray<NSString *> *)languages {
    for (NSString *language in languages) {
        NSString *wanted = [[language componentsSeparatedByString:@"-"] firstObject];
        NSMutableArray *r = [NSMutableArray array];
        BOOL any = NO;
        for (AVMetadataItem *i in items) {
            NSString *tag = i.extendedLanguageTag;
            NSString *base = [[tag componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"-_"]]
                firstObject];
            if (tag == nil)
                [r addObject:i];
            else if ([tag isEqual:language] || [base isEqual:wanted]) {
                [r insertObject:i atIndex:0];
                any = YES;
            }
        }
        if (any)
            return r;
    }
    return items;
}

@end

@implementation AVMutableMetadataItem

+ (AVMutableMetadataItem *)metadataItem {
    return [[self alloc] init];
}

- (id)copyWithZone:(NSZone *)zone {
    AVMetadataItem *c = [[AVMetadataItem allocWithZone:zone] init];
    [self _copyInto:c];
    return c;
}

- (void)setIdentifier:(AVMetadataIdentifier)identifier {
    _identifier = [identifier copy];
    if (identifier != nil) {
        _key = [AVMetadataItem keyForIdentifier:identifier];
        _keySpace = [AVMetadataItem keySpaceForIdentifier:identifier];
    }
}

- (void)setExtendedLanguageTag:(NSString *)tag {
    _extendedLanguageTag = [tag copy];
}

- (void)setLocale:(NSLocale *)locale {
    _locale = [locale copy];
}

- (void)setTime:(CMTime)time {
    _time = time;
}

- (void)setDuration:(CMTime)duration {
    _duration = duration;
}

- (void)setDataType:(NSString *)dataType {
    _dataType = [dataType copy];
}

- (void)setValue:(id<NSObject, NSCopying>)value {
    _value = [(id)value copy];
}

- (void)setExtraAttributes:(NSDictionary *)extraAttributes {
    _extraAttributes = [extraAttributes copy];
}

- (void)setKey:(id<NSObject, NSCopying>)key {
    _key = [(id)key copy];
    _identifier = nil;
}

- (void)setKeySpace:(AVMetadataKeySpace)keySpace {
    _keySpace = [keySpace copy];
    _identifier = nil;
}

- (void)setStartDate:(NSDate *)startDate {
    _startDate = [startDate copy];
}

@end
