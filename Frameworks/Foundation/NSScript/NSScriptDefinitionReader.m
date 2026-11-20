// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import "NSScriptDefinitionReader.h"

/* An element of the sdef: its name, attributes and children. */
@interface _NSSDefNode : NSObject
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy) NSDictionary<NSString *, NSString *> *attributes;
@property (nonatomic, strong) NSMutableArray<_NSSDefNode *> *children;
@property (nonatomic, weak) _NSSDefNode *parent;
@end

@implementation _NSSDefNode
- (NSArray<_NSSDefNode *> *)childrenNamed:(NSString *)name {
    return [self.children filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(_NSSDefNode *node, NSDictionary *bindings) {
        return [node.name isEqualToString:name];
    }]];
}
- (_NSSDefNode *)childNamed:(NSString *)name {
    return [self childrenNamed:name].firstObject;
}
/* The <cocoa> child's attribute (class, key, method). */
- (NSString *)cocoa:(NSString *)attribute {
    return [self childNamed:@"cocoa"].attributes[attribute];
}
@end

@interface _NSSDefBuilder : NSObject <NSXMLParserDelegate>
@property (nonatomic, strong) _NSSDefNode *root;
@property (nonatomic, weak) _NSSDefNode *current;
@end

@implementation _NSSDefBuilder
- (void)parser:(NSXMLParser *)parser didStartElement:(NSString *)element namespaceURI:(NSString *)uri qualifiedName:(NSString *)qName attributes:(NSDictionary<NSString *, NSString *> *)attributes {
    _NSSDefNode *node = [_NSSDefNode new];
    node.name = element;
    node.attributes = attributes;
    node.children = [NSMutableArray array];
    node.parent = self.current;
    if (self.current)
        [self.current.children addObject:node];
    else
        self.root = node;
    self.current = node;
}
- (void)parser:(NSXMLParser *)parser didEndElement:(NSString *)element namespaceURI:(NSString *)uri qualifiedName:(NSString *)qName {
    self.current = self.current.parent;
}
@end

/* "file name" -> "fileName". */
static NSString *camelCase(NSString *name) {
    NSArray<NSString *> *words = [name componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    NSMutableString *result = [NSMutableString string];
    for (NSString *word in words) {
        if (word.length == 0)
            continue;
        [result appendString:result.length == 0 ? word : [[word substringToIndex:1].uppercaseString stringByAppendingString:[word substringFromIndex:1]]];
    }
    return result;
}

/* "rich text" -> "RichText": a class's Cocoa name by default. */
static NSString *capitalizedName(NSString *name) {
    NSString *camel = camelCase(name);
    return camel.length ? [[camel substringToIndex:1].uppercaseString stringByAppendingString:[camel substringFromIndex:1]] : camel;
}

/* The sdef's types as the dictionary format's. */
static NSString *typeName(NSString *type, NSSet<NSString *> *classNames, NSDictionary<NSString *, NSString *> *valueTypes, NSSet<NSString *> *enumerations) {
    static NSDictionary<NSString *, NSString *> *basic;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        basic = @{ @"text": @"NSString", @"string": @"NSString", @"Unicode text": @"NSString",
                   @"integer": @"NSNumber<Int>", @"real": @"NSNumber<Double>", @"number": @"NSNumber",
                   @"boolean": @"NSNumber<Bool>", @"date": @"NSDate", @"file": @"NSURL", @"alias": @"NSURL",
                   @"record": @"NSDictionary", @"any": @"NSObject", @"type": @"NSNumber<TypeCode>",
                   @"point": @"NSData<QDPoint>", @"rectangle": @"NSData<QDRect>",
                   @"location specifier": @"NSPositionalSpecifier", @"specifier": @"NSScriptObjectSpecifier",
                   @"color": @"NSColor", @"data": @"NSData" };
    });
    if (type == nil)
        return nil;
    if (basic[type] != nil)
        return basic[type];
    if ([classNames containsObject:type])
        return type;
    if (valueTypes[type] != nil)
        return valueTypes[type];
    if ([enumerations containsObject:type])
        return [NSString stringWithFormat:@"NSNumber<%@>", capitalizedName(type)];
    return type;
}

/* A property's or parameter's type: its type attribute, or its <type>
   children (a list when list="yes"). */
static NSString *declaredType(_NSSDefNode *node, NSSet *classNames, NSDictionary *valueTypes, NSSet *enumerations) {
    NSString *type = node.attributes[@"type"];
    BOOL list = NO;
    _NSSDefNode *child = [node childNamed:@"type"];
    if (type == nil && child != nil) {
        type = child.attributes[@"type"];
        list = [child.attributes[@"list"] isEqualToString:@"yes"];
    }
    NSString *name = typeName(type, classNames, valueTypes, enumerations);
    return list ? [NSString stringWithFormat:@"NSArray<%@>", name ?: @"NSObject"] : name;
}

