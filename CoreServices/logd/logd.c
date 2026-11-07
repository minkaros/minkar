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
 * logd (Phase 3): keeps os_log's messages. Every message it is sent is in
 * memory (the newest kMemoryBytes); default, error and fault messages
 * are also written to /var/db/diagnostics (two files, each up to
 * kFileBytes, the older dropped when the newer is full), so they outlive
 * logd and restarts. Info is sent by default, debug when a subsystem's
 * level says so (log config); both stay in memory only.
 *
 * Messages arrive on org.minkar.logd as hand-written messages
 * (logd_message.h); the log command asks through the MIG interface
 * (logd.defs). The sender of a message is the audit trailer's.
 */

#include <bsm/libbsm.h>
#include <dispatch/dispatch.h>
#include <errno.h>
#include <fcntl.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <minkar/logd_message.h>
#include "logd_record.h"
#include "logdServer.h"		/* MIG, generated */

#define kDir		"/var/db/diagnostics"
#define kCurrent	kDir "/logd.0"
#define kPrevious	kDir "/logd.1"
#define kLevels		"/var/db/logd/levels"
#define kMemoryBytes	(8u << 20)
#define kFileBytes	(16u << 20)
#define kReplyMax	(32u << 20)
static const char kMagic[8] = "MKLOGD1";

/* The newest messages, oldest first, in a list of records. */
typedef struct Entry {
	struct Entry	*next;
	logd_record_t	record;		/* and its strings */
} Entry;

static Entry *first, *last;
static size_t memoryBytes;
static uint64_t nextSeq = 1;
static FILE *current;
static off_t currentBytes;

/* Subsystems with a level of their own. */
typedef struct Level {
	struct Level	*next;
	char		name[LOGD_NAME_MAX];
	int		level;
} Level;
static Level *levels;

static int
levelOf(const char *subsystem)
{
	for (Level *l = levels; l; l = l->next)
		if (strcmp(l->name, subsystem) == 0)
			return (l->level);
	return (LOGD_LEVEL_INFO);	/* info in memory, debug off */
}

static void
saveLevels(void)
{
	FILE *f;

	mkdir("/var/db/logd", 0755);
	f = fopen(kLevels ".new", "w");
	if (f == NULL)
		return;
	for (Level *l = levels; l; l = l->next)
		fprintf(f, "%s %d\n", l->name, l->level);
	if (fclose(f) == 0)
		rename(kLevels ".new", kLevels);
}

static void
loadLevels(void)
{
	FILE *f = fopen(kLevels, "r");
	char name[LOGD_NAME_MAX];
	int level;

	if (f == NULL)
		return;
	while (fscanf(f, "%255s %d", name, &level) == 2) {
		Level *l = calloc(1, sizeof(*l));
		if (l == NULL)
			break;
		strlcpy(l->name, name, sizeof(l->name));
		l->level = level;
		l->next = levels;
		levels = l;
	}
	fclose(f);
}

static int
persisted(uint32_t type)
{
	return (type != LOGD_TYPE_INFO && type != LOGD_TYPE_DEBUG);
}

/* Each record of a store file, in order; false at its end or damage. */
static int
readRecord(FILE *f, logd_record_t **out)
{
	logd_record_t header;

	if (fread(&header, sizeof(header), 1, f) != 1 || header.size < sizeof(header) ||
	    header.size > sizeof(header) + 2 * LOGD_NAME_MAX + LOGD_TEXT_MAX + 1024 ||
	    sizeof(header) + header.process_len + header.subsystem_len + header.category_len +
	    header.text_len > header.size)
		return (0);
	logd_record_t *r = malloc(header.size);
	if (r == NULL)
		return (0);
	*r = header;
	if (fread(r + 1, header.size - sizeof(header), 1, f) != 1) {
		free(r);
		return (0);
	}
	*out = r;
	return (1);
}

static FILE *
openStore(const char *path)
{
	FILE *f = fopen(path, "r");
	char magic[8];

	if (f == NULL)
		return (NULL);
	if (fread(magic, sizeof(magic), 1, f) != 1 || memcmp(magic, kMagic, sizeof(magic)) != 0) {
		fclose(f);
		return (NULL);
	}
	return (f);
}

