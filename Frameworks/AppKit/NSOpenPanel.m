/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSOpenPanel.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSDisplay.h>
#import <Foundation/NSURL.h>
#import <AppKit/NSView.h>
#import <AppKit/NSScrollView.h>
#import <AppKit/NSClipView.h>
#import <AppKit/NSPopUpButton.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSWorkspace.h>
#import <AppKit/NSStringDrawing.h>
#import <AppKit/NSAttributedString.h>

@interface NSSavePanel (Private) 
-(void)_setFilename:(NSString*)filename;
-(void)_setPanelTitle:(NSString *)title;
@end

@interface NSOpenPanel (Minkar)
-(void)_goToFolder:(NSString *)folder;
-(void)_activateEntryAtIndex:(NSInteger)index;
-(void)_selectionChanged;
@end

static NSString *openPanelText(NSString *english) {
   return NSLocalizedStringFromTableInBundle(english, nil, [NSBundle bundleForClass:[NSOpenPanel class]], nil);
}

/*
 * Minkar's open panel (the platform's NSDisplay had none): the folder
 * shown, a menu of the folders above it and the usual ones, its items in
 * a list (folders open with a double click; what cannot be chosen is
 * grey), Cancel and Open. Atlas's provisional look.
 */
@interface NSOpenPanelFileList : NSView {
@public
   NSOpenPanel *_panel;
   NSArray *_entries;           /* dictionaries: name, path, folder, enabled */
   NSMutableIndexSet *_selected;
   BOOL _multiple;
}
@end

static const CGFloat kOpenRowHeight=20;

@implementation NSOpenPanelFileList

-(instancetype)initWithFrame:(NSRect)frame {
   self=[super initWithFrame:frame];
   _selected=[NSMutableIndexSet new];
   _entries=[NSArray new];
   return self;
}

-(void)dealloc {
   [_entries release];
   [_selected release];
   [super dealloc];
}

-(BOOL)isFlipped { return YES; }
-(BOOL)isOpaque { return YES; }
-(BOOL)acceptsFirstResponder { return YES; }

-(void)setEntries:(NSArray *)entries {
   entries=[entries copy];
   [_entries release];
   _entries=entries;
   [_selected removeAllIndexes];
   NSScrollView *scroll=[self enclosingScrollView];
   CGFloat height=MAX([_entries count]*kOpenRowHeight+4, scroll ? NSHeight([[scroll contentView] bounds]) : 0);
   [self setFrameSize:NSMakeSize(scroll ? NSWidth([[scroll contentView] bounds]) : NSWidth([self frame]), height)];
   [self scrollPoint:NSZeroPoint];
   [self setNeedsDisplay:YES];
}

-(NSArray *)entries { return _entries; }
-(NSIndexSet *)selectedIndexes { return _selected; }

-(NSRect)rectForRow:(NSUInteger)row {
   return NSMakeRect(0, 2+row*kOpenRowHeight, NSWidth([self bounds]), kOpenRowHeight);
}

-(void)drawRect:(NSRect)dirty {
   NSFont *font=[NSFont systemFontOfSize:12];
   NSDictionary *plain=[NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, [NSColor blackColor], NSForegroundColorAttributeName, nil];
   NSDictionary *grey=[NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, [NSColor colorWithCalibratedWhite:0.6 alpha:1], NSForegroundColorAttributeName, nil];
   NSDictionary *white=[NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, [NSColor whiteColor], NSForegroundColorAttributeName, nil];
   BOOL key=[[self window] firstResponder]==self;

   [[NSColor whiteColor] set];
   NSRectFill(dirty);
   for(NSUInteger i=0;i<[_entries count];i++) {
      NSRect row=[self rectForRow:i];
      if(!NSIntersectsRect(row,dirty))
         continue;
      NSDictionary *entry=[_entries objectAtIndex:i];
      BOOL selected=[_selected containsIndex:i], enabled=[[entry objectForKey:@"enabled"] boolValue];
      if(selected) {
         [(key ? [NSColor colorWithCalibratedRed:0.25 green:0.50 blue:0.85 alpha:1] : [NSColor colorWithCalibratedWhite:0.84 alpha:1]) set];
         NSRectFill(row);
      }
      NSImage *icon=[[NSWorkspace sharedWorkspace] iconForFile:[entry objectForKey:@"path"]];
      [icon drawInRect:NSMakeRect(8,NSMinY(row)+2,16,16) fromRect:NSZeroRect operation:NSCompositeSourceOver
         fraction:enabled ? 1 : 0.4 respectFlipped:YES hints:nil];
      NSDictionary *attributes=selected && key ? white : (enabled ? plain : grey);
      NSString *name=[entry objectForKey:@"name"];
      NSSize size=[name sizeWithAttributes:attributes];
      [name drawAtPoint:NSMakePoint(30,NSMinY(row)+floor((kOpenRowHeight-size.height)/2)) withAttributes:attributes];
      if([[entry objectForKey:@"folder"] boolValue])
         [@"›" drawAtPoint:NSMakePoint(NSWidth(row)-16,NSMinY(row)+floor((kOpenRowHeight-size.height)/2)) withAttributes:selected && key ? white : grey];
   }
}

