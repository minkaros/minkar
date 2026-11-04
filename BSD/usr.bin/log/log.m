/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * log: os_log's messages, as logd keeps them.
 *
 *   log show [--last TIME | --start DATE] [--end DATE] [--info] [--debug]
 *            [--predicate PREDICATE] [--style default|compact|syslog|json]
 *   log stream [--level default|info|debug] [--predicate PREDICATE]
 *            [--style ...] [--timeout SECONDS]
 *   log config --subsystem NAME --mode level:default|info|debug
 *   log config --subsystem NAME --reset
 *   log config --status
 *   log erase --all
 *
 * syslog(3)'s messages (ASL's, kept by syslogd) are shown with logd's, by
 * time, without a subsystem.
 *
 * Predicates are NSPredicate's, over process, processIdentifier,
 * subsystem, category, messageType, eventMessage (composedMessage),
 * threadIdentifier, activityIdentifier, userIdentifier and timestamp;
 * the types are also names: messageType == error.
 */

#import <Foundation/Foundation.h>
#include <asl.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "logd.h"
#include "logd_record.h"

#define kService	"org.minkar.logd"
#define kPage		(4u << 20)

static mach_port_t logd;
static NSPredicate *predicate;
static const char *style = "default";
static BOOL jsonFirst = YES;

static void
usage(void)
{
	fprintf(stderr,
	    "usage: log show [--last TIME | --start DATE] [--end DATE] [--info] [--debug]\n"
	    "                [--predicate PREDICATE] [--style default|compact|syslog|json]\n"
	    "       log stream [--level default|info|debug] [--predicate PREDICATE]\n"
	    "                [--style default|compact|syslog|json] [--timeout SECONDS]\n"
	    "       log config --subsystem NAME --mode level:default|info|debug\n"
	    "       log config --subsystem NAME --reset\n"
	    "       log config --status\n"
	    "       log erase --all\n");
	exit(64);
}

static void
connectLogd(void)
{
	if (bootstrap_look_up(bootstrap_port, kService, &logd) != KERN_SUCCESS) {
		fprintf(stderr, "log: logd is not running\n");
		exit(1);
	}
}

static const char *
typeName(uint32_t type)
{
	switch (type) {
	case LOGD_TYPE_INFO:	return "info";
	case LOGD_TYPE_DEBUG:	return "debug";
	case LOGD_TYPE_ERROR:	return "error";
	case LOGD_TYPE_FAULT:	return "fault";
	}
	return "default";
}

/* As the default style names them. */
static const char *
typeLabel(uint32_t type)
{
	switch (type) {
	case LOGD_TYPE_INFO:	return "Info";
	case LOGD_TYPE_DEBUG:	return "Debug";
	case LOGD_TYPE_ERROR:	return "Error";
	case LOGD_TYPE_FAULT:	return "Fault";
	}
	return "Default";
}

static const char *
typeShort(uint32_t type)
{
	switch (type) {
	case LOGD_TYPE_INFO:	return "I ";
	case LOGD_TYPE_DEBUG:	return "Db";
	case LOGD_TYPE_ERROR:	return "E ";
	case LOGD_TYPE_FAULT:	return "F ";
	}
	return "Df";
}

/* 2026-11-03 23:40:01.123456-0500, in local time. */
static void
timestamp(uint64_t ns, char *out, size_t size)
{
	time_t t = (time_t)(ns / 1000000000ULL);
	struct tm tm;
	char date[32], zone[8];

	localtime_r(&t, &tm);
	strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &tm);
	strftime(zone, sizeof(zone), "%z", &tm);
	snprintf(out, size, "%s.%06llu%s", date, (unsigned long long)(ns % 1000000000ULL) / 1000,
	    zone);
}

static NSString *
str(const char *s)
{
	NSString *v = [NSString stringWithUTF8String:s];
	return v != nil ? v : @"";
}

