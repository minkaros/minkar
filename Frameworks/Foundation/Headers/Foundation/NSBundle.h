/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSArray.h>

@class NSDictionary<KeyType, ObjectType>, NSString, NSError, NSMutableDictionary<KeyType, ObjectType>, NSURL, NSNumber;

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSString *const NSBundleDidLoadNotification;
FOUNDATION_EXPORT NSString *const NSLoadedClasses;

@interface NSBundle : NSObject {
    NSString *_path;
    NSString *_resourcePath;
    NSString *_pluginPath;
    NSDictionary *_infoDictionary;
    NSString *_executablePath;
    NSArray *_lookInDirectories;
    NSMutableDictionary *_localizedTables;
    BOOL _isLoaded;
}

@property (class, readonly, copy) NSArray<NSBundle *> *allBundles;
@property (class, readonly, copy) NSArray<NSBundle *> *allFrameworks;

@property (class, readonly, strong) NSBundle *mainBundle;

+ (NSBundle *)bundleForClass:(Class)aClass;
+ (nullable NSBundle *)bundleWithIdentifier:(NSString *)identifier;
+ (nullable instancetype)bundleWithPath:(NSString *)path;
+ (nullable NSBundle *)bundleWithModulePath:(NSString *)path;

+ (nullable NSString *)pathForResource:(nullable NSString *)name ofType:(nullable NSString *)type inDirectory:(NSString *)path;
+ (NSArray<NSString *> *)pathsForResourcesOfType:(nullable NSString *)type inDirectory:(NSString *)path;
+ (NSArray<NSString *> *)preferredLocalizationsFromArray:(NSArray<NSString *> *)localizations;
+ (NSArray<NSString *> *)preferredLocalizationsFromArray:(NSArray<NSString *> *)localizations forPreferences:(nullable NSArray<NSString *> *)preferences;

- (nullable instancetype)initWithPath:(NSString *)path;

@property (readonly, copy) NSString *bundlePath;
@property (nullable, readonly, copy) NSString *resourcePath;
@property (nullable, readonly, copy) NSString *builtInPlugInsPath;
@property (nullable, readonly, copy) NSDictionary<NSString *, id> *infoDictionary;
@property (nullable, readonly, copy) NSDictionary<NSString *, id> *localizedInfoDictionary;
- (nullable id)objectForInfoDictionaryKey:(NSString *)key;
@property (nullable, readonly, copy) NSString *bundleIdentifier;
@property (nullable, readonly, copy) NSString *developmentLocalization;
@property (nullable, readonly, copy) NSArray<NSNumber *> *executableArchitectures;
@property (nullable, readonly, copy) NSString *executablePath;
@property (readonly, copy) NSArray<NSString *> *localizations;
@property (readonly, copy) NSArray<NSString *> *preferredLocalizations;
@property (nullable, readonly, copy) NSString *privateFrameworksPath;
@property (nullable, readonly, copy) NSString *sharedFrameworksPath;
@property (nullable, readonly, copy) NSString *sharedSupportPath;

- (nullable NSString *)pathForAuxiliaryExecutable:(NSString *)executable;

@property (nullable, readonly) Class principalClass;
- (nullable Class)classNamed:(NSString *)className;

@property (readonly, getter=isLoaded) BOOL loaded;
- (BOOL)preflightAndReturnError:(NSError **)error;

- (BOOL)load;
- (BOOL)loadAndReturnError:(NSError **)error;
- (BOOL)unload;

- (nullable NSURL *)URLForResource:(nullable NSString *)name withExtension:(nullable NSString *)ext;
- (nullable NSURL *)URLForResource:(nullable NSString *)name withExtension:(nullable NSString *)ext subdirectory:(nullable NSString *)subpath;
- (nullable NSURL *)URLForResource:(nullable NSString *)name withExtension:(nullable NSString *)ext subdirectory:(nullable NSString *)subpath localization:(nullable NSString *)localizationName;

- (nullable NSString *)pathForResource:(nullable NSString *)name ofType:(nullable NSString *)type;
- (nullable NSString *)pathForResource:(nullable NSString *)name ofType:(nullable NSString *)type inDirectory:(nullable NSString *)path;
- (nullable NSString *)pathForResource:(nullable NSString *)name ofType:(nullable NSString *)type inDirectory:(nullable NSString *)path forLocalization:(nullable NSString *)localization;

- (NSArray<NSString *> *)pathsForResourcesOfType:(nullable NSString *)type inDirectory:(nullable NSString *)path;
- (NSArray<NSString *> *)pathsForResourcesOfType:(nullable NSString *)type inDirectory:(nullable NSString *)path forLocalization:(nullable NSString *)localization;

- (NSString *)localizedStringForKey:(NSString *)key value:(nullable NSString *)value table:(nullable NSString *)table;

@end

@interface NSBundle (NSURLResources)
+ (nullable instancetype)bundleWithURL:(NSURL *)url;
- (nullable instancetype)initWithURL:(NSURL *)url;
@property (readonly, copy) NSURL *bundleURL;
@property (nullable, readonly, copy) NSURL *resourceURL;
@property (nullable, readonly, copy) NSURL *executableURL;
@property (nullable, readonly, copy) NSURL *privateFrameworksURL;
@property (nullable, readonly, copy) NSURL *sharedFrameworksURL;
@property (nullable, readonly, copy) NSURL *sharedSupportURL;
@property (nullable, readonly, copy) NSURL *builtInPlugInsURL;
@property (nullable, readonly, copy) NSURL *appStoreReceiptURL;
- (nullable NSURL *)URLForAuxiliaryExecutable:(NSString *)executableName;
- (nullable NSArray<NSURL *> *)URLsForResourcesWithExtension:(nullable NSString *)ext subdirectory:(nullable NSString *)subpath;
- (nullable NSArray<NSURL *> *)URLsForResourcesWithExtension:(nullable NSString *)ext subdirectory:(nullable NSString *)subpath
                                       localization:(nullable NSString *)localizationName;
+ (nullable NSURL *)URLForResource:(nullable NSString *)name withExtension:(nullable NSString *)ext subdirectory:(nullable NSString *)subpath
          inBundleWithURL:(NSURL *)bundleURL;
+ (nullable NSArray<NSURL *> *)URLsForResourcesWithExtension:(nullable NSString *)ext subdirectory:(nullable NSString *)subpath
                                    inBundleWithURL:(NSURL *)bundleURL;
@end


NS_ASSUME_NONNULL_END

FOUNDATION_EXPORT NSString *NSLocalizedString(NSString *key, NSString *comment);
FOUNDATION_EXPORT NSString *NSLocalizedStringFromTable(NSString *key, NSString *table, NSString *comment);
FOUNDATION_EXPORT NSString *NSLocalizedStringFromTableInBundle(NSString *key, NSString *table, NSBundle *bundle, NSString *comment);
