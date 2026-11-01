/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/Foundation.h>
#import <Foundation/NSKeyedUnarchiver.h>
#import <Foundation/NSKeyedArchiver.h>
#import <Foundation/NSRaise.h>

NSString * const NSURLFileScheme=@"file";

/* RFC 1808, no IPV6

   RFC1808 says ? is part of net_loc prior to / abs_path
   Apple treats ? in net_loc as a query without /, seems reasonable
 */

@implementation NSURL

typedef struct {
   NSString *original;
   NSInteger length;
   unichar  *unicode;
   NSInteger position;
   unichar  *part;
   NSInteger partLength;
   NSInteger partPosition;
} urlScanner;

static void initScanner(urlScanner *scanner,NSString *string){
   scanner->original=string;
   scanner->length=[string length];
   scanner->unicode=NSZoneMalloc(NULL,sizeof(unichar)*scanner->length);
   [string getCharacters:scanner->unicode];
   scanner->position=0;
   scanner->part=NSZoneMalloc(NULL,sizeof(unichar)*scanner->length);
   scanner->partLength=0;
   scanner->partPosition=0;
}

static void deallocScanner(urlScanner *scanner){
   NSZoneFree(NULL,scanner->unicode);
   NSZoneFree(NULL,scanner->part);
}

static void beginPartScan(urlScanner *scanner){
   scanner->partLength=0;
   scanner->partPosition=scanner->position;
}

static BOOL resetPartScan(urlScanner *scanner){
   scanner->position=scanner->partPosition;
   return NO;
}

static BOOL more_characters(urlScanner *scanner){
   return (scanner->position<scanner->length)?YES:NO;
}

static int peekCharacter(urlScanner *scanner){
   if(!more_characters(scanner))
    return -1;

   return scanner->unicode[scanner->position];
}

static int nextCharacter(urlScanner *scanner){
   if(!more_characters(scanner))
    return -1;
    
   return scanner->unicode[scanner->position++];
}

static void backupCharacter(urlScanner *scanner){
   if(scanner->position==0)
    [NSException raise:@"NSURLInternalErrorException" format:@"scanning before string in backupCharacter()"];

   scanner->position--;
}

static void internPartCharacter(urlScanner *scanner,unichar code){
   scanner->part[scanner->partLength++]=code;
}

static void consume_next(urlScanner *scanner){
   internPartCharacter(scanner,nextCharacter(scanner));
}

static NSString *allocPart(urlScanner *scanner){
	if (scanner->partLength > 0) return [[NSString alloc] initWithCharacters:scanner->part length:scanner->partLength];
	else return nil;
}

static BOOL consume_reserved(urlScanner *scanner){
   BOOL result=NO;
   
   while(more_characters(scanner)){
    unichar check=peekCharacter(scanner);
    
    if((check==';') || (check=='/') || (check=='?') || (check==':') || (check=='@') || (check=='&') || (check=='=')){
     result=YES;
     consume_next(scanner);
    }
    else
     break;
   }
   
   return result;
}

static BOOL consume_extra(urlScanner *scanner){
   BOOL result=NO;
   
   while(more_characters(scanner)){
    unichar check=peekCharacter(scanner);
    
    if((check=='!') || (check=='*') || (check=='\'') || (check=='(') || (check==')') || (check==',')){
     result=YES;
     consume_next(scanner);
    }
    else
     break;
   }
   
   return result;
}

static BOOL consume_safe(urlScanner *scanner){
   BOOL result=NO;
   
   while(more_characters(scanner)){
    unichar check=peekCharacter(scanner);
    
    if((check=='$') || (check=='-') || (check=='_') || (check=='.') || (check=='+')){
     result=YES;
     consume_next(scanner);
    }
    else
     break;
   }
   
   return result;
}

static BOOL consume_digit(urlScanner *scanner){
   BOOL result=NO;
   
   while(more_characters(scanner)){
    unichar check=peekCharacter(scanner);
    
    if(check>='0' && check<='9'){
     result=YES;
     consume_next(scanner);
    }
    else
     break;
   }
   
   return result;
}

static BOOL consume_alpha(urlScanner *scanner){
   BOOL result=NO;
   
   while(more_characters(scanner)){
    unichar check=peekCharacter(scanner);
    
    if((check>='A' && check<='Z') || (check>='a' && check<='z')){
     result=YES;
     consume_next(scanner);
    }
    else
     break;
   }
   
   return result;
}