/* The newest file to append to, after the last good record. */
static void
openCurrent(void)
{
	FILE *f = openStore(kCurrent);
	off_t end = sizeof(kMagic);
	logd_record_t *r;

	if (f != NULL) {
		while (readRecord(f, &r)) {
			if (r->seq >= nextSeq)
				nextSeq = r->seq + 1;
			end = ftello(f);
			free(r);
		}
		fclose(f);
	} else {
		/* The older file still numbers what came before. */
		FILE *p = openStore(kPrevious);
		if (p != NULL) {
			while (readRecord(p, &r)) {
				if (r->seq >= nextSeq)
					nextSeq = r->seq + 1;
				free(r);
			}
			fclose(p);
		}
		f = fopen(kCurrent, "w");
		if (f == NULL || fwrite(kMagic, sizeof(kMagic), 1, f) != 1) {
			fprintf(stderr, "logd: cannot write %s: %s\n", kCurrent, strerror(errno));
			if (f != NULL)
				fclose(f);
			return;
		}
		fclose(f);
	}
	truncate(kCurrent, end);
	current = fopen(kCurrent, "a");
	currentBytes = end;
}

static void
writeRecord(const logd_record_t *r)
{
	if (current == NULL)
		return;
	if (currentBytes + r->size > kFileBytes) {
		fclose(current);
		rename(kCurrent, kPrevious);
		current = fopen(kCurrent, "w");
		currentBytes = 0;
		if (current == NULL || fwrite(kMagic, sizeof(kMagic), 1, current) != 1)
			return;
		currentBytes = sizeof(kMagic);
	}
	if (fwrite(r, r->size, 1, current) == 1)
		currentBytes += r->size;
	/* Errors and faults on disk at once; the others within a second. */
	if (r->type == LOGD_TYPE_ERROR || r->type == LOGD_TYPE_FAULT)
		fflush(current);
}

static void
keep(Entry *e)
{
	e->next = NULL;
	if (last != NULL)
		last->next = e;
	else
		first = e;
	last = e;
	memoryBytes += e->record.size;
	while (memoryBytes > kMemoryBytes && first != NULL && first != last) {
		Entry *old = first;
		first = old->next;
		memoryBytes -= old->record.size;
		free(old);
	}
	if (persisted(e->record.type))
		writeRecord(&e->record);
}

static void
received(const logd_log_message_t *m, size_t size, audit_token_t token)
{
	size_t fixed = offsetof(logd_log_message_t, data);
	char process[2 * MAXCOMLEN + 1] = "";
	pid_t pid = audit_token_to_pid(token);

	if (size < fixed || (size_t)m->subsystem_len + m->category_len + m->text_len +
	    m->process_len > size - fixed ||
	    m->subsystem_len == 0 || m->category_len == 0 || m->text_len == 0 ||
	    m->subsystem_len > LOGD_NAME_MAX || m->category_len > LOGD_NAME_MAX ||
	    m->text_len > LOGD_TEXT_MAX || m->process_len > LOGD_NAME_MAX)
		return;
	const char *subsystem = m->data;
	const char *category = subsystem + m->subsystem_len;
	const char *text = category + m->category_len;
	const char *given = text + m->text_len;
	if (subsystem[m->subsystem_len - 1] || category[m->category_len - 1] ||
	    text[m->text_len - 1] || (m->process_len && given[m->process_len - 1]))
		return;
	/* A sender may skip the level check: logd keeps only what it would. */
	if ((m->type == LOGD_TYPE_DEBUG && levelOf(subsystem) < LOGD_LEVEL_DEBUG) ||
	    (m->type == LOGD_TYPE_INFO && levelOf(subsystem) < LOGD_LEVEL_INFO))
		return;
	if (m->type != LOGD_TYPE_DEFAULT && m->type != LOGD_TYPE_INFO &&
	    m->type != LOGD_TYPE_DEBUG && m->type != LOGD_TYPE_ERROR && m->type != LOGD_TYPE_FAULT)
		return;
	/* The kernel's name for the process; the one it gave once it has
	 * exited (pid and user are always the kernel's). */
	if (proc_name(pid, process, sizeof(process)) <= 0 && m->process_len > 1)
		strlcpy(process, given, sizeof(process));
	uint16_t plen = (uint16_t)strlen(process) + 1;
	size_t rsize = sizeof(logd_record_t) + plen + m->subsystem_len + m->category_len +
	    m->text_len;
	rsize = (rsize + 7) & ~(size_t)7;
	Entry *e = calloc(1, offsetof(Entry, record) + rsize);
	if (e == NULL)
		return;
	logd_record_t *r = &e->record;
	r->size = (uint32_t)rsize;
	r->type = m->type;
	r->seq = nextSeq++;
	r->time = m->time;
	r->thread = m->thread;
	r->activity = m->activity;
	r->pid = pid;
	r->uid = audit_token_to_euid(token);
	r->process_len = plen;
	r->subsystem_len = m->subsystem_len;
	r->category_len = m->category_len;
	r->text_len = m->text_len;
	char *p = (char *)(r + 1);
	memcpy(p, process, plen);
	p += plen;
	memcpy(p, m->data, m->subsystem_len + m->category_len + m->text_len);
	keep(e);
}

