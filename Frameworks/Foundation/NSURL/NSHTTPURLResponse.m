/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSHTTPURLResponse.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSString.h>

@implementation NSHTTPURLResponse

static NSString *valueInHeaders(NSDictionary *headers,NSString *key){
   NSEnumerator *state=[headers keyEnumerator];
   NSString     *checkKey;
   
   key=[key lowercaseString];
   
   while((checkKey=[state nextObject])!=nil){
    if([[checkKey lowercaseString] isEqualToString:key])
     return [headers objectForKey:checkKey];
   }
   
   return nil;
}

// Minkar: the MIME type and the text encoding come from Content-Type
// ("text/html; charset=utf-8"), and the length is -1 when unknown.
static NSString *trimmed(NSString *string){
   return [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}

static NSString *mimeTypeFromContentType(NSString *contentType){
   if(contentType==nil)
    return nil;
   NSString *type=trimmed([[contentType componentsSeparatedByString:@";"] objectAtIndex:0]);
   return [type length]>0?[type lowercaseString]:nil;
}

static NSString *textEncodingNameFromContentType(NSString *contentType){
   NSArray *parts=[contentType componentsSeparatedByString:@";"];
   NSUInteger i;

   for(i=1;i<[parts count];i++){
    NSString *part=trimmed([parts objectAtIndex:i]);
    NSRange   equal=[part rangeOfString:@"="];

    if(equal.location==NSNotFound)
     continue;
    if([trimmed([part substringToIndex:equal.location]) caseInsensitiveCompare:@"charset"]!=NSOrderedSame)
     continue;
    NSString *value=trimmed([part substringFromIndex:NSMaxRange(equal)]);
    if([value length]>=2 && [value hasPrefix:@"\""] && [value hasSuffix:@"\""])
     value=[value substringWithRange:NSMakeRange(1,[value length]-2)];
    return [value length]>0?value:nil;
   }
   return nil;
}

-initWithURL:(NSURL *)url statusCode:(NSInteger)statusCode HTTPVersion:(NSString *)HTTPVersion headerFields:(NSDictionary *)headers {
   NSString *contentType=valueInHeaders(headers,@"content-type");
   NSString *contentLength=valueInHeaders(headers,@"content-length");
   long long length=-1;

   if(contentLength!=nil){
    length=[trimmed(contentLength) longLongValue];
    if(length<0)
     length=-1;
   }

   if((self=[super initWithURL:url MIMEType:mimeTypeFromContentType(contentType) expectedContentLength:(NSInteger)length textEncodingName:textEncodingNameFromContentType(contentType)])==nil)
    return nil;

   _statusCode=statusCode;
   _allHeaderFields=[(headers?headers:[NSDictionary dictionary]) copy];
   return self;
}

-initWithURL:(NSURL *)url statusCode:(NSInteger)statusCode headers:(NSDictionary *)headers {
   return [self initWithURL:url statusCode:statusCode HTTPVersion:nil headerFields:headers];
}

-(void)dealloc {
   [_allHeaderFields release];
   [super dealloc];
}

// The reason phrases, in lower case.
+(NSString *)localizedStringForStatusCode:(NSInteger)statusCode {
   switch(statusCode){
    case 100: return @"continue";
    case 101: return @"switching protocols";
    case 200: return @"no error";
    case 201: return @"created";
    case 202: return @"accepted";
    case 203: return @"non-authoritative information";
    case 204: return @"no content";
    case 205: return @"reset content";
    case 206: return @"partial content";
    case 300: return @"multiple choices";
    case 301: return @"moved permanently";
    case 302: return @"found";
    case 303: return @"see other";
    case 304: return @"not modified";
    case 305: return @"use proxy";
    case 307: return @"temporarily redirected";
    case 308: return @"permanent redirect";
    case 400: return @"bad request";
    case 401: return @"unauthorized";
    case 402: return @"payment required";
    case 403: return @"forbidden";
    case 404: return @"not found";
    case 405: return @"method not allowed";
    case 406: return @"unacceptable";
    case 407: return @"proxy authentication required";
    case 408: return @"request timed out";
    case 409: return @"conflict";
    case 410: return @"no longer exists";
    case 411: return @"length required";
    case 412: return @"precondition failed";
    case 413: return @"request too large";
    case 414: return @"requested URL too long";
    case 415: return @"unsupported media type";
    case 416: return @"requested range not satisfiable";
    case 417: return @"expectation failed";
    case 429: return @"too many requests";
    case 500: return @"internal server error";
    case 501: return @"unimplemented";
    case 502: return @"bad gateway";
    case 503: return @"service unavailable";
    case 504: return @"gateway timed out";
    case 505: return @"unsupported version";
   }
   if(statusCode>=100 && statusCode<200) return @"informational";
   if(statusCode>=200 && statusCode<300) return @"success";
   if(statusCode>=300 && statusCode<400) return @"redirected";
   if(statusCode>=400 && statusCode<500) return @"client error";
   if(statusCode>=500 && statusCode<600) return @"server error";
   return @"unknown";
}

-(NSDictionary *)allHeaderFields {
   return _allHeaderFields;
}

-(NSInteger)statusCode {
   return _statusCode;
}

// Header names are not case sensitive.
-(NSString *)valueForHTTPHeaderField:(NSString *)field {
   if(field==nil)
    return nil;
   return valueInHeaders(_allHeaderFields,field);
}

@end