static BOOL isHex(unichar code){   
   if(code>='a' && code<='f')
    return YES;
   if(code>='A' && code<='F')
    return YES;
   if(code>='0' && code<='9')
    return YES;

   return NO;
}

static BOOL consume_escape_hex(urlScanner *scanner){
   if(!more_characters(scanner))
    return NO;
   if(!isHex(peekCharacter(scanner)))
    return NO;
   
   consume_next(scanner);

   if(!more_characters(scanner))
    return NO;
   if(!isHex(peekCharacter(scanner)))
    return NO;

   consume_next(scanner);

   return YES;
}


static BOOL consume_unreserved(urlScanner *scanner){
   BOOL result=NO;

   while(more_characters(scanner)){
    BOOL alpha=consume_alpha(scanner);
    BOOL digit=consume_digit(scanner);
    BOOL safe=consume_safe(scanner);
    BOOL extra=consume_extra(scanner);
    
    if(alpha || digit || safe || extra)
     result=YES;
    else
     break;
   }
   return result;
}

static BOOL consume_uchar(urlScanner *scanner){
   BOOL result=NO;
 
   while(more_characters(scanner)){
    if(consume_unreserved(scanner))
     result=YES;
    else {
     if(peekCharacter(scanner)=='%'){
      consume_next(scanner);
      if(!consume_escape_hex(scanner))
       return NO;
      else
       result=YES;
     }
     else
      break;
    }
   }
   
   return result;
}

static BOOL consume_pchar(urlScanner *scanner){
   BOOL result=NO;
   
   while(more_characters(scanner)){
    if(consume_uchar(scanner))
     result=YES;
    
    unichar code=peekCharacter(scanner);
    if(code==':' || code=='@' || code=='&' || code=='='){
     result=YES;
     consume_next(scanner);
    }
    else
     break;
   }
   
   return result;
}

static void scan_fragment(urlScanner *scanner,NSURL *url){
   if(peekCharacter(scanner)!='#')
    return;
    
   nextCharacter(scanner);
   
   beginPartScan(scanner);
   
   while(more_characters(scanner)){
    BOOL uchar=consume_uchar(scanner);
    BOOL reserved=consume_reserved(scanner);

    if(!(uchar || reserved))
     break;
   }
   
   url->_fragment=allocPart(scanner);
}

static void scan_query(urlScanner *scanner,NSURL *url){
   if(peekCharacter(scanner)!='?')
    return;
    
   nextCharacter(scanner);
   
   beginPartScan(scanner);
   
   while(more_characters(scanner)){
    BOOL uchar=consume_uchar(scanner);
    BOOL reserved=consume_reserved(scanner);
    
    if(!(uchar || reserved))
     break;
   }
   
   url->_query=allocPart(scanner);
}

static void scan_net_loc(urlScanner *scanner,NSURL *url){
   if(peekCharacter(scanner)!='/')
    return;
    
   nextCharacter(scanner);
   if(peekCharacter(scanner)!='/'){
    backupCharacter(scanner);
    return;
   }
   nextCharacter(scanner);

   beginPartScan(scanner);

   while(more_characters(scanner)){
    consume_pchar(scanner);
    
    unichar code=peekCharacter(scanner);
 
    if(code==';')
     consume_next(scanner);
    else
     break;
   }

// split into user:pw@host:port
   NSInteger hostEnd=scanner->partLength;
   int host=0;
   int login;
   
   for(login=0;login<scanner->partLength;login++)
    if(scanner->part[login]=='@')
     break;
   
   if(login<scanner->partLength){
    host=login+1;

    int user;
    
    for(user=0;user<login;user++)
     if(scanner->part[user]==':')
      break;
      
    url->_user=[[NSString alloc] initWithCharacters:scanner->part length:user];
    if(user<login)
     url->_password=[[NSString alloc] initWithCharacters:scanner->part+user+1 length:login-(user+1)];
   }

   NSInteger portEnd=scanner->partLength;   
   int port;
   
   for(port=host;port<scanner->partLength;port++)
    if(scanner->part[port]==':')
     break;
   
   if(port<scanner->partLength){
    hostEnd=port;
    
    port++;
    NSString  *string=[[NSString alloc] initWithCharacters:scanner->part+port length:portEnd-port];
    NSScanner *portScanner=[[NSScanner alloc] initWithString:string];
    int        portNumber;
    
    if([portScanner scanInt:&portNumber] && [portScanner isAtEnd])
     url->_port=[[NSNumber alloc] initWithInt:portNumber];

    [portScanner release];
    [string release];
   }
   
   if(hostEnd-host>0)
    url->_host=[[NSString alloc] initWithCharacters:scanner->part+host length:hostEnd-host];
}

