/* Copyright (c) 2006-2007 Christopher J. W. Lloyd <cjwl@objc.net>, 2008 Johannes Fortmann
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSSavePanel.h>
#import <AppKit/NSView.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSDisplay.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSPopUpButton.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSAlert.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSWorkspace.h>

/*
 * Minkar's save panel (the platform's NSDisplay had none): the file's
 * name, where it goes (the folder given, the usual ones and the
 * computer), Cancel and Save. Return saves, Escape cancels; a file that
 * exists is replaced only when the user says so. Atlas's provisional
 * look; folders are browsed as in the file manager later.
 */
static NSString *panelText(NSString *english) {
   return NSLocalizedStringFromTableInBundle(english, nil, [NSBundle bundleForClass:[NSSavePanel class]], nil);
}

@implementation NSSavePanel

+(NSSavePanel *)savePanel {
   return [[self new] autorelease];
}

-init {
   [super initWithContentRect:NSMakeRect(0,0,1,1) styleMask:NSTitledWindowMask|NSResizableWindowMask backing:NSBackingStoreBuffered defer:YES];

   _dialogTitle=@"";
   _filename=@"";
   _directory=[NSHomeDirectory() copy];
   _requiredFileType=@"";
   _treatsFilePackagesAsDirectories=NO;
   _accessoryView=nil;
   return self;
}

-(void)dealloc {
   [_dialogTitle release];
   [_filename release];
   [_directory release];
   [_requiredFileType release];
   [_accessoryView release];
   [_allowedFileTypes release];
   [_nameFieldLabel release];
   [_whereFolders release];
   [super dealloc];
}

-(void)_setFilename:(NSString*)filename {
   @synchronized(self) {
      if(filename!=_filename) {
         [_filename release];
         _filename=[filename copy];
         if(_filename==nil) {
            _filename=@"";
         }
      }
   }
}

-(NSURL *)URL {
   NSString *path=[self filename];

   return [path length] ? [NSURL fileURLWithPath:path] : nil;
}

-(NSString *)filename {
   id ret=nil;
	@synchronized(self)
	{
		ret=[[_filename copy] autorelease];
	}
   return ret;
}

-(int)runModalForDirectory:(NSString *)directory file:(NSString *)file {
   if([directory length])
      [self setDirectory:directory];
   if(file!=nil)
      [self _setFilename:file];
   return [self _runSavePanel];
}

-(int)runModal {
   return [self _runSavePanel];
}

