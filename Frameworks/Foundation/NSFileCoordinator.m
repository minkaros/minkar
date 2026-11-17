/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSFileCoordinator, within the process. An access is a set of items,
 * each read or written; it waits while another access holds an item that
 * is the same, inside or around one of its own and either of them writes.
 * Before it, the items' presenters (but the coordinator's own) are asked
 * on their queues to save their changes, to let the item go (and are
 * given it back after), and to make way for a deletion; after a write
 * they hear the item changed, its folders' presenters that something in
 * them did. Moves reported with itemAtURL:didMoveToURL: reach the old
 * place's presenters and its folders'. Processes do not coordinate with
 * one another yet: that needs a system service.
 */

#import <Foundation/NSFileCoordinator.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSError.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSLock.h>
#import <Foundation/NSOperation.h>
#import <Foundation/NSOperationQueue.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSUUID.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/FoundationErrors.h>
#include <dispatch/dispatch.h>

/* ---- What is held ---- */

@interface _NSFileAccess : NSObject {
@public
	NSString *path;
	BOOL write;
	__unsafe_unretained id owner;
}
@end
@implementation _NSFileAccess
@end

static NSCondition *gLock;
static NSMutableArray<_NSFileAccess *> *gHeld;
static NSMutableArray *gPresenters;

static void
setup(void)
{
	static dispatch_once_t once;
	dispatch_once(&once, ^{
		gLock = [[NSCondition alloc] init];
		gHeld = [[NSMutableArray alloc] init];
		gPresenters = [[NSMutableArray alloc] init];
	});
}

/* The same item, or one inside the other. */
static BOOL
overlaps(NSString *a, NSString *b)
{
	if ([a isEqualToString:b])
		return YES;
	if ([a hasPrefix:[b stringByAppendingString:@"/"]] || [b isEqualToString:@"/"])
		return YES;
	return [b hasPrefix:[a stringByAppendingString:@"/"]] || [a isEqualToString:@"/"];
}

/* a is a folder b is in. */
static BOOL
contains(NSString *a, NSString *b)
{
	return ![a isEqualToString:b] &&
	    ([a isEqualToString:@"/"] || [b hasPrefix:[a stringByAppendingString:@"/"]]);
}

static NSString *
pathOf(NSURL *url, BOOL resolveLink)
{
	NSURL *u = [url URLByStandardizingPath];

	if (resolveLink) {
		NSString *dest = [[NSFileManager defaultManager] destinationOfSymbolicLinkAtPath:[u path] error:NULL];
		if (dest != nil)
			u = [[NSURL fileURLWithPath:dest relativeToURL:[u URLByDeletingLastPathComponent]] URLByStandardizingPath];
	}
	NSString *p = [u path];
	if ([p length] > 1 && [p hasSuffix:@"/"])
		p = [p substringToIndex:[p length] - 1];
	return p ?: @"";
}

/* ---- Talking to presenters ---- */

/* block on the presenter's queue: run here when that is this queue (or it
   has none), else queued; with wait, this thread waits for it. */
static void
onQueue(id<NSFilePresenter> presenter, BOOL wait, void (^block)(void))
{
	NSOperationQueue *queue = [presenter presentedItemOperationQueue];

	if (queue == nil || queue == [NSOperationQueue currentQueue]) {
		block();
		return;
	}
	if (!wait) {
		[queue addOperationWithBlock:block];
		return;
	}
	dispatch_semaphore_t done = dispatch_semaphore_create(0);
	[queue addOperationWithBlock:^{
		block();
		dispatch_semaphore_signal(done);
	}];
	dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
}

/* A presenter method with a completion handler, waited for: its error. */
static NSError *
waitForHandler(id<NSFilePresenter> presenter, void (^call)(void (^handler)(NSError *)))
{
	__block NSError *result = nil;
	dispatch_semaphore_t done = dispatch_semaphore_create(0);

	onQueue(presenter, NO, ^{
		call(^(NSError *error) {
			result = error;
			dispatch_semaphore_signal(done);
		});
	});
	/* A presenter whose queue is this one has run it already; others
	   answer when they can. A minute at most, so a presenter that never
	   answers does not hold the access forever. */
	dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 60 * NSEC_PER_SEC));
	return result;
}

