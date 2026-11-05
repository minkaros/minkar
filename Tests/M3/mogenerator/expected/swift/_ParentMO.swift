// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to ParentMO.swift instead.

import Foundation
import CoreData

public enum ParentMOAttributes: String {
    case myBinaryData = "myBinaryData"
    case myBoolean = "myBoolean"
    case myDate = "myDate"
    case myDecimal = "myDecimal"
    case myDouble = "myDouble"
    case myFloat = "myFloat"
    case myInt16 = "myInt16"
    case myInt16Transient = "myInt16Transient"
    case myInt32 = "myInt32"
    case myInt64 = "myInt64"
    case myString = "myString"
    case myTransformableSansClassName = "myTransformableSansClassName"
    case parentName = "parentName"
}

public enum ParentMORelationships: String {
    case children = "children"
    case orderedChildren = "orderedChildren"
}

open class _ParentMO: HumanMO {

    // MARK: - Class methods

    override open class func entityName () -> String {
        return "Parent"
    }

    override open class func entity(managedObjectContext: NSManagedObjectContext) -> NSEntityDescription? {
        return NSEntityDescription.entity(forEntityName: self.entityName(), in: managedObjectContext)
    }

    @nonobjc
    open class func fetchRequest() -> NSFetchRequest<ParentMO> {
        return NSFetchRequest(entityName: self.entityName())
    }

    // MARK: - Life cycle methods

    public override init(entity: NSEntityDescription, insertInto context: NSManagedObjectContext?) {
        super.init(entity: entity, insertInto: context)
    }

    public convenience init?(managedObjectContext: NSManagedObjectContext) {
        guard let entity = _ParentMO.entity(managedObjectContext: managedObjectContext) else { return nil }
        self.init(entity: entity, insertInto: managedObjectContext)
    }

    // MARK: - Properties

    @NSManaged open
    var myBinaryData: Data?

    @NSManaged open
    var myBoolean: NSNumber?

    @NSManaged open
    var myDate: Date?

    @NSManaged open
    var myDecimal: NSDecimalNumber?

    @NSManaged open
    var myDouble: NSNumber?

    @NSManaged open
    var myFloat: NSNumber?

    @NSManaged open
    var myInt16: NSNumber?

    @NSManaged open
    var myInt16Transient: NSNumber?

    @NSManaged open
    var myInt32: NSNumber?

    @NSManaged open
    var myInt64: NSNumber?

    @NSManaged open
    var myString: String?

    @NSManaged open
    var myTransformableSansClassName: AnyObject?

    @NSManaged open
    var parentName: String?

    // MARK: - Relationships

    @NSManaged open
    var children: NSSet

    open func childrenSet() -> NSMutableSet {
        return self.children.mutableCopy() as! NSMutableSet
    }

    @NSManaged open
    var orderedChildren: NSOrderedSet

    open func orderedChildrenSet() -> NSMutableOrderedSet {
        return self.orderedChildren.mutableCopy() as! NSMutableOrderedSet
    }

}

extension _ParentMO {

    open func addChildren(_ objects: NSSet) {
        let mutable = self.children.mutableCopy() as! NSMutableSet
        mutable.union(objects as Set<NSObject>)
        self.children = mutable.copy() as! NSSet
    }

    open func removeChildren(_ objects: NSSet) {
        let mutable = self.children.mutableCopy() as! NSMutableSet
        mutable.minus(objects as Set<NSObject>)
        self.children = mutable.copy() as! NSSet
    }

    open func addChildrenObject(_ value: ChildMO) {
        let mutable = self.children.mutableCopy() as! NSMutableSet
        mutable.add(value)
        self.children = mutable.copy() as! NSSet
    }

    open func removeChildrenObject(_ value: ChildMO) {
        let mutable = self.children.mutableCopy() as! NSMutableSet
        mutable.remove(value)
        self.children = mutable.copy() as! NSSet
    }

}

extension _ParentMO {

    open func addOrderedChildren(_ objects: NSOrderedSet) {
        let mutable = self.orderedChildren.mutableCopy() as! NSMutableOrderedSet
        mutable.union(objects)
        self.orderedChildren = mutable.copy() as! NSOrderedSet
    }

    open func removeOrderedChildren(_ objects: NSOrderedSet) {
        let mutable = self.orderedChildren.mutableCopy() as! NSMutableOrderedSet
        mutable.minus(objects)
        self.orderedChildren = mutable.copy() as! NSOrderedSet
    }

    open func addOrderedChildrenObject(_ value: ChildMO) {
        let mutable = self.orderedChildren.mutableCopy() as! NSMutableOrderedSet
        mutable.add(value)
        self.orderedChildren = mutable.copy() as! NSOrderedSet
    }

    open func removeOrderedChildrenObject(_ value: ChildMO) {
        let mutable = self.orderedChildren.mutableCopy() as! NSMutableOrderedSet
        mutable.remove(value)
        self.orderedChildren = mutable.copy() as! NSOrderedSet
    }

}