/* The name as typed, the type added when it must have one. */
-(NSString *)_chosenName {
   NSString *name=[[_nameField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
   NSString *type=[_requiredFileType length] ? _requiredFileType : [_allowedFileTypes firstObject];

   if([name length]==0)
      return nil;
   /* No extension: the type's. One not allowed: the type's added. */
   if([type length]) {
      BOOL allowed=[[name pathExtension] caseInsensitiveCompare:type]==NSOrderedSame;
      for(NSString *t in _allowedFileTypes)
         allowed|=[[name pathExtension] caseInsensitiveCompare:t]==NSOrderedSame;
      if([[name pathExtension] length]==0 || (!allowed && !_allowsOtherFileTypes))
         name=[name stringByAppendingPathExtension:type];
   }
   return name;
}

-(void)_save:(id)sender {
   NSString *name=[self _chosenName];
   NSInteger where=[_wherePopUp indexOfSelectedItem];
   NSString *folder=(where>=0 && (NSUInteger)where<[_whereFolders count]) ? [_whereFolders objectAtIndex:where] : _directory;
   BOOL isDir=NO;

   if(name==nil || [name rangeOfString:@"/"].location!=NSNotFound ||
      ![[NSFileManager defaultManager] fileExistsAtPath:folder isDirectory:&isDir] || !isDir) {
      NSBeep();
      return;
   }
   NSString *path=[folder stringByAppendingPathComponent:name];
   if([[NSFileManager defaultManager] fileExistsAtPath:path]) {
      NSAlert *alert=[[[NSAlert alloc] init] autorelease];
      [alert setMessageText:[NSString stringWithFormat:panelText(@"“%@” already exists. Do you want to replace it?"), name]];
      [alert setInformativeText:panelText(@"Replacing it will overwrite its current contents.")];
      [alert addButtonWithTitle:panelText(@"Replace")];
      [alert addButtonWithTitle:panelText(@"Cancel")];
      if([alert runModal]!=NSAlertFirstButtonReturn)
         return;
   }
   [self setDirectory:folder];
   [self _setFilename:path];
   [NSApp stopModalWithCode:NSFileHandlingPanelOKButton];
}

-(void)_cancel:(id)sender {
   [NSApp stopModalWithCode:NSFileHandlingPanelCancelButton];
}

-(void)_addFolder:(NSString *)path title:(NSString *)title to:(NSMutableArray *)folders {
   path=[path stringByStandardizingPath];
   if([folders containsObject:path])
      return;
   [folders addObject:path];
   [_wherePopUp addItemWithTitle:title];
}

/* The panel's content, for this run. */
-(void)_buildSavePanel {
   NSString *home=NSHomeDirectory();
   NSMutableArray *folders=[NSMutableArray array];
   CGFloat width=480, height=[_message length] ? 168 : 140;
   NSView *content=[[[NSView alloc] initWithFrame:NSMakeRect(0,0,width,height)] autorelease];
   CGFloat top=height-20;

   [self _setPanelTitle:[_dialogTitle length] ? _dialogTitle : panelText(@"Save")];
   if([_message length]) {
      NSTextField *message=[[[NSTextField alloc] initWithFrame:NSMakeRect(20,top-18,width-40,18)] autorelease];
      [message setStringValue:_message];
      [message setEditable:NO];
      [message setBordered:NO];
      [message setDrawsBackground:NO];
      [content addSubview:message];
      top-=28;
   }
   NSTextField *nameLabel=[[[NSTextField alloc] initWithFrame:NSMakeRect(20,top-22,110,20)] autorelease];
   [nameLabel setStringValue:[_nameFieldLabel length] ? _nameFieldLabel : panelText(@"Save As:")];
   [nameLabel setEditable:NO];
   [nameLabel setBordered:NO];
   [nameLabel setDrawsBackground:NO];
   [nameLabel setAlignment:NSRightTextAlignment];
   [content addSubview:nameLabel];
   _nameField=[[[NSTextField alloc] initWithFrame:NSMakeRect(140,top-24,width-160,24)] autorelease];
   [_nameField setStringValue:[_filename lastPathComponent] ?: @""];
   [content addSubview:_nameField];

   NSTextField *whereLabel=[[[NSTextField alloc] initWithFrame:NSMakeRect(20,top-58,110,20)] autorelease];
   [whereLabel setStringValue:panelText(@"Where:")];
   [whereLabel setEditable:NO];
   [whereLabel setBordered:NO];
   [whereLabel setDrawsBackground:NO];
   [whereLabel setAlignment:NSRightTextAlignment];
   [content addSubview:whereLabel];
   _wherePopUp=[[[NSPopUpButton alloc] initWithFrame:NSMakeRect(140,top-62,width-160,26) pullsDown:NO] autorelease];
   if([_directory length])
      [self _addFolder:_directory title:[[_directory stringByStandardizingPath] isEqualToString:@"/"] ?
         panelText(@"Computer") : [_directory lastPathComponent] to:folders];
   [self _addFolder:[home stringByAppendingPathComponent:@"Documents"] title:panelText(@"Documents") to:folders];
   [self _addFolder:[home stringByAppendingPathComponent:@"Desktop"] title:panelText(@"Desktop") to:folders];
   [self _addFolder:[home stringByAppendingPathComponent:@"Downloads"] title:panelText(@"Downloads") to:folders];
   [self _addFolder:home title:[home lastPathComponent] to:folders];
   [_wherePopUp selectItemAtIndex:0];
   [content addSubview:_wherePopUp];
   [_whereFolders release];
   _whereFolders=[folders copy];

   NSButton *save=[[[NSButton alloc] initWithFrame:NSMakeRect(width-110,14,90,26)] autorelease];
   [save setTitle:[_prompt length] ? _prompt : panelText(@"Save")];
   [save setBezelStyle:NSRoundedBezelStyle];
   [save setKeyEquivalent:@"\r"];
   [save setTarget:self];
   [save setAction:@selector(_save:)];
   [content addSubview:save];
   NSButton *cancel=[[[NSButton alloc] initWithFrame:NSMakeRect(width-210,14,90,26)] autorelease];
   [cancel setTitle:panelText(@"Cancel")];
   [cancel setBezelStyle:NSRoundedBezelStyle];
   [cancel setKeyEquivalent:@"\033"];
   [cancel setTarget:self];
   [cancel setAction:@selector(_cancel:)];
   [content addSubview:cancel];

   [self setContentView:content];
   [self setContentSize:NSMakeSize(width,height)];
   [self center];
}

/* The window's own title (setTitle: keeps the dialog's, as the API has it). */
-(void)_setPanelTitle:(NSString *)title {
   [super setTitle:title];
}

-(int)_runSavePanel {
   [self _buildSavePanel];
   [self makeKeyAndOrderFront:nil];
   [self makeFirstResponder:_nameField];
   [_nameField selectText:nil];
   int result=[NSApp runModalForWindow:self];
   [self orderOut:nil];
   return result;
}

-(NSString *)nameFieldStringValue {
   return _nameField ? [_nameField stringValue] : [_filename lastPathComponent];
}

-(void)setNameFieldStringValue:(NSString *)value {
   [self _setFilename:[_directory stringByAppendingPathComponent:value ?: @""]];
   [_nameField setStringValue:value ?: @""];
}

-(NSString *)nameFieldLabel {
   return _nameFieldLabel;
}

-(void)setNameFieldLabel:(NSString *)value {
   value=[value copy];
   [_nameFieldLabel release];
   _nameFieldLabel=value;
}

-(NSURL *)directoryURL {
   return [_directory length] ? [NSURL fileURLWithPath:_directory isDirectory:YES] : nil;
}

-(void)setDirectoryURL:(NSURL *)url {
   if([url isFileURL])
      [self setDirectory:[url path]];
}

-(NSArray *)allowedFileTypes {
   return _allowedFileTypes;
}

-(BOOL)canCreateDirectories {
   return _canCreateDirectories;
}

-(BOOL)allowsOtherFileTypes {
   return _allowsOtherFileTypes;
}

/* Not a sheet yet: the panel runs modal on its own, then the handler. */
-(void)beginSheetModalForWindow:(NSWindow *)window completionHandler:(void (^)(NSInteger result))handler {
   NSInteger result=[self runModal];
   if(handler!=nil)
      handler(result);
}

-(void)beginWithCompletionHandler:(void (^)(NSInteger result))handler {
   [self beginSheetModalForWindow:nil completionHandler:handler];
}

-(NSString *)directory {
   return _directory;
}

-(BOOL)treatsFilePackagesAsDirectories {
   return _treatsFilePackagesAsDirectories;
}

-(NSView *)accessoryView {
   return _accessoryView;
}

-(void)setTitle:(NSString *)title {
   title=[title copy];
   [_dialogTitle release];
   _dialogTitle=title;
}

-(void)setDirectory:(NSString *)directory {
   directory=[directory copy];
   [_directory release];
   _directory=directory;
}

-(void)setRequiredFileType:(NSString *)type {
	@synchronized(self)
	{
		type=[type copy];
		[_requiredFileType release];
		_requiredFileType=type;
	}
}

-(NSString*)requiredFileType
{
   id ret=nil;
	@synchronized(self)
	{
		ret=[[_requiredFileType copy] autorelease];
	}
   return ret;
}

-(void)setMessage:(NSString*)message;
{
	@synchronized(self)
	{
		if(_message!=message)
		{
			[_message release];
			_message=[message copy];
		}
	}
}

-(NSString*)message;
{
   id ret=nil;
	@synchronized(self)
	{
		ret=[[_message copy] autorelease];
	}
   return ret;
}

-(void)setPrompt:(NSString*)prompt;
{
	@synchronized(self)
	{
		if(_prompt!=prompt)
		{
			[_prompt release];
			_prompt=[prompt copy];
		}
	}
}

-(NSString*)prompt;
{
	id ret=nil;
	@synchronized(self)
	{
		ret=[[_prompt copy] autorelease];
	}
	return ret;
}


-(void)setTreatsFilePackagesAsDirectories:(BOOL)flag {
   _treatsFilePackagesAsDirectories=flag;
}

-(void)setAccessoryView:(NSView *)view {
   view=[view retain];
   [_accessoryView release];
   _accessoryView=view;
}

-(void)setCanCreateDirectories:(BOOL)value {
   _canCreateDirectories=value;
}

-(void)setAllowedFileTypes:(NSArray *)value {
   value=[value copy];
   [_allowedFileTypes release];
   _allowedFileTypes=value;
}

-(void)setAllowsOtherFileTypes:(BOOL)value {
   _allowsOtherFileTypes=value;
}

- (void)beginSheetForDirectory:(NSString *)path
						  file:(NSString *)name 
				modalForWindow:(NSWindow *)docWindow
				 modalDelegate:(id)modalDelegate 
				didEndSelector:(SEL)didEndSelector 
				   contextInfo:(void *)contextInfo
{
	id inv=[NSInvocation invocationWithMethodSignature:[self methodSignatureForSelector:@selector(_background_beginSheetForDirectory:file:modalForWindow:modalDelegate:didEndSelector:contextInfo:)]];
	[inv setTarget:self];
	[inv setSelector:@selector(_background_beginSheetForDirectory:file:modalForWindow:modalDelegate:didEndSelector:contextInfo:)];
	[inv setArgument:&path atIndex:2];
	[inv setArgument:&name atIndex:3];
	[inv setArgument:&docWindow atIndex:4];
	[inv setArgument:&modalDelegate atIndex:5];
	[inv setArgument:&didEndSelector atIndex:6];
	[inv setArgument:&contextInfo atIndex:7];
	[inv retainArguments];
	[inv performSelectorInBackground:@selector(invoke) withObject:nil];
}

- (void)_selector_savePanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode  contextInfo:(void  *)contextInfo;
{
	
}

- (void)_background_beginSheetForDirectory:(NSString *)path
									  file:(NSString *)name 
							modalForWindow:(NSWindow *)docWindow
							 modalDelegate:(id)modalDelegate 
							didEndSelector:(SEL)didEndSelector 
							   contextInfo:(void *)contextInfo
{
	id pool=[NSAutoreleasePool new];
	int ret=[self runModalForDirectory:path file:name];
   
	id inv=[NSInvocation invocationWithMethodSignature:[self methodSignatureForSelector:@selector(_selector_savePanelDidEnd:returnCode:contextInfo:)]];
	
	[inv setTarget:modalDelegate];
	[inv setSelector:didEndSelector];
	[inv setArgument:&self atIndex:2];
	[inv setArgument:&ret atIndex:3];
	[inv setArgument:&contextInfo atIndex:4];
	[inv retainArguments];
	
	[inv performSelectorOnMainThread:@selector(invoke) withObject:nil waitUntilDone:YES];
   [pool release];
}

@end
