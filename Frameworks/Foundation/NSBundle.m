/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
                 2009 Markus Hitter (mah@jump-ing.de)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSBundle.h>
#import <Foundation/NSPropertyList.h>
#import <mach-o/getsect.h>
#import <mach-o/dyld.h>
#import <Foundation/NSLocale.h>
#import <Foundation/NSMapTable.h>
#import <Foundation/NSString.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSProcessInfo.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSError.h>
#import <Foundation/NSString.h>
#import <Foundation/NSPlatform.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSUserDefaults.h>
#import <objc/runtime.h>
#import <Foundation/NSRaiseException.h>

#import <objc/objc.h>
#include <stdio.h>

typedef void *NSModuleHandle;
#if !defined(OBJC_EXPORT) && defined(OBJC_PUBLIC)
#define OBJC_EXPORT OBJC_PUBLIC
#endif
OBJC_EXPORT NSModuleHandle NSLoadModule(const char *path, NSError **error);
OBJC_EXPORT BOOL NSUnloadModule(NSModuleHandle handle);
OBJC_EXPORT const char *NSLastModuleError(void);
OBJC_EXPORT void *NSSymbolInModule(NSModuleHandle handle, const char *symbol);

#include <dlfcn.h>
#include <sys/param.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

NSModuleHandle NSLoadModule(const char *path, NSError **error) {
   NSModuleHandle handle;

    // dlopen doesn't accept partial paths.
    if (path[0] != '/' && path[0] != '.') {
        char buf[MAXPATHLEN];

        if (getcwd(buf, MAXPATHLEN) != NULL) {
            if (strlen(buf) < MAXPATHLEN - (1 + strlen(path))) {
                strncat(buf, "/", 1);
                strncat(buf, path, MAXPATHLEN - (1 + strlen(buf)));
                path = buf;
            } else {
                NSCLog("NSLoadModule: resulting path '%s/%s' exceeds MAXPATHLEN (%d)",
                        buf, path, MAXPATHLEN);
                return NULL;
            }
        } else {
          if (error != NULL) {
              *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:0 userInfo:[NSDictionary dictionaryWithObject:@"NSLoadModule: cannot find cwd and relative path specified" forKey:NSLocalizedDescriptionKey]];
          }
            return NULL;
        }
    }

    handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (handle == NULL) {
       if (error != NULL) {
           *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:0 userInfo:[NSDictionary dictionaryWithObject:[NSString stringWithFormat:@"%s", NSLastModuleError()] forKey:NSLocalizedDescriptionKey]];
       }
    }

    return handle;
}

BOOL NSUnloadModule(NSModuleHandle handle) {
   if (dlclose(handle))
       return NO;

   return YES;
}

const char *NSLastModuleError(void) {
   return dlerror();
}

void *NSSymbolInModule(NSModuleHandle handle, const char *symbol) {
   return dlsym(handle, symbol);
}

NSString * const NSBundleDidLoadNotification=@"NSBundleDidLoadNotification";
NSString * const NSLoadedClasses=@"NSLoadedClasses";

@implementation NSBundle

static NSMutableArray *_allBundles=nil;
static NSMutableArray *_allFrameworks=nil;

static NSBundle   *mainBundle=nil;
static NSMapTable *nameToBundle=NULL;
static NSMapTable *pathToObject=NULL;

-(void)_setLoaded:(BOOL)loaded {
   _isLoaded=loaded;
}


/*
  Executables support:
    MyProgram.app/Contents/<platform>/MyProgram[.exe]
   or
    MyProgram[.exe]
    MyProgram.app/Contents/
  or
    MyProgram.app/MyProgram[.exe]
    [MyProgram.app/Resources]

 */