static NSDictionary *
fields(const logd_record_t *r)
{
	return @{
		@"process": str(logd_record_process(r)),
		@"processIdentifier": @(r->pid),
		@"userIdentifier": @(r->uid),
		@"subsystem": str(logd_record_subsystem(r)),
		@"category": str(logd_record_category(r)),
		@"messageType": str(typeName(r->type)),
		@"eventMessage": str(logd_record_text(r)),
		@"composedMessage": str(logd_record_text(r)),
		@"threadIdentifier": @(r->thread),
		@"activityIdentifier": @(r->activity),
		@"timestamp": [NSDate dateWithTimeIntervalSince1970:r->time / 1e9],
		/* So that messageType == error compares names. */
		@"default": @"default", @"info": @"info", @"debug": @"debug",
		@"error": @"error", @"fault": @"fault",
	};
}

static void
printJSONString(const char *s)
{
	putchar('"');
	for (; *s; s++) {
		unsigned char c = (unsigned char)*s;
		if (c == '"' || c == '\\')
			printf("\\%c", c);
		else if (c == '\n')
			fputs("\\n", stdout);
		else if (c == '\t')
			fputs("\\t", stdout);
		else if (c < 0x20)
			printf("\\u%04x", c);
		else
			putchar(c);
	}
	putchar('"');
}

static void
print(const logd_record_t *r)
{
	char when[64], scope[600] = "";
	const char *sub = logd_record_subsystem(r), *cat = logd_record_category(r);

	if (predicate != nil && ![predicate evaluateWithObject:fields(r)])
		return;
	timestamp(r->time, when, sizeof(when));
	if (*sub)
		snprintf(scope, sizeof(scope), "[%s:%s] ", sub, cat);
	if (strcmp(style, "json") == 0) {
		printf("%s{\n  \"timestamp\" : \"%s\",\n  \"messageType\" : \"%s\",\n"
		    "  \"processID\" : %d,\n  \"userID\" : %u,\n  \"threadID\" : %llu,\n"
		    "  \"activityIdentifier\" : %llu,\n  \"process\" : ",
		    jsonFirst ? "[" : ",", when, typeLabel(r->type), r->pid, r->uid,
		    (unsigned long long)r->thread, (unsigned long long)r->activity);
		printJSONString(logd_record_process(r));
		fputs(",\n  \"subsystem\" : ", stdout);
		printJSONString(sub);
		fputs(",\n  \"category\" : ", stdout);
		printJSONString(cat);
		fputs(",\n  \"eventMessage\" : ", stdout);
		printJSONString(logd_record_text(r));
		fputs("\n}", stdout);
		jsonFirst = NO;
	} else if (strcmp(style, "compact") == 0) {
		printf("%s %s %s[%d:%llx] %s%s\n", when, typeShort(r->type), logd_record_process(r),
		    r->pid, (unsigned long long)r->thread, scope, logd_record_text(r));
	} else if (strcmp(style, "syslog") == 0) {
		char host[256] = "localhost";
		gethostname(host, sizeof(host));
		printf("%s %s %s[%d]: %s%s\n", when, host, logd_record_process(r), r->pid, scope,
		    logd_record_text(r));
	} else {
		printf("%-31s 0x%-8llx %-8s 0x%-8llx %-6d %s: %s%s\n", when,
		    (unsigned long long)r->thread, typeLabel(r->type),
		    (unsigned long long)r->activity, r->pid, logd_record_process(r), scope,
		    logd_record_text(r));
	}
	fflush(stdout);
}

/* Records to print together, sorted by time (log show), or as they come
 * (log stream, collect == nil). */
static NSMutableArray<NSData *> *collect;

static void
emit(const logd_record_t *r)
{
	if (collect != nil)
		[collect addObject:[NSData dataWithBytes:r length:r->size]];
	else
		print(r);
}

/* ASL's levels as os_log's types. */
static uint32_t
aslType(int level)
{
	if (level <= ASL_LEVEL_CRIT)
		return (LOGD_TYPE_FAULT);
	if (level == ASL_LEVEL_ERR)
		return (LOGD_TYPE_ERROR);
	if (level == ASL_LEVEL_INFO)
		return (LOGD_TYPE_INFO);
	if (level >= ASL_LEVEL_DEBUG)
		return (LOGD_TYPE_DEBUG);
	return (LOGD_TYPE_DEFAULT);
}

static const char *
aslValue(asl_object_t m, const char *key)
{
	const char *v = asl_get(m, key);
	return (v != NULL ? v : "");
}

