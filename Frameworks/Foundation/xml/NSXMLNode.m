/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSXMLNode.h>
#import <Foundation/NSXMLDTDNode.h>
#import <Foundation/NSXMLElement.h>
#import <Foundation/NSXMLDocument.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSArray.h>

@implementation NSXMLNode

+document {
   return [[[NSXMLDocument alloc] init] autorelease];
}

+documentWithRootElement:(NSXMLElement *)element {
   return [[[NSXMLDocument alloc] initWithRootElement:element] autorelease];
}


+elementWithName:(NSString *)name {
   return [[[NSXMLElement alloc] initWithName:name] autorelease];
}

+elementWithName:(NSString *)name children:(NSArray *)children attributes:(NSArray *)attributes {
   NSXMLElement *result=[[[NSXMLElement alloc] initWithName:name] autorelease];
   
   [result setChildren:children];
   [result setAttributes:attributes];
   
   return result;
}

+elementWithName:(NSString *)name stringValue:(NSString *)string {
   return [[[NSXMLElement alloc] initWithName:name stringValue:string] autorelease];
}

+attributeWithName:(NSString *)name stringValue:(NSString *)string {
   NSXMLNode *result=[[[self alloc] initWithKind:NSXMLAttributeKind] autorelease];

   [result setName:name];
   [result setStringValue:string];

   return result;
}

+commentWithStringValue:(NSString *)string {
   NSXMLNode *result=[[[self alloc] initWithKind:NSXMLCommentKind] autorelease];

   [result setStringValue:string];

   return result;
}

+textWithStringValue:(NSString *)string {
   NSXMLNode *result=[[[self alloc] initWithKind:NSXMLTextKind] autorelease];

   [result setStringValue:string];

   return result;
}

+processingInstructionWithName:(NSString *)name stringValue:(NSString *)string {
   NSXMLNode *result=[[[self alloc] initWithKind:NSXMLProcessingInstructionKind] autorelease];

   [result setName:name];
   [result setStringValue:string];

   return result;
}

+DTDNodeWithXMLString:(NSString *)string {
   return [[[NSXMLDTDNode alloc] initWithXMLString:string] autorelease];
}

+namespaceWithName:(NSString *)name stringValue:(NSString *)string {
   NSXMLNode *result=[[[self alloc] initWithKind:NSXMLNamespaceKind] autorelease];

   [result setName:name];
   [result setStringValue:string];

   return result;
}

+(NSXMLNode *)predefinedNamespaceForPrefix:(NSString *)prefix {
   NSUnimplementedMethod();
   return nil;
}

+(NSString *)prefixForName:(NSString *)name {
   NSUnimplementedMethod();
   return nil;
}

+(NSString *)localNameForName:(NSString *)name {
   NSUnimplementedMethod();
   return nil;
}


-initWithKind:(NSXMLNodeKind)kind {
   return [self initWithKind:kind options:NSXMLNodeOptionsNone];
}

-initWithKind:(NSXMLNodeKind)kind options:(NSUInteger)options {
   _parent=nil;
   _children=[[NSMutableArray alloc] init];
   _index=0;
   _kind=kind;
   _options=options;
   _name=nil;
   _value=@""; // Behavior is empty string not null
   return self;
}

