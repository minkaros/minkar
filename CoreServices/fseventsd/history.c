/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * fseventsd's history (Atlas.md, K0): each local, writable volume keeps
 * its events in .fseventsd at its root (only root can read it: the paths
 * are every user's), so a stream can ask for the events since an ID. The
 * files are Minkar's own; only fseventsd reads them. Each is named by its
 * first event ID (16 hex digits) and holds records of the event's ID,
 * flags, time (seconds since 2001, as CFAbsoluteTime) and path relative
 * to the volume's root. A file is closed past HISTORY_FILE_SIZE bytes;
 * past HISTORY_VOLUME_SIZE in all, the oldest go. fseventsd-uuid names
 * the volume's history: a new UUID is a new history.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "history.h"

#define HISTORY_DIR		".fseventsd"
#define HISTORY_FILE_SIZE	(1024 * 1024)
#define HISTORY_VOLUME_SIZE	(32 * 1024 * 1024)
#define MAX_VOLUMES		32
#define CF_EPOCH		978307200	/* 2001-01-01 in Unix time */

typedef struct {
	uint64_t id;
	uint32_t flags;
	uint32_t time;
	uint16_t length;	/* of the path that follows, without a NUL */
} __attribute__((packed)) Record;

typedef struct {
	char mount[MNAMELEN];	/* "" when the slot is free */
	FILE *file;		/* open only while a batch of events is written */
	uint64_t current;	/* the file being written: its first ID, or 0 */
	size_t size;
	int failed;		/* cannot keep a history: not tried again */
} Volume;

static Volume volumes[MAX_VOLUMES];
static struct statfs *mounts;
static int mountCount;
static time_t mountsRead;

static void
readMounts(void)
{
	mountCount = getmntinfo(&mounts, MNT_NOWAIT);
	mountsRead = time(NULL);
}

/* The volume a path is on: the longest mount point that holds it. */
static const struct statfs *
mountOf(const char *path)
{
	const struct statfs *best = NULL;
	size_t bestLength = 0;

	if (mountCount <= 0 || time(NULL) != mountsRead)
		readMounts();
	for (int i = 0; i < mountCount; i++) {
		const char *m = mounts[i].f_mntonname;
		size_t n = strlen(m);
		if (strcmp(m, "/") == 0)
			n = 0;
		else if (strncmp(path, m, n) != 0 || (path[n] != '\0' && path[n] != '/'))
			continue;
		if (best == NULL || n >= bestLength) {
			best = &mounts[i];
			bestLength = n;
		}
	}
	return best;
}

int
historyMountOf(const char *path, char *mount, size_t length)
{
	const struct statfs *m = mountOf(path);

	if (m == NULL)
		return -1;
	strlcpy(mount, m->f_mntonname, length);
	return 0;
}

int
historyMountOfDevice(dev_t device, char *mount, size_t length)
{
	readMounts();
	for (int i = 0; i < mountCount; i++) {
		struct stat st;
		if (stat(mounts[i].f_mntonname, &st) == 0 && st.st_dev == device) {
			strlcpy(mount, mounts[i].f_mntonname, length);
			return 0;
		}
	}
	return -1;
}

/* Whether a volume keeps a history: local, writable, a disk's. */
static int
keepsHistory(const struct statfs *m)
{
	const char *type = m->f_fstypename;

	if (!(m->f_flags & MNT_LOCAL) || (m->f_flags & MNT_RDONLY))
		return 0;
	return strcmp(type, "devfs") != 0 && strcmp(type, "fdesc") != 0 &&
	    strcmp(type, "autofs") != 0 && strcmp(type, "nullfs") != 0;
}

static void
directoryOf(const char *mount, char *dir, size_t length)
{
	snprintf(dir, length, "%s/%s", strcmp(mount, "/") == 0 ? "" : mount, HISTORY_DIR);
}

int
historyIsOwnPath(const char *path)
{
	const char *p = strstr(path, "/" HISTORY_DIR);

	for (; p != NULL; p = strstr(p + 1, "/" HISTORY_DIR)) {
		char next = p[strlen("/" HISTORY_DIR)];
		if (next == '\0' || next == '/')
			return 1;
	}
	return 0;
}

/* The volume's history directory, made with its UUID when missing. */
static int
makeDirectory(const char *mount)
{
	char dir[PATH_MAX], uuidPath[PATH_MAX];
	struct stat st;

	directoryOf(mount, dir, sizeof(dir));
	if (stat(dir, &st) != 0) {
		if (mkdir(dir, 0700) != 0)
			return -1;
	}
	snprintf(uuidPath, sizeof(uuidPath), "%s/fseventsd-uuid", dir);
	if (stat(uuidPath, &st) != 0) {
		uuid_t u;
		uuid_string_t s;
		FILE *f = fopen(uuidPath, "w");

		if (f == NULL)
			return -1;
		uuid_generate_random(u);
		uuid_unparse_upper(u, s);
		fprintf(f, "%s\n", s);
		fclose(f);
	}
	return 0;
}

/* The history's files, oldest first: their first IDs. */
static int
listFiles(const char *dir, uint64_t **ids)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	int count = 0, capacity = 0;

	*ids = NULL;
	if (d == NULL)
		return 0;
	while ((e = readdir(d)) != NULL) {
		char *end;
		if (strlen(e->d_name) != 16)
			continue;
		uint64_t id = strtoull(e->d_name, &end, 16);
		if (*end != '\0')
			continue;
		if (count == capacity) {
			capacity = capacity ? capacity * 2 : 64;
			*ids = realloc(*ids, sizeof(uint64_t) * (size_t)capacity);
		}
		(*ids)[count++] = id;
	}
	closedir(d);
	for (int i = 1; i < count; i++)
		for (int j = i; j > 0 && (*ids)[j - 1] > (*ids)[j]; j--) {
			uint64_t t = (*ids)[j];
			(*ids)[j] = (*ids)[j - 1];
			(*ids)[j - 1] = t;
		}
	return count;
}

