/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Resource files: a header (offsets and lengths of the data and the map,
 * big-endian), the data (each resource a 4-byte length and its bytes) and
 * the map (a list of types, each with a list of references: ID, name
 * offset, attributes and data offset). Open files form a chain, newest
 * first; Get1 calls look in the current file, Get calls down the chain
 * from it.
 */

#include "Resources.h"
#include "MacErrors.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

typedef struct ResFile {
    ResFileRefNum refNum;
    UInt8 *bytes;
    size_t size;
    UInt32 dataOffset, mapOffset, dataLength, mapLength;
    struct ResFile *next;
} ResFile;

typedef struct LoadedResource {
    Handle handle;
    ResFileRefNum file;
    ResType type;
    ResID id;
    ResAttributes attributes;
    UInt8 name[256];
    struct LoadedResource *next;
} LoadedResource;

static ResFile *sFiles;
static ResFileRefNum sCurrent = kResFileNotOpened;
static ResFileRefNum sNextRefNum = 1000;
static LoadedResource *sLoaded;
static __thread OSErr sResError;
static Boolean sResLoad = true;

static UInt32 be32(const UInt8 *p) { return ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) | ((UInt32)p[2] << 8) | p[3]; }
static UInt16 be16(const UInt8 *p) { return (UInt16)((p[0] << 8) | p[1]); }

OSErr ResError(void) {
    return sResError;
}

static ResFile *fileFor(ResFileRefNum refNum) {
    for (ResFile *f = sFiles; f != NULL; f = f->next)
        if (f->refNum == refNum)
            return f;
    return NULL;
}

static UInt8 *readAll(const char *path, size_t *size) {
    FILE *fp = fopen(path, "rb");
    UInt8 *bytes = NULL;
    long n;
    if (fp == NULL)
        return NULL;
    if (fseek(fp, 0, SEEK_END) == 0 && (n = ftell(fp)) > 0 && fseek(fp, 0, SEEK_SET) == 0) {
        bytes = malloc((size_t)n);
        if (bytes != NULL && fread(bytes, 1, (size_t)n, fp) != (size_t)n) {
            free(bytes);
            bytes = NULL;
        } else
            *size = (size_t)n;
    }
    fclose(fp);
    return bytes;
}

/* The map's type list: count at map+28+... (offset to type list at
   map+24, to name list at map+26). */
static Boolean validMap(const ResFile *f) {
    if (f->size < 16)
        return false;
    if ((size_t)f->mapOffset + f->mapLength > f->size || (size_t)f->dataOffset + f->dataLength > f->size)
        return false;
    if (f->mapLength < 30)
        return false;
    UInt16 typeList = be16(f->bytes + f->mapOffset + 24);
    return (size_t)f->mapOffset + typeList + 2 <= f->size;
}

OSErr FSOpenResourceFile(const FSRef *ref, UniCharCount forkNameLength, const UniChar *forkName, SInt8 permissions,
                         ResFileRefNum *refNum) {
    char path[PATH_MAX + 32];
    ResFile *f;

    if (refNum != NULL)
        *refNum = kResFileNotOpened;
    if (permissions & fsWrPerm)
        return sResError = wrPermErr;
    if (FSRefMakePath(ref, (UInt8 *)path, PATH_MAX) != noErr)
        return sResError = fnfErr;
    /* Fork names: "RESOURCE_FORK" (or the resource fork by default
       through FSOpenResFile), or none for the data fork. */
    if (forkNameLength > 0)
        strlcat(path, "/..namedfork/rsrc", sizeof(path));
    f = calloc(1, sizeof(ResFile));
    if (f == NULL)
        return sResError = memFullErr;
    f->bytes = readAll(path, &f->size);
    if (f->bytes == NULL) {
        free(f);
        return sResError = eofErr;
    }
    f->dataOffset = be32(f->bytes);
    f->mapOffset = be32(f->bytes + 4);
    f->dataLength = be32(f->bytes + 8);
    f->mapLength = be32(f->bytes + 12);
    if (!validMap(f)) {
        free(f->bytes);
        free(f);
        return sResError = mapReadErr;
    }
    f->refNum = sNextRefNum++;
    f->next = sFiles;
    sFiles = f;
    sCurrent = f->refNum;
    if (refNum != NULL)
        *refNum = f->refNum;
    return sResError = noErr;
}

ResFileRefNum FSOpenResFile(const FSRef *ref, SInt8 permission) {
    static const UniChar rsrc[] = { 'R', 'E', 'S', 'O', 'U', 'R', 'C', 'E', '_', 'F', 'O', 'R', 'K' };
    ResFileRefNum refNum = kResFileNotOpened;
    FSOpenResourceFile(ref, 13, rsrc, permission, &refNum);
    return refNum;
}

