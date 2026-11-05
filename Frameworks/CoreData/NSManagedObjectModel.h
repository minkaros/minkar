/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/Foundation.h>
#import <CoreData/CoreDataExports.h>

@class NSEntityDescription, NSFetchRequest;

NS_ASSUME_NONNULL_BEGIN

@interface NSManagedObjectModel : NSObject <NSCoding, NSCopying, NSFastEnumeration> {
    NSMutableArray *_entities;
    NSMutableDictionary *_fetchRequestTemplates;
    NSSet *_versionIdentifiers;
    NSDictionary *_localizationDictionary;
    NSMutableDictionary *_configurations;
    BOOL _immutable;
}

+ (nullable NSManagedObjectModel *)mergedModelFromBundles:(nullable NSArray<NSBundle *> *)bundles;
+ (nullable NSManagedObjectModel *)modelByMergingModels:(nullable NSArray<NSManagedObjectModel *> *)models;
+ (nullable NSManagedObjectModel *)mergedModelFromBundles:(nullable NSArray<NSBundle *> *)bundles forStoreMetadata:(NSDictionary<NSString *, id> *)metadata;
+ (nullable NSManagedObjectModel *)modelByMergingModels:(NSArray<NSManagedObjectModel *> *)models forStoreMetadata:(NSDictionary<NSString *, id> *)metadata;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithContentsOfURL:(NSURL *)url;

@property (readonly, copy) NSDictionary<NSString *, NSEntityDescription *> *entitiesByName;
@property (strong) NSArray<NSEntityDescription *> *entities;
@property (readonly, strong) NSArray<NSString *> *configurations;

- (nullable NSArray<NSEntityDescription *> *)entitiesForConfiguration:(nullable NSString *)configuration;
- (void)setEntities:(NSArray<NSEntityDescription *> *)entities forConfiguration:(NSString *)configuration;
- (void)setFetchRequestTemplate:(nullable NSFetchRequest *)fetchRequestTemplate forName:(NSString *)name;
- (nullable NSFetchRequest *)fetchRequestTemplateForName:(NSString *)name;
- (nullable NSFetchRequest *)fetchRequestFromTemplateWithName:(NSString *)name substitutionVariables:(NSDictionary<NSString *, id> *)variables;

@property (nullable, strong) NSDictionary<NSString *, NSString *> *localizationDictionary;
@property (readonly, copy) NSDictionary<NSString *, NSFetchRequest *> *fetchRequestTemplatesByName;
@property (copy) NSSet *versionIdentifiers;
@property (readonly, copy) NSDictionary<NSString *, NSData *> *entityVersionHashesByName;

- (BOOL)isConfiguration:(nullable NSString *)configuration compatibleWithStoreMetadata:(NSDictionary<NSString *, id> *)metadata;

@end

NS_ASSUME_NONNULL_END