/* The oldest files go while the history is over its size. */
static void
trim(const char *dir)
{
	uint64_t *ids;
	int count = listFiles(dir, &ids);
	off_t total = 0;
	struct stat st;
	char path[PATH_MAX];

	for (int i = 0; i < count; i++) {
		snprintf(path, sizeof(path), "%s/%016llx", dir, (unsigned long long)ids[i]);
		if (stat(path, &st) == 0)
			total += st.st_size;
	}
	/* The newest file, being written, always stays. */
	for (int i = 0; i + 1 < count && total > HISTORY_VOLUME_SIZE; i++) {
		snprintf(path, sizeof(path), "%s/%016llx", dir, (unsigned long long)ids[i]);
		if (stat(path, &st) == 0 && unlink(path) == 0)
			total -= st.st_size;
	}
	free(ids);
}

static Volume *
volumeFor(const struct statfs *m)
{
	Volume *free_ = NULL;

	for (int i = 0; i < MAX_VOLUMES; i++) {
		if (strcmp(volumes[i].mount, m->f_mntonname) == 0)
			return &volumes[i];
		if (free_ == NULL && volumes[i].mount[0] == '\0')
			free_ = &volumes[i];
	}
	if (free_ == NULL)
		return NULL;
	memset(free_, 0, sizeof(*free_));
	strlcpy(free_->mount, m->f_mntonname, sizeof(free_->mount));
	return free_;
}

void
historyAppend(uint64_t id, uint32_t flags, const char *path)
{
	const struct statfs *m = mountOf(path);
	Volume *v;
	char dir[PATH_MAX], file[PATH_MAX];
	const char *relative;
	Record r;

	if (m == NULL || !keepsHistory(m) || historyIsOwnPath(path))
		return;
	v = volumeFor(m);
	if (v == NULL || v->failed)
		return;
	if (v->current == 0 || v->size >= HISTORY_FILE_SIZE) {
		/* A new file. */
		if (v->file != NULL) {
			fclose(v->file);
			v->file = NULL;
		}
		if (makeDirectory(v->mount) != 0) {
			v->failed = 1;
			return;
		}
		directoryOf(v->mount, dir, sizeof(dir));
		trim(dir);
		v->current = id;
		v->size = 0;
	}
	if (v->file == NULL) {
		/* The file being written, again after a batch closed it. */
		directoryOf(v->mount, dir, sizeof(dir));
		snprintf(file, sizeof(file), "%s/%016llx", dir, (unsigned long long)v->current);
		v->file = fopen(file, "a");
		if (v->file == NULL) {
			v->failed = 1;
			return;
		}
	}
	relative = path + (strcmp(v->mount, "/") == 0 ? 0 : strlen(v->mount));
	while (*relative == '/')
		relative++;
	r.id = id;
	r.flags = flags;
	r.time = (uint32_t)(time(NULL) - CF_EPOCH);
	r.length = (uint16_t)strnlen(relative, PATH_MAX - 1);
	fwrite(&r, sizeof(r), 1, v->file);
	fwrite(relative, 1, r.length, v->file);
	v->size += sizeof(r) + r.length;
}

/* Each batch's files closed: an idle history holds no file open on a
 * volume, which could then not be unmounted (an image, a USB disk). */
void
historyFlush(void)
{
	for (int i = 0; i < MAX_VOLUMES; i++)
		if (volumes[i].file != NULL) {
			fclose(volumes[i].file);
			volumes[i].file = NULL;
		}
}

