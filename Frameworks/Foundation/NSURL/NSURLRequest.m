/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSMutableURLRequest.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSStream.h>
#import <Foundation/NSData.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSMutableDictionary.h>

@implementation NSURLRequest

-initWithURLRequest:(NSURLRequest *)other {
   _url=[[other URL] copy];
   _cachePolicy=[other cachePolicy];
   _timeoutInterval=[other timeoutInterval];
   
   NSData *data=[other HTTPBody];
   if(data!=nil)
    _bodyDataOrStream=[data copy];
   else
    _bodyDataOrStream=[[other HTTPBodyStream] retain];
   
   _headerFields=[[other allHTTPHeaderFields] mutableCopy];
   _method=[[other HTTPMethod] copy];
   _mainDocumentURL=[[other mainDocumentURL] copy];
   _handleCookies=[other HTTPShouldHandleCookies];
   _usePipelining=[other HTTPShouldUsePipelining];
   _allowsCellular=[other allowsCellularAccess];
   _allowsExpensive=[other allowsExpensiveNetworkAccess];
   _allowsConstrained=[other allowsConstrainedNetworkAccess];
   _serviceType=[other networkServiceType];
   return self;
}

-initWithURL:(NSURL *)url {
// Minkar: the documented default is 60 seconds (it was 4 minutes).
   return [self initWithURL:url cachePolicy:NSURLRequestUseProtocolCachePolicy timeoutInterval:60];
}

-initWithURL:(NSURL *)url cachePolicy:(NSURLRequestCachePolicy)cachePolicy timeoutInterval:(NSTimeInterval)timeout {
   _url=[url copy];
   _cachePolicy=cachePolicy;
   _timeoutInterval=timeout;
   _bodyDataOrStream=nil;
   _headerFields=[NSMutableDictionary new];
   _method=@"GET";
   _handleCookies=YES;
   _allowsCellular=YES;
   _allowsExpensive=YES;
   _allowsConstrained=YES;
   return self;
}

-init {
   return [self initWithURL:nil];
}

-(void)dealloc {
   [_url release];
   [_bodyDataOrStream release];
   [_headerFields release];
   [_method release];
   [_mainDocumentURL release];
   [super dealloc];
}

+requestWithURL:(NSURL *)url {
   return [[[self alloc] initWithURL:url] autorelease];
}

+requestWithURL:(NSURL *)url cachePolicy:(NSURLRequestCachePolicy)cachePolicy timeoutInterval:(NSTimeInterval)timeout {
   return [[[self alloc] initWithURL:url cachePolicy:cachePolicy timeoutInterval:timeout] autorelease];
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

-mutableCopyWithZone:(NSZone *)zone {
   return [[NSMutableURLRequest alloc] initWithURLRequest:self];
}

-(NSURL *)URL {
   return _url;
}

-(NSURLRequestCachePolicy)cachePolicy {
   return _cachePolicy;
}

-(NSTimeInterval)timeoutInterval {
   return _timeoutInterval;
}

-(NSString *)HTTPMethod {
   return _method;
}

-(NSData *)HTTPBody {
   if([_bodyDataOrStream isKindOfClass:[NSData class]])
    return _bodyDataOrStream;
    
   return nil;
}

-(NSInputStream *)HTTPBodyStream {
   if([_bodyDataOrStream isKindOfClass:[NSInputStream class]])
    return _bodyDataOrStream;
    
   return nil;
}

-(NSDictionary *)allHTTPHeaderFields {
   return _headerFields;
}

-(NSString *)valueForHTTPHeaderField:(NSString *)field {
   field=[field uppercaseString];
   
   return [_headerFields objectForKey:field];
}

-(NSURL *)mainDocumentURL {
   return _mainDocumentURL;
}

-(BOOL)HTTPShouldHandleCookies {
   return _handleCookies;
}

+(BOOL)supportsSecureCoding {
   return YES;
}

-(BOOL)HTTPShouldUsePipelining {
   return _usePipelining;
}

-(NSURLRequestNetworkServiceType)networkServiceType {
   return _serviceType;
}

-(BOOL)allowsCellularAccess {
   return _allowsCellular;
}

-(BOOL)allowsExpensiveNetworkAccess {
   return _allowsExpensive;
}

-(BOOL)allowsConstrainedNetworkAccess {
   return _allowsConstrained;
}

/* Keyed coding of what makes the request. */
-(void)encodeWithCoder:(NSCoder *)coder {
   [coder encodeObject:_url forKey:@"NS.url"];
   [coder encodeInteger:_cachePolicy forKey:@"NS.cachePolicy"];
   [coder encodeDouble:_timeoutInterval forKey:@"NS.timeoutInterval"];
   [coder encodeObject:_method forKey:@"NS.method"];
   [coder encodeObject:_headerFields forKey:@"NS.headers"];
   if([_bodyDataOrStream isKindOfClass:[NSData class]])
    [coder encodeObject:_bodyDataOrStream forKey:@"NS.body"];
   [coder encodeObject:_mainDocumentURL forKey:@"NS.mainDocumentURL"];
   [coder encodeBool:_handleCookies forKey:@"NS.handleCookies"];
}

-initWithCoder:(NSCoder *)coder {
   self=[self initWithURL:[coder decodeObjectOfClass:[NSURL class] forKey:@"NS.url"]
            cachePolicy:[coder decodeIntegerForKey:@"NS.cachePolicy"]
        timeoutInterval:[coder decodeDoubleForKey:@"NS.timeoutInterval"]];
   NSString *method=[coder decodeObjectOfClass:[NSString class] forKey:@"NS.method"];
   if(method!=nil){
    [_method release];
    _method=[method copy];
   }
   NSDictionary *headers=[coder decodeObjectOfClass:[NSDictionary class] forKey:@"NS.headers"];
   if(headers!=nil)
    [_headerFields setDictionary:headers];
   _bodyDataOrStream=[[coder decodeObjectOfClass:[NSData class] forKey:@"NS.body"] copy];
   _mainDocumentURL=[[coder decodeObjectOfClass:[NSURL class] forKey:@"NS.mainDocumentURL"] copy];
   _handleCookies=[coder decodeBoolForKey:@"NS.handleCookies"];
   return self;
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@ %x: url=%@>",object_getClass(self),self,_url];
}

@end