/* syslog(3)'s messages from ASL's store after message ID after (and from
 * start, nanoseconds, when not 0); returns the last ID. ASL itself keeps
 * other users' messages from a caller that may not read them. */
static uint64_t
fetchASL(uint64_t after, uint64_t start, uint64_t end, uint32_t types)
{
	asl_object_t q = asl_new(ASL_TYPE_QUERY);
	char buf[32];

	if (q == NULL)
		return (after);
	snprintf(buf, sizeof(buf), "%llu", (unsigned long long)after);
	asl_set_query(q, ASL_KEY_MSG_ID, buf, ASL_QUERY_OP_GREATER | ASL_QUERY_OP_NUMERIC);
	if (start != 0) {
		snprintf(buf, sizeof(buf), "%llu", (unsigned long long)(start / 1000000000ULL));
		asl_set_query(q, ASL_KEY_TIME, buf, ASL_QUERY_OP_GREATER_EQUAL | ASL_QUERY_OP_NUMERIC);
	}
	asl_object_t found = asl_search(NULL, q);
	asl_release(q);
	if (found == NULL)
		return (after);
	for (asl_object_t m = asl_next(found); m != NULL; m = asl_next(found)) {
		uint64_t id = strtoull(aslValue(m, ASL_KEY_MSG_ID), NULL, 10);
		if (id > after)
			after = id;
		uint32_t type = aslType(atoi(aslValue(m, ASL_KEY_LEVEL)));
		uint64_t time = strtoull(aslValue(m, ASL_KEY_TIME), NULL, 10) * 1000000000ULL +
		    strtoull(aslValue(m, ASL_KEY_TIME_NSEC), NULL, 10);
		if (!(types & LOGD_MASK(type)) || (end && time > end) || (start && time < start))
			continue;
		const char *process = aslValue(m, ASL_KEY_SENDER), *text = aslValue(m, ASL_KEY_MSG);
		size_t plen = strnlen(process, 255) + 1, tlen = strnlen(text, 4095) + 1;
		size_t size = (sizeof(logd_record_t) + plen + 1 + 1 + tlen + 7) & ~(size_t)7;
		logd_record_t *r = calloc(1, size);
		if (r == NULL)
			continue;
		r->size = (uint32_t)size;
		r->type = type;
		r->time = time;
		r->pid = atoi(aslValue(m, ASL_KEY_PID));
		r->uid = (uint32_t)strtoul(aslValue(m, ASL_KEY_UID), NULL, 10);
		r->process_len = (uint16_t)plen;
		r->subsystem_len = 1;
		r->category_len = 1;
		r->text_len = (uint16_t)tlen;
		char *p = (char *)(r + 1);
		memcpy(p, process, plen - 1);
		memcpy(p + plen + 2, text, tlen - 1);
		emit(r);
		free(r);
	}
	asl_release(found);
	return (after);
}

/* The records after seq after; returns the last one's sequence number. */
static uint64_t
fetch(uint64_t after, uint64_t start, uint64_t end, uint32_t types, BOOL *any)
{
	for (;;) {
		logd_bytes_t bytes = NULL;
		mach_msg_type_number_t len = 0;
		uint64_t last = after;
		kern_return_t kr = logd_query(logd, after, start, end, types, kPage, &bytes, &len,
		    &last);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "log: logd did not answer (0x%x)\n", kr);
			exit(1);
		}
		for (mach_msg_type_number_t off = 0; off + sizeof(logd_record_t) <= len;) {
			const logd_record_t *r = (const logd_record_t *)(bytes + off);
			if (r->size < sizeof(*r) || off + r->size > len)
				break;
			if (any != NULL)
				*any = YES;
			emit(r);
			off += r->size;
		}
		if (bytes != NULL)
			vm_deallocate(mach_task_self(), (vm_address_t)bytes, len);
		/* A full page: there is more. */
		if (len == 0 || last == after)
			return (last);
		after = last;
		if (len < kPage / 2)
			return (last);
	}
}

/* 30s, 5m, 2h, 1d: nanoseconds. */
static uint64_t
duration(const char *s)
{
	char *end;
	double v = strtod(s, &end);
	double unit = 1;

	if (end == s || v < 0)
		usage();
	switch (*end) {
	case 's': case '\0': unit = 1; break;
	case 'm': unit = 60; break;
	case 'h': unit = 3600; break;
	case 'd': unit = 86400; break;
	default: usage();
	}
	return ((uint64_t)(v * unit * 1e9));
}