static BOOL scan_scheme(urlScanner *scanner,NSURL *url){
   beginPartScan(scanner);

   while(more_characters(scanner)){
    BOOL alpha=consume_alpha(scanner);
    BOOL digit=consume_digit(scanner);
    
    if(!(alpha || digit)){
     unichar check=peekCharacter(scanner);
     
     if((check=='+') || (check=='-') || (check=='.'))
      consume_next(scanner);
     else if(check==':'){
      nextCharacter(scanner);
      url->_scheme=allocPart(scanner);
      return YES;
     }
     else
      break;
    }
   }
   
   resetPartScan(scanner);
   return NO;
}

static void scan_params(urlScanner *scanner,NSURL *url){
   if(!more_characters(scanner))
    return;
    
   if(peekCharacter(scanner)!=';')
    return;
   
   nextCharacter(scanner);
   
   beginPartScan(scanner);

   while(more_characters(scanner)){
    consume_pchar(scanner);

    unichar check=peekCharacter(scanner);
    if((check=='/') || (check==';'))
     consume_next(scanner);
    else
     break;
   }

   url->_parameter=allocPart(scanner);
}

static void scan_rel_path(urlScanner *scanner,NSURL *url){
   beginPartScan(scanner);

   while(more_characters(scanner)){
    consume_pchar(scanner);

    unichar check=peekCharacter(scanner);
    if(check=='/')
     consume_next(scanner);
    else
     break;
   }

   url->_path=allocPart(scanner);
   
   scan_params(scanner,url);
   scan_query(scanner,url);
}

static void scan_abs_path(urlScanner *scanner,NSURL *url){
   scan_rel_path(scanner,url);
}

static void scan_net_path(urlScanner *scanner,NSURL *url){
   scan_net_loc(scanner,url);
   scan_abs_path(scanner,url);
}

static BOOL scanURL(urlScanner *scanner,NSURL *url){
   scan_scheme(scanner,url);
   scan_net_path(scanner,url);
   scan_fragment(scanner,url);

   return more_characters(scanner)?NO:YES;
}

-initWithScheme:(NSString *)scheme host:(NSString *)host path:(NSString *)path {
   _scheme=[scheme copy];
   _host=[host copy];
   _path=[path copy];
   return self;
}

// Minkar: as the API makes them: "file:///a%20b/" (no "localhost"), the
// path percent-encoded, a directory's ending with "/", and a relative path
// taken from the current directory (it was made absolute by a "/").
-initFileURLWithPath:(NSString *)path isDirectory:(BOOL)isDirectory {
   if (path == nil)
    [NSException raise:NSInvalidArgumentException format:@"-[%@ %s] nil path",object_getClass(self),sel_getName(_cmd)];
   if (![path hasPrefix:@"/"]) {
    NSString *cwd = [[NSFileManager defaultManager] currentDirectoryPath];
    path = [path length] ? [cwd stringByAppendingPathComponent:path] : cwd;
   }
   if (isDirectory && ![path hasSuffix:@"/"])
    path = [path stringByAppendingString:@"/"];
   NSString *encoded = [path stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]];
   return [self initWithScheme:NSURLFileScheme host:@"" path:encoded ? encoded : path];
}

// A directory is known by asking the file system.
-initFileURLWithPath:(NSString *)path {
   BOOL isDirectory = NO;
   NSString *absolute = path;
   if (path != nil && ![path hasPrefix:@"/"])
    absolute = [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:path];
   if (path != nil && ![path hasSuffix:@"/"] &&
       ![[NSFileManager defaultManager] fileExistsAtPath:absolute isDirectory:&isDirectory])
    isDirectory = NO;
   return [self initFileURLWithPath:path isDirectory:isDirectory];
}

-initWithString:(NSString *)string {
   return [self initWithString:string relativeToURL:nil];
}

-initWithString:(NSString *)string relativeToURL:(NSURL *)parent {
   urlScanner scannerStruct,*scanner=&scannerStruct;

   if(string==nil){
    [NSException raise:NSInvalidArgumentException format:@"-[%@ %s] string == nil",object_getClass(self),sel_getName(_cmd)];
   }
   
   initScanner(scanner,string);

   if(!scanURL(scanner,self)){
    [self dealloc];
    return nil;
   }
   
   _baseURL=[parent copy];
   _string=[string copy];
   
   deallocScanner(scanner);
   return self;
}