/* A relinquish method, waited for until it hands the item over: what it
   gives to call when the item is back. */
static void (^relinquish(id<NSFilePresenter> presenter, BOOL toWriter))(void)
{
	__block void (^reacquirer)(void) = nil;
	dispatch_semaphore_t done = dispatch_semaphore_create(0);
	void (^taker)(void (^)(void)) = ^(void (^back)(void)) {
		reacquirer = [back copy];
		dispatch_semaphore_signal(done);
	};

	onQueue(presenter, NO, ^{
		if (toWriter)
			[presenter relinquishPresentedItemToWriter:taker];
		else
			[presenter relinquishPresentedItemToReader:taker];
	});
	dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 60 * NSEC_PER_SEC));
	return reacquirer;
}

static NSArray *
presentersSnapshot(void)
{
	setup();
	@synchronized(gPresenters) {
		return [gPresenters copy];
	}
}

/* ---- NSFileAccessIntent ---- */

@interface NSFileAccessIntent () {
@public
	NSURL *_url;
	BOOL _write;
	NSUInteger _options;
}
@end

@implementation NSFileAccessIntent

+ (instancetype)readingIntentWithURL:(NSURL *)url options:(NSFileCoordinatorReadingOptions)options
{
	NSFileAccessIntent *intent = [[self alloc] init];
	intent->_url = [url copy];
	intent->_options = options;
	return intent;
}

+ (instancetype)writingIntentWithURL:(NSURL *)url options:(NSFileCoordinatorWritingOptions)options
{
	NSFileAccessIntent *intent = [[self alloc] init];
	intent->_url = [url copy];
	intent->_write = YES;
	intent->_options = options;
	return intent;
}

- (NSURL *)URL
{
	@synchronized(self) {
		return _url;
	}
}

@end

/* ---- NSFileCoordinator ---- */

@implementation NSFileCoordinator {
	id<NSFilePresenter> _presenter;
	NSString *_purposeIdentifier;
	BOOL _cancelled;
	/* Moves reported during an access: old path to new URL. */
	NSMutableDictionary<NSString *, NSURL *> *_moves;
}

+ (void)addFilePresenter:(id<NSFilePresenter>)filePresenter
{
	setup();
	if (filePresenter == nil)
		return;
	@synchronized(gPresenters) {
		if ([gPresenters indexOfObjectIdenticalTo:filePresenter] == NSNotFound)
			[gPresenters addObject:filePresenter];
	}
}

+ (void)removeFilePresenter:(id<NSFilePresenter>)filePresenter
{
	setup();
	@synchronized(gPresenters) {
		[gPresenters removeObjectIdenticalTo:filePresenter];
	}
}

+ (NSArray *)filePresenters
{
	return presentersSnapshot();
}

- (instancetype)init
{
	return [self initWithFilePresenter:nil];
}

- (instancetype)initWithFilePresenter:(id<NSFilePresenter>)filePresenterOrNil
{
	if ((self = [super init]) != nil) {
		setup();
		_presenter = filePresenterOrNil;
		_purposeIdentifier = [[[NSUUID UUID] UUIDString] copy];
		_moves = [[NSMutableDictionary alloc] init];
	}
	return self;
}

- (NSString *)purposeIdentifier
{
	@synchronized(self) {
		return _purposeIdentifier;
	}
}

- (void)setPurposeIdentifier:(NSString *)purposeIdentifier
{
	@synchronized(self) {
		_purposeIdentifier = [purposeIdentifier copy];
	}
}

- (void)cancel
{
	[gLock lock];
	_cancelled = YES;
	[gLock broadcast];
	[gLock unlock];
}