static void
answerLevel(const logd_level_request_t *m, size_t size)
{
	logd_level_reply_t reply;
	char subsystem[LOGD_NAME_MAX];

	if (!MACH_PORT_VALID(m->header.msgh_remote_port))
		return;
	if (size >= sizeof(*m))
		strlcpy(subsystem, m->subsystem, sizeof(subsystem));
	else
		subsystem[0] = '\0';
	memset(&reply, 0, sizeof(reply));
	reply.header.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(m->header.msgh_bits), 0);
	reply.header.msgh_size = sizeof(reply);
	reply.header.msgh_remote_port = m->header.msgh_remote_port;
	reply.header.msgh_id = LOGD_MSG_LEVEL_REPLY;
	reply.level = levelOf(subsystem);
	if (mach_msg(&reply.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(reply), 0,
	    MACH_PORT_NULL, 0, MACH_PORT_NULL) != MACH_MSG_SUCCESS)
		mach_msg_destroy(&reply.header);
}

/* ---- The log command's requests ---- */

typedef struct {
	uint8_t		*buf;
	size_t		len, cap, limit;
	uint64_t	after, start, end, last;
	uint32_t	types;
	uid_t		uid;		/* (uid_t)-1: any */
	int		full;
} Reply;

static void
add(Reply *q, const logd_record_t *r)
{
	if (q->full || r->seq <= q->after || !(q->types & LOGD_MASK(r->type)) ||
	    (q->start && r->time < q->start) || (q->end && r->time > q->end) ||
	    (q->uid != (uid_t)-1 && r->uid != q->uid))
		return;
	if (q->len + r->size > q->limit) {
		q->full = 1;
		return;
	}
	if (q->len + r->size > q->cap) {
		size_t cap = q->cap ? q->cap * 2 : 65536;
		while (cap < q->len + r->size)
			cap *= 2;
		uint8_t *grown = realloc(q->buf, cap);
		if (grown == NULL) {
			q->full = 1;
			return;
		}
		q->buf = grown;
		q->cap = cap;
	}
	memcpy(q->buf + q->len, r, r->size);
	q->len += r->size;
	q->last = r->seq;
}

/* Persisted records older than memory, from the files. */
static void
addFile(Reply *q, const char *path, uint64_t below)
{
	FILE *f = openStore(path);
	logd_record_t *r;

	if (f == NULL)
		return;
	while (!q->full && readRecord(f, &r)) {
		if (r->seq < below)
			add(q, r);
		free(r);
	}
	fclose(f);
}

static void
replyBytes(Reply *q, uint8_t **out, mach_msg_type_number_t *outLen)
{
	vm_address_t addr = 0;

	*out = NULL;
	*outLen = 0;
	if (q->len > 0 && vm_allocate(mach_task_self(), &addr, q->len, VM_FLAGS_ANYWHERE) ==
	    KERN_SUCCESS) {
		memcpy((void *)addr, q->buf, q->len);
		*out = (uint8_t *)addr;
		*outLen = (mach_msg_type_number_t)q->len;
	}
	free(q->buf);
}