+fileURLWithPath:(NSString *)path {
   return [[[self alloc] initFileURLWithPath:path] autorelease];
}

+URLWithString:(NSString *)string {
   return [[[self alloc] initWithString:string] autorelease];
}

+URLWithString:(NSString *)string relativeToURL:(NSURL *)parent {
   return [[[self alloc] initWithString:string relativeToURL:parent] autorelease];
}

-(void)dealloc {
   [_baseURL release];
   [_string release];
   [_scheme release];
   [_host release];
   [_user release];
   [_password release];
   [_fragment release];
   [_path release];
   [_port release];
   [_query release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

-initWithCoder:(NSCoder *)coder {
   if([coder allowsKeyedCoding]){
    NSKeyedUnarchiver *keyed=(NSKeyedUnarchiver *)coder;
    NSString          *rel=[keyed decodeObjectForKey:@"NS.relative"];
    
    // Minkar: file URLs too ("file:///..."; older archives have
    // "file://localhost/...").
    [self initWithString:rel relativeToURL:[keyed decodeObjectForKey:@"NS.base"]];
    
    return self;
   }
   else {
    NSLog(@"NSURL only supports keyed unarchiving");
   [self release];
   return nil;
   }
}

-(void)encodeWithCoder:(NSCoder *)coder {
   if([coder isKindOfClass:[NSKeyedArchiver class]]){
     NSKeyedArchiver *keyed=(NSKeyedArchiver *)coder;
     NSString        *rel=_baseURL ? [self relativeString] : [self absoluteString];
     
     if (_baseURL)  [keyed encodeObject:_baseURL forKey:@"NS.base"];
     if (rel)       [keyed encodeObject:rel forKey:@"NS.relative"];
   }
   else {
    NSLog(@"NSURL only supports keyed archiving");
   }
}


// Minkar: two URLs are equal when their strings are (fields compared one
// by one told "file:///x" made from a path from the same URL read back).
-(NSUInteger)hash {
	return [[self absoluteString] hash];
}

-(BOOL)isEqual:other {
	if (self == other) return YES;
	if (![other isKindOfClass:[NSURL class]])
		return NO;
	return [[self absoluteString] isEqualToString:[other absoluteString]];
}

-(NSString *)_baseHost  {
   if (!_scheme && !_host)
    return [_baseURL _baseHost];
    
   else return _host;
}

// Minkar: the port goes with the host: a relative URL without a host of
// its own has the base URL's port (it was lost from absoluteString).
-(NSNumber *)_basePort  {
   if (!_scheme && !_host)
    return [_baseURL _basePort];

   return _port;
}

-(NSString *)_baseUser  {
   if (!_user && !_host && !_scheme )
     return [_baseURL _baseUser];
     
   else return _user;
}

-(NSString *)_basePassword  {
	if (!_password && !_user && !_host && !_scheme)
      return [_baseURL _basePassword];
	
    return _password;
}

static NSString *NormalizePath( NSString *path )
{
	NSArray *components = [path componentsSeparatedByString: @"/"];
	NSMutableArray *actualComponents = [NSMutableArray arrayWithCapacity: [components count]];
	
	NSString *lastComponent = [components lastObject];
	if (![lastComponent isEqualToString: @".."]) components = [components subarrayWithRange: NSMakeRange( 0, [components count] - 1 )];
	
	for (NSString *part in components) {
		if ([part isEqualToString: @".."]) {
			if ([actualComponents count] > 0 && ![[actualComponents lastObject] isEqualToString: @".."]) [actualComponents removeLastObject];
			else [actualComponents addObject: part];
		} else if (![part isEqualToString: @"."] && ![part isEqualToString: @""]) {
			[actualComponents addObject: part];
		}
	}
	
	if ([lastComponent isEqualToString: @"."] || [lastComponent isEqualToString: @".."]) {
		[actualComponents addObject: @""];
	} else {
		[actualComponents addObject: lastComponent];
	}
	
	return [actualComponents componentsJoinedByString: @"/"];	
}

-(NSString *)_basePath 
{
	NSString *result = _path;

	if (!_host && !_scheme && _baseURL && ![_path hasPrefix: @"/"]) {
		result = [_baseURL _basePath];

		if (_path ) {
			if (![result hasSuffix:@"/"]) result = [result stringByDeletingLastPathComponent];
			result = [result stringByAppendingFormat: @"/%@", _path];    
		}
		
		result = NormalizePath( result );
		
		result = [@"/" stringByAppendingString: result];
	} 

	return result;
}

static void AppendValueWithPrefix( NSMutableString *orig, NSString *prefix, NSString *value )
{
	if (nil != value) {
		[orig appendString: prefix];
		[orig appendString: value];
	}
}

static NSMutableString *AssembleResourceSpecifier( NSMutableString *result, NSString *host, NSNumber *port, NSString *user, NSString *password, NSString *path, NSString *parameterString, NSString * query, NSString *fragment )
{
	if (host) {
		[result appendString:@"//"];
		if (user) {
			[result appendString: user];
			if (password) {
				[result appendString: @":"];
				[result appendString: password];
			}
			[result appendString: @"@"];
		}
		[result appendString: host];
		if (port) {
			[result appendString: @":"];
			[result appendString: [port stringValue]];
		}
	}
	
	[result appendString: path];
	
	AppendValueWithPrefix( result, @";", parameterString );
	AppendValueWithPrefix( result, @"?", query );
	AppendValueWithPrefix( result, @"#", fragment );
	
	return result;
}

-(NSString *)_buildResourceSpecifier 
{
	NSString *host = [self _baseHost];
	NSString *user = [self _baseUser];
	NSString *password = [self _basePassword];
	NSString *path = [self _basePath];
	NSString *parameterString = [self parameterString];
	NSString *query = [self query];
	NSString *fragment = [self fragment];
	
	return AssembleResourceSpecifier( [NSMutableString string], host, [self _basePort], user, password, path, parameterString, query, fragment );
}

-(NSString *)absoluteString 
{
   if(_scheme && _string) {
	   return _string;   
   } else {
	   NSMutableString *result = [NSMutableString string];
   
	   NSString *scheme = [self scheme];
	   if(scheme){
		   [result appendString: scheme];
		   [result appendString: @":"];
	   }
	   [result appendString: [self _buildResourceSpecifier]];

	   return result;
   }
}

-(NSString *)parameterString 
{
	if (!_scheme && !_host && !_path && !_parameter) return [_baseURL parameterString];
	else return _parameter;
}

-propertyForKey:(NSString *)key {
   NSUnimplementedMethod();
   return nil;
}

-(NSString *)scheme 
{
	if (!_scheme) return [_baseURL scheme];
	else return _scheme;
}

-(NSString *)host {
   NSString *host = [self _baseHost];
   return [host length] ? host : nil;
}

-(NSString *)user {
   return [self _baseUser];
}

-(NSString *)password {
   return [self _basePassword];
}

-(NSString *)fragment  {
	if (!_fragment && !_query && !_parameter && !_path && !_scheme && !_host)
      return [_baseURL fragment];
	else return _fragment;
}

// Minkar: without percent escapes, as the API has it.
-(NSString *)path {
   NSString *result=[self _basePath];
   NSString *decoded=[result stringByRemovingPercentEncoding];

   if(decoded!=nil)
    result=decoded;
   
   if([result length]>1 && [result hasSuffix:@"/"])
    result=[result substringToIndex:[result length]-1];

   return result;
}

-(NSNumber *)port {
   return [self _basePort];
}

-(NSString *)query 
{
	if (!_scheme && !_host && !_path && !_parameter && !_query) return [_baseURL query];
	else return _query;
}

-(NSString *)relativePath {
   NSString *result=[_path stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];

   if([result length]>1 && [result hasSuffix:@"/"])
    result=[result substringToIndex:[result length]-1];
   
   return result;
}

-(NSString *)relativeString {
   return _string;
}

-(NSString *)resourceSpecifier 
{
   if (_baseURL && !_scheme ) return _string;
   else return [self _buildResourceSpecifier];
}

-(BOOL)isFileURL {
   return [_scheme isEqualToString:NSURLFileScheme];
}

-(NSURL *)standardizedURL {
   NSUnimplementedMethod();
   return nil;
}

- (NSString *)description;
{
	return [self absoluteString];
}

-(NSURL *)absoluteURL {
   if(_baseURL==nil)
    return self;

   return [NSURL URLWithString:[self absoluteString]];
}

-(NSURL *)baseURL {
   return _baseURL;
}

-(BOOL)setProperty:property forKey:(NSString *)key {
   NSUnimplementedMethod();
   return NO;
}

-(BOOL)setResourceData:(NSData *)data {
   NSUnimplementedMethod();
   return NO;
}

-(NSData *)resourceDataUsingCache:(BOOL)useCache {
   NSUnimplementedMethod();
   return nil;
}

-(NSURLHandle *)URLHandleUsingCache:(BOOL)useCache {
   NSUnimplementedMethod();
   return nil;
}

-(void)loadResourceDataNotifyingClient:client usingCache:(BOOL)useCache {
   NSUnimplementedMethod();
}


// Minkar: these work on the path (they worked on the whole string, query
// and percent escapes included), and keep the rest of the URL.
- (NSURL *)_URLWithPath:(NSString *)path isDirectory:(BOOL)isDirectory
{
    if ([self isFileURL])
        return [NSURL fileURLWithPath:path isDirectory:isDirectory];

    if (isDirectory && ![path hasSuffix:@"/"])
        path = [path stringByAppendingString:@"/"];
    NSMutableString *string = [NSMutableString string];
    NSString *encoded = [path stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]];
    if ([self scheme])
        [string appendFormat:@"%@:", [self scheme]];
    if ([self host]) {
        [string appendString:@"//"];
        if ([self user])
            [string appendFormat:@"%@%@@", [self user], [self password] ? [@":" stringByAppendingString:[self password]] : @""];
        [string appendString:[self host]];
        if ([self port])
            [string appendFormat:@":%@", [self port]];
    }
    [string appendString:encoded ? encoded : path];
    if ([self query])
        [string appendFormat:@"?%@", [self query]];
    if ([self fragment])
        [string appendFormat:@"#%@", [self fragment]];
    return [NSURL URLWithString:string];
}

- (BOOL)hasDirectoryPath
{
    return [[self _basePath] hasSuffix:@"/"];
}

- (NSURL *)URLByAppendingPathComponent:(NSString *)pathComponent
{
    BOOL isDirectory = [pathComponent hasSuffix:@"/"];
    if (!isDirectory && [self isFileURL])
        [[NSFileManager defaultManager] fileExistsAtPath:[[self path] stringByAppendingPathComponent:pathComponent] isDirectory:&isDirectory];
    return [self URLByAppendingPathComponent:pathComponent isDirectory:isDirectory];
}

- (NSURL *)URLByAppendingPathExtension:(NSString *)pathExtension
{
    return [self _URLWithPath:[[self path] stringByAppendingPathExtension:pathExtension] isDirectory:[self hasDirectoryPath]];
}

- (NSURL *)URLByDeletingLastPathComponent
{
    NSString *path = [self path];
    NSString *parent = [path stringByDeletingLastPathComponent];
    if ([parent length] == 0)
        parent = [path hasPrefix:@"/"] ? @"/" : @"../";
    return [self _URLWithPath:parent isDirectory:YES];
}

- (NSURL *)URLByDeletingPathExtension
{
    return [self _URLWithPath:[[self path] stringByDeletingPathExtension] isDirectory:[self hasDirectoryPath]];
}

- (NSString *)lastPathComponent
{
    return [[self path] lastPathComponent];
}

- (NSString *)pathExtension
{
    return [[self path] pathExtension];
}

@end

@implementation NSURL (NSURLPathUtilities)

+ (NSURL *)fileURLWithPath:(NSString *)path isDirectory:(BOOL)isDirectory {
    return [[[self alloc] initFileURLWithPath:path isDirectory:isDirectory] autorelease];
}

+ (NSURL *)fileURLWithPathComponents:(NSArray *)components {
    return [NSURL fileURLWithPath:[NSString pathWithComponents:components]];
}

- (NSArray *)pathComponents {
    return [[self path] pathComponents];
}

- (NSURL *)URLByAppendingPathComponent:(NSString *)pathComponent isDirectory:(BOOL)isDirectory {
    NSString *path = [self path];
    if ([path length] == 0)
        path = @"/";
    return [self _URLWithPath:[path stringByAppendingPathComponent:pathComponent] isDirectory:isDirectory];
}

- (NSURL *)URLByStandardizingPath {
    return [NSURL fileURLWithPath:[[self path] stringByStandardizingPath]];
}

- (NSURL *)URLByResolvingSymlinksInPath {
    return [NSURL fileURLWithPath:[[self path] stringByResolvingSymlinksInPath]];
}

@end