static NSError *
cancelledError(void)
{
	return [NSError errorWithDomain:NSCocoaErrorDomain code:NSUserCancelledError userInfo:nil];
}

/* The presenters of items an access touches (but ours), before it: saved,
   made way for, let go. The reacquirers to call after. */
- (NSError *)_prepareReads:(NSArray<NSString *> *)reads readOptions:(NSFileCoordinatorReadingOptions)readOptions
                    writes:(NSArray<NSString *> *)writes writeOptions:(NSFileCoordinatorWritingOptions)writeOptions
               reacquirers:(NSMutableArray *)reacquirers
{
	for (id<NSFilePresenter> p in presentersSnapshot()) {
		NSURL *url = [p presentedItemURL];
		if (p == _presenter || url == nil)
			continue;
		NSString *pp = pathOf(url, NO);
		BOOL touchedByWrite = NO, deleted = NO, touchedByRead = NO;
		for (NSString *w in writes)
			if (overlaps(pp, w)) {
				touchedByWrite = YES;
				if ((writeOptions & NSFileCoordinatorWritingForDeleting) && !contains(pp, w))
					deleted = YES;
			}
		if (!(readOptions & NSFileCoordinatorReadingWithoutChanges))
			for (NSString *r in reads)
				if (overlaps(pp, r))
					touchedByRead = YES;
		if (!touchedByWrite && !touchedByRead)
			continue;

		NSError *error = nil;
		if ([p respondsToSelector:@selector(savePresentedItemChangesWithCompletionHandler:)])
			error = waitForHandler(p, ^(void (^h)(NSError *)) { [p savePresentedItemChangesWithCompletionHandler:h]; });
		if (error != nil)
			return error;
		if (deleted && [p respondsToSelector:@selector(accommodatePresentedItemDeletionWithCompletionHandler:)]) {
			error = waitForHandler(p, ^(void (^h)(NSError *)) { [p accommodatePresentedItemDeletionWithCompletionHandler:h]; });
			if (error != nil)
				return error;
		}
		if (touchedByWrite && !deleted)
			for (NSString *w in writes)
				if (contains(pp, w) && (writeOptions & NSFileCoordinatorWritingForDeleting) &&
				    [p respondsToSelector:@selector(accommodatePresentedSubitemDeletionAtURL:completionHandler:)]) {
					NSURL *sub = [NSURL fileURLWithPath:w];
					error = waitForHandler(p, ^(void (^h)(NSError *)) {
						[p accommodatePresentedSubitemDeletionAtURL:sub completionHandler:h];
					});
					if (error != nil)
						return error;
				}
		SEL give = touchedByWrite ? @selector(relinquishPresentedItemToWriter:) : @selector(relinquishPresentedItemToReader:);
		if ([p respondsToSelector:give]) {
			void (^back)(void) = relinquish(p, touchedByWrite);
			if (back != nil)
				[reacquirers addObject:@[ p, back ]];
		}
	}
	return nil;
}

/* Waits for the items, then holds them (or the coordinator was
   cancelled: NO). */
- (BOOL)_acquireReads:(NSArray<NSString *> *)reads writes:(NSArray<NSString *> *)writes held:(NSMutableArray *)held
{
	[gLock lock];
	for (;;) {
		if (_cancelled) {
			[gLock unlock];
			return NO;
		}
		BOOL clash = NO;
		for (_NSFileAccess *a in gHeld) {
			if (a->owner == self)
				continue;
			for (NSString *w in writes)
				if (overlaps(a->path, w))
					clash = YES;
			if (a->write)
				for (NSString *r in reads)
					if (overlaps(a->path, r))
						clash = YES;
			if (clash)
				break;
		}
		if (!clash)
			break;
		[gLock wait];
	}
	for (NSString *r in reads) {
		_NSFileAccess *a = [[_NSFileAccess alloc] init];
		a->path = r;
		a->owner = self;
		[held addObject:a];
	}
	for (NSString *w in writes) {
		_NSFileAccess *a = [[_NSFileAccess alloc] init];
		a->path = w;
		a->write = YES;
		a->owner = self;
		[held addObject:a];
	}
	[gHeld addObjectsFromArray:held];
	[gLock unlock];
	return YES;
}