void CloseResFile(ResFileRefNum refNum) {
    ResFile **link = &sFiles;
    while (*link != NULL && (*link)->refNum != refNum)
        link = &(*link)->next;
    if (*link == NULL) {
        sResError = resFNotFound;
        return;
    }
    ResFile *f = *link;
    *link = f->next;
    /* Resources read from it stay as plain handles. */
    for (LoadedResource *r = sLoaded; r != NULL; r = r->next)
        if (r->file == refNum)
            r->file = kResFileNotOpened;
    free(f->bytes);
    free(f);
    if (sCurrent == refNum)
        sCurrent = (sFiles != NULL) ? sFiles->refNum : kResFileNotOpened;
    sResError = noErr;
}

ResFileRefNum CurResFile(void) {
    return sCurrent;
}

void UseResFile(ResFileRefNum refNum) {
    if (fileFor(refNum) == NULL) {
        sResError = resFNotFound;
        return;
    }
    sCurrent = refNum;
    sResError = noErr;
}

void SetResLoad(Boolean load) {
    sResLoad = load;
}

/* Walks the references of a type: calls back with each one's ID, name and
   data offset; stops when it says so. */
typedef Boolean (*RefVisitor)(const ResFile *f, ResType type, ResID id, const UInt8 *name, UInt8 attributes,
                              UInt32 dataOffset, void *context);

static Boolean visit(const ResFile *f, ResType wanted, Boolean anyType, RefVisitor visitor, void *context) {
    const UInt8 *map = f->bytes + f->mapOffset;
    UInt16 typeListOffset = be16(map + 24), nameListOffset = be16(map + 26);
    const UInt8 *typeList = map + typeListOffset;
    int types = (SInt16)be16(typeList) + 1;
    const UInt8 *end = f->bytes + f->size;

    for (int t = 0; t < types; t++) {
        const UInt8 *entry = typeList + 2 + t * 8;
        if (entry + 8 > end)
            return false;
        ResType type = be32(entry);
        int count = be16(entry + 4) + 1;
        const UInt8 *refs = typeList + be16(entry + 6);
        if (!anyType && type != wanted)
            continue;
        for (int i = 0; i < count; i++) {
            const UInt8 *r = refs + i * 12;
            if (r + 12 > end)
                return false;
            ResID id = (ResID)be16(r);
            SInt16 nameOffset = (SInt16)be16(r + 2);
            UInt8 attributes = r[4];
            UInt32 dataOffset = ((UInt32)r[5] << 16) | ((UInt32)r[6] << 8) | r[7];
            const UInt8 *name = NULL;
            if (nameOffset != -1) {
                name = map + nameListOffset + (UInt16)nameOffset;
                if (name >= end || name + 1 + name[0] > end)
                    name = NULL;
            }
            if (visitor(f, type, id, name, attributes, dataOffset, context))
                return true;
        }
    }
    return false;
}

typedef struct Match {
    Boolean byName, byIndex;
    ResID id;
    const UInt8 *name;
    int index, seen;
    ResType type;
    ResID foundID;
    UInt8 foundName[256];
    UInt8 attributes;
    UInt32 dataOffset;
} Match;

static Boolean matchVisitor(const ResFile *f, ResType type, ResID id, const UInt8 *name, UInt8 attributes,
                            UInt32 dataOffset, void *context) {
    Match *m = context;
    Boolean hit;
    if (m->byIndex)
        hit = (++m->seen == m->index);
    else if (m->byName)
        hit = (name != NULL && name[0] == m->name[0] && memcmp(name + 1, m->name + 1, name[0]) == 0);
    else
        hit = (id == m->id);
    if (!hit)
        return false;
    m->type = type;
    m->foundID = id;
    m->foundName[0] = 0;
    if (name != NULL)
        memcpy(m->foundName, name, (size_t)name[0] + 1);
    m->attributes = attributes;
    m->dataOffset = dataOffset;
    return true;
}

static Handle load(const ResFile *f, Match *m) {
    size_t at = (size_t)f->dataOffset + m->dataOffset;
    if (at + 4 > f->size) {
        sResError = mapReadErr;
        return NULL;
    }
    UInt32 length = be32(f->bytes + at);
    if (at + 4 + length > f->size) {
        sResError = mapReadErr;
        return NULL;
    }
    Handle h = sResLoad ? NewHandle(length) : NewEmptyHandle();
    if (h == NULL) {
        sResError = memFullErr;
        return NULL;
    }
    if (sResLoad && length > 0)
        memcpy(*h, f->bytes + at + 4, length);
    LoadedResource *r = calloc(1, sizeof(LoadedResource));
    if (r != NULL) {
        r->handle = h;
        r->file = f->refNum;
        r->type = m->type;
        r->id = m->foundID;
        r->attributes = m->attributes;
        memcpy(r->name, m->foundName, (size_t)m->foundName[0] + 1);
        r->next = sLoaded;
        sLoaded = r;
    }
    sResError = noErr;
    return h;
}

static Handle find(ResType type, Match *m, Boolean chain) {
    ResFile *f = fileFor(sCurrent);
    if (f == NULL) {
        sResError = resFNotFound;
        return NULL;
    }
    for (; f != NULL; f = chain ? f->next : NULL) {
        m->seen = 0;
        if (visit(f, type, false, matchVisitor, m))
            return load(f, m);
    }
    sResError = resNotFound;
    return NULL;
}