-(void)selectRow:(NSInteger)row extend:(BOOL)extend {
   if(!extend || !_multiple)
      [_selected removeAllIndexes];
   if(row>=0 && (NSUInteger)row<[_entries count]) {
      [_selected addIndex:row];
      [self scrollRectToVisible:[self rectForRow:row]];
   }
   [self setNeedsDisplay:YES];
   [_panel _selectionChanged];
}

-(void)mouseDown:(NSEvent *)event {
   NSPoint p=[self convertPoint:[event locationInWindow] fromView:nil];
   NSInteger row=(NSInteger)floor((p.y-2)/kOpenRowHeight);
   BOOL extend=([event modifierFlags]&(NSCommandKeyMask|NSShiftKeyMask))!=0;

   [[self window] makeFirstResponder:self];
   if(p.y<2 || row>=(NSInteger)[_entries count])
      row=-1;
   if(extend && row>=0 && [_selected containsIndex:row] && _multiple) {
      [_selected removeIndex:row];
      [self setNeedsDisplay:YES];
      [_panel _selectionChanged];
      return;
   }
   [self selectRow:row extend:extend];
   if([event clickCount]==2 && row>=0)
      [_panel _activateEntryAtIndex:row];
}

-(void)keyDown:(NSEvent *)event {
   NSString *chars=[event charactersIgnoringModifiers];
   unichar c=[chars length] ? [chars characterAtIndex:0] : 0;
   NSInteger count=[_entries count], current=[_selected count] ? (NSInteger)[_selected lastIndex] : -1;

   if(c==NSUpArrowFunctionKey && ([event modifierFlags]&NSCommandKeyMask)) {
      NSString *folder=[_panel directory];
      if(![folder isEqualToString:@"/"])
         [_panel _goToFolder:[folder stringByDeletingLastPathComponent]];
      return;
   }
   if(c==NSDownArrowFunctionKey && ([event modifierFlags]&NSCommandKeyMask)) {
      if(current>=0)
         [_panel _activateEntryAtIndex:current];
      return;
   }
   if(c==NSDownArrowFunctionKey && count>0) {
      [self selectRow:current<0 ? 0 : MIN(current+1,count-1) extend:NO];
      return;
   }
   if(c==NSUpArrowFunctionKey && count>0) {
      [self selectRow:current<0 ? 0 : MAX(current-1,0) extend:NO];
      return;
   }
   [super keyDown:event];
}

@end

@implementation NSOpenPanel

+(NSOpenPanel *)openPanel {
   return [[self new] autorelease];
}

-init {
   [super init];
   _filenames=[NSArray new];
   [_dialogTitle release];
   _dialogTitle= [NSLocalizedStringFromTableInBundle(@"Open", nil, [NSBundle bundleForClass: [NSOpenPanel class]], @"The title of the open panel") copy];
   _allowsMultipleSelection=NO;
   _canChooseDirectories=NO;
   _canChooseFiles=YES;
   _resolvesAliases=YES;
   return self;
}

-(void)dealloc {
   [_filenames release];
   [_fileList release];
   [_types release];
   [super dealloc];
}

-(NSArray *)filenames {
   id ret=nil;
   @synchronized(self)
   {
      ret=[[_filenames copy] autorelease];
   }
   return ret;
}

-(NSArray *)URLs {
   NSArray        *paths=[self filenames];
   NSMutableArray *result=[NSMutableArray arrayWithCapacity:[paths count]];
   int             i,count=[paths count];
   
   for(i=0;i<count;i++)
    [result addObject:[NSURL fileURLWithPath:[paths objectAtIndex:i]]];
    
   return result;
}