- (void)_release:(NSArray *)held
{
	[gLock lock];
	for (_NSFileAccess *a in held)
		[gHeld removeObjectIdenticalTo:a];
	[gLock broadcast];
	[gLock unlock];
}

/* After the access: presenters hear of what was written, then get their
   items back. */
- (void)_finishWrites:(NSArray<NSString *> *)writes writeOptions:(NSFileCoordinatorWritingOptions)writeOptions
              existed:(NSArray<NSNumber *> *)existed reacquirers:(NSArray *)reacquirers
{
	BOOL quiet = (writeOptions & (NSFileCoordinatorWritingForDeleting | NSFileCoordinatorWritingForMoving)) != 0;

	for (id<NSFilePresenter> p in presentersSnapshot()) {
		NSURL *url = [p presentedItemURL];
		if (p == _presenter || url == nil)
			continue;
		NSString *pp = pathOf(url, NO);
		[writes enumerateObjectsUsingBlock:^(NSString *w, NSUInteger i, BOOL *stop) {
			if (quiet)
				return;
			if ([pp isEqualToString:w] || contains(w, pp)) {
				if ([p respondsToSelector:@selector(presentedItemDidChange)])
					onQueue(p, NO, ^{ [p presentedItemDidChange]; });
			} else if (contains(pp, w)) {
				NSURL *sub = [NSURL fileURLWithPath:w];
				BOOL appeared = ![existed[i] boolValue] &&
				    [[NSFileManager defaultManager] fileExistsAtPath:w];
				if (appeared && [p respondsToSelector:@selector(presentedSubitemDidAppearAtURL:)])
					onQueue(p, NO, ^{ [p presentedSubitemDidAppearAtURL:sub]; });
				else if ([p respondsToSelector:@selector(presentedSubitemDidChangeAtURL:)])
					onQueue(p, NO, ^{ [p presentedSubitemDidChangeAtURL:sub]; });
			}
		}];
	}
	for (NSArray *pair in reacquirers) {
		id<NSFilePresenter> p = pair[0];
		void (^back)(void) = pair[1];
		onQueue(p, NO, back);
	}
}

/* The URL an accessor gets: where the item went if it was moved. */
- (NSURL *)_currentURL:(NSURL *)url
{
	@synchronized(self) {
		NSURL *moved = _moves[pathOf(url, NO)];
		return moved ?: url;
	}
}

/* One coordinated access to all these items. */
- (BOOL)_coordinateReads:(NSArray<NSURL *> *)readURLs readOptions:(NSFileCoordinatorReadingOptions)readOptions
                  writes:(NSArray<NSURL *> *)writeURLs writeOptions:(NSFileCoordinatorWritingOptions)writeOptions
                   error:(NSError **)outError accessor:(void (NS_NOESCAPE ^)(void))accessor
{
	NSMutableArray<NSString *> *reads = [NSMutableArray array], *writes = [NSMutableArray array];
	NSMutableArray<NSNumber *> *existed = [NSMutableArray array];
	NSMutableArray *reacquirers = [NSMutableArray array], *held = [NSMutableArray array];

	for (NSURL *u in readURLs)
		[reads addObject:pathOf(u, (readOptions & NSFileCoordinatorReadingResolvesSymbolicLink) != 0)];
	for (NSURL *u in writeURLs) {
		NSString *p = pathOf(u, NO);
		[writes addObject:p];
		[existed addObject:@([[NSFileManager defaultManager] fileExistsAtPath:p])];
	}
	if (_cancelled) {
		if (outError != NULL)
			*outError = cancelledError();
		return NO;
	}
	NSError *error = [self _prepareReads:reads readOptions:readOptions writes:writes
	    writeOptions:writeOptions reacquirers:reacquirers];
	if (error == nil && ![self _acquireReads:reads writes:writes held:held])
		error = cancelledError();
	if (error != nil) {
		for (NSArray *pair in reacquirers)
			onQueue(pair[0], NO, pair[1]);
		if (outError != NULL)
			*outError = error;
		return NO;
	}
	@try {
		accessor();
	} @finally {
		[self _release:held];
		[self _finishWrites:writes writeOptions:writeOptions existed:existed reacquirers:reacquirers];
	}
	return YES;
}

