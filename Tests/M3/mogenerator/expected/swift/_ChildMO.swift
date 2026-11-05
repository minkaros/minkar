// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to ChildMO.swift instead.

import Foundation
import CoreData

public enum ChildMOAttributes: String {
    case childName = "childName"
    case type = "type"
}

public enum ChildMORelationships: String {
    case parent = "parent"
}

open class _ChildMO: HumanMO {

    // MARK: - Class methods

    override open class func entityName () -> String {
        return "Child"
    }

    override open class func entity(managedObjectContext: NSManagedObjectContext) -> NSEntityDescription? {
        return NSEntityDescription.entity(forEntityName: self.entityName(), in: managedObjectContext)
    }

    @nonobjc
    open class func fetchRequest() -> NSFetchRequest<ChildMO> {
        return NSFetchRequest(entityName: self.entityName())
    }

    // MARK: - Life cycle methods

    public override init(entity: NSEntityDescription, insertInto context: NSManagedObjectContext?) {
        super.init(entity: entity, insertInto: context)
    }

    public convenience init?(managedObjectContext: NSManagedObjectContext) {
        guard let entity = _ChildMO.entity(managedObjectContext: managedObjectContext) else { return nil }
        self.init(entity: entity, insertInto: managedObjectContext)
    }

    // MARK: - Properties

    @NSManaged open
    var childName: String?

    @NSManaged open
    var type: NSNumber?

    // MARK: - Relationships

    @NSManaged open
    var parent: ParentMO?

    class func fetchByParent(managedObjectContext: NSManagedObjectContext, parent: ParentMO) -> [Any]? {
        return self.fetchByParent(managedObjectContext: managedObjectContext, parent: parent, error: nil)
    }

    class func fetchByParent(managedObjectContext: NSManagedObjectContext, parent: ParentMO, error outError: NSErrorPointer) -> [Any]? {
        guard let psc = managedObjectContext.persistentStoreCoordinator else { return nil }
        let model = psc.managedObjectModel
        let substitutionVariables : [String : Any] = [
            "parent": parent,
]

        guard let fetchRequest = model.fetchRequestFromTemplate(withName: "byParent", substitutionVariables: substitutionVariables) else {
        	assert(false, "Can't find fetch request named \"byParent\".")
		return nil
	}
        var results = Array<Any>()
        do {
             results = try managedObjectContext.fetch(fetchRequest)
        } catch {
          print("Error executing fetch request: \(error)")
        }

        return results
    }

    class func fetchByParentName(managedObjectContext: NSManagedObjectContext, parentName: String) -> [Any]? {
        return self.fetchByParentName(managedObjectContext: managedObjectContext, parentName: parentName, error: nil)
    }

    class func fetchByParentName(managedObjectContext: NSManagedObjectContext, parentName: String, error outError: NSErrorPointer) -> [Any]? {
        guard let psc = managedObjectContext.persistentStoreCoordinator else { return nil }
        let model = psc.managedObjectModel
        let substitutionVariables : [String : Any] = [
            "parentName": parentName,
]

        guard let fetchRequest = model.fetchRequestFromTemplate(withName: "byParentName", substitutionVariables: substitutionVariables) else {
        	assert(false, "Can't find fetch request named \"byParentName\".")
		return nil
	}
        var results = Array<Any>()
        do {
             results = try managedObjectContext.fetch(fetchRequest)
        } catch {
          print("Error executing fetch request: \(error)")
        }

        return results
    }

}