-(NSInteger)runModalForDirectory:(NSString *)directory file:(NSString *)file types:(NSArray *)types {
   BOOL isDir=NO;

   if(file!=nil)
      [self _setFilename:file];
   if([directory length])
      [self setDirectory:directory];
   if(![[NSFileManager defaultManager] fileExistsAtPath:[self directory] isDirectory:&isDir] || !isDir)
      [self setDirectory:NSHomeDirectory()];
   [_types release];
   _types=[types copy];
   return [self _runOpenPanel];
}

/* -- The panel -- */

/* An item may be chosen: a folder when folders may, else a file of an
 * allowed type when files may (packages count as files). */
-(BOOL)_canChoosePath:(NSString *)path folder:(BOOL)folder {
   if(folder)
      return _canChooseDirectories;
   if(!_canChooseFiles)
      return NO;
   NSArray *types=[_types count] ? _types : [self allowedFileTypes];
   if([types count]==0)
      return YES;
   for(NSString *type in types)
      if([[path pathExtension] caseInsensitiveCompare:type]==NSOrderedSame)
         return YES;
   return NO;
}

-(BOOL)_isFolderToBrowse:(NSString *)path {
   BOOL isDir=NO;

   if(![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDir] || !isDir)
      return NO;
   if(_treatsFilePackagesAsDirectories)
      return YES;
   NSString *ext=[[path pathExtension] lowercaseString];
   return !([ext isEqualToString:@"app"] || [ext isEqualToString:@"bundle"] || [ext isEqualToString:@"framework"] ||
      [ext isEqualToString:@"prefpane"] || [ext isEqualToString:@"plugin"]);
}

-(void)_fillFolderMenu {
   NSString *home=NSHomeDirectory();
   NSMutableArray *folders=[NSMutableArray array];
   NSString *folder=[_directory stringByStandardizingPath];

   [_wherePopUp removeAllItems];
   /* The folder and those above it, then the usual ones. */
   for(NSString *f=folder;;f=[f stringByDeletingLastPathComponent]) {
      [folders addObject:f];
      [[_wherePopUp menu] addItemWithTitle:[f isEqualToString:@"/"] ? openPanelText(@"Computer") : [f lastPathComponent]
         action:NULL keyEquivalent:@""];
      if([f isEqualToString:@"/"] || [f length]==0)
         break;
   }
   [[_wherePopUp menu] addItem:[NSMenuItem separatorItem]];
   [folders addObject:@""];
   NSArray *usual=[NSArray arrayWithObjects:home,[home stringByAppendingPathComponent:@"Desktop"],
      [home stringByAppendingPathComponent:@"Documents"],[home stringByAppendingPathComponent:@"Downloads"],@"/Applications",nil];
   NSArray *titles=[NSArray arrayWithObjects:[home lastPathComponent],openPanelText(@"Desktop"),
      openPanelText(@"Documents"),openPanelText(@"Downloads"),openPanelText(@"Applications"),nil];
   for(NSUInteger i=0;i<[usual count];i++) {
      [folders addObject:[usual objectAtIndex:i]];
      /* Through the menu: a title may repeat one above (the home folder). */
      [[_wherePopUp menu] addItemWithTitle:[titles objectAtIndex:i] action:NULL keyEquivalent:@""];
   }
   [_wherePopUp selectItemAtIndex:0];
   [_whereFolders release];
   _whereFolders=[folders copy];
}

-(void)_goToFolder:(NSString *)folder {
   NSFileManager *fm=[NSFileManager defaultManager];
   NSArray *names=[fm contentsOfDirectoryAtPath:folder error:NULL];
   NSMutableArray *entries=[NSMutableArray array];

   if(names==nil) {
      NSBeep();
      return;
   }
   [self setDirectory:[folder stringByStandardizingPath]];
   for(NSString *name in [names sortedArrayUsingSelector:@selector(localizedStandardCompare:)]) {
      if([name hasPrefix:@"."])
         continue;
      NSString *path=[folder stringByAppendingPathComponent:name];
      BOOL browse=[self _isFolderToBrowse:path];
      [entries addObject:[NSDictionary dictionaryWithObjectsAndKeys:name,@"name",path,@"path",
         [NSNumber numberWithBool:browse],@"folder",
         [NSNumber numberWithBool:browse || [self _canChoosePath:path folder:NO]],@"enabled",nil]];
   }
   [(NSOpenPanelFileList *)_fileList setEntries:entries];
   [self _fillFolderMenu];
   [self _selectionChanged];
}

-(void)_chooseFolder:(id)sender {
   NSInteger i=[_wherePopUp indexOfSelectedItem];

   if(i>=0 && (NSUInteger)i<[_whereFolders count] && [[_whereFolders objectAtIndex:i] length])
      [self _goToFolder:[_whereFolders objectAtIndex:i]];
}