kern_return_t
logds_query(mach_port_t server, audit_token_t token, uint64_t after, uint64_t start,
    uint64_t end, uint32_t types, uint32_t limit, uint8_t **records,
    mach_msg_type_number_t *recordsCnt, uint64_t *lastSeq)
{
	Reply q = { .after = after, .start = start, .end = end, .types = types,
	    .limit = limit && limit < kReplyMax ? limit : kReplyMax };
	uid_t uid = audit_token_to_euid(token);

	q.uid = uid == 0 ? (uid_t)-1 : uid;
	if (current != NULL)
		fflush(current);
	uint64_t below = first != NULL ? first->record.seq : nextSeq;
	if (after + 1 < below) {
		addFile(&q, kPrevious, below);
		addFile(&q, kCurrent, below);
	}
	for (Entry *e = first; e && !q.full; e = e->next)
		add(&q, &e->record);
	*lastSeq = q.last ? q.last : (q.full ? after : nextSeq - 1);
	replyBytes(&q, records, recordsCnt);
	return (KERN_SUCCESS);
}

kern_return_t
logds_set_level(mach_port_t server, audit_token_t token, logd_name_t subsystem,
    int32_t level, int32_t *result)
{
	Level **lp;

	*result = 0;
	if (audit_token_to_euid(token) != 0) {
		*result = EPERM;
		return (KERN_SUCCESS);
	}
	if (subsystem[0] == '\0' || strchr(subsystem, ' ') || level > LOGD_LEVEL_DEBUG) {
		*result = EINVAL;
		return (KERN_SUCCESS);
	}
	for (lp = &levels; *lp; lp = &(*lp)->next)
		if (strcmp((*lp)->name, subsystem) == 0)
			break;
	if (level < 0) {
		if (*lp != NULL) {
			Level *gone = *lp;
			*lp = gone->next;
			free(gone);
		}
	} else {
		if (*lp == NULL) {
			*lp = calloc(1, sizeof(Level));
			if (*lp == NULL) {
				*result = ENOMEM;
				return (KERN_SUCCESS);
			}
			strlcpy((*lp)->name, subsystem, sizeof((*lp)->name));
		}
		(*lp)->level = level;
	}
	saveLevels();
	return (KERN_SUCCESS);
}

kern_return_t
logds_levels(mach_port_t server, uint8_t **text, mach_msg_type_number_t *textCnt)
{
	Reply q = { 0 };
	char line[LOGD_NAME_MAX + 16];

	for (Level *l = levels; l; l = l->next) {
		size_t n = (size_t)snprintf(line, sizeof(line), "%s %d\n", l->name, l->level);
		if (q.len + n > q.cap) {
			size_t cap = q.cap ? q.cap * 2 : 4096;
			while (cap < q.len + n)
				cap *= 2;
			uint8_t *grown = realloc(q.buf, cap);
			if (grown == NULL)
				break;
			q.buf = grown;
			q.cap = cap;
		}
		memcpy(q.buf + q.len, line, n);
		q.len += n;
	}
	replyBytes(&q, text, textCnt);
	return (KERN_SUCCESS);
}

kern_return_t
logds_erase(mach_port_t server, audit_token_t token, int32_t *result)
{
	*result = 0;
	if (audit_token_to_euid(token) != 0) {
		*result = EPERM;
		return (KERN_SUCCESS);
	}
	while (first != NULL) {
		Entry *e = first;
		first = e->next;
		free(e);
	}
	last = NULL;
	memoryBytes = 0;
	if (current != NULL)
		fclose(current);
	unlink(kPrevious);
	unlink(kCurrent);
	current = NULL;
	openCurrent();
	return (KERN_SUCCESS);
}

/* ---- The port ---- */

static mach_port_t service;

