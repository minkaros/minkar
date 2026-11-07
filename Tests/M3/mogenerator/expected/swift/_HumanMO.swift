// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to HumanMO.swift instead.

import Foundation
import CoreData

public enum HumanMOAttributes: String {
    case age = "age"
    case gender = "gender"
    case humanName = "humanName"
}

public enum HumanMORelationships: String {
    case meaninglessRelationship = "meaninglessRelationship"
}

public enum HumanMOUserInfo: String {
    case additionalHeaderFileName = "additionalHeaderFileName"
}

open class _HumanMO: MyBaseClass {

    // MARK: - Class methods

    open class func entityName () -> String {
        return "Human"
    }

    open class func entity(managedObjectContext: NSManagedObjectContext) -> NSEntityDescription? {
        return NSEntityDescription.entity(forEntityName: self.entityName(), in: managedObjectContext)
    }

    @nonobjc
    open class func fetchRequest() -> NSFetchRequest<HumanMO> {
        return NSFetchRequest(entityName: self.entityName())
    }

    // MARK: - Life cycle methods

    public override init(entity: NSEntityDescription, insertInto context: NSManagedObjectContext?) {
        super.init(entity: entity, insertInto: context)
    }

    public convenience init?(managedObjectContext: NSManagedObjectContext) {
        guard let entity = _HumanMO.entity(managedObjectContext: managedObjectContext) else { return nil }
        self.init(entity: entity, insertInto: managedObjectContext)
    }

    // MARK: - Properties

    open var age: NSNumber!
    {
        self.willAccessValue(forKey: HumanMOAttributes.age.rawValue)
        let age = self.primitiveValue(forKey: HumanMOAttributes.age.rawValue) as? NSNumber
        self.didAccessValue(forKey: HumanMOAttributes.age.rawValue)
        return age
    }

    @NSManaged open
    var gender: NSNumber!

    @NSManaged open
    var humanName: String?

    // MARK: - Relationships

    @NSManaged open
    var meaninglessRelationship: NSManagedObject?

    class func fetchByHumanName(managedObjectContext: NSManagedObjectContext, humanName: String) -> [Any]? {
        return self.fetchByHumanName(managedObjectContext: managedObjectContext, humanName: humanName, error: nil)
    }

    class func fetchByHumanName(managedObjectContext: NSManagedObjectContext, humanName: String, error outError: NSErrorPointer) -> [Any]? {
        guard let psc = managedObjectContext.persistentStoreCoordinator else { return nil }
        let model = psc.managedObjectModel
        let substitutionVariables : [String : Any] = [
            "humanName": humanName,
]

        guard let fetchRequest = model.fetchRequestFromTemplate(withName: "byHumanName", substitutionVariables: substitutionVariables) else {
        	assert(false, "Can't find fetch request named \"byHumanName\".")
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

    class func fetchOneByHumanName(managedObjectContext: NSManagedObjectContext, humanName: String) -> Any? {
        return self.fetchOneByHumanName(managedObjectContext: managedObjectContext, humanName: humanName, error: nil)
    }

    class func fetchOneByHumanName(managedObjectContext: NSManagedObjectContext, humanName: String, error outError: NSErrorPointer) -> Any? {
        guard let psc = managedObjectContext.persistentStoreCoordinator else { return nil }
        let model = psc.managedObjectModel
		let substitutionVariables : [String : Any] = [
		            "humanName": humanName,
		]

        guard let fetchRequest = model.fetchRequestFromTemplate(withName: "oneByHumanName", substitutionVariables: substitutionVariables) else {
          assert(false, "Can't find fetch request named \"oneByHumanName\".")
          return nil
        }

        var result: Any? = nil
        do {
            let results = try managedObjectContext.fetch(fetchRequest)
            switch results.count {
            case 0:
                // Nothing found matching the fetch request. That's cool, though: we'll just return nil.
                break
            case 1:
                result = results.first
            default:
                print("WARN fetch request oneByHumanName: 0 or 1 objects expected, \(results.count) found (substitutionVariables: \(substitutionVariables), results: \(results))")
            }

        } catch {
            print("Error executing fetch request: \(error)")
        }
        return result
    }

    class func fetchAllHumans(managedObjectContext: NSManagedObjectContext) -> [Any]? {
        return self.fetchAllHumans(managedObjectContext: managedObjectContext, error: nil)
    }

    class func fetchAllHumans(managedObjectContext: NSManagedObjectContext, error outError: NSErrorPointer) -> [Any]? {
        guard let psc = managedObjectContext.persistentStoreCoordinator else { return nil }
        let model = psc.managedObjectModel
        let substitutionVariables : [String : Any] = [:]

        guard let fetchRequest = model.fetchRequestFromTemplate(withName: "allHumans", substitutionVariables: substitutionVariables) else {
        	assert(false, "Can't find fetch request named \"allHumans\".")
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