/* Open enabled with something to choose. */
-(void)_selectionChanged {
   NSButton *open=[[self contentView] viewWithTag:1];
   NSIndexSet *selected=[(NSOpenPanelFileList *)_fileList selectedIndexes];
   NSArray *entries=[(NSOpenPanelFileList *)_fileList entries];
   BOOL ok=[selected count]==0 ? _canChooseDirectories : YES;

   for(NSUInteger i=[selected firstIndex];i!=NSNotFound;i=[selected indexGreaterThanIndex:i]) {
      NSDictionary *entry=[entries objectAtIndex:i];
      BOOL folder=[[entry objectForKey:@"folder"] boolValue];
      if(!(folder || [self _canChoosePath:[entry objectForKey:@"path"] folder:NO]))
         ok=NO;
   }
   [open setEnabled:ok];
}

/* A double click (or Command-Down): into a folder, else choose it. */
-(void)_activateEntryAtIndex:(NSInteger)index {
   NSDictionary *entry=[[(NSOpenPanelFileList *)_fileList entries] objectAtIndex:index];

   if([[entry objectForKey:@"folder"] boolValue])
      [self _goToFolder:[entry objectForKey:@"path"]];
   else if([[entry objectForKey:@"enabled"] boolValue])
      [self _open:nil];
}

-(void)_open:(id)sender {
   NSIndexSet *selected=[(NSOpenPanelFileList *)_fileList selectedIndexes];
   NSArray *entries=[(NSOpenPanelFileList *)_fileList entries];
   NSMutableArray *chosen=[NSMutableArray array];

   if([selected count]==0) {
      if(!_canChooseDirectories)
         return;
      [chosen addObject:[self directory]];
   }
   for(NSUInteger i=[selected firstIndex];i!=NSNotFound;i=[selected indexGreaterThanIndex:i]) {
      NSDictionary *entry=[entries objectAtIndex:i];
      NSString *path=[entry objectForKey:@"path"];
      BOOL folder=[[entry objectForKey:@"folder"] boolValue];
      /* One folder that cannot be chosen: go into it. */
      if(folder && !_canChooseDirectories) {
         if([selected count]==1)
            [self _goToFolder:path];
         else
            NSBeep();
         return;
      }
      if(![self _canChoosePath:path folder:folder]) {
         NSBeep();
         return;
      }
      [chosen addObject:path];
   }
   @synchronized(self) {
      [_filenames release];
      _filenames=[chosen copy];
   }
   [self _setFilename:[chosen firstObject]];
   [NSApp stopModalWithCode:NSFileHandlingPanelOKButton];
}

-(void)_cancelOpen:(id)sender {
   [NSApp stopModalWithCode:NSFileHandlingPanelCancelButton];
}

-(void)_buildOpenPanel {
   CGFloat width=560, height=[_message length] ? 400 : 372;
   NSView *content=[[[NSView alloc] initWithFrame:NSMakeRect(0,0,width,height)] autorelease];
   CGFloat top=height-14;

   [self _setPanelTitle:[_dialogTitle length] ? _dialogTitle : openPanelText(@"Open")];
   if([_message length]) {
      NSTextField *message=[[[NSTextField alloc] initWithFrame:NSMakeRect(20,top-18,width-40,18)] autorelease];
      [message setStringValue:_message];
      [message setEditable:NO];
      [message setBordered:NO];
      [message setDrawsBackground:NO];
      [content addSubview:message];
      top-=26;
   }
   _wherePopUp=[[[NSPopUpButton alloc] initWithFrame:NSMakeRect(floor((width-240)/2),top-26,240,26) pullsDown:NO] autorelease];
   [_wherePopUp setTarget:self];
   [_wherePopUp setAction:@selector(_chooseFolder:)];
   [content addSubview:_wherePopUp];

   NSScrollView *scroll=[[[NSScrollView alloc] initWithFrame:NSMakeRect(20,54,width-40,top-36-54)] autorelease];
   [scroll setHasVerticalScroller:YES];
   [scroll setBorderType:NSBezelBorder];
   NSOpenPanelFileList *list=[[NSOpenPanelFileList alloc] initWithFrame:[[scroll contentView] bounds]];
   list->_panel=self;
   list->_multiple=_allowsMultipleSelection;
   [scroll setDocumentView:list];
   [content addSubview:scroll];
   [_fileList release];
   _fileList=list;

   NSButton *open=[[[NSButton alloc] initWithFrame:NSMakeRect(width-110,14,90,26)] autorelease];
   [open setTitle:[_prompt length] ? _prompt : openPanelText(@"Open")];
   [open setBezelStyle:NSRoundedBezelStyle];
   [open setKeyEquivalent:@"\r"];
   [open setTag:1];
   [open setTarget:self];
   [open setAction:@selector(_open:)];
   [content addSubview:open];
   NSButton *cancel=[[[NSButton alloc] initWithFrame:NSMakeRect(width-210,14,90,26)] autorelease];
   [cancel setTitle:openPanelText(@"Cancel")];
   [cancel setBezelStyle:NSRoundedBezelStyle];
   [cancel setKeyEquivalent:@"\033"];
   [cancel setTarget:self];
   [cancel setAction:@selector(_cancelOpen:)];
   [content addSubview:cancel];

   [self setContentView:content];
   [self setContentSize:NSMakeSize(width,height)];
   [self center];
   [self _goToFolder:[self directory]];
}

