/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSHelpManager.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSScrollView.h>
#import <AppKit/NSTextStorage.h>
#import <AppKit/NSTextView.h>
#import <Foundation/Foundation.h>
#import "NSHelpViewer.h"
#import "NSHTMLReader.h"

NSString *const NSContextHelpModeDidActivateNotification=@"NSContextHelpModeDidActivateNotification";
NSString *const NSContextHelpModeDidDeactivateNotification=@"NSContextHelpModeDidDeactivateNotification";

/* Help books (Minkar): a bundle named by an app's CFBundleHelpBookFolder,
   its start page HPDBookAccessPath (or CFBundleHelpTOCFile, else
   index.html) in the user's language, its title HPDBookTitle. A book is
   asked for by the app's CFBundleHelpBookName, the book's identifier, or
   its title. They are shown in AppKit's help window (NSHelpViewer.m). */

static NSString *localized(NSString *string) {
   return [[NSBundle bundleForClass:[NSHelpManager class]] localizedStringForKey:string value:string table:nil];
}

@implementation NSHelpManager

static BOOL _isContextHelpModeActive=NO;
static NSMutableArray *_books;          /* NSDictionary: bundle, index, title, names */

+(NSHelpManager *)sharedHelpManager {
   static NSHelpManager *shared=nil;

   if(shared==nil){
    shared=[[NSHelpManager alloc] init];
    [shared registerBooksInBundle:[NSBundle mainBundle]];
   }
   return shared;
}

+(BOOL)isContextHelpModeActive {
   return _isContextHelpModeActive;
}

+(void)setContextHelpModeActive:(BOOL)flag {
   if(flag==_isContextHelpModeActive)
    return;
   _isContextHelpModeActive=flag;
   [[NSNotificationCenter defaultCenter] postNotificationName:flag?NSContextHelpModeDidActivateNotification:
      NSContextHelpModeDidDeactivateNotification object:[NSHelpManager sharedHelpManager]];
}

-init {
   _objectToText=NSCreateMapTable(NSNonOwnedPointerMapKeyCallBacks,NSObjectMapValueCallBacks,0);
   return self;
}

-(void)dealloc {
   NSFreeMapTable(_objectToText);
   [super dealloc];
}

/* The bundle's help book (CFBundleHelpBookFolder), if it has one. */
-(BOOL)registerBooksInBundle:(NSBundle *)bundle {
   NSString *folderName=[bundle objectForInfoDictionaryKey:@"CFBundleHelpBookFolder"];
   NSString *folder=[folderName isKindOfClass:[NSString class]]?[bundle pathForResource:folderName ofType:nil]:nil;
   NSBundle *book=(folder!=nil)?[NSBundle bundleWithPath:folder]:nil;
   NSString *start,*index,*title;
   NSMutableArray *names=[NSMutableArray array];

   if(book==nil)
    return NO;
   start=[book objectForInfoDictionaryKey:@"HPDBookAccessPath"];
   if([start length]==0)
    start=[book objectForInfoDictionaryKey:@"CFBundleHelpTOCFile"];
   if([start length]==0)
    start=@"index.html";
   index=[book pathForResource:start ofType:nil];
   if(index==nil)
    return NO;
   title=[book objectForInfoDictionaryKey:@"HPDBookTitle"];
   if([title length]==0){
    NSString *app=[bundle objectForInfoDictionaryKey:@"CFBundleName"];

    title=[NSString stringWithFormat:localized(@"%@ Help"),[app length]?app:[[NSProcessInfo processInfo] processName]];
   }
   for(id name in [NSArray arrayWithObjects:[bundle objectForInfoDictionaryKey:@"CFBundleHelpBookName"],
      [book bundleIdentifier],title,[[book infoDictionary] objectForKey:@"HPDBookTitle"],nil])
    if([name isKindOfClass:[NSString class]] && [name length])
     [names addObject:name];

   if(_books==nil)
    _books=[[NSMutableArray alloc] init];
   for(NSDictionary *existing in _books)
    if([[existing objectForKey:@"index"] isEqual:index])
     return YES;
   [_books addObject:[NSDictionary dictionaryWithObjectsAndKeys:book,@"bundle",index,@"index",title,@"title",
      names,@"names",nil]];
   return YES;
}