static void
serve(void)
{
	union {
		mach_msg_header_t header;
		uint8_t bytes[sizeof(logd_log_message_t) + MAX_TRAILER_SIZE + 4096];
	} in;
	union {
		mach_msg_header_t header;
		uint8_t bytes[4096];
	} out;
	kern_return_t kr;

	kr = mach_msg(&in.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT | MACH_RCV_LARGE |
	    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
	    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT), 0, sizeof(in), service, 0,
	    MACH_PORT_NULL);
	if (kr == MACH_RCV_TOO_LARGE) {
		/* Not one of ours: dropped (MACH_RCV_LARGE left it queued). */
		mach_msg(&in.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, 0, service, 0,
		    MACH_PORT_NULL);
		return;
	}
	if (kr != MACH_MSG_SUCCESS)
		return;
	mach_msg_audit_trailer_t *trailer = (mach_msg_audit_trailer_t *)
	    ((uint8_t *)&in + round_msg(in.header.msgh_size));
	switch (in.header.msgh_id) {
	case LOGD_MSG_LOG:
		if (!(in.header.msgh_bits & MACH_MSGH_BITS_COMPLEX))
			received((const logd_log_message_t *)&in, in.header.msgh_size,
			    trailer->msgh_audit);
		mach_msg_destroy(&in.header);
		return;
	case LOGD_MSG_LEVEL:
		if (!(in.header.msgh_bits & MACH_MSGH_BITS_COMPLEX))
			answerLevel((const logd_level_request_t *)&in, in.header.msgh_size);
		else
			mach_msg_destroy(&in.header);
		return;
	}
	if (!logd_server(&in.header, &out.header)) {
		mach_msg_destroy(&in.header);
		return;
	}
	/* As mach_msg_server does: an error reply means the request was not
	 * consumed; no reply is sent for MIG_NO_REPLY. */
	mig_reply_error_t *reply = (mig_reply_error_t *)&out;
	if (!(out.header.msgh_bits & MACH_MSGH_BITS_COMPLEX) && reply->RetCode != KERN_SUCCESS) {
		if (reply->RetCode == MIG_NO_REPLY)
			return;
		in.header.msgh_remote_port = MACH_PORT_NULL;
		mach_msg_destroy(&in.header);
	}
	if (!MACH_PORT_VALID(out.header.msgh_remote_port)) {
		mach_msg_destroy(&out.header);
		return;
	}
	if (mach_msg(&out.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, out.header.msgh_size, 0,
	    MACH_PORT_NULL, 100, MACH_PORT_NULL) != MACH_MSG_SUCCESS)
		mach_msg_destroy(&out.header);
}

int
main(int argc, char *argv[])
{
	kern_return_t kr;

	setvbuf(stderr, NULL, _IOLBF, 0);
	kr = bootstrap_check_in(bootstrap_port, LOGD_SERVICE, &service);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "logd: cannot check in %s: 0x%x\n", LOGD_SERVICE, kr);
		return (1);
	}
	mkdir(kDir, 0755);
	chmod(kDir, 0755);
	loadLevels();
	openCurrent();
	/* A bigger queue: bursts of messages wait instead of being dropped. */
	mach_port_limits_t limits = { MACH_PORT_QLIMIT_LARGE };
	mach_port_set_attributes(mach_task_self(), service, MACH_PORT_LIMITS_INFO,
	    (mach_port_info_t)&limits, MACH_PORT_LIMITS_INFO_COUNT);

	/* One queue: messages and requests in order. */
	dispatch_queue_t queue = dispatch_queue_create(LOGD_SERVICE, DISPATCH_QUEUE_SERIAL);
	dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
	    service, 0, queue);
	dispatch_source_set_event_handler(source, ^{ serve(); });
	dispatch_resume(source);
	dispatch_source_t flush = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
	dispatch_source_set_timer(flush, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC),
	    NSEC_PER_SEC, NSEC_PER_SEC / 4);
	dispatch_source_set_event_handler(flush, ^{
		if (current != NULL)
			fflush(current);
	});
	dispatch_resume(flush);
	fprintf(stderr, "logd: serving %s, %llu message(s) kept before\n", LOGD_SERVICE,
	    (unsigned long long)(nextSeq - 1));
	dispatch_main();
}