+ (NSString *)bundlePathFromModulePath:(NSString *)path
{
    path = [path stringByStandardizingPath];

    NSString *result = nil;
    NSString *directory = [path stringByDeletingLastPathComponent];
    NSString *extension = [[path pathExtension] lowercaseString];
    NSString *name = [[path lastPathComponent] stringByDeletingPathExtension];
    NSRange version = [name rangeOfString:@"."];

    if (version.location != NSNotFound) {
        name = [name substringToIndex:version.location];
    }

    if (![extension isEqualToString:NSPlatformLoadableObjectFileExtension]) {
        // Support for MyProgram.app/MyProgram[.exe]
        if ([[directory lastPathComponent] isEqualToString:[NSString stringWithFormat:@"%@.app", name]] == YES) {
            return directory;
        }
        else {
            NSString *check = [[directory stringByAppendingPathComponent:name] stringByAppendingPathExtension:@"app"];
            
            if ([[NSFileManager defaultManager] fileExistsAtPath:check]) {
                result = check;
            } else if ([[directory lastPathComponent] isEqualToString:@"MacOS"] &&
                       [[[directory stringByDeletingLastPathComponent] lastPathComponent] isEqualToString:@"Contents"]) {
                // X.app/Contents/MacOS/X
                result = [[directory stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];
            } else {
                // Minkar: a command-line tool's bundle is its directory, as
                // on the reference platform (it was two directories up).
                result = directory;
            }
        }
    } else {
        NSString *loadablePrefix = NSPlatformLoadableObjectFilePrefix;
        NSString *check;

        if ([loadablePrefix length] > 0 && [name hasPrefix:loadablePrefix]) {
            name = [name substringFromIndex:[loadablePrefix length]];
        }
        
        if ([[directory lastPathComponent] isEqualToString:[NSString stringWithFormat:@"%@.bundle", name]] == YES) {
            return directory;
        }
        else {
            check = [[directory stringByAppendingPathComponent:name] stringByAppendingPathExtension:@"framework"];
            
            if ([[NSFileManager defaultManager] fileExistsAtPath:check]) {
                result = check;
            } else {
                check = [[[directory stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"Frameworks"] stringByAppendingPathComponent:[name stringByAppendingPathExtension:@"framework"]];
                if ([[NSFileManager defaultManager] fileExistsAtPath:check]) {
                    result = check;
                } else {
                    result = [[directory stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];
                }
            }
        }
    }
    return result;
}


+(NSBundle *)bundleWithModulePath:(NSString *)path {
   NSBundle *result;

   path=[self bundlePathFromModulePath:path];

   result=[NSBundle bundleWithPath:path];

   [result _setLoaded:YES];

   return result;
}


+ (void)registerFrameworks
{
    unsigned i, count;
    const char **array = objc_copyImageNames(&count);

    for (i = 0; i < count; i++) {
        NSString *path = [NSString stringWithUTF8String:array[i]];
        NSBundle *bundle = [NSBundle bundleWithModulePath:path];

        [_allFrameworks addObject:bundle];
    }
    free(array);
}


+ (void)initialize
{
    if (self == [NSBundle class]) {
        const char *module = getenv("CFProcessPath");
        if (!module) {
#if defined(GCC_RUNTIME_3) || defined(APPLE_RUNTIME_4)
            ssize_t bufSize = MAXPATHLEN;
            char *executablePath = alloca(bufSize + 1);
            if (_NSGetExecutablePath(executablePath, &bufSize) < 0) {
                executablePath = alloca(bufSize + 1);
                _NSGetExecutablePath(executablePath, &bufSize);
            }
            executablePath[bufSize] = 0;
            module = executablePath;
#else
            module = objc_mainImageName();
#endif
        }
        NSString *path = [NSString stringWithUTF8String:module];

        if (module == NULL) {
            NSCLog("+[NSBundle initialize]: module path for process is NULL");
        }

        _allBundles = [NSMutableArray new];
        _allFrameworks = [NSMutableArray new];
        pathToObject = NSCreateMapTable(NSObjectMapKeyCallBacks, NSObjectMapValueCallBacks, 0);
        nameToBundle = NSCreateMapTable(NSObjectMapKeyCallBacks, NSObjectMapValueCallBacks, 0);

        mainBundle = [NSBundle bundleWithModulePath:path];

        [self registerFrameworks];
    }
}


+(NSArray *)allBundles {
   return _allBundles;
}


+(NSArray *)allFrameworks {
   return _allFrameworks;
}


+(NSBundle *)mainBundle {
   return mainBundle;
}


+ (NSBundle *)bundleForClass:(Class)class
{
    NSBundle *bundle = NSMapGet(nameToBundle, NSStringFromClass(class));

    if (bundle == nil) {
#ifdef GCC_RUNTIME_3
        const char *module = NULL;
        Dl_info info;
        if (dladdr(class, &info)) {
            module = info.dli_fname;
        }
#else
        const char *module = class_getImageName(class);
#endif

        if (module == NULL) {
            return [self mainBundle]; // this is correct behaviour for Nil class
        } else {
            NSString *path = [NSString stringWithUTF8String:module];

            bundle = [NSBundle bundleWithModulePath:path];
            NSMapInsert(nameToBundle, NSStringFromClass(class), bundle);
        }
    }

    return bundle;
}


+(NSBundle *)bundleWithIdentifier:(NSString *)identifier {
   NSUnimplementedMethod();
   return 0;
}


- initWithPath:(NSString *)path
{
    NSBundle *realBundle = NSMapGet(pathToObject, path);

    if (realBundle != nil) {
        [self dealloc];
        return [realBundle retain];
    }

    _path = [[path stringByStandardizingPath] retain];
    /* Minkar: Contents/Resources (an application), Resources (a
     * framework), or else the bundle itself: a flat bundle keeps its
     * resources at its top, as the package manager's resource bundles do
     * (it was taken as having a Resources folder it lacked). */
    _resourcePath = [[_path stringByAppendingPathComponent:@"Contents"] stringByAppendingPathComponent:@"Resources"];
    if (![[NSFileManager defaultManager] fileExistsAtPath:_resourcePath]) {
        _resourcePath = [_path stringByAppendingPathComponent:@"Resources"];
        if (![[NSFileManager defaultManager] fileExistsAtPath:_resourcePath] &&
            ![[NSFileManager defaultManager] fileExistsAtPath:[_path stringByAppendingPathComponent:@"Contents"]])
            _resourcePath = _path;
    }
    [_resourcePath retain];

    _pluginPath = [[_path stringByAppendingPathComponent:@"Contents"] stringByAppendingPathComponent:@"PlugIns"];
    if (![[NSFileManager defaultManager] fileExistsAtPath:_pluginPath]) {
        _pluginPath = [_path stringByAppendingPathComponent:@"PlugIns"];
    }
    [_pluginPath retain];

    _infoDictionary = nil;
    _localizedTables = nil;
    _isLoaded = NO;

    NSMapInsert(pathToObject, path, self);
    [_allBundles addObject:self];
    return self;
}

-(void)dealloc
{
	[_path release];
	[_resourcePath release];
	[_pluginPath release];
	[_infoDictionary release];
	[_localizedTables release];
	[super dealloc];
}

+(NSBundle *)bundleWithPath:(NSString *)path {
   return [[[self allocWithZone:NULL] initWithPath:path] autorelease];
}

+(NSString *)pathForResource:(NSString *)name ofType:(NSString *)type inDirectory:(NSString *)path {
	return [[NSBundle bundleWithPath: path] pathForResource:name ofType:type];
}
+(NSArray *)pathsForResourcesOfType:(NSString *)type inDirectory:(NSString *)path {
	return [[NSBundle bundleWithPath: path] pathsForResourcesOfType:type inDirectory: nil];
}
/* The user's languages, the preferred first (AppleLanguages), or the
   current locale's language. */
static NSArray *userLanguages(void) {
   id languages=nil;

   if([NSUserDefaults standardUserDefaultsAvailable])
      languages=[[NSUserDefaults standardUserDefaults] objectForKey:@"AppleLanguages"];
   if([languages isKindOfClass:[NSString class]])
      languages=[NSArray arrayWithObject:languages];
   if([languages isKindOfClass:[NSArray class]] && [languages count]>0)
      return languages;
   NSString *language=[[NSLocale currentLocale] objectForKey:NSLocaleLanguageCode];
   return [NSArray arrayWithObject:[language length] ? language : @"en"];
}

/* "es-ES", "es_ES" and "Spanish"-style names to their language, "es". */
static NSString *baseLanguage(NSString *name) {
   static NSDictionary *names;

   if(names==nil)
      names=[[NSDictionary alloc] initWithObjectsAndKeys:@"en",@"English",@"es",@"Spanish",
         @"fr",@"French",@"de",@"German",@"it",@"Italian",@"ja",@"Japanese",nil];
   if([names objectForKey:name]!=nil)
      return [names objectForKey:name];
   NSRange separator=[name rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"-_"]];
   return [[separator.location==NSNotFound ? name : [name substringToIndex:separator.location] lowercaseString] copy];
}

/* The localizations to use, of those a bundle has, for the preferences:
   the first preference the bundle has (exactly, then by language). */
+(NSArray *)preferredLocalizationsFromArray:(NSArray *)localizations forPreferences:(NSArray *)preferences {
   if([localizations count]==0)
      return [NSArray array];
   if(preferences==nil)
      preferences=userLanguages();
   for(NSString *preference in preferences) {
      for(NSString *localization in localizations)
         if([localization caseInsensitiveCompare:preference]==NSOrderedSame)
            return [NSArray arrayWithObject:localization];
      for(NSString *localization in localizations)
         if([baseLanguage(localization) isEqualToString:baseLanguage(preference)])
            return [NSArray arrayWithObject:localization];
   }
   for(NSString *localization in localizations)
      if([baseLanguage(localization) isEqualToString:@"en"])
         return [NSArray arrayWithObject:localization];
   return [NSArray arrayWithObject:[localizations objectAtIndex:0]];
}

+(NSArray *)preferredLocalizationsFromArray:(NSArray *)localizations {
   return [self preferredLocalizationsFromArray:localizations forPreferences:nil];
}

-(NSString *)bundlePath {
   return _path;
}

-(NSString *)resourcePath {
   return _resourcePath;
}

-(NSString *)builtInPlugInsPath {
   return _pluginPath;
}

-(NSDictionary *)infoDictionary {
   if(_infoDictionary==nil){

    NSString *path=[[[_path stringByAppendingPathComponent:@"Contents"] stringByAppendingPathComponent:@"Info"] stringByAppendingPathExtension:@"plist"];

    if(![[NSFileManager defaultManager] fileExistsAtPath:path])
     path=nil;

    if(path==nil)
     path=[self pathForResource:@"Info" ofType:@"plist" inDirectory:@"Resources"];
       
    if(![[NSFileManager defaultManager] fileExistsAtPath:path])
        path=[[_path stringByAppendingPathComponent:@"Info"] stringByAppendingPathExtension:@"plist"];

    _infoDictionary=[[NSDictionary allocWithZone:NULL] initWithContentsOfFile:path];

    // A command-line tool may carry its Info.plist in the executable, in
    // its __TEXT,__info_plist section.
    if(_infoDictionary==nil && self==[NSBundle mainBundle]){
     unsigned long size=0;
     uint8_t      *bytes=getsectiondata((const struct mach_header_64 *)_dyld_get_image_header(0),"__TEXT","__info_plist",&size);

     if(bytes!=NULL && size>0){
      NSData *data=[NSData dataWithBytesNoCopy:bytes length:size freeWhenDone:NO];
      id      plist=[NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:NULL error:NULL];

      if([plist isKindOfClass:[NSDictionary class]])
       _infoDictionary=[plist retain];
     }
    }

    if(_infoDictionary==nil)
     _infoDictionary=[NSDictionary new];
   }

   return _infoDictionary;
}

/* Info.plist with the values its InfoPlist.strings has for the preferred
   localization (the app's name, its copyright) in place of its own. */
-(NSDictionary *)localizedInfoDictionary {
   static NSString *const cacheKey=@" NSLocalizedInfoDictionary";
   static NSString *const missing=@" NSMissingLocalization";
   NSDictionary        *cached;
   NSMutableDictionary *result;
   NSDictionary        *info=[self infoDictionary];

   @synchronized(self){
    cached=[[[_localizedTables objectForKey:cacheKey] retain] autorelease];
   }
   if(cached!=nil)
    return cached;

   result=[NSMutableDictionary dictionaryWithDictionary:(info!=nil)?info:[NSDictionary dictionary]];
   for(NSString *key in info){
    NSString *localized=[self localizedStringForKey:key value:missing table:@"InfoPlist"];

    if(localized!=nil && localized!=missing && ![localized isEqualToString:missing])
     [result setObject:localized forKey:key];
   }
   @synchronized(self){
    if(_localizedTables==nil)
     _localizedTables=[[NSMutableDictionary alloc] init];
    [_localizedTables setObject:result forKey:cacheKey];
   }
   return result;
}

/* The localized value when the bundle has one (InfoPlist.strings), as
   the reference platform gives it. */
-objectForInfoDictionaryKey:(NSString *)key {
   id value=[[self localizedInfoDictionary] objectForKey:key];

   return (value!=nil)?value:[[self infoDictionary] objectForKey:key];
}

-(NSString *)bundleIdentifier {
   return [[self infoDictionary] objectForKey:@"CFBundleIdentifier"];
}

-(NSString *)developmentLocalization {
   NSString *region=[[self infoDictionary] objectForKey:@"CFBundleDevelopmentRegion"];

   return [region length] ? region : @"en";
}
-(NSArray *)executableArchitectures {
   NSUnimplementedMethod();
   return 0;
}

/* The bundle's .lproj folders (Base is not a localization). */
-(NSArray *)localizations {
   NSMutableArray *result=[NSMutableArray array];

   for(NSString *name in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:[self resourcePath] error:NULL])
      if([[name pathExtension] isEqualToString:@"lproj"] && ![name isEqualToString:@"Base.lproj"])
         [result addObject:[name stringByDeletingPathExtension]];
   return result;
}

/* The localization the user's languages choose among the bundle's; the
   development one (the keys' language) when it has none of them. */
-(NSArray *)preferredLocalizations {
   NSMutableArray *available=[NSMutableArray arrayWithArray:[self localizations]];
   NSString *development=[self developmentLocalization];

   if(![available containsObject:development])
      [available addObject:development];
   return [NSBundle preferredLocalizationsFromArray:available];
}
-(NSString *)privateFrameworksPath {
   NSUnimplementedMethod();
   return 0;
}
-(NSString *)sharedFrameworksPath {
   NSUnimplementedMethod();
   return 0;
}
-(NSString *)sharedSupportPath {
   NSUnimplementedMethod();
   return 0;
}

-(NSString *)pathForAuxiliaryExecutable:(NSString *)executable {
   NSFileManager *fm=[NSFileManager defaultManager];
   NSString *path=[[self executablePath] stringByDeletingLastPathComponent];

   path=[path stringByAppendingPathComponent:executable];
   if ([fm isExecutableFileAtPath:path])
      return path;

   // Try to enhance compatibility with Unix-ish code.
   if ([NSPlatformExecutableFileExtension length]) {
      path=[path stringByAppendingPathExtension:NSPlatformExecutableFileExtension];
      if ([fm isExecutableFileAtPath:path])
         return path;
   }

   return nil;
}

-(Class)principalClass {
   NSString *name=[[self infoDictionary] objectForKey:@"NSPrincipalClass"];

   [self load];

   return (name==nil)?Nil:NSClassFromString(name);
}

-(Class)classNamed:(NSString *)className {
   [self load];

   return NSClassFromString(className);
}

-(BOOL)isLoaded {
   return _isLoaded;
}
-(BOOL)preflightAndReturnError:(NSError **)error {
   NSUnimplementedMethod();
   return 0;
}

/*
  Frameworks are organized as:
    Versions/Current/*.so
	Frameworks/MyFramework.framework/
  Bundles are organized like OS X with Contents/<operating system>
 */

-(NSString *)_findExecutable {
   NSString *type=[_path pathExtension];
   NSString *name=[[[self infoDictionary] objectForKey:@"CFBundleExecutable"] stringByDeletingPathExtension];
   NSString *checkDir;
   NSArray  *contents;
   NSInteger       i,count;

   if(name==nil)
    name=[[_path lastPathComponent] stringByDeletingPathExtension];

   if([type isEqualToString:@"framework"])
    checkDir=[[[_path stringByDeletingLastPathComponent] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"Versions"];
   else
    checkDir=[[_path stringByAppendingPathComponent:@"Contents"] stringByAppendingPathComponent:NSPlatformExecutableDirectory];

   contents=[[NSFileManager defaultManager] directoryContentsAtPath:checkDir];
   count=[contents count];

   if(count == 0) {
    checkDir=[[_path stringByAppendingPathComponent:@"Contents"] stringByAppendingPathComponent:NSPlatformAlternateExecutableDirectory];
    contents=[[NSFileManager defaultManager] directoryContentsAtPath:checkDir];
    count=[contents count];
   }

// Need to check for <name>*  versioning
   for(i=0;i<count;i++){
    NSString *check=[contents objectAtIndex:i];

    if([check hasPrefix:name]){
     NSString *ext=[check pathExtension];

     if([ext isEqualToString:NSPlatformLoadableObjectFileExtension] ||
        [ext isEqualToString:NSPlatformExecutableFileExtension])
      return [checkDir stringByAppendingPathComponent:check];
    }
   }
    if ([type isEqualToString:@"app"] == YES) {
        return [[_path stringByAppendingPathComponent:name] stringByAppendingPathExtension:NSPlatformExecutableFileExtension];
    }
    else {
        return [[_path stringByAppendingPathComponent:name] stringByAppendingPathExtension:NSPlatformLoadableObjectFileExtension];
    }
}

-(NSString *)executablePath {
	if(!_executablePath)
	{
		_executablePath=[[self _findExecutable] retain];
	}
	return _executablePath;
}

-(BOOL)load {
    NSError *error = nil;
    BOOL result = [self loadAndReturnError:&error]; 
    
    if (result == NO) {
        NSLog(@"load of %@ FAILED [%@]", [self executablePath] , [error localizedDescription]);
    }
    
    return result;
}

-(BOOL)loadAndReturnError:(NSError **)error {
	if(!_isLoaded){
		NSString *load=[self executablePath];
        
        if(NSLoadModule([load fileSystemRepresentation], error) == NULL){
            return NO;
        }
    }
	_isLoaded=YES;
    return YES;
}

-(BOOL)unload {
   NSUnimplementedMethod();
   return 0;
}

-(NSArray *)lookInDirectories {

    if (_lookInDirectories == nil) {
        NSLocale *cl = [NSLocale currentLocale];
        NSFileManager *fm = [NSFileManager defaultManager];
        NSMutableArray *validDirectories = [NSMutableArray array];

        BOOL isDirectory = NO;
        if (_resourcePath && [fm fileExistsAtPath:_resourcePath isDirectory:&isDirectory] && isDirectory) {
            // Check if there's an override on the language preference.
            NSString *localeid = nil;
            NSString *language = nil;
            NSString *longlang = nil;

            // NSUserDefaults uses the NSBundle system during initialization so to avoid a recursion
            // of doom we'll only check the preferred language once the defaults are available.
            if ([NSUserDefaults standardUserDefaultsAvailable]) {
                /* The user's languages, first the preferred, as on the
                 * reference platform (AppleLanguages: the global domain,
                 * or -AppleLanguages "(es)" for one run); "es-ES" and
                 * "es_ES" look in es-ES.lproj or es_ES.lproj, then es.lproj. */
                id languages = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleLanguages"];
                if ([languages isKindOfClass:[NSString class]])
                    languages = [NSArray arrayWithObject:languages];
                if ([languages isKindOfClass:[NSArray class]] && [languages count] > 0 &&
                    [[languages objectAtIndex:0] isKindOfClass:[NSString class]]) {
                    NSString *preferred = [languages objectAtIndex:0];
                    NSRange separator = [preferred rangeOfCharacterFromSet:
                        [NSCharacterSet characterSetWithCharactersInString:@"-_"]];
                    if (separator.location != NSNotFound) {
                        localeid = [preferred stringByAppendingPathExtension:@"lproj"];
                        language = [preferred substringToIndex:separator.location];
                    } else
                        language = preferred;
                }
                if (language == nil)
                    language = [[NSUserDefaults standardUserDefaults] objectForKey: @"PreferredLanguage"];
            }
            
            if (language == nil || [language isEqualToString: @""]) {
                // FIXME: This should be based on language preference order, and tested for presence in bundle before adding
                localeid = [[cl objectForKey:NSLocaleIdentifier] stringByAppendingPathExtension:@"lproj"];
                language = [cl objectForKey:NSLocaleLanguageCode];
                if ([language isEqualToString: @"de"])
                   longlang = @"German.lproj";
                else if ([language isEqualToString: @"fr"])
                   longlang = @"French.lproj";
                else if ([language isEqualToString: @"jp"])
                   longlang = @"Japanese.lproj";
                // anything else, including 'en', would default to en.lproj or English.lproj anyway.
            }

            NSArray *lookInDirectories = nil;
            if (localeid)
               if (longlang)
                  lookInDirectories = [NSArray arrayWithObjects:localeid, [language stringByAppendingPathExtension:@"lproj"], longlang, @"en.lproj", @"English.lproj", @"", nil];
               else
                  lookInDirectories = [NSArray arrayWithObjects:localeid, [language stringByAppendingPathExtension:@"lproj"], @"en.lproj", @"English.lproj", @"", nil];
            else if ([language isEqualToString:@"English"])
                lookInDirectories = [NSArray arrayWithObjects:@"English.lproj", @"en.lproj", @"", nil];
            else
                lookInDirectories = [NSArray arrayWithObjects:[language stringByAppendingPathExtension:@"lproj"], @"en.lproj", @"English.lproj", @"", nil];

            for (NSString *proj in lookInDirectories) {
                NSString *path = [_resourcePath stringByAppendingPathComponent:proj];
                BOOL isDirectory = NO;
                if ([fm fileExistsAtPath:path isDirectory:&isDirectory] && isDirectory) {
                    [validDirectories addObject:proj];
                }
            }

            if ([NSUserDefaults standardUserDefaultsAvailable] == YES) {
                // Now it's safe to cache
                _lookInDirectories = [validDirectories retain];
            }
        }
        return validDirectories;
    } else {
        return _lookInDirectories;
    }
}

-(NSURL *)URLForResource:(NSString *)name withExtension:(NSString *)ext
{
    NSString *path = [self pathForResource: name ofType: ext inDirectory:nil];
    
    // attention: fileURLWithPath returns a non-nil object for nil input, we don't want that
    return path ? [NSURL fileURLWithPath:path] : nil;
}

-(NSURL *)URLForResource:(NSString *)name withExtension:(NSString *)ext subdirectory:(NSString *)subpath
{
    NSString *path = [self pathForResource: name ofType: ext inDirectory: subpath];
    return [NSURL fileURLWithPath: path];
}

-(NSURL *)URLForResource:(NSString *)name withExtension:(NSString *)ext subdirectory:(NSString *)subpath localization:(NSString *)localizationName
{
    NSString *path = [self pathForResource: name ofType: ext inDirectory: subpath forLocalization: localizationName];
    return [NSURL fileURLWithPath: path];
}

-(NSString *)pathForResourceFile:(NSString *)file inDirectory:(NSString *)directory {
   NSArray  *lookIn=[self lookInDirectories];
   NSInteger i,count=[lookIn count];

   for(i=0;i<count;i++){
    NSString *path=[_resourcePath stringByAppendingPathComponent:[lookIn objectAtIndex:i]];
    if(directory!=nil)
     path=[path stringByAppendingPathComponent:directory];

    path=[path stringByAppendingPathComponent:file];

    BOOL value=[[NSFileManager defaultManager] fileExistsAtPath:path];

    if(value){
     return path;
    }
   }

   return nil;
}

-(NSString *)pathForResource:(NSString *)name ofType:(NSString *)type inDirectory:(NSString *)directory {
   NSString *file,*path;

	if ([type length] && [[name pathExtension] isEqualToString:type]) {
		// Kill the type form the extension part if it's already there
		name = [name stringByDeletingPathExtension];
	}
    
#if 0 // Disabled - we don't use that and it's increasing the number of files to check for the resource
	if(type && [type length]!=0)
    file=[[name stringByAppendingFormat:@"-%@",NSPlatformResourceNameSuffix] stringByAppendingPathExtension:type];
   else
    file=[name stringByAppendingFormat:@"-%@",NSPlatformResourceNameSuffix];

   if((path=[self pathForResourceFile:file inDirectory:directory])!=nil)
    return path;
#endif

   /* No name: the first resource of the type, as the reference does. */
   if([name length]==0){
    if([type length]==0)
     return nil;
    return [[self pathsForResourcesOfType:type inDirectory:directory] firstObject];
   }

   if(type && [type length]!=0)
    file=[name stringByAppendingPathExtension:type];
   else
	file=name;

   path=[self pathForResourceFile:file inDirectory:directory];

   return path;
}

-(NSString *)pathForResource:(NSString *)name ofType:(NSString *)type inDirectory:(NSString *)path forLocalization:(NSString *)localization {
   NSUnimplementedMethod();
   return 0;
}

-(NSString *)pathForResource:(NSString *)name ofType:(NSString *)type {
   NSString *result=[self pathForResource:name ofType:type inDirectory:nil];

   return result;
}

-(NSArray *)pathsForResourcesOfType:(NSString *)type inDirectory:(NSString *)path {
	NSMutableArray *result=[NSMutableArray array];
 	NSString       *fullPath=[self resourcePath];

    if(path!=nil)
 	 fullPath=[fullPath stringByAppendingPathComponent:path];

	NSArray  *allFiles=[[NSFileManager defaultManager] directoryContentsAtPath:fullPath];
	NSInteger i,count=[allFiles count];

	for(i=0;i<count;i++){
     NSString *check=[allFiles objectAtIndex:i];

     if([type length] == 0 || [[check pathExtension] isEqualToString:type])
      [result addObject:[fullPath stringByAppendingPathComponent:check]];
	}

   return result;
}

-(NSArray *)pathsForResourcesOfType:(NSString *)type inDirectory:(NSString *)path forLocalization:(NSString *)localization {
   NSUnimplementedMethod();
   return 0;
}

-(NSString *)localizedStringForKey:(NSString *)key value:(NSString *)value table:(NSString *)table {
   NSString     *result;
   NSDictionary *dictionary;

   if([table length]==0)
    table=@"Localizable";

// NSLocalizedString needs to be thread safe.

   @synchronized(self){
    dictionary=[_localizedTables objectForKey:table];

    if(dictionary==nil){
     NSString     *path;
     NSString     *contents=nil;

     if(_localizedTables==nil)
      _localizedTables=[[NSMutableDictionary alloc] init];

     /* A strings file is UTF-16 (with its byte order mark) or UTF-8, as on
        the reference platform; it was read in the default C string
        encoding, and accented letters came out wrong. */
     if((path=[self pathForResource:table ofType:@"strings"])!=nil) {
      NSData *data=[NSData dataWithContentsOfFile:path];
      const unsigned char *bytes=[data bytes];
      NSUInteger length=[data length];

      if(length>=2 && ((bytes[0]==0xFF && bytes[1]==0xFE) || (bytes[0]==0xFE && bytes[1]==0xFF)))
       contents=[[[NSString alloc] initWithData:data encoding:NSUnicodeStringEncoding] autorelease];
      else if(data!=nil) {
       contents=[[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
       if(contents==nil)
        contents=[[[NSString alloc] initWithData:data encoding:NSMacOSRomanStringEncoding] autorelease];
      }
     }
     if(path!=nil)
      if(contents!=nil){
       NS_DURING
        dictionary=[contents propertyListFromStringsFileFormat];
       NS_HANDLER
        dictionary=nil;
       NS_ENDHANDLER
      }

     if(dictionary==nil)
      dictionary=[NSDictionary dictionary];

     [_localizedTables setObject:dictionary forKey:table];
    }
   }

   if((result=[dictionary objectForKey:key])==nil)
	   result=(value!=nil && [value isEqual:@""] == NO)?value:key;

   return result;
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@[0x%lx] path: %@ resourcePath: %@ isLoaded: %@>", object_getClass(self), self, _path, _resourcePath, (_isLoaded ? @"YES" : @"NO")];
}

@end

NSString *NSLocalizedString(NSString *key,NSString *comment) {
   return [[NSBundle mainBundle] localizedStringForKey:key value:nil table:nil];
}

NSString *NSLocalizedStringFromTable(NSString *key,NSString *table,NSString *comment) {
   return [[NSBundle mainBundle] localizedStringForKey:key value:nil table:table];
}

NSString *NSLocalizedStringFromTableInBundle(NSString *key,NSString *table,NSBundle *bundle,NSString *comment) {
   return [bundle localizedStringForKey:key value:nil table:table];
}