NSArray<NSDictionary *> *NSScriptSuitesFromDefinition(NSData *data) {
    if (data.length == 0)
        return @[];
    NSXMLParser *parser = [[NSXMLParser alloc] initWithData:data];
    _NSSDefBuilder *builder = [_NSSDefBuilder new];
    parser.delegate = builder;
    if (![parser parse] || builder.root == nil)
        return @[];

    NSArray<_NSSDefNode *> *suites = [builder.root childrenNamed:@"suite"];

    /* The names every suite's classes, value types and enumerations are
       known by, for the types that name them. */
    NSMutableSet<NSString *> *classNames = [NSMutableSet set];
    NSMutableDictionary<NSString *, NSString *> *valueTypes = [NSMutableDictionary dictionary];
    NSMutableSet<NSString *> *enumerations = [NSMutableSet set];
    NSMutableDictionary<NSString *, NSString *> *pluralKeys = [NSMutableDictionary dictionary];
    NSMutableDictionary<NSString *, NSString *> *classCodes = [NSMutableDictionary dictionary];
    for (_NSSDefNode *suite in suites) {
        for (_NSSDefNode *node in [suite childrenNamed:@"class"]) {
            NSString *name = node.attributes[@"name"];
            if (name == nil)
                continue;
            [classNames addObject:name];
            pluralKeys[name] = camelCase(node.attributes[@"plural"] ?: [name stringByAppendingString:@"s"]);
            if (node.attributes[@"code"])
                classCodes[name] = node.attributes[@"code"];
        }
        for (_NSSDefNode *node in [suite childrenNamed:@"value-type"])
            if (node.attributes[@"name"])
                valueTypes[node.attributes[@"name"]] = [node cocoa:@"class"] ?: @"NSObject";
        for (_NSSDefNode *node in [suite childrenNamed:@"enumeration"])
            if (node.attributes[@"name"])
                [enumerations addObject:node.attributes[@"name"]];
    }

    NSMutableArray<NSMutableDictionary *> *result = [NSMutableArray array];
    NSMutableDictionary<NSString *, NSMutableDictionary *> *allClasses = [NSMutableDictionary dictionary];
    NSMutableArray<NSArray *> *extensions = [NSMutableArray array];

    void (^addKeys)(_NSSDefNode *, NSMutableDictionary *) = ^(_NSSDefNode *classNode, NSMutableDictionary *declaration) {
        for (_NSSDefNode *property in [classNode childrenNamed:@"property"]) {
            NSString *name = property.attributes[@"name"];
            if (name == nil)
                continue;
            NSString *key = [property cocoa:@"key"] ?: camelCase(name);
            NSString *type = declaredType(property, classNames, valueTypes, enumerations);
            NSMutableDictionary *entry = [NSMutableDictionary dictionary];
            if (type)
                entry[@"Type"] = type;
            if (property.attributes[@"code"])
                entry[@"AppleEventCode"] = property.attributes[@"code"];
            if ([property.attributes[@"access"] isEqualToString:@"r"])
                entry[@"ReadOnly"] = @"YES";
            NSString *section = [classNames containsObject:property.attributes[@"type"] ?: @""] ? @"ToOneRelationships" : @"Attributes";
            NSMutableDictionary *keys = declaration[section] ?: [NSMutableDictionary dictionary];
            keys[key] = entry;
            declaration[section] = keys;
        }
        for (_NSSDefNode *element in [classNode childrenNamed:@"element"]) {
            NSString *type = element.attributes[@"type"];
            if (type == nil)
                continue;
            NSString *key = [element cocoa:@"key"] ?: pluralKeys[type] ?: camelCase([type stringByAppendingString:@"s"]);
            NSMutableDictionary *entry = [NSMutableDictionary dictionaryWithObject:type forKey:@"Type"];
            if (classCodes[type])
                entry[@"AppleEventCode"] = classCodes[type];
            if ([element.attributes[@"access"] isEqualToString:@"r"])
                entry[@"ReadOnly"] = @"YES";
            NSMutableDictionary *keys = declaration[@"ToManyRelationships"] ?: [NSMutableDictionary dictionary];
            keys[key] = entry;
            declaration[@"ToManyRelationships"] = keys;
        }
        for (_NSSDefNode *respondsTo in [classNode childrenNamed:@"responds-to"]) {
            NSString *command = respondsTo.attributes[@"command"] ?: respondsTo.attributes[@"name"];
            if (command == nil)
                continue;
            NSMutableDictionary *supported = declaration[@"SupportedCommands"] ?: [NSMutableDictionary dictionary];
            supported[command] = [respondsTo cocoa:@"method"] ?: @"";
            declaration[@"SupportedCommands"] = supported;
        }
    };

    for (_NSSDefNode *suiteNode in suites) {
        NSString *suiteName = suiteNode.attributes[@"name"];
        if (suiteName == nil)
            continue;
        NSMutableDictionary *suite = [NSMutableDictionary dictionaryWithObject:suiteName forKey:@"Name"];
        if (suiteNode.attributes[@"code"])
            suite[@"AppleEventCode"] = suiteNode.attributes[@"code"];

        NSMutableDictionary *classes = [NSMutableDictionary dictionary];
        for (_NSSDefNode *classNode in [suiteNode childrenNamed:@"class"]) {
            NSString *name = classNode.attributes[@"name"];
            if (name == nil)
                continue;
            NSMutableDictionary *declaration = [NSMutableDictionary dictionary];
            declaration[@"ImplementationClassName"] = [classNode cocoa:@"class"] ?: capitalizedName(name);
            if (classNode.attributes[@"code"])
                declaration[@"AppleEventCode"] = classNode.attributes[@"code"];
            if (classNode.attributes[@"inherits"])
                declaration[@"Superclass"] = classNode.attributes[@"inherits"];
            addKeys(classNode, declaration);
            classes[name] = declaration;
            allClasses[name] = declaration;
        }
        for (_NSSDefNode *extension in [suiteNode childrenNamed:@"class-extension"])
            [extensions addObject:@[ extension, classes ]];
        suite[@"Classes"] = classes;

        NSMutableDictionary *commands = [NSMutableDictionary dictionary];
        for (_NSSDefNode *commandNode in [suiteNode childrenNamed:@"command"]) {
            NSString *name = commandNode.attributes[@"name"];
            NSString *code = commandNode.attributes[@"code"];
            if (name == nil || code.length < 8)
                continue;
            NSMutableDictionary *declaration = [NSMutableDictionary dictionary];
            declaration[@"CommandClass"] = [commandNode cocoa:@"class"] ?: @"NSScriptCommand";
            declaration[@"AppleEventClassCode"] = [code substringToIndex:4];
            declaration[@"AppleEventCode"] = [code substringWithRange:NSMakeRange(4, 4)];
            NSMutableDictionary *arguments = [NSMutableDictionary dictionary];
            for (_NSSDefNode *parameter in [commandNode childrenNamed:@"parameter"]) {
                NSString *parameterName = parameter.attributes[@"name"];
                if (parameterName == nil)
                    continue;
                NSMutableDictionary *argument = [NSMutableDictionary dictionary];
                NSString *type = declaredType(parameter, classNames, valueTypes, enumerations);
                if (type)
                    argument[@"Type"] = type;
                if (parameter.attributes[@"code"])
                    argument[@"AppleEventCode"] = parameter.attributes[@"code"];
                if ([parameter.attributes[@"optional"] isEqualToString:@"yes"])
                    argument[@"Optional"] = @"YES";
                arguments[[parameter cocoa:@"key"] ?: camelCase(parameterName)] = argument;
            }
            if (arguments.count)
                declaration[@"Arguments"] = arguments;
            _NSSDefNode *resultNode = [commandNode childNamed:@"result"];
            if (resultNode != nil) {
                NSString *type = declaredType(resultNode, classNames, valueTypes, enumerations);
                if (type)
                    declaration[@"Type"] = type;
            }
            commands[capitalizedName(name)] = declaration;
        }
        if (commands.count)
            suite[@"Commands"] = commands;
        [result addObject:suite];
    }

    /* Class extensions add to the class they name: in this definition, or
       else a class of their own name in their suite. */
    for (NSArray *pair in extensions) {
        _NSSDefNode *extension = pair[0];
        NSMutableDictionary *classes = pair[1];
        NSString *extended = extension.attributes[@"extends"];
        if (extended == nil)
            continue;
        NSMutableDictionary *declaration = allClasses[extended];
        if (declaration == nil) {
            declaration = [NSMutableDictionary dictionary];
            declaration[@"ImplementationClassName"] = [extension cocoa:@"class"] ?: capitalizedName(extended);
            if (classCodes[extended])
                declaration[@"AppleEventCode"] = classCodes[extended];
            classes[extended] = declaration;
            allClasses[extended] = declaration;
        }
        addKeys(extension, declaration);
    }
    return result;
}
