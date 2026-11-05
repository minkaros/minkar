// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to ConcreteEntityWithCustomClass2.swift instead.

import Foundation
import CoreData

public enum ConcreteEntityWithCustomClass2Attributes: String {
    case dummy3 = "dummy3"
}

open class _ConcreteEntityWithCustomClass2: NSManagedObject {

    // MARK: - Class methods

    open class func entityName () -> String {
        return "ConcreteEntityWithCustomClass2"
    }

    open class func entity(managedObjectContext: NSManagedObjectContext) -> NSEntityDescription? {
        return NSEntityDescription.entity(forEntityName: self.entityName(), in: managedObjectContext)
    }

    @nonobjc
    open class func fetchRequest() -> NSFetchRequest<ConcreteEntityWithCustomClass2> {
        return NSFetchRequest(entityName: self.entityName())
    }

    // MARK: - Life cycle methods

    public override init(entity: NSEntityDescription, insertInto context: NSManagedObjectContext?) {
        super.init(entity: entity, insertInto: context)
    }

    public convenience init?(managedObjectContext: NSManagedObjectContext) {
        guard let entity = _ConcreteEntityWithCustomClass2.entity(managedObjectContext: managedObjectContext) else { return nil }
        self.init(entity: entity, insertInto: managedObjectContext)
    }

    // MARK: - Properties

    @NSManaged open
    var dummy3: NSNumber?

    // MARK: - Relationships

}

