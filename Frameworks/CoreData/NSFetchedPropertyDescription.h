#import <CoreData/NSPropertyDescription.h>

@class NSFetchRequest;

NS_ASSUME_NONNULL_BEGIN

@interface NSFetchedPropertyDescription : NSPropertyDescription {
    NSFetchRequest *_fetchRequest;
}

@property (nullable, retain) NSFetchRequest *fetchRequest;

@end

NS_ASSUME_NONNULL_END