/* A book by name, or the app's own for nil. */
-(NSDictionary *)_bookNamed:(NSString *)name {
   if(name==nil)
    return [_books count]?[_books objectAtIndex:0]:nil;
   for(NSDictionary *book in _books)
    for(NSString *check in [book objectForKey:@"names"])
     if([check caseInsensitiveCompare:name]==NSOrderedSame)
      return book;
   return nil;
}

-(BOOL)_showBookNamed:(NSString *)name page:(NSString *)page anchor:(NSString *)anchor {
   NSDictionary *book=[self _bookNamed:name];
   NSURL        *home;

   if(book==nil)
    return NO;
   home=[NSURL fileURLWithPath:[book objectForKey:@"index"]];
   [[NSHelpViewer sharedViewer] showPage:(page!=nil)?[NSURL fileURLWithPath:page]:home anchor:anchor home:home
      title:[book objectForKey:@"title"]];
   return YES;
}

/* The app's book from its start page (-[NSApplication showHelp:]). */
-(BOOL)_showApplicationHelp {
   return [self _showBookNamed:nil page:nil anchor:nil];
}

/* The book's pages in the user's language: the start page's folder. */
-(NSArray *)_pagesOfBook:(NSDictionary *)book {
   NSString       *folder=[[book objectForKey:@"index"] stringByDeletingLastPathComponent];
   NSMutableArray *pages=[NSMutableArray array];

   for(NSString *file in [[[NSFileManager defaultManager] contentsOfDirectoryAtPath:folder error:NULL]
      sortedArrayUsingSelector:@selector(compare:)])
    if([[file pathExtension] caseInsensitiveCompare:@"html"]==NSOrderedSame ||
       [[file pathExtension] caseInsensitiveCompare:@"htm"]==NSOrderedSame)
     [pages addObject:[folder stringByAppendingPathComponent:file]];
   return pages;
}

-(void)openHelpAnchor:(NSString *)anchor inBook:(NSString *)bookName {
   NSDictionary *book=[self _bookNamed:bookName];
   NSString     *idPattern=[NSString stringWithFormat:@"id=\"%@\"",anchor];
   NSString     *namePattern=[NSString stringWithFormat:@"name=\"%@\"",anchor];

   if(book==nil){
    NSBeep();
    return;
   }
   for(NSString *page in [self _pagesOfBook:book]){
    NSString *html=[NSString stringWithContentsOfFile:page encoding:NSUTF8StringEncoding error:NULL];

    if([html rangeOfString:idPattern].location!=NSNotFound || [html rangeOfString:namePattern].location!=NSNotFound){
     [self _showBookNamed:bookName page:page anchor:anchor];
     return;
    }
   }
   [self _showBookNamed:bookName page:nil anchor:nil];
}

static NSString *escapeHTML(NSString *string) {
   NSMutableString *result=[NSMutableString stringWithString:string?string:@""];

   [result replaceOccurrencesOfString:@"&" withString:@"&amp;" options:0 range:NSMakeRange(0,[result length])];
   [result replaceOccurrencesOfString:@"<" withString:@"&lt;" options:0 range:NSMakeRange(0,[result length])];
   [result replaceOccurrencesOfString:@">" withString:@"&gt;" options:0 range:NSMakeRange(0,[result length])];
   return result;
}

