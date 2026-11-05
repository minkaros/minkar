#import <Foundation/NSString.h>
#import <CoreData/CoreDataExports.h>

// Keys in the userInfo of a CoreData error.
COREDATA_EXPORT NSString *const NSDetailedErrorsKey;
COREDATA_EXPORT NSString *const NSValidationObjectErrorKey;
COREDATA_EXPORT NSString *const NSValidationKeyErrorKey;
COREDATA_EXPORT NSString *const NSValidationPredicateErrorKey;
COREDATA_EXPORT NSString *const NSValidationValueErrorKey;
COREDATA_EXPORT NSString *const NSAffectedStoresErrorKey;
COREDATA_EXPORT NSString *const NSAffectedObjectsErrorKey;
COREDATA_EXPORT NSString *const NSPersistentStoreSaveConflictsErrorKey;
COREDATA_EXPORT NSString *const NSSQLiteErrorDomain;

// Codes of CoreData errors, in NSCocoaErrorDomain.
enum : NSInteger {
    NSManagedObjectValidationError = 1550,
    NSManagedObjectConstraintValidationError = 1551,
    NSValidationMultipleErrorsError = 1560,
    NSValidationMissingMandatoryPropertyError = 1570,
    NSValidationRelationshipLacksMinimumCountError = 1580,
    NSValidationRelationshipExceedsMaximumCountError = 1590,
    NSValidationRelationshipDeniedDeleteError = 1600,
    NSValidationNumberTooLargeError = 1610,
    NSValidationNumberTooSmallError = 1620,
    NSValidationDateTooLateError = 1630,
    NSValidationDateTooSoonError = 1640,
    NSValidationInvalidDateError = 1650,
    NSValidationStringTooLongError = 1660,
    NSValidationStringTooShortError = 1670,
    NSValidationStringPatternMatchingError = 1680,
    NSValidationInvalidURIError = 1690,

    NSManagedObjectContextLockingError = 132000,
    NSPersistentStoreCoordinatorLockingError = 132010,

    NSManagedObjectReferentialIntegrityError = 133000,
    NSManagedObjectExternalRelationshipError = 133010,
    NSManagedObjectMergeError = 133020,
    NSManagedObjectConstraintMergeError = 133021,

    NSPersistentStoreInvalidTypeError = 134000,
    NSPersistentStoreTypeMismatchError = 134010,
    NSPersistentStoreIncompatibleSchemaError = 134020,
    NSPersistentStoreSaveError = 134030,
    NSPersistentStoreIncompleteSaveError = 134040,
    NSPersistentStoreSaveConflictsError = 134050,

    NSCoreDataError = 134060,
    NSPersistentStoreOperationError = 134070,
    NSPersistentStoreOpenError = 134080,
    NSPersistentStoreTimeoutError = 134090,
    NSPersistentStoreUnsupportedRequestTypeError = 134091,
    NSPersistentStoreIncompatibleVersionHashError = 134100,

    NSMigrationError = 134110,
    NSMigrationConstraintViolationError = 134111,
    NSMigrationCancelledError = 134120,
    NSMigrationMissingSourceModelError = 134130,
    NSMigrationMissingMappingModelError = 134140,
    NSMigrationManagerSourceStoreError = 134150,
    NSMigrationManagerDestinationStoreError = 134160,
    NSEntityMigrationPolicyError = 134170,

    NSSQLiteError = 134180,

    NSInferredMappingModelError = 134190,
    NSExternalRecordImportError = 134200,

    NSPersistentHistoryTokenExpiredError = 134301,
};
