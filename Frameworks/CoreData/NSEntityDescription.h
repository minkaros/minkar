#import <Foundation/Foundation.h>
#import <CoreData/CoreDataExports.h>

@class NSManagedObject, NSManagedObjectContext, NSManagedObjectModel, NSPropertyDescription, NSAttributeDescription, NSRelationshipDescription;

NS_ASSUME_NONNULL_BEGIN

@interface NSEntityDescription : NSObject <NSCoding, NSCopying, NSFastEnumeration> {
    NSString *_className;
    NSString *_name;
    NSManagedObjectModel *_model;
    NSMutableArray *_properties;
    NSMutableArray *_subentities;
    NSEntityDescription *_superentity;
    NSDictionary *_userInfo;
    NSString *_versionHashModifier;
    NSString *_renamingIdentifier;
    NSArray *_compoundIndexes;
    NSArray *_uniquenessConstraints;
    NSDictionary *_propertiesByName;
    BOOL _isAbstract;
}

+ (nullable NSEntityDescription *)entityForName:(NSString *)entityName inManagedObjectContext:(NSManagedObjectContext *)context;
+ (__kindof NSManagedObject *)insertNewObjectForEntityForName:(NSString *)entityName inManagedObjectContext:(NSManagedObjectContext *)context;

@property (readonly, assign) NSManagedObjectModel *managedObjectModel;
@property (null_resettable, copy) NSString *managedObjectClassName;
@property (nullable, copy) NSString *name;
@property (getter=isAbstract) BOOL abstract;
@property (readonly, copy) NSDictionary<NSString *, NSEntityDescription *> *subentitiesByName;
@property (strong) NSArray<NSEntityDescription *> *subentities;
@property (nullable, readonly, assign) NSEntityDescription *superentity;
@property (readonly, copy) NSDictionary<NSString *, __kindof NSPropertyDescription *> *propertiesByName;
@property (strong) NSArray<__kindof NSPropertyDescription *> *properties;
@property (nullable, nonatomic, strong) NSDictionary *userInfo;
@property (readonly, copy) NSDictionary<NSString *, NSAttributeDescription *> *attributesByName;
@property (readonly, copy) NSDictionary<NSString *, NSRelationshipDescription *> *relationshipsByName;
@property (readonly, copy) NSData *versionHash;
@property (nullable, copy) NSString *versionHashModifier;
@property (nullable, copy) NSString *renamingIdentifier;
@property (retain) NSArray<NSArray<id> *> *compoundIndexes API_DEPRECATED("Use NSEntityDescription.indexes instead", macos(10.5, 10.13));
@property (strong) NSArray<NSArray<id> *> *uniquenessConstraints;

- (NSArray<NSRelationshipDescription *> *)relationshipsWithDestinationEntity:(NSEntityDescription *)entity;
- (BOOL)isKindOfEntity:(NSEntityDescription *)entity;

@end

NS_ASSUME_NONNULL_END