/* "2026-11-03 23:40:01", "2026-11-03 23:40" or "2026-11-03", local time. */
static uint64_t
date(const char *s)
{
	struct tm tm;

	memset(&tm, 0, sizeof(tm));
	if (strptime(s, "%Y-%m-%d %H:%M:%S", &tm) == NULL &&
	    strptime(s, "%Y-%m-%d %H:%M", &tm) == NULL &&
	    strptime(s, "%Y-%m-%d", &tm) == NULL) {
		fprintf(stderr, "log: not a date: %s\n", s);
		exit(64);
	}
	tm.tm_isdst = -1;
	return ((uint64_t)mktime(&tm) * 1000000000ULL);
}

static void
setPredicate(const char *text)
{
	@try {
		predicate = [NSPredicate predicateWithFormat:str(text)];
	} @catch (NSException *e) {
		predicate = nil;
	}
	if (predicate == nil) {
		fprintf(stderr, "log: not a predicate: %s\n", text);
		exit(64);
	}
}

static uint64_t
now(void)
{
	return (clock_gettime_nsec_np(CLOCK_REALTIME));
}

static int
show(int argc, char **argv)
{
	uint64_t start = 0, end = 0;
	uint32_t types = LOGD_MASK(LOGD_TYPE_DEFAULT) | LOGD_MASK(LOGD_TYPE_ERROR) |
	    LOGD_MASK(LOGD_TYPE_FAULT);

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--last") == 0 && i + 1 < argc)
			start = now() - duration(argv[++i]);
		else if (strcmp(argv[i], "--start") == 0 && i + 1 < argc)
			start = date(argv[++i]);
		else if (strcmp(argv[i], "--end") == 0 && i + 1 < argc)
			end = date(argv[++i]);
		else if (strcmp(argv[i], "--info") == 0)
			types |= LOGD_MASK(LOGD_TYPE_INFO);
		else if (strcmp(argv[i], "--debug") == 0)
			types |= LOGD_MASK(LOGD_TYPE_INFO) | LOGD_MASK(LOGD_TYPE_DEBUG);
		else if (strcmp(argv[i], "--predicate") == 0 && i + 1 < argc)
			setPredicate(argv[++i]);
		else if (strcmp(argv[i], "--style") == 0 && i + 1 < argc)
			style = argv[++i];
		else
			usage();
	}
	connectLogd();
	if (strcmp(style, "default") == 0)
		printf("%-31s %-10s %-8s %-10s %-6s %s\n", "Timestamp", "Thread", "Type",
		    "Activity", "PID", "Message");
	collect = [NSMutableArray array];
	fetch(0, start, end, types, NULL);
	fetchASL(0, start, end, types);
	[collect sortWithOptions:NSSortStable usingComparator:^NSComparisonResult(NSData *a, NSData *b) {
		uint64_t ta = ((const logd_record_t *)a.bytes)->time;
		uint64_t tb = ((const logd_record_t *)b.bytes)->time;
		return ta < tb ? NSOrderedAscending : ta > tb ? NSOrderedDescending : NSOrderedSame;
	}];
	NSArray<NSData *> *all = collect;
	collect = nil;
	for (NSData *d in all)
		print((const logd_record_t *)d.bytes);
	if (strcmp(style, "json") == 0)
		puts(jsonFirst ? "[]" : "]");
	return (0);
}

