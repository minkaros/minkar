// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to ConcreteEntityWithCustomClass1.swift instead.

import Foundation
import CoreData

public enum ConcreteEntityWithCustomClass1Attributes: String {
    case dummy2 = "dummy2"
}

open class _ConcreteEntityWithCustomClass1: NSManagedObject {

    // MARK: - Class methods

    open class func entityName () -> String {
        return "ConcreteEntityWithCustomClass1"
    }

    open class func entity(managedObjectContext: NSManagedObjectContext) -> NSEntityDescription? {
        return NSEntityDescription.entity(forEntityName: self.entityName(), in: managedObjectContext)
    }

    @nonobjc
    open class func fetchRequest() -> NSFetchRequest<ConcreteEntityWithCustomClass1> {
        return NSFetchRequest(entityName: self.entityName())
    }

    // MARK: - Life cycle methods

    public override init(entity: NSEntityDescription, insertInto context: NSManagedObjectContext?) {
        super.init(entity: entity, insertInto: context)
    }

    public convenience init?(managedObjectContext: NSManagedObjectContext) {
        guard let entity = _ConcreteEntityWithCustomClass1.entity(managedObjectContext: managedObjectContext) else { return nil }
        self.init(entity: entity, insertInto: managedObjectContext)
    }

    // MARK: - Properties

    @NSManaged open
    var dummy2: NSNumber?

    // MARK: - Relationships

}