- (void)coordinateReadingItemAtURL:(NSURL *)url options:(NSFileCoordinatorReadingOptions)options
    error:(NSError **)outError byAccessor:(void (NS_NOESCAPE ^)(NSURL *))reader
{
	[self _coordinateReads:@[ url ] readOptions:options writes:@[] writeOptions:0 error:outError accessor:^{
		NSURL *u = [self _currentURL:url];
		if (options & NSFileCoordinatorReadingResolvesSymbolicLink)
			u = [NSURL fileURLWithPath:pathOf(u, YES)];
		reader(u);
	}];
}

- (void)coordinateWritingItemAtURL:(NSURL *)url options:(NSFileCoordinatorWritingOptions)options
    error:(NSError **)outError byAccessor:(void (NS_NOESCAPE ^)(NSURL *))writer
{
	[self _coordinateReads:@[] readOptions:0 writes:@[ url ] writeOptions:options error:outError accessor:^{
		writer([self _currentURL:url]);
	}];
}

- (void)coordinateReadingItemAtURL:(NSURL *)readingURL options:(NSFileCoordinatorReadingOptions)readingOptions
    writingItemAtURL:(NSURL *)writingURL options:(NSFileCoordinatorWritingOptions)writingOptions
    error:(NSError **)outError byAccessor:(void (NS_NOESCAPE ^)(NSURL *, NSURL *))readerWriter
{
	[self _coordinateReads:@[ readingURL ] readOptions:readingOptions writes:@[ writingURL ]
	    writeOptions:writingOptions error:outError accessor:^{
		readerWriter([self _currentURL:readingURL], [self _currentURL:writingURL]);
	}];
}

- (void)coordinateWritingItemAtURL:(NSURL *)url1 options:(NSFileCoordinatorWritingOptions)options1
    writingItemAtURL:(NSURL *)url2 options:(NSFileCoordinatorWritingOptions)options2
    error:(NSError **)outError byAccessor:(void (NS_NOESCAPE ^)(NSURL *, NSURL *))writer
{
	[self _coordinateReads:@[] readOptions:0 writes:@[ url1, url2 ] writeOptions:options1 | options2
	    error:outError accessor:^{
		writer([self _currentURL:url1], [self _currentURL:url2]);
	}];
}

/* Presenters are told once for the whole batch; the accessor makes its
   own coordinated accesses, and calls the completion handler when done. */
- (void)prepareForReadingItemsAtURLs:(NSArray<NSURL *> *)readingURLs options:(NSFileCoordinatorReadingOptions)readingOptions
    writingItemsAtURLs:(NSArray<NSURL *> *)writingURLs options:(NSFileCoordinatorWritingOptions)writingOptions
    error:(NSError **)outError byAccessor:(void (NS_NOESCAPE ^)(void (^)(void)))batchAccessor
{
	NSMutableArray<NSString *> *reads = [NSMutableArray array], *writes = [NSMutableArray array];
	NSMutableArray *reacquirers = [NSMutableArray array];

	for (NSURL *u in readingURLs)
		[reads addObject:pathOf(u, (readingOptions & NSFileCoordinatorReadingResolvesSymbolicLink) != 0)];
	for (NSURL *u in writingURLs)
		[writes addObject:pathOf(u, NO)];
	NSError *error = _cancelled ? cancelledError() :
	    [self _prepareReads:reads readOptions:readingOptions writes:writes writeOptions:writingOptions
	        reacquirers:reacquirers];
	if (error != nil) {
		for (NSArray *pair in reacquirers)
			onQueue(pair[0], NO, pair[1]);
		if (outError != NULL)
			*outError = error;
		return;
	}
	__block BOOL completed = NO;
	batchAccessor(^{
		if (completed)
			return;
		completed = YES;
		for (NSArray *pair in reacquirers)
			onQueue(pair[0], NO, pair[1]);
	});
}

