/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
                 2009 Markus Hitter <mah@jump-ing.de>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <LaunchServices/LaunchServices.h>
#import <AppKit/NSDocumentController.h>
#import <AppKit/NSWorkspace.h>
#import <AppKit/NSDocument.h>
#import <AppKit/NSOpenPanel.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindowController.h>
#import <AppKit/NSRaise.h>

@interface NSDocument(private)
-(void)_setUntitledNumber:(NSInteger)number;
@end

@interface NSDocumentController(forward)
-(void)_updateRecentDocumentsMenu;
@end

@interface NSMenu(private)
-(NSMenu *)_menuWithName:(NSString *)name;
@end

@implementation NSDocumentController

static NSDocumentController *shared=nil;

+sharedDocumentController {
   if(shared==nil){
    shared = [[NSDocumentController alloc] init];
    [shared _updateRecentDocumentsMenu];
   }

   return shared;
}

-init {
   if(shared==nil)
    shared=self;

   _documents=[NSMutableArray new];
   _fileTypes=[[[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleDocumentTypes"] retain];
   _autosavingDelay=0;
   return self;
}

-(void)dealloc {
   [_documents release];
   [_fileTypes release];
   [super dealloc];
}

-initWithCoder:(NSCoder *)coder {
   NSUnimplementedMethod();
   return nil;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   NSUnimplementedMethod();
}

-(NSString *)defaultType {
   if([_fileTypes count]==0)
    return nil;

   return [(NSDictionary *)[_fileTypes objectAtIndex:0] objectForKey:@"CFBundleTypeName"];
}

-(NSArray *)documentClassNames {
   NSMutableSet *result=[NSMutableSet set];
   int           i,count=[_fileTypes count];
   
   for(i=0;i<count;i++)
    [result addObject:[(NSDictionary *)[_fileTypes objectAtIndex:i] objectForKey:@"NSDocumentClass"]];
   
   return [result allObjects];
}

-(NSTimeInterval)autosavingDelay {
   return _autosavingDelay;
}

/* A document type is its declaration's name or one of its content types
   (LSItemContentTypes: what apps written for UTIs use). */
-(NSDictionary *)_infoForType:(NSString *)type {
   if(type==nil)
    return nil;
   for(NSDictionary *check in _fileTypes){
    if([[check objectForKey:@"CFBundleTypeName"] isEqualToString:type])
     return check;
    for(NSString *uti in [check objectForKey:@"LSItemContentTypes"])
     if([uti caseInsensitiveCompare:type]==NSOrderedSame)
      return check;
   }
   return nil;
}

-(NSString *)displayNameForType:(NSString *)type {
   NSDictionary *info=[self _infoForType:type];
   NSString     *result=[info objectForKey:@"CFBundleTypeName"];

   return (result==nil)?type:result;
}

-(Class)documentClassForType:(NSString *)type {
   NSDictionary *info=[self _infoForType:type];
   NSString     *result=[info objectForKey:@"NSDocumentClass"];

   return (result==nil)?Nil:NSClassFromString(result);
}

-(NSArray *)fileExtensionsFromType:(NSString *)type {
   NSDictionary *info=[self _infoForType:type];

   return [info objectForKey:@"CFBundleTypeExtensions"];
}

-(NSArray *)_allFileExtensions {
   NSMutableSet *set=[NSMutableSet set];
   int           i,count=[_fileTypes count];

   for(i=0;i<count;i++){
    NSArray *add=[(NSDictionary *)[_fileTypes objectAtIndex:i] objectForKey:@"CFBundleTypeExtensions"];
    [set addObjectsFromArray:add];
   }

   return [set allObjects];
}

-(NSString *)typeFromFileExtension:(NSString *)extension {
   int i,count=[_fileTypes count];

   extension=[extension lowercaseString];
   for(i=0;i<count;i++){
    NSDictionary *check=[_fileTypes objectAtIndex:i];
    NSArray      *names=[check objectForKey:@"CFBundleTypeExtensions"];
    int           count=[names count];
    
    while(--count>=0)
     if([[[names objectAtIndex:count] lowercaseString] isEqual:extension])
      return [check objectForKey:@"CFBundleTypeName"];
   }
   return nil;
}

-(void)setAutosavingDelay:(NSTimeInterval)value {
   _autosavingDelay=value;
   
   NSUnimplementedMethod();
}

-(NSArray *)documents {
   return _documents;
}

-(void)addDocument:(NSDocument *)document {
   [_documents addObject:document];
}

-(void)removeDocument:(NSDocument *)document {
   [_documents removeObjectIdenticalTo:document];
}

-documentForURL:(NSURL *)url {
   int i,count=[_documents count];
   
   for(i=0;i<count;i++){
    NSDocument *document=[_documents objectAtIndex:i];
    NSURL      *check=[document fileURL];
    
    if(check!=nil && [check isEqual:url])
     return document;
   }
   
   return nil;
}

-currentDocument {
   return [[[NSApp mainWindow] windowController] document];
}

-(NSString *)currentDirectory {
   NSDocument *current=[self currentDocument];
   NSURL      *url=[current fileURL];
   
   if(url!=nil && [url isFileURL]){
    NSString *check=[[url path] stringByDeletingLastPathComponent];
    BOOL      isDirectory;
    
    if([[NSFileManager defaultManager] fileExistsAtPath:check isDirectory:&isDirectory])
     if(isDirectory)
      return check;
   }
   
   NSString * lastOpenPanelDirectory = [[NSUserDefaults standardUserDefaults] stringForKey:@"NSNavLastRootDirectory"];
   if(lastOpenPanelDirectory!=nil)
    return lastOpenPanelDirectory;
    
   return NSHomeDirectory();
}

-(BOOL)hasEditedDocuments {
   int count=[_documents count];
   
   while(--count)
    if([[_documents objectAtIndex:count] isDocumentEdited])
     return YES;
     
   return NO;
}

-documentForWindow:(NSWindow *)window {
   return [[window windowController] document];
}

/* The first declared content type the file's conforms to, in the
   declarations' order; else, for declarations by extension, the one whose
   extensions (or "*") take it. */
-(NSString *)typeForContentsOfURL:(NSURL *)url error:(NSError **)error {
   NSString *uti=nil;

   if([url isFileURL]){
    [url getResourceValue:&uti forKey:NSURLTypeIdentifierKey error:NULL];
    if(uti==nil)
     uti=[[NSWorkspace sharedWorkspace] typeOfFile:[url path] error:NULL];
   }
   if(uti!=nil)
    for(NSDictionary *check in _fileTypes)
     for(NSString *type in [check objectForKey:@"LSItemContentTypes"])
      if(UTTypeConformsTo((CFStringRef)uti,(CFStringRef)type))
       return type;
   NSString *extension=[[[url path] pathExtension] lowercaseString];
   for(NSDictionary *check in _fileTypes)
    for(NSString *name in [check objectForKey:@"CFBundleTypeExtensions"])
     if([name isEqualToString:@"*"] || [[name lowercaseString] isEqualToString:extension])
      return [check objectForKey:@"CFBundleTypeName"];
   if(error!=NULL)
    *error=[NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError
     userInfo:[NSDictionary dictionaryWithObject:url forKey:NSURLErrorKey]];
   return nil;
}

-makeDocumentWithContentsOfFile:(NSString *)path ofType:(NSString *)type {
   id    result;
   Class class=[self documentClassForType:type];

   result=[[[class alloc] initWithContentsOfFile:path ofType:type] autorelease];

   return result;
}

-makeDocumentWithContentsOfURL:(NSURL *)url ofType:(NSString *)type error:(NSError **)error {
   Class class=[self documentClassForType:type];

   if(class==Nil){
    if(error!=NULL)
     *error=[NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError
      userInfo:[NSDictionary dictionaryWithObject:url forKey:NSURLErrorKey]];
    return nil;
   }
   return [[[class alloc] initWithContentsOfURL:url ofType:type error:error] autorelease];
}

-makeDocumentForURL:(NSURL *)url withContentsOfURL:(NSURL *)contentsURL ofType:(NSString *)type error:(NSError **)error {
   id    result;
   Class class=[self documentClassForType:type];

   result=[[[class alloc] initForURL:url withContentsOfURL:contentsURL ofType:type error:error] autorelease];

   return result;
}

-(id)makeUntitledDocumentOfType:(NSString *)type {
   static int nextUntitledNumber=1;
   id    result;
   Class class=[self documentClassForType:type];

   NSError *error;
   result=[[[class alloc] initWithType:type error:&error] autorelease];
   if (result)
    [result _setUntitledNumber:nextUntitledNumber++];
   else
    [self presentError:error];

   return result;
}

-(id)makeUntitledDocumentOfType:(NSString *)type error:(NSError **)outError {
   /* Cocoa documentation says:
        "For backward binary compatibility with Mac OS X v10.3 and earlier,
         the default implementation of this method instead invokes makeUntitledDocumentOfType: if it is overridden."
      Hence we need to check if this class is a subclass that has this method overridden.
   */
   IMP mine=[NSDocumentController instanceMethodForSelector:@selector(makeUntitledDocumentOfType:)];
   IMP theirs=[self methodForSelector:@selector(makeUntitledDocumentOfType:)];   
   if (mine != theirs) {
    return [self makeUntitledDocumentOfType:type];
   }

   static int nextUntitledNumber=1;
   id    result;
   Class class=[self documentClassForType:type];

   NSError *error;
   result=[[[class alloc] initWithType:type error:&error] autorelease];
   if (result)
    [result _setUntitledNumber:nextUntitledNumber++];
   else {
    if (outError) *outError = error;
    else [self presentError:error];
   }

   return result;
}

-openUntitledDocumentOfType:(NSString *)type display:(BOOL)display {
   NSDocument *result=[self makeUntitledDocumentOfType:type error:NULL];

   if(result!=nil)
    [self addDocument:result];

   [result makeWindowControllers];

   if(display)
    [result showWindows];

   return result;
}

-openUntitledDocumentAndDisplay:(BOOL)display error:(NSError **)error {
   NSString   *type=[self defaultType];

   /* Cocoa documentation says:
        "For backward binary compatibility with Mac OS X v10.3 and earlier,
         the default implementation of this method instead invokes openUntitledDocumentOfType:display: if it is overridden."
      Hence we need to check if this class is a subclass that has this method overridden.
   */
   IMP mine=[NSDocumentController instanceMethodForSelector:@selector(openUntitledDocumentOfType:display:)];
   IMP theirs=[self methodForSelector:@selector(openUntitledDocumentOfType:display:)];
   if (mine != theirs) {
    return [self openUntitledDocumentOfType:type display:display];
   }
   
   NSDocument *result=[self makeUntitledDocumentOfType:type error:error];

   if(result!=nil)
    [self addDocument:result];

   [result makeWindowControllers];

   if(display)
    [result showWindows];

   return result;
}

-openDocumentWithContentsOfFile:(NSString *)path display:(BOOL)display {
   NSString   *extension=[path pathExtension];
   NSString   *type=[self typeFromFileExtension:extension];
   NSDocument *result=[self makeDocumentWithContentsOfFile:path ofType:type];

   if(result!=nil)
    [self addDocument:result];

   [result makeWindowControllers];

   if(display)
    [result showWindows];

   [self noteNewRecentDocument:result];

   return result;
}

/* The untitled document opened as the app launched, while untouched: the
   first document opened takes its place (as on the reference). */
static NSDocument *transientDocument;

-(void)_noteTransientDocument:(NSDocument *)document {
   transientDocument=document;
}

-(NSDocument *)_transientDocumentToReplace {
   NSDocument *doc=transientDocument;

   if(doc==nil || ![_documents containsObject:doc] || [doc fileURL]!=nil || [doc isDocumentEdited] ||
      [_documents count]!=1){
    transientDocument=nil;
    return nil;
   }
   return doc;
}

/* Opens url: the document already open for it, or a new one of the type
   its contents have; replaces an untouched untitled document; shows its
   windows when asked; the handler hears of it (or of the error). */
-(void)openDocumentWithContentsOfURL:(NSURL *)url display:(BOOL)display
   completionHandler:(void (^)(NSDocument *document, BOOL documentWasAlreadyOpen, NSError *error))completionHandler {
   NSDocument *document=[self documentForURL:url];
   NSError    *error=nil;

   if(document!=nil){
    if(display)
     [document showWindows];
    if(completionHandler!=nil)
     completionHandler(document,YES,nil);
    return;
   }
   NSString *type=[self typeForContentsOfURL:url error:&error];

   document=(type==nil)?nil:[self makeDocumentWithContentsOfURL:url ofType:type error:&error];
   if(document!=nil){
    NSDocument *replaced=[self _transientDocumentToReplace];
    NSWindow   *place=[[[replaced windowControllers] firstObject] window];
    NSRect      frame=[place frame];

    [self addDocument:document];
    [document makeWindowControllers];
    if(replaced!=nil){
     transientDocument=nil;
     /* Where the untitled window was. */
     for(NSWindowController *controller in [document windowControllers])
      if([controller window]!=nil && place!=nil)
       [[controller window] setFrameTopLeftPoint:NSMakePoint(NSMinX(frame),NSMaxY(frame))];
     [replaced close];
    }
    if(display)
     [document showWindows];
    [self noteNewRecentDocument:document];
   }
   else if(error==nil)
    error=[NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError
     userInfo:[NSDictionary dictionaryWithObject:url forKey:NSURLErrorKey]];
   if(completionHandler!=nil)
    completionHandler(document,NO,document!=nil?nil:error);
}

/* The older way, through the newer: a subclass that overrides either
   one is called. */
-openDocumentWithContentsOfURL:(NSURL *)url display:(BOOL)display error:(NSError **)error {
   IMP mine=[NSDocumentController instanceMethodForSelector:@selector(openDocumentWithContentsOfFile:display:)];
   IMP theirs=[self methodForSelector:@selector(openDocumentWithContentsOfFile:display:)];
   __block NSDocument *result=nil;
   __block NSError    *failure=nil;

   if([url isFileURL] && mine!=theirs)
    return [self openDocumentWithContentsOfFile:[url path] display:display];
   [self openDocumentWithContentsOfURL:url display:display completionHandler:^(NSDocument *document,BOOL open,NSError *e){
    result=[document retain];
    failure=[e retain];
   }];
   if(error!=NULL)
    *error=[failure autorelease];
   else
    [failure release];
   return [result autorelease];
}

/* What the system opens (LaunchServices, Open, Open Recent): through the
   newer method, an error shown as the reference shows it. */
-(void)_openDocumentPresentingError:(NSURL *)url {
   [self openDocumentWithContentsOfURL:url display:YES completionHandler:^(NSDocument *document,BOOL open,NSError *error){
    if(document==nil && error!=nil && !([[error domain] isEqualToString:NSCocoaErrorDomain] && [error code]==NSUserCancelledError))
     [self presentError:error];
   }];
}


-(BOOL)reopenDocumentForURL:(NSURL *)url withContentsOfURL:(NSURL *)contentsUL error:(NSError **)error {
   NSUnimplementedMethod();
   return 0;
}

-(void)_closeDocumentsStartingWith:(NSDocument *)document 
                       shouldClose:(BOOL)shouldClose 
                   closeAllContext:(NSDictionary *)context
{
  // This is a recursive callback method. Start it by passing in a document of nil.
  void (*delegateMethod)(id, SEL, id, BOOL, void *);
  
  if (shouldClose)
    {
      [document close];
      if ([_documents count] > 0)
        {
          [[_documents lastObject] canCloseDocumentWithDelegate:self 
                                            shouldCloseSelector:@selector(_closeDocumentsStartingWith:shouldClose:closeAllContext:)
                                                    contextInfo:context];
          return;
        }
    }
  
  // Inform our closeAllDocuments delegate of the results.
  id delegate = [context objectForKey:@"delegate"];
  SEL selector = NSSelectorFromString([context objectForKey:@"selector"]);
  void * info = [[context objectForKey:@"contextInfo"] pointerValue];
  [context release];
  if ([delegate respondsToSelector:selector])
    {
      void (*delegateMethod)(id, SEL, id, BOOL, void *);
      delegateMethod = (void (*)(id, SEL, id, BOOL, void *))[delegate methodForSelector:selector];
      delegateMethod(delegate, selector, self, ([_documents count] == 0), info);
    }
}

-(void)closeAllDocumentsWithDelegate:delegate didCloseAllSelector:(SEL)selector contextInfo:(void *)info 
{
  NSDictionary * closeAllContext = [[NSDictionary alloc] initWithObjectsAndKeys:
                                    delegate, @"delegate",
                                    NSStringFromSelector(selector), @"selector",
                                    [NSValue valueWithPointer:info], @"contextInfo",
                                    nil];
  [self _closeDocumentsStartingWith:nil shouldClose:YES closeAllContext:closeAllContext];
}

-(void)reviewUnsavedDocumentsWithAlertTitle:(NSString *)title cancellable:(BOOL)cancellable delegate:delegate didReviewAllSelector:(SEL)selector info:(void *)info {
   NSUnimplementedMethod();
}

-(NSError *)willPresentError:(NSError *)error {
// do nothing
   return error;
}

-(BOOL)presentError:(NSError *)error {
	return [NSApp presentError:[self willPresentError:error]];
}

-(void)presentError:(NSError *)error modalForWindow:(NSWindow *)window delegate:delegate didPresentSelector:(SEL)selector contextInfo:(void *)info {
	[NSApp presentError:[self willPresentError:error] modalForWindow:window delegate:delegate didPresentSelector:selector contextInfo:info];
}

-(NSInteger)runModalOpenPanel:(NSOpenPanel *)openPanel forTypes:(NSArray *)extensions {
  int result = [openPanel runModalForDirectory:[self currentDirectory] file:nil types:extensions];
  if (result)
    [[NSUserDefaults standardUserDefaults] setObject:[openPanel directory] 
                                              forKey:@"NSNavLastRootDirectory"];
  return result;
}

-(NSOpenPanel *)_runOpenPanel {
   NSOpenPanel *openPanel=[NSOpenPanel openPanel];

   [openPanel setAllowsMultipleSelection:YES];

   if([self runModalOpenPanel:openPanel forTypes:[self _allFileExtensions]]){
    return openPanel;
   }
    
   return nil;
}

-(NSArray *)fileNamesFromRunningOpenPanel {
   return [[self _runOpenPanel] filenames];
}

-(NSArray *)URLsFromRunningOpenPanel {
   return [[self _runOpenPanel] URLs];
}

-(NSMutableArray *)_recentDocumentPaths {
   NSFileManager  *fileManager = [NSFileManager defaultManager];
   NSMutableArray *paths=[NSMutableArray arrayWithArray:[[NSUserDefaults standardUserDefaults] arrayForKey:@"NSRecentDocumentPaths"]];
   int             i,count=[paths count];

   for (i=count-1;i>=0;i--)
    if (![fileManager fileExistsAtPath:[paths objectAtIndex:i]])
     [paths removeObjectAtIndex:i];

   if ([paths count]!=count)
     [[NSUserDefaults standardUserDefaults] setObject:paths forKey:@"NSRecentDocumentPaths"];

   return paths;
}

-(void)_openRecentDocument:sender {
   NSArray    *paths=[self _recentDocumentPaths];
   NSMenuItem *item=sender;
   int         tag=[item tag];

   if(tag>=0 && tag<[paths count]){
    NSError *error=nil;
    NSURL   *url=[NSURL fileURLWithPath:[paths objectAtIndex:tag]];
    
    if (![self openDocumentWithContentsOfURL:url display:YES error:&error] || error) {
     if ([url isFileURL]) {
      NSObject <NSApplicationDelegate> *appDelegate = [NSApp delegate];
      if ([appDelegate respondsToSelector:@selector(application:openFiles:)]) {
       [appDelegate application:NSApp openFiles:[NSArray arrayWithObject:[url path]]];       
      } else if ([appDelegate respondsToSelector:@selector(application:openFile:)]) {
       [appDelegate application:NSApp openFile:[url path]];       
      }
     }
    }
   }
}

-(void)_removeAllRecentDocumentsFromMenu:(NSMenu *)menu {
   int count=[[menu itemArray] count];

   while(--count>=0){
    NSMenuItem *check=[[menu itemArray] objectAtIndex:count];
    
    if(sel_isEqual([check action],@selector(_openRecentDocument:))){
     [menu removeItemAtIndex:count];
    }
   }
}

-(void)_updateRecentDocumentsMenu {
   NSMenu         *menu=[[NSApp mainMenu] _menuWithName:@"_NSRecentDocumentsMenu"];
   NSArray        *array=[self _recentDocumentPaths];
   int             i,j,count=[array count];
   NSMutableArray *lastPathArray=[[NSMutableArray alloc] init];
 
   [self _removeAllRecentDocumentsFromMenu:menu];
   
   if([[menu itemArray] count]>0){
    if([array count]==0){
     if([[[menu itemArray] objectAtIndex:0] isSeparatorItem])
      [menu removeItemAtIndex:0];
    }
    else {
     if(![[[menu itemArray] objectAtIndex:0] isSeparatorItem])
      [menu insertItem:[NSMenuItem separatorItem] atIndex:0];
    }
   }

   // Shorten entries to the last path component, but not for then-duplicates.
   for(i=0;i<count;i++){
    NSString *lastPath=[(NSString *)[array objectAtIndex:i] lastPathComponent];
    NSInteger occurences=0;

    for(j=0;j<count;j++){
     if([[(NSString *)[array objectAtIndex:j] lastPathComponent] isEqualToString:lastPath])
      occurences++;
    }
    if(occurences>1)
     [lastPathArray addObject:[array objectAtIndex:i]];
    else
     [lastPathArray addObject:lastPath];
   }

   while(--count>=0){
    NSString   *path=[lastPathArray objectAtIndex:count];
    NSMenuItem *item=[[[NSMenuItem alloc] initWithTitle:path action:@selector(_openRecentDocument:) keyEquivalent:nil] autorelease];
    
    [item setTag:count];
    [menu insertItem:item atIndex:0];
   }
   [lastPathArray release];
}

-(NSArray *)recentDocumentURLs {
   NSArray        *paths=[self _recentDocumentPaths];
   int             i,count=[paths count];
   NSMutableArray *result=[NSMutableArray arrayWithCapacity:count];
   
   for(i=0;i<count;i++)
    [result addObject:[NSURL fileURLWithPath:[paths objectAtIndex:i]]];
    
   return result;
}

-(NSUInteger)maximumRecentDocumentCount {
   NSString *value=[[NSUserDefaults standardUserDefaults] stringForKey:@"NSRecentDocumentMaximum"];
   
   return (value==nil)?10:[value intValue];
}

-(void)noteNewRecentDocumentURL:(NSURL *)url {
   NSString       *path=[url path];
   NSMutableArray *array=[self _recentDocumentPaths];

   /* Also among the system's recent documents (the system menu's Recent
      Items), newest first. */
   if([url isFileURL]){
    LSSharedFileListRef list=LSSharedFileListCreate(NULL,kLSSharedFileListRecentDocumentItems,NULL);
    if(list!=NULL){
     LSSharedFileListItemRef item=LSSharedFileListInsertItemURL(list,kLSSharedFileListItemBeforeFirst,NULL,NULL,
        (CFURLRef)url,NULL,NULL);
     if(item!=NULL)
      CFRelease(item);
     CFRelease(list);
    }
   }

   [array removeObject:path];
   [array insertObject:path atIndex:0];
   
   while([array count]>[self maximumRecentDocumentCount])
    [array removeLastObject];
    
   [[NSUserDefaults standardUserDefaults] setObject:array forKey:@"NSRecentDocumentPaths"];
   [self _updateRecentDocumentsMenu];
   [NSApp addRecentItem:url];
}

-(void)noteNewRecentDocument:(NSDocument *)document {
   NSURL *url=[document fileURL];
  
   if(url!=nil)
    [self noteNewRecentDocumentURL:url];
}

-(void)clearRecentDocuments:sender {
   NSArray *array=[NSArray array];
   
   [[NSUserDefaults standardUserDefaults] setObject:array forKey:@"NSRecentDocumentPaths"];
   [self _updateRecentDocumentsMenu];
}

-(void)newDocument:sender {
   [self openUntitledDocumentAndDisplay:YES error:NULL];
}

-(void)openDocument:sender {
   NSArray *files=[self fileNamesFromRunningOpenPanel];
   int      i,count=[files count];

   for(i=0;i<count;i++){
    NSError *error=nil;
    NSURL   *url=[NSURL fileURLWithPath:[files objectAtIndex:i]];

    (void)error;
    [self _openDocumentPresentingError:url];
   }
}

-(void)saveAllDocuments:sender {
   int i,count=[_documents count];
   
   while(--count>=0){
    NSDocument *document=[_documents objectAtIndex:count];
    
    if([document isDocumentEdited])
     [document saveDocument:sender];
   }
}

static BOOL actionIsDocumentController(SEL selector){
   if(sel_isEqual(selector,@selector(saveAllDocuments:)))
    return YES;
   if(sel_isEqual(selector,@selector(openDocument:)))
    return YES;
   if(sel_isEqual(selector,@selector(newDocument:)))
    return YES;
   if(sel_isEqual(selector,@selector(clearRecentDocuments:)))
    return YES;
   if(sel_isEqual(selector,@selector(_openRecentDocument:)))
    return YES;
   
   return NO;
}

-(BOOL)validateMenuItem:(NSMenuItem *)item {
   return (actionIsDocumentController([item action]) || [self respondsToSelector:[item action]]);
}

-(BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item {
   return (actionIsDocumentController([item action]) || [self respondsToSelector:[item action]]);
}

-(BOOL)application:sender openFile:(NSString *)path {
   NSError    *error=nil;
   NSURL      *url=[NSURL fileURLWithPath:path];
   NSDocument *document=[self openDocumentWithContentsOfURL:url display:YES error:&error];

   return (document!=nil)?YES:NO;
}

-(BOOL)closeAllDocuments {
   NSUnimplementedMethod();
   return NO;
}

-(BOOL)reviewUnsavedDocumentsWithAlertTitle:(NSString *)title cancellable:(BOOL)cancellable {
   NSUnimplementedMethod();
   return 0;
}

@end