static int
stream(int argc, char **argv)
{
	uint32_t types = LOGD_MASK(LOGD_TYPE_DEFAULT) | LOGD_MASK(LOGD_TYPE_ERROR) |
	    LOGD_MASK(LOGD_TYPE_FAULT);
	double timeout = 0;

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--level") == 0 && i + 1 < argc) {
			const char *l = argv[++i];
			if (strcmp(l, "debug") == 0)
				types |= LOGD_MASK(LOGD_TYPE_INFO) | LOGD_MASK(LOGD_TYPE_DEBUG);
			else if (strcmp(l, "info") == 0)
				types |= LOGD_MASK(LOGD_TYPE_INFO);
			else if (strcmp(l, "default") != 0)
				usage();
		} else if (strcmp(argv[i], "--predicate") == 0 && i + 1 < argc)
			setPredicate(argv[++i]);
		else if (strcmp(argv[i], "--style") == 0 && i + 1 < argc)
			style = argv[++i];
		else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
			timeout = strtod(argv[++i], NULL);
		else
			usage();
	}
	connectLogd();
	/* From now on: the newest record's number first. */
	uint64_t last = fetch(UINT64_MAX - 1, 0, 0, 0, NULL);
	/* syslog(3)'s messages from now on. */
	uint64_t since = now(), lastASL = 0;
	uint64_t stop = timeout > 0 ? now() + (uint64_t)(timeout * 1e9) : 0;
	if (strcmp(style, "default") == 0)
		printf("Filtering the log data using \"%s\"\n",
		    predicate != nil ? predicate.predicateFormat.UTF8String : "(none)");
	fflush(stdout);
	while (stop == 0 || now() < stop) {
		last = fetch(last, 0, 0, types, NULL);
		lastASL = fetchASL(lastASL, since, 0, types);
		usleep(200000);
	}
	if (strcmp(style, "json") == 0)
		puts(jsonFirst ? "[]" : "]");
	return (0);
}

static int
config(int argc, char **argv)
{
	const char *subsystem = NULL, *mode = NULL;
	BOOL reset = NO, status = NO;
	int32_t level = -1, result = 0;

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--subsystem") == 0 && i + 1 < argc)
			subsystem = argv[++i];
		else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
			mode = argv[++i];
		else if (strcmp(argv[i], "--reset") == 0)
			reset = YES;
		else if (strcmp(argv[i], "--status") == 0)
			status = YES;
		else
			usage();
	}
	connectLogd();
	if (status) {
		logd_bytes_t text = NULL;
		mach_msg_type_number_t len = 0;
		if (logd_levels(logd, &text, &len) != KERN_SUCCESS)
			return (1);
		printf("System mode = INFO\n");
		for (mach_msg_type_number_t off = 0; off < len;) {
			char name[256];
			int l;
			const char *line = (const char *)text + off;
			const char *nl = memchr(line, '\n', len - off);
			if (nl == NULL || sscanf(line, "%255s %d", name, &l) != 2)
				break;
			printf("%s mode = %s\n", name, l == 2 ? "DEBUG" : l == 1 ? "INFO" : "DEFAULT");
			off = (mach_msg_type_number_t)(nl - (const char *)text) + 1;
		}
		if (text != NULL)
			vm_deallocate(mach_task_self(), (vm_address_t)text, len);
		return (0);
	}
	if (subsystem == NULL || (mode == NULL) == !reset)
		usage();
	if (mode != NULL) {
		if (strcmp(mode, "level:debug") == 0)
			level = 2;
		else if (strcmp(mode, "level:info") == 0)
			level = 1;
		else if (strcmp(mode, "level:default") == 0)
			level = 0;
		else
			usage();
	}
	if (logd_set_level(logd, (char *)subsystem, level, &result) != KERN_SUCCESS)
		return (1);
	if (result != 0) {
		fprintf(stderr, "log: %s\n", result == EPERM ? "only root may change levels" :
		    strerror(result));
		return (1);
	}
	return (0);
}

static int
erase(int argc, char **argv)
{
	int32_t result = 0;

	if (argc != 1 || strcmp(argv[0], "--all") != 0)
		usage();
	connectLogd();
	if (logd_erase(logd, &result) != KERN_SUCCESS)
		return (1);
	if (result != 0) {
		fprintf(stderr, "log: %s\n", result == EPERM ? "only root may erase the log" :
		    strerror(result));
		return (1);
	}
	return (0);
}

int
main(int argc, char **argv)
{
	@autoreleasepool {
		if (argc < 2)
			usage();
		if (strcmp(argv[1], "show") == 0)
			return (show(argc - 2, argv + 2));
		if (strcmp(argv[1], "stream") == 0)
			return (stream(argc - 2, argv + 2));
		if (strcmp(argv[1], "config") == 0)
			return (config(argc - 2, argv + 2));
		if (strcmp(argv[1], "erase") == 0)
			return (erase(argc - 2, argv + 2));
		usage();
	}
	return (0);
}