/* A volume unmounted: its file is closed. */
void
historyForgetUnmounted(void)
{
	readMounts();
	for (int i = 0; i < MAX_VOLUMES; i++) {
		int mounted = 0;
		if (volumes[i].mount[0] == '\0')
			continue;
		for (int j = 0; j < mountCount; j++)
			if (strcmp(mounts[j].f_mntonname, volumes[i].mount) == 0)
				mounted = 1;
		if (!mounted) {
			if (volumes[i].file != NULL)
				fclose(volumes[i].file);
			memset(&volumes[i], 0, sizeof(volumes[i]));
		}
	}
}

/* Every record of a volume's history, oldest first, to fn; stops when
 * fn returns nonzero (and returns that). */
static int
eachRecord(const char *mount, int (*fn)(void *, const Record *, const char *), void *context)
{
	char dir[PATH_MAX], path[PATH_MAX], relative[PATH_MAX];
	uint64_t *ids;
	int count, stop = 0;

	directoryOf(mount, dir, sizeof(dir));
	count = listFiles(dir, &ids);
	for (int i = 0; i < count && !stop; i++) {
		FILE *f;
		Record r;

		snprintf(path, sizeof(path), "%s/%016llx", dir, (unsigned long long)ids[i]);
		if ((f = fopen(path, "r")) == NULL)
			continue;
		while (!stop && fread(&r, sizeof(r), 1, f) == 1) {
			if (r.length >= sizeof(relative) || fread(relative, 1, r.length, f) != r.length)
				break;
			relative[r.length] = '\0';
			stop = fn(context, &r, relative);
		}
		fclose(f);
	}
	free(ids);
	return stop;
}

uint64_t
historyOldest(const char *mount)
{
	char dir[PATH_MAX];
	uint64_t *ids, oldest = 0;

	directoryOf(mount, dir, sizeof(dir));
	if (listFiles(dir, &ids) > 0)
		oldest = ids[0];
	free(ids);
	return oldest;
}

typedef struct {
	const char *mount;
	uint64_t since, upto;
	int (*fn)(void *, uint64_t, uint32_t, const char *);
	void *context;
} Replay;

static int
replayOne(void *context, const Record *r, const char *relative)
{
	Replay *p = context;
	char path[PATH_MAX];

	if (r->id <= p->since)
		return 0;
	if (r->id > p->upto)
		return 1;
	if (strcmp(p->mount, "/") == 0)
		snprintf(path, sizeof(path), "/%s", relative);
	else
		snprintf(path, sizeof(path), "%s%s%s", p->mount, *relative ? "/" : "", relative);
	return p->fn(p->context, r->id, r->flags, path);
}

int
historyReplay(const char *mount, uint64_t since, uint64_t upto,
    int (*fn)(void *, uint64_t, uint32_t, const char *), void *context)
{
	Replay p = { mount, since, upto, fn, context };

	return eachRecord(mount, replayOne, &p);
}

int
historyUUID(const char *mount, char uuid[37])
{
	char path[PATH_MAX];
	FILE *f;

	uuid[0] = '\0';
	directoryOf(mount, path, sizeof(path));
	strlcat(path, "/fseventsd-uuid", sizeof(path));
	if ((f = fopen(path, "r")) == NULL)
		return -1;
	if (fscanf(f, "%36s", uuid) != 1)
		uuid[0] = '\0';
	fclose(f);
	return uuid[0] ? 0 : -1;
}

typedef struct {
	uint32_t before;
	uint64_t last;
} LastBefore;

static int
lastBeforeOne(void *context, const Record *r, const char *relative)
{
	LastBefore *p = context;

	(void)relative;
	if (r->time >= p->before)
		return 1;
	p->last = r->id;
	return 0;
}

uint64_t
historyLastBefore(const char *mount, double absoluteTime)
{
	LastBefore p = { absoluteTime <= 0 ? 0 : (uint32_t)absoluteTime, 0 };

	eachRecord(mount, lastBeforeOne, &p);
	return p.last;
}

/* The files whose events are all up to the ID go; one that holds it and
 * later events stays whole. Returns 0, or -1 if there is no history. */
int
historyPurge(const char *mount, uint64_t upto)
{
	char dir[PATH_MAX], path[PATH_MAX];
	uint64_t *ids;
	int count;

	directoryOf(mount, dir, sizeof(dir));
	count = listFiles(dir, &ids);
	if (count == 0) {
		free(ids);
		return -1;
	}
	for (int i = 0; i + 1 < count; i++)
		if (ids[i + 1] <= upto + 1) {
			snprintf(path, sizeof(path), "%s/%016llx", dir, (unsigned long long)ids[i]);
			unlink(path);
		}
	free(ids);
	return 0;
}