Handle Get1Resource(ResType theType, ResID theID) {
    Match m = { .id = theID };
    return find(theType, &m, false);
}

Handle GetResource(ResType theType, ResID theID) {
    Match m = { .id = theID };
    return find(theType, &m, true);
}

Handle Get1NamedResource(ResType theType, ConstStr255Param name) {
    Match m = { .byName = true, .name = name };
    if (name == NULL)
        return NULL;
    return find(theType, &m, false);
}

Handle GetNamedResource(ResType theType, ConstStr255Param name) {
    Match m = { .byName = true, .name = name };
    if (name == NULL)
        return NULL;
    return find(theType, &m, true);
}

Handle Get1IndResource(ResType theType, ResourceIndex index) {
    Match m = { .byIndex = true, .index = index };
    if (index < 1) {
        sResError = resNotFound;
        return NULL;
    }
    return find(theType, &m, false);
}

static Boolean countVisitor(const ResFile *f, ResType type, ResID id, const UInt8 *name, UInt8 attributes,
                            UInt32 dataOffset, void *context) {
    (*(int *)context)++;
    return false;
}

ResourceCount Count1Resources(ResType theType) {
    ResFile *f = fileFor(sCurrent);
    int count = 0;
    if (f == NULL) {
        sResError = resFNotFound;
        return 0;
    }
    visit(f, theType, false, countVisitor, &count);
    sResError = noErr;
    return (ResourceCount)count;
}

ResourceCount Count1Types(void) {
    ResFile *f = fileFor(sCurrent);
    if (f == NULL) {
        sResError = resFNotFound;
        return 0;
    }
    sResError = noErr;
    return (ResourceCount)((SInt16)be16(f->bytes + f->mapOffset + be16(f->bytes + f->mapOffset + 24)) + 1);
}

void Get1IndType(ResType *theType, ResourceIndex index) {
    ResFile *f = fileFor(sCurrent);
    if (theType == NULL)
        return;
    *theType = 0;
    if (f == NULL || index < 1 || index > Count1Types()) {
        sResError = resNotFound;
        return;
    }
    const UInt8 *typeList = f->bytes + f->mapOffset + be16(f->bytes + f->mapOffset + 24);
    *theType = be32(typeList + 2 + (index - 1) * 8);
    sResError = noErr;
}

static LoadedResource *loaded(Handle h) {
    for (LoadedResource *r = sLoaded; r != NULL; r = r->next)
        if (r->handle == h)
            return r;
    return NULL;
}

void GetResInfo(Handle theResource, ResID *theID, ResType *theType, Str255 name) {
    LoadedResource *r = loaded(theResource);
    if (r == NULL) {
        sResError = resNotFound;
        return;
    }
    if (theID != NULL) *theID = r->id;
    if (theType != NULL) *theType = r->type;
    if (name != NULL) memcpy(name, r->name, (size_t)r->name[0] + 1);
    sResError = noErr;
}

ResAttributes GetResAttrs(Handle theResource) {
    LoadedResource *r = loaded(theResource);
    sResError = (r != NULL) ? noErr : resNotFound;
    return (r != NULL) ? r->attributes : 0;
}

long GetResourceSizeOnDisk(Handle theResource) {
    LoadedResource *r = loaded(theResource);
    if (r == NULL) {
        sResError = resNotFound;
        return -1;
    }
    sResError = noErr;
    return GetHandleSize(theResource);
}

static void forget(Handle h, Boolean dispose) {
    LoadedResource **link = &sLoaded;
    while (*link != NULL && (*link)->handle != h)
        link = &(*link)->next;
    if (*link == NULL) {
        sResError = resNotFound;
        return;
    }
    LoadedResource *r = *link;
    *link = r->next;
    free(r);
    if (dispose)
        DisposeHandle(h);
    sResError = noErr;
}

void ReleaseResource(Handle theResource) {
    forget(theResource, true);
}

void DetachResource(Handle theResource) {
    forget(theResource, false);
}

/* With SetResLoad(false) a resource comes empty: loading reads it now. */
void LoadResource(Handle theResource) {
    LoadedResource *r = loaded(theResource);
    ResFile *f = (r != NULL) ? fileFor(r->file) : NULL;
    Match m = { .id = (r != NULL) ? r->id : 0 };
    if (r == NULL || f == NULL) {
        sResError = resNotFound;
        return;
    }
    if (GetHandleSize(theResource) > 0) {
        sResError = noErr;
        return;
    }
    if (!visit(f, r->type, false, matchVisitor, &m)) {
        sResError = resNotFound;
        return;
    }
    size_t at = (size_t)f->dataOffset + m.dataOffset;
    UInt32 length = be32(f->bytes + at);
    SetHandleSize(theResource, length);
    if (MemError() == noErr)
        memcpy(*theResource, f->bytes + at + 4, length);
    sResError = MemError();
}