-(NSInteger)_runOpenPanel {
   [self _buildOpenPanel];
   [self makeKeyAndOrderFront:nil];
   [self makeFirstResponder:_fileList];
   int result=[NSApp runModalForWindow:self];
   [self orderOut:nil];
   return result;
}

-(NSInteger)runModalForTypes:(NSArray *)types {
   return [self runModalForDirectory:[self directory] file:nil types:types];
}

-(NSInteger)runModalForDirectory:(NSString *)directory file:(NSString *)file {
   return [self runModalForDirectory:directory file:file types:nil];
}

-(NSInteger)runModal {
   return [self runModalForDirectory:nil file:nil types:nil];
}

-(BOOL)allowsMultipleSelection {
   return _allowsMultipleSelection;
}

-(BOOL)canChooseDirectories {
   return _canChooseDirectories;
}

-(BOOL)canChooseFiles {
   return _canChooseFiles;
}

-(BOOL)resolvesAliases {
   return _resolvesAliases;
}

-(void)setAllowsMultipleSelection:(BOOL)flag {
   _allowsMultipleSelection=flag;
}

-(void)setCanChooseDirectories:(BOOL)flag {
   _canChooseDirectories=flag;
}

-(void)setCanChooseFiles:(BOOL)flag {
   _canChooseFiles=flag;
}

-(void)setResolvesAliases:(BOOL)value {
   _resolvesAliases=value;
}

#pragma mark -
#pragma mark Sheet methods
- (void)beginSheetForDirectory:(NSString *)path
                          file:(NSString *)name 
                         types:(NSArray *)fileTypes
                modalForWindow:(NSWindow *)docWindow
                 modalDelegate:(id)modalDelegate 
                didEndSelector:(SEL)didEndSelector 
                   contextInfo:(void *)contextInfo
{
	id inv=[NSInvocation invocationWithMethodSignature:[self methodSignatureForSelector:@selector(_background_beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo:)]];
	[inv setTarget:self];
	[inv setSelector:@selector(_background_beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo:)];
	[inv setArgument:&path atIndex:2];
	[inv setArgument:&name atIndex:3];
   [inv setArgument:&fileTypes atIndex:4];
	[inv setArgument:&docWindow atIndex:5];
	[inv setArgument:&modalDelegate atIndex:6];
	[inv setArgument:&didEndSelector atIndex:7];
	[inv setArgument:&contextInfo atIndex:8];
	[inv retainArguments];
	[inv performSelectorInBackground:@selector(invoke) withObject:nil];
}

- (void)_background_beginSheetForDirectory:(NSString *)path
                                      file:(NSString *)name 
                                     types:(NSArray *)fileTypes
                            modalForWindow:(NSWindow *)docWindow
                             modalDelegate:(id)modalDelegate 
                            didEndSelector:(SEL)didEndSelector 
                               contextInfo:(void *)contextInfo
{
	id pool=[NSAutoreleasePool new];
	int ret=[self runModalForDirectory:path file:name types:fileTypes];
	
	id inv=[NSInvocation invocationWithMethodSignature:[self methodSignatureForSelector:@selector(_selector_savePanelDidEnd:returnCode:contextInfo:)]];
	
	[inv setTarget:modalDelegate];
	[inv setSelector:didEndSelector];
	[inv setArgument:&self atIndex:2];
	[inv setArgument:&ret atIndex:3];
	[inv setArgument:&contextInfo atIndex:4];
	[inv retainArguments];
	
	[inv performSelectorOnMainThread:@selector(invoke) withObject:nil waitUntilDone:NO];
   [pool release];
}

@end