-(void)dealloc {
   _parent=nil;
   [_children release];
   [_name release];
   [_value release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   NSXMLNode *copy=NSCopyObject(self,0,zone);
   
   copy->_name=[_name copy];
   copy->_value=[_value copy];
   
   return copy;
}

-(NSUInteger)index {
   return _index;
}

-(NSXMLNodeKind)kind {
   return _kind;
}

/* Its depth: the document is level 0, its root element 1. */
-(NSUInteger)level {
   NSUInteger level=0;
   for(NSXMLNode *node=_parent;node!=nil;node=node->_parent)
    level++;
   return level;
}

/* A qualified name's parts: "sparkle:version" is local name "version" in
   prefix "sparkle"; a name without one has prefix "". */
-(NSString *)localName {
   NSRange colon=[_name rangeOfString:@":"];
   if(_name==nil)
    return nil;
   return colon.location==NSNotFound?_name:[_name substringFromIndex:NSMaxRange(colon)];
}

-(NSString *)name {
   return _name;
}

-(NSXMLNode *)nextNode {
   NSUnimplementedMethod();
   return nil;
}

-(NSXMLNode *)nextSibling {
   NSArray   *siblings=[_parent children];
   NSUInteger i=[siblings indexOfObjectIdenticalTo:self];
   return (i!=NSNotFound && i+1<[siblings count])?[siblings objectAtIndex:i+1]:nil;
}

-(NSString *)stringValue {
   return _value;
}

/* The namespace URI of its prefix (or, for an element without one, the
   default namespace), as declared by xmlns attributes on it or its
   ancestors; nil when none is. Sparkle tells its own elements by it. */
-(NSString *)URI {
   NSString *prefix=[self prefix];
   NSString *declaration;

   if(_kind!=NSXMLElementKind && _kind!=NSXMLAttributeKind)
    return nil;
   if(_kind==NSXMLAttributeKind && [prefix length]==0)
    return nil;
   declaration=[prefix length]?[@"xmlns:" stringByAppendingString:prefix]:@"xmlns";
   for(NSXMLNode *node=(_kind==NSXMLAttributeKind)?_parent:self;node!=nil;node=node->_parent)
    if(node->_kind==NSXMLElementKind){
     NSXMLNode *attribute=[(NSXMLElement *)node attributeForName:declaration];
     if(attribute!=nil)
      return [attribute stringValue];
    }
   return nil;
}

-objectValue {
   return _value;
}

-(NSXMLNode *)parent {
   return _parent;
}

-(NSString *)prefix {
   NSRange colon=[_name rangeOfString:@":"];
   if(_name==nil)
    return nil;
   return colon.location==NSNotFound?@"":[_name substringToIndex:colon.location];
}

-(NSXMLNode *)previousNode {
   NSUnimplementedMethod();
   return nil;
}

-(NSXMLNode *)previousSibling {
   NSArray   *siblings=[_parent children];
   NSUInteger i=[siblings indexOfObjectIdenticalTo:self];
   return (i!=NSNotFound && i>0)?[siblings objectAtIndex:i-1]:nil;
}

-(NSXMLDocument *)rootDocument {
   NSXMLNode *node=self;
   while(node->_parent!=nil)
    node=node->_parent;
   return (node->_kind==NSXMLDocumentKind)?(NSXMLDocument *)node:nil;
}

-(NSUInteger)childCount {
   return [_children count];
}

-(NSArray *)children {
   return _children;
}

-(NSXMLNode *)childAtIndex:(NSUInteger)index {
   return [_children objectAtIndex:index];
}

-(void)setName:(NSString *)name {
   name=[name copy];
   [_name release];
   _name=name;
}

-(void)setObjectValue:object {
   object=[object retain];
   [_value release];
   _value=object;
}

-(void)setStringValue:(NSString *)string {
   string=[string copy];
   [_value release];
   _value=string;
}

-(void)setStringValue:(NSString *)string resolvingEntities:(BOOL)resolveEntities {
   NSUnimplementedMethod();
}

-(void)detach {
//   [_parent removeChild:self];
   _parent=nil;
}

/* XPath, the part apps use (Sparkle's appcasts among them): location
 * paths, absolute or relative; steps by name (prefixed names as written),
 * "*", "node()", "text()", "." and "..", attributes by "@name" or "@*",
 * "//" for descendants; predicates by position ("[2]", "[last()]") or by
 * value ("[@name]", "[@name='v']", "[name='v']", "[text()='v']"), and
 * several in turn. A union ("|") of paths. Nodes come once each, in
 * document order. */
static NSArray *xpathChildren(NSXMLNode *node) {
   switch(node->_kind){
    case NSXMLAttributeKind:
    case NSXMLNamespaceKind:
    case NSXMLTextKind:
    case NSXMLCommentKind:
    case NSXMLProcessingInstructionKind:
     return [NSArray array];
    default:	/* a document, an element, or a node made of another kind that has children */
     return [node children]?:[NSArray array];
   }
}

static void xpathDescendantsOrSelf(NSXMLNode *node,NSMutableArray *out) {
   [out addObject:node];
   for(NSXMLNode *child in xpathChildren(node))
    xpathDescendantsOrSelf(child,out);
}

static BOOL xpathTest(NSXMLNode *node,NSString *test,BOOL attributeAxis) {
   if([test isEqualToString:@"node()"])
    return YES;
   if([test isEqualToString:@"text()"])
    return node->_kind==NSXMLTextKind;
   if(attributeAxis)
    return [test isEqualToString:@"*"] || [test isEqualToString:[node name]];
   if(node->_kind!=NSXMLElementKind)
    return NO;
   return [test isEqualToString:@"*"] || [test isEqualToString:[node name]];
}

static NSString *xpathUnquote(NSString *s) {
   s=[s stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
   if([s length]>=2 && ([s hasPrefix:@"'"] || [s hasPrefix:@"\""]))
    return [s substringWithRange:NSMakeRange(1,[s length]-2)];
   return s;
}

/* A predicate's value test on one node: true or false. */
static BOOL xpathValueTest(NSXMLNode *node,NSString *predicate) {
   NSRange eq=[predicate rangeOfString:@"="];
   NSString *left=[[(eq.location==NSNotFound)?predicate:[predicate substringToIndex:eq.location]
      stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] copy];
   NSString *right=(eq.location==NSNotFound)?nil:xpathUnquote([predicate substringFromIndex:NSMaxRange(eq)]);
   NSArray *found=[node nodesForXPath:left error:NULL];
   [left release];
   if(right==nil)
    return [found count]>0;
   for(NSXMLNode *n in found)
    if([[n stringValue] isEqualToString:right])
     return YES;
   return NO;
}

static NSArray *xpathPredicate(NSArray *nodes,NSString *predicate) {
   NSMutableArray *out=[NSMutableArray array];
   NSString *trimmed=[predicate stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
   NSInteger position=[trimmed integerValue];

   if([trimmed isEqualToString:@"last()"])
    return [nodes count]?[NSArray arrayWithObject:[nodes lastObject]]:out;
   if(position>0 && [[NSString stringWithFormat:@"%ld",(long)position] isEqualToString:trimmed])
    return (position<=(NSInteger)[nodes count])?[NSArray arrayWithObject:[nodes objectAtIndex:position-1]]:out;
   for(NSXMLNode *node in nodes)
    if(xpathValueTest(node,trimmed))
     [out addObject:node];
   return out;
}

/* One step from each node of the set; its predicates apply per context
   node, as XPath's do. */
static NSArray *xpathStep(NSArray *context,NSString *step,BOOL descendants) {
   NSMutableArray *result=[NSMutableArray array];
   NSMutableArray *predicates=[NSMutableArray array];
   NSRange bracket=[step rangeOfString:@"["];
   NSString *test=step;

   if(bracket.location!=NSNotFound){
    NSString *rest=[step substringFromIndex:bracket.location];
    test=[step substringToIndex:bracket.location];
    while([rest hasPrefix:@"["]){
     NSInteger depth=0,i;
     for(i=0;i<(NSInteger)[rest length];i++){
      unichar c=[rest characterAtIndex:i];
      if(c=='[') depth++;
      if(c==']' && --depth==0) break;
     }
     if(i>=(NSInteger)[rest length]) break;
     [predicates addObject:[rest substringWithRange:NSMakeRange(1,i-1)]];
     rest=[rest substringFromIndex:i+1];
    }
   }
   for(NSXMLNode *node in context){
    NSMutableArray *candidates=[NSMutableArray array];
    NSArray *bases=[NSArray arrayWithObject:node];

    if(descendants){
     NSMutableArray *all=[NSMutableArray array];
     xpathDescendantsOrSelf(node,all);
     bases=all;
    }
    for(NSXMLNode *base in bases){
     if([test isEqualToString:@"."])
      [candidates addObject:base];
     else if([test isEqualToString:@".."]){
      if(base->_parent!=nil)
       [candidates addObject:base->_parent];
     }
     else if([test hasPrefix:@"@"]){
      if(base->_kind==NSXMLElementKind)
       for(NSXMLNode *attribute in [(NSXMLElement *)base attributes])
        if(xpathTest(attribute,[test substringFromIndex:1],YES))
         [candidates addObject:attribute];
     }
     else
      for(NSXMLNode *child in xpathChildren(base))
       if(xpathTest(child,test,NO))
        [candidates addObject:child];
    }
    for(NSString *predicate in predicates)
     candidates=(NSMutableArray *)xpathPredicate(candidates,predicate);
    for(NSXMLNode *candidate in candidates)
     if([result indexOfObjectIdenticalTo:candidate]==NSNotFound)
      [result addObject:candidate];
   }
   return result;
}

/* Nodes in document order: an element, its attributes, then its
   children. */
static void xpathDocumentOrderFrom(NSXMLNode *node,NSMutableArray *out) {
   [out addObject:node];
   if(node->_kind==NSXMLElementKind)
    [out addObjectsFromArray:[(NSXMLElement *)node attributes]];
   for(NSXMLNode *child in xpathChildren(node))
    xpathDocumentOrderFrom(child,out);
}

static NSArray *xpathInDocumentOrder(NSArray *nodes) {
   NSMutableArray *all,*result;
   NSXMLNode      *top;

   if([nodes count]<2)
    return nodes;
   top=[nodes objectAtIndex:0];
   while(top->_parent!=nil)
    top=top->_parent;
   all=[NSMutableArray array];
   xpathDocumentOrderFrom(top,all);
   result=[NSMutableArray array];
   for(NSXMLNode *node in all)
    if([nodes indexOfObjectIdenticalTo:node]!=NSNotFound)
     [result addObject:node];
   /* Nodes outside that tree (detached ones) keep their place at the end. */
   for(NSXMLNode *node in nodes)
    if([result indexOfObjectIdenticalTo:node]==NSNotFound)
     [result addObject:node];
   return result;
}

/* A path's steps, split at "/" outside predicates and quotes. */
static NSArray *xpathSteps(NSString *path) {
   NSMutableArray *steps=[NSMutableArray array];
   NSInteger depth=0,start=0,i,length=[path length];
   unichar quote=0;

   for(i=0;i<length;i++){
    unichar c=[path characterAtIndex:i];
    if(quote){ if(c==quote) quote=0; continue; }
    if(c=='\'' || c=='"'){ quote=c; continue; }
    if(c=='[') depth++;
    if(c==']') depth--;
    if(c=='/' && depth==0){
     [steps addObject:[path substringWithRange:NSMakeRange(start,i-start)]];
     start=i+1;
    }
   }
   [steps addObject:[path substringFromIndex:start]];
   return steps;
}

-(NSArray *)nodesForXPath:(NSString *)xpath error:(NSError **)error {
   NSString *path=[xpath stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
   NSArray  *alternatives=[path componentsSeparatedByString:@"|"];

   if([alternatives count]>1){
    NSMutableArray *result=[NSMutableArray array];
    for(NSString *alternative in alternatives)
     for(NSXMLNode *node in [self nodesForXPath:alternative error:error])
      if([result indexOfObjectIdenticalTo:node]==NSNotFound)
       [result addObject:node];
    return xpathInDocumentOrder(result);
   }
   if([path length]==0)
    return [NSArray array];

   NSArray *context=[NSArray arrayWithObject:self];
   NSArray *steps=xpathSteps(path);
   NSUInteger i=0;
   BOOL descendants=NO;

   /* Absolute: from the document (or the topmost node). */
   if([path hasPrefix:@"/"]){
    NSXMLNode *top=self;
    while(top->_parent!=nil)
     top=top->_parent;
    context=[NSArray arrayWithObject:top];
    i=1;
   }
   for(;i<[steps count];i++){
    NSString *step=[steps objectAtIndex:i];
    if([step length]==0){	/* "//": the next step among descendants */
     descendants=YES;
     continue;
    }
    context=xpathStep(context,step,descendants);
    descendants=NO;
   }
   return xpathInDocumentOrder(context);
}

-(NSArray *)objectsForXQuery:(NSString *)xquery constants:(NSDictionary *)constants error:(NSError **)error {
   NSUnimplementedMethod();
   return nil;
}

-(NSArray *)objectsForXQuery:(NSString *)xquery error:(NSError **)error {
   NSUnimplementedMethod();
   return nil;
}

-(NSString *)XMLString {
   return [self XMLStringWithOptions:0];
}

-(NSString *)XMLStringWithOptions:(NSUInteger)options {
   NSMutableString *result=[NSMutableString string];
   
   switch([self kind]){

    case NSXMLInvalidKind:
     break;

    case NSXMLDocumentKind:
     // Handled in subclass
     break;

    case NSXMLElementKind:
     // Handled in subclass
     break;

    case NSXMLAttributeKind:
     [result appendString:[self name]];
     [result appendString:@"=\""];
     [result appendString:[self stringValue]];
     [result appendString:@"\""];
     break;

    case NSXMLNamespaceKind:
     break;

    case NSXMLProcessingInstructionKind:
     break;

    case NSXMLCommentKind:
     break;

    case NSXMLTextKind:
     break;

    case NSXMLDTDKind:
     break;

    case NSXMLEntityDeclarationKind:
     break;

    case NSXMLAttributeDeclarationKind:
     break;

    case NSXMLElementDeclarationKind:
     break;

    case NSXMLNotationDeclarationKind:
     break;
}

   return result;
}

-(NSString *)XPath {
   NSUnimplementedMethod();
   return nil;
}

-(NSString *)canonicalXMLStringPreservingComments:(BOOL)comments {
   NSUnimplementedMethod();
   return nil;
}

-(NSString *)description {
   return [self XMLString];
}

@end