/* Off the caller's thread: the items held while the accessor runs on
   queue; each intent's URL follows its item. */
- (void)coordinateAccessWithIntents:(NSArray<NSFileAccessIntent *> *)intents queue:(NSOperationQueue *)queue
    byAccessor:(void (^)(NSError *))accessor
{
	NSMutableArray *reads = [NSMutableArray array], *writes = [NSMutableArray array];
	NSUInteger readOptions = 0, writeOptions = 0;

	for (NSFileAccessIntent *intent in intents) {
		if (intent->_write) {
			[writes addObject:intent->_url];
			writeOptions |= intent->_options;
		} else {
			[reads addObject:intent->_url];
			readOptions |= intent->_options;
		}
	}
	void (^userAccessor)(NSError *) = [accessor copy];
	dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
		NSError *error = nil;
		BOOL ok = [self _coordinateReads:reads readOptions:readOptions writes:writes writeOptions:writeOptions
		    error:&error accessor:^{
			for (NSFileAccessIntent *intent in intents)
				@synchronized(intent) {
					intent->_url = [self _currentURL:intent->_url];
				}
			dispatch_semaphore_t done = dispatch_semaphore_create(0);
			[queue addOperationWithBlock:^{
				userAccessor(nil);
				dispatch_semaphore_signal(done);
			}];
			dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
		}];
		if (!ok)
			[queue addOperationWithBlock:^{ userAccessor(error); }];
	});
}

- (void)itemAtURL:(NSURL *)oldURL willMoveToURL:(NSURL *)newURL
{
	(void)oldURL; (void)newURL;
}

/* The presenters of the moved item (or of something in it) hear where it
   went; the presenters of its old and new folders, that something in
   them moved. */
- (void)itemAtURL:(NSURL *)oldURL didMoveToURL:(NSURL *)newURL
{
	NSString *from = pathOf(oldURL, NO), *to = pathOf(newURL, NO);

	@synchronized(self) {
		_moves[from] = newURL;
	}
	for (id<NSFilePresenter> p in presentersSnapshot()) {
		NSURL *url = [p presentedItemURL];
		if (p == _presenter || url == nil)
			continue;
		NSString *pp = pathOf(url, NO);
		if ([pp isEqualToString:from] || contains(from, pp)) {
			NSString *rest = [pp substringFromIndex:[from length]];
			NSURL *moved = [NSURL fileURLWithPath:[to stringByAppendingString:rest]];
			if ([p respondsToSelector:@selector(presentedItemDidMoveToURL:)])
				onQueue(p, NO, ^{ [p presentedItemDidMoveToURL:moved]; });
		} else if ((contains(pp, from) || contains(pp, to)) &&
		    [p respondsToSelector:@selector(presentedSubitemAtURL:didMoveToURL:)]) {
			onQueue(p, NO, ^{ [p presentedSubitemAtURL:oldURL didMoveToURL:newURL]; });
		}
	}
}

- (void)itemAtURL:(NSURL *)url didChangeUbiquityAttributes:(NSSet<NSString *> *)attributes
{
	NSString *path = pathOf(url, NO);

	for (id<NSFilePresenter> p in presentersSnapshot()) {
		NSURL *pu = [p presentedItemURL];
		if (p == _presenter || pu == nil || ![pathOf(pu, NO) isEqualToString:path])
			continue;
		if ([p respondsToSelector:@selector(presentedItemDidChangeUbiquityAttributes:)])
			onQueue(p, NO, ^{ [p presentedItemDidChangeUbiquityAttributes:attributes]; });
	}
}

@end
