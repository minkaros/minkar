MK_WERROR = no
FRAMEWORK = CoreData
FMWK_VERSION = A
SUBDIR =

SRCS= \
CoreDataErrors.m \
NSAtomicStore.m \
NSAtomicStoreCacheNode.m \
NSAttributeDescription.m \
NSEntityDescription.m \
NSFetchRequest.m \
NSFetchedPropertyDescription.m \
NSInMemoryPersistentStore.m \
NSManagedObject.m \
NSManagedObjectContext.m \
NSManagedObjectID.m \
NSManagedObjectModel.m \
NSManagedObjectMutableSet.m \
NSManagedObjectSet.m \
NSManagedObjectSetEnumerator.m \
NSPersistentStore.m \
NSPersistentStoreCoordinator.m \
NSPropertyDescription.m \
NSRelationshipDescription.m \
NSXMLPersistentStore.m 

HEADERS= \
CoreData.h \
CoreDataErrors.h \
CoreDataExports.h \
NSAtomicStore.h \
NSAtomicStoreCacheNode.h \
NSAttributeDescription.h \
NSEntityDescription-Private.h \
NSEntityDescription.h \
NSFetchRequest.h \
NSFetchedPropertyDescription.h \
NSInMemoryPersistentStore.h \
NSManagedObject-Private.h \
NSManagedObject.h \
NSManagedObjectContext-Private.h \
NSManagedObjectContext.h \
NSManagedObjectID-Private.h \
NSManagedObjectID.h \
NSManagedObjectModel.h \
NSManagedObjectMutableSet.h \
NSManagedObjectSet.h \
NSManagedObjectSetEnumerator.h \
NSPersistentStore.h \
NSPersistentStoreCoordinator-Private.h \
NSPersistentStoreCoordinator.h \
NSPropertyDescription.h \
NSRelationshipDescription.h \
NSXMLPersistentStore.h

RESOURCES_FILES = Info.plist
RESOURCES_DIRS = English.lproj

CFLAGS = -I${.CURDIR:H} -fblocks
LDFLAGS = -framework Foundation -framework CoreFoundation -lobjc

fmwk-install-hook:
.for dir in ${MINKAR_SDKROOT}/System/Library/Frameworks/ ${SYSROOT_DIR}/System/Library/Frameworks/
	mkdir -p ${dir}
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${dir}
.endfor

.include <rvn.framework.mk>