/* The book's pages that have the string, as a page of links. */
-(void)findString:(NSString *)string inBook:(NSString *)bookName {
   NSDictionary    *book=[self _bookNamed:bookName];
   NSMutableString *html=[NSMutableString string];
   NSUInteger       found=0;
   NSString         *path;

   if(book==nil || [string length]==0){
    NSBeep();
    return;
   }
   [html appendFormat:@"<html><head><meta charset=\"utf-8\"><title>%@</title></head><body><h1>%@</h1><ul>",
      escapeHTML(localized(@"Search Results")),
      escapeHTML([NSString stringWithFormat:localized(@"Search Results for “%@”"),string])];
   for(NSString *page in [self _pagesOfBook:book]){
    NSData             *data=[NSData dataWithContentsOfFile:page];
    NSDictionary       *attributes=nil;
    NSAttributedString *text=(data!=nil)?[NSHTMLReader attributedStringWithHTML:data
       baseURL:[NSURL fileURLWithPath:page] documentAttributes:&attributes]:nil;
    NSString           *title=[attributes objectForKey:NSTitleDocumentAttribute];

    if([[text string] rangeOfString:string options:NSCaseInsensitiveSearch|NSDiacriticInsensitiveSearch].location==NSNotFound)
     continue;
    [html appendFormat:@"<li><a href=\"%@\">%@</a></li>",[[NSURL fileURLWithPath:page] absoluteString],
       escapeHTML([title length]?title:[page lastPathComponent])];
    found++;
   }
   [html appendString:@"</ul>"];
   if(found==0)
    [html appendFormat:@"<p>%@</p>",escapeHTML([NSString stringWithFormat:localized(@"No pages contain “%@”."),string])];
   [html appendString:@"</body></html>"];

   path=[NSTemporaryDirectory() stringByAppendingPathComponent:
      [NSString stringWithFormat:@"help-search-%d.html",[[NSProcessInfo processInfo] processIdentifier]]];
   [[html dataUsingEncoding:NSUTF8StringEncoding] writeToFile:path atomically:YES];
   [[NSHelpViewer sharedViewer] showPage:[NSURL fileURLWithPath:path] anchor:nil
      home:[NSURL fileURLWithPath:[book objectForKey:@"index"]] title:[book objectForKey:@"title"]];
}

-(NSAttributedString *)contextHelpForObject:object {
   return NSMapGet(_objectToText,object);
}

-(void)setContextHelp:(NSAttributedString *)text forObject:object {
   NSMapInsert(_objectToText,object,text);
}

-(void)removeContextHelpForObject:object {
   NSMapRemove(_objectToText,object);
}

/* The object's help text in a small panel at the point (screen
   coordinates), one at a time; the context help mode ends. */
-(BOOL)showContextHelpForObject:object locationHint:(NSPoint)hintPoint {
   static NSPanel *panel;
   NSAttributedString *text=[self contextHelpForObject:object];
   NSTextView *view;
   NSScrollView *scroll;
   NSRect frame;

   if(text==nil)
    return NO;
   [NSHelpManager setContextHelpModeActive:NO];
   if(panel==nil){
    panel=[[NSPanel alloc] initWithContentRect:NSMakeRect(0,0,320,140)
       styleMask:NSTitledWindowMask|NSClosableWindowMask|NSUtilityWindowMask backing:NSBackingStoreBuffered defer:NO];
    [panel setTitle:localized(@"Help")];
    [panel setReleasedWhenClosed:NO];
    [panel setFloatingPanel:YES];
    scroll=[[[NSScrollView alloc] initWithFrame:[[panel contentView] bounds]] autorelease];
    [scroll setHasVerticalScroller:YES];
    [scroll setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    view=[[[NSTextView alloc] initWithFrame:[[scroll contentView] bounds]] autorelease];
    [view setEditable:NO];
    [view setTextContainerInset:NSMakeSize(8,6)];
    [scroll setDocumentView:view];
    [[panel contentView] addSubview:scroll];
   }
   view=(NSTextView *)[(NSScrollView *)[[[panel contentView] subviews] lastObject] documentView];
   [[view textStorage] setAttributedString:text];
   frame=[panel frame];
   frame.origin=NSMakePoint(hintPoint.x,hintPoint.y-NSHeight(frame));
   [panel setFrame:[panel constrainFrameRect:frame toScreen:[NSScreen mainScreen]] display:NO];
   [panel orderFront:nil];
   return YES;
}

@end
