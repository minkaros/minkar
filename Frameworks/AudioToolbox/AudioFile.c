/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio files of linear PCM: WAVE (and RF64 and the extensible format),
 * AIFF, AIFC and CAF, read and written by URL or through an app's
 * callbacks. A packet is a frame. The containers' layouts are the public
 * specifications'. Compressed files come with the codecs (AU4).
 */

#include "AUInternal.h"
#include <AudioToolbox/AudioFile.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define kFileMagic 0x41466c65u     /* 'AFle' */

struct OpaqueAudioFileID {
    uint32_t magic;
    AudioFileTypeID type;
    AudioStreamBasicDescription format;
    int fd;                     /* -1 with callbacks */
    void *client;
    AudioFile_ReadProc readProc;
    AudioFile_WriteProc writeProc;
    AudioFile_GetSizeProc getSizeProc;
    AudioFile_SetSizeProc setSizeProc;
    Boolean writable, dirty, deferSizes;
    SInt64 dataOffset, dataBytes;
    UInt32 sourceBits;          /* valid bits, when the container's are more */
    void *cookie;
    UInt32 cookieSize;

    /* User data: the chunks that are not the container's own. */
    struct UserChunk {
        UInt32 id;
        SInt64 offset;          /* its body in the file, or -1 when only in memory */
        UInt32 size;
        void *data;             /* in memory: set by the app, or read to be moved */
        Boolean afterData;
    } *chunks;
    UInt32 chunkCount;
    Boolean userDirty;

    /* A file that was opened (not made here) keeps its layout: only its
       size fields change, where they are (0: none). */
    Boolean opened;
    SInt64 dataSizeAt, framesAt;
};

static Boolean alive(AudioFileID f) {
    return f != NULL && f->magic == kFileMagic;
}

/* ---- I/O ------------------------------------------------------------------ */

static OSStatus readAt(AudioFileID f, SInt64 pos, UInt32 n, void *buf, UInt32 *got) {
    if (f->fd >= 0) {
        ssize_t r = pread(f->fd, buf, n, pos);
        if (r < 0)
            return kAudioFilePositionError;
        *got = (UInt32)r;
        return noErr;
    }
    if (f->readProc == NULL)
        return kAudioFilePermissionsError;
    *got = 0;
    return f->readProc(f->client, pos, n, buf, got);
}

static OSStatus writeAt(AudioFileID f, SInt64 pos, UInt32 n, const void *buf) {
    UInt32 put = 0;

    if (!f->writable)
        return kAudioFilePermissionsError;
    if (f->fd >= 0)
        return pwrite(f->fd, buf, n, pos) == (ssize_t)n ? noErr : kAudioFilePositionError;
    if (f->writeProc == NULL)
        return kAudioFilePermissionsError;
    if (f->writeProc(f->client, pos, n, buf, &put) != noErr || put != n)
        return kAudioFilePositionError;
    return noErr;
}

static SInt64 fileSize(AudioFileID f) {
    struct stat st;

    if (f->fd >= 0)
        return fstat(f->fd, &st) == 0 ? st.st_size : 0;
    return f->getSizeProc != NULL ? f->getSizeProc(f->client) : 0;
}

static Boolean readExact(AudioFileID f, SInt64 pos, UInt32 n, void *buf) {
    UInt32 got = 0;
    return readAt(f, pos, n, buf, &got) == noErr && got == n;
}

static UInt16 le16(const unsigned char *p) { return (UInt16)(p[0] | p[1] << 8); }
static UInt32 le32(const unsigned char *p) { return (UInt32)p[0] | (UInt32)p[1] << 8 | (UInt32)p[2] << 16 | (UInt32)p[3] << 24; }
static UInt64 le64(const unsigned char *p) { return le32(p) | (UInt64)le32(p + 4) << 32; }
static UInt16 be16(const unsigned char *p) { return (UInt16)(p[0] << 8 | p[1]); }
static UInt32 be32(const unsigned char *p) { return (UInt32)p[0] << 24 | (UInt32)p[1] << 16 | (UInt32)p[2] << 8 | p[3]; }
static UInt64 be64(const unsigned char *p) { return (UInt64)be32(p) << 32 | be32(p + 4); }
static void putLE16(unsigned char *p, UInt32 v) { p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; }
static void putLE32(unsigned char *p, UInt32 v) { putLE16(p, v & 0xffff); putLE16(p + 2, v >> 16); }
static void putBE16(unsigned char *p, UInt32 v) { p[0] = (v >> 8) & 0xff; p[1] = v & 0xff; }
static void putBE32(unsigned char *p, UInt32 v) { putBE16(p, v >> 16); putBE16(p + 2, v & 0xffff); }
static void putBE64(unsigned char *p, UInt64 v) { putBE32(p, (UInt32)(v >> 32)); putBE32(p + 4, (UInt32)v); }

/* AIFF's rates: an 80-bit extended float. */
static double fromExtended(const unsigned char *p) {
    int exponent = ((p[0] & 0x7f) << 8 | p[1]) - 16383;
    UInt64 mantissa = be64(p + 2);
    double v = ldexp((double)mantissa, exponent - 63);
    return (p[0] & 0x80) ? -v : v;
}

static void toExtended(unsigned char *p, double v) {
    int exponent;
    double m;
    UInt64 mantissa;

    memset(p, 0, 10);
    if (v <= 0)
        return;
    m = frexp(v, &exponent);          /* v = m * 2^exponent, m in [0.5, 1) */
    mantissa = (UInt64)ldexp(m, 64);
    exponent += 16382;
    putBE16(p, (UInt32)exponent);
    putBE64(p + 2, mantissa);
}

static void pcmFormat(AudioStreamBasicDescription *f, Float64 rate, UInt32 channels, UInt32 bits, UInt32 flags) {
    memset(f, 0, sizeof(*f));
    f->mSampleRate = rate;
    f->mFormatID = kAudioFormatLinearPCM;
    f->mFormatFlags = flags | kAudioFormatFlagIsPacked;
    f->mChannelsPerFrame = channels;
    f->mBitsPerChannel = bits;
    f->mFramesPerPacket = 1;
    f->mBytesPerFrame = f->mBytesPerPacket = bits / 8 * channels;
}

/* ---- User data ------------------------------------------------------------------ */

static Boolean structural(AudioFileTypeID type, UInt32 id) {
    switch (type) {
    case kAudioFileCAFType:
        return id == 'desc' || id == 'data' || id == 'pakt' || id == 'kuki' || id == 'free';
    case kAudioFileAIFFType:
    case kAudioFileAIFCType:
        return id == 'COMM' || id == 'SSND' || id == 'FVER' || id == 'JUNK';
    default:
        return id == 'fmt ' || id == 'data' || id == 'ds64' || id == 'fact' || id == 'JUNK';
    }
}

static void noteChunk(AudioFileID f, UInt32 id, SInt64 offset, SInt64 size, Boolean afterData) {
    struct UserChunk *grown;

    if (structural(f->type, id) || size < 0 || size > 0x7FFFFFFF)
        return;
    grown = realloc(f->chunks, (f->chunkCount + 1) * sizeof(*grown));
    if (grown == NULL)
        return;
    f->chunks = grown;
    memset(&f->chunks[f->chunkCount], 0, sizeof(*grown));
    f->chunks[f->chunkCount].id = id;
    f->chunks[f->chunkCount].offset = offset;
    f->chunks[f->chunkCount].size = (UInt32)size;
    f->chunks[f->chunkCount].afterData = afterData;
    f->chunkCount++;
}

/* ---- Reading the containers ------------------------------------------------- */

static OSStatus parseWAVE(AudioFileID f, SInt64 size, Boolean rf64) {
    unsigned char h[40];
    SInt64 pos = 12, dataSize64 = -1;
    Boolean haveFormat = false;

    while (pos + 8 <= size) {
        UInt32 id, len;
        if (!readExact(f, pos, 8, h))
            break;
        id = be32(h);
        len = le32(h + 4);
        if (id == 'ds64' && len >= 24 && readExact(f, pos + 8, 24, h)) {
            dataSize64 = (SInt64)le64(h + 8);
        } else if (id == 'fmt ') {
            UInt32 n = len < sizeof(h) ? len : sizeof(h), tag, channels, bits, align;
            if (n < 16 || !readExact(f, pos + 8, n, h))
                return kAudioFileInvalidFileError;
            tag = le16(h);
            channels = le16(h + 2);
            align = le16(h + 12);
            bits = le16(h + 14);
            if (tag == 0xFFFE && n >= 40) {
                f->sourceBits = le16(h + 18);
                tag = le16(h + 24);     /* the sub-format's first two bytes */
            }
            if (channels == 0 || align == 0 || align % channels != 0)
                return kAudioFileInvalidFileError;
            bits = align / channels * 8;    /* the container's */
            if (tag == 1)
                pcmFormat(&f->format, le32(h + 4), channels, bits, bits > 8 ? kAudioFormatFlagIsSignedInteger : 0);
            else if (tag == 3)
                pcmFormat(&f->format, le32(h + 4), channels, bits, kAudioFormatFlagIsFloat);
            else
                return kAudioFileUnsupportedDataFormatError;
            haveFormat = true;
        } else if (id == 'data') {
            if (!haveFormat)
                return kAudioFileInvalidFileError;
            f->dataSizeAt = rf64 ? 0 : pos + 4;
            f->dataOffset = pos + 8;
            f->dataBytes = rf64 && len == 0xFFFFFFFFu && dataSize64 >= 0 ? dataSize64 : len;
            if (f->dataOffset + f->dataBytes > size)
                f->dataBytes = size - f->dataOffset;
            pos = f->dataOffset + f->dataBytes + (f->dataBytes & 1);
            continue;   /* the chunks after it */
        } else {
            noteChunk(f, id, pos + 8, len, f->dataOffset > 0);
        }
        pos += 8 + (SInt64)len + (len & 1);
    }
    return f->dataOffset > 0 ? noErr : kAudioFileInvalidFileError;
}

static OSStatus parseAIFF(AudioFileID f, SInt64 size, Boolean aifc) {
    unsigned char h[32];
    SInt64 pos = 12;
    UInt32 frames = 0;
    Boolean haveFormat = false;

    while (pos + 8 <= size) {
        UInt32 id, len;
        if (!readExact(f, pos, 8, h))
            break;
        id = be32(h);
        len = be32(h + 4);
        if (id == 'COMM') {
            f->framesAt = pos + 8 + 2;
            UInt32 channels, bits, bytes, flags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian;
            if (len < 18 || !readExact(f, pos + 8, len < 22 ? len : 22, h))
                return kAudioFileInvalidFileError;
            channels = be16(h);
            frames = be32(h + 2);
            bits = be16(h + 6);
            if (aifc && len >= 22) {
                switch (be32(h + 18)) {
                case 'NONE':
                case 'twos':
                    break;
                case 'sowt':
                    flags = kAudioFormatFlagIsSignedInteger;
                    break;
                case 'fl32':
                case 'FL32':
                    flags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsBigEndian;
                    bits = 32;
                    break;
                case 'fl64':
                case 'FL64':
                    flags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsBigEndian;
                    bits = 64;
                    break;
                default:
                    return kAudioFileUnsupportedDataFormatError;
                }
            }
            if (channels == 0 || bits == 0)
                return kAudioFileInvalidFileError;
            bytes = (bits + 7) / 8;
            if (bytes * 8 != bits)
                f->sourceBits = bits;
            pcmFormat(&f->format, fromExtended(h + 8), channels, bytes * 8, flags);
            haveFormat = true;
        } else if (id == 'SSND') {
            UInt32 offset;
            if (!haveFormat || len < 8 || !readExact(f, pos + 8, 8, h))
                return kAudioFileInvalidFileError;
            offset = be32(h);
            f->dataSizeAt = pos + 4;
            f->dataOffset = pos + 16 + offset;
            f->dataBytes = (SInt64)len - 8 - offset;
            if ((SInt64)frames * f->format.mBytesPerFrame < f->dataBytes)
                f->dataBytes = (SInt64)frames * f->format.mBytesPerFrame;
            if (f->dataOffset + f->dataBytes > size)
                f->dataBytes = size - f->dataOffset;
            pos += 8 + (SInt64)len + (len & 1);
            continue;   /* the chunks after it */
        } else {
            noteChunk(f, id, pos + 8, len, f->dataOffset > 0);
        }
        pos += 8 + (SInt64)len + (len & 1);
    }
    return f->dataOffset > 0 ? noErr : kAudioFileInvalidFileError;
}

enum { kCAFFloat = 1, kCAFLittleEndian = 2 };

static OSStatus parseCAF(AudioFileID f, SInt64 size) {
    unsigned char h[32];
    SInt64 pos = 8;
    Boolean haveFormat = false;

    while (pos + 12 <= size) {
        UInt32 id;
        SInt64 len;
        if (!readExact(f, pos, 12, h))
            break;
        id = be32(h);
        len = (SInt64)be64(h + 4);
        if (id == 'desc') {
            UInt64 rate;
            UInt32 flags, cafFlags, bits, channels;
            double r;
            if (len < 32 || !readExact(f, pos + 12, 32, h))
                return kAudioFileInvalidFileError;
            if (be32(h + 8) != kAudioFormatLinearPCM)
                return kAudioFileUnsupportedDataFormatError;
            rate = be64(h);
            memcpy(&r, &rate, sizeof(r));
            cafFlags = be32(h + 12);
            channels = be32(h + 24);
            bits = be32(h + 28);
            flags = (cafFlags & kCAFFloat) ? kAudioFormatFlagIsFloat : kAudioFormatFlagIsSignedInteger;
            if (!(cafFlags & kCAFLittleEndian))
                flags |= kAudioFormatFlagIsBigEndian;
            pcmFormat(&f->format, r, channels, bits, flags);
            if (be32(h + 16) != f->format.mBytesPerPacket || be32(h + 20) != 1)
                return kAudioFileUnsupportedDataFormatError;
            haveFormat = true;
        } else if (id == 'kuki' && len > 0 && len < (1 << 20)) {
            free(f->cookie);
            f->cookie = malloc((size_t)len);
            if (f->cookie != NULL && readExact(f, pos + 12, (UInt32)len, f->cookie))
                f->cookieSize = (UInt32)len;
        } else if (id == 'data') {
            if (!haveFormat)
                return kAudioFileInvalidFileError;
            f->dataSizeAt = pos + 4;
            f->dataOffset = pos + 12 + 4;   /* after the edit count */
            f->dataBytes = len < 0 ? size - f->dataOffset : len - 4;
            if (f->dataOffset + f->dataBytes > size)
                f->dataBytes = size - f->dataOffset;
            if (len < 0)
                break;
            pos += 12 + len;
            continue;   /* the chunks after it */
        } else {
            noteChunk(f, id, pos + 12, len, f->dataOffset > 0);
        }
        if (len < 0)
            break;
        pos += 12 + len;
    }
    return f->dataOffset > 0 ? noErr : kAudioFileInvalidFileError;
}

static OSStatus parse(AudioFileID f) {
    unsigned char h[12];
    SInt64 size = fileSize(f);

    if (!readExact(f, 0, 12, h))
        return kAudioFileInvalidFileError;
    if ((memcmp(h, "RIFF", 4) == 0 || memcmp(h, "RF64", 4) == 0 || memcmp(h, "BW64", 4) == 0) &&
        memcmp(h + 8, "WAVE", 4) == 0) {
        f->type = memcmp(h, "RIFF", 4) == 0 ? kAudioFileWAVEType : kAudioFileRF64Type;
        return parseWAVE(f, size, f->type != kAudioFileWAVEType);
    }
    if (memcmp(h, "FORM", 4) == 0 && (memcmp(h + 8, "AIFF", 4) == 0 || memcmp(h + 8, "AIFC", 4) == 0)) {
        f->type = memcmp(h + 8, "AIFF", 4) == 0 ? kAudioFileAIFFType : kAudioFileAIFCType;
        return parseAIFF(f, size, f->type == kAudioFileAIFCType);
    }
    if (memcmp(h, "caff", 4) == 0 && be16(h + 4) == 1) {
        f->type = kAudioFileCAFType;
        return parseCAF(f, size);
    }
    return kAudioFileUnsupportedFileTypeError;
}

/* ---- A header in memory (for AudioFileStream) ------------------------------ */

typedef struct {
    const unsigned char *bytes;
    UInt32 size;
} HeaderBytes;

static OSStatus headerRead(void *client, SInt64 position, UInt32 count, void *buffer, UInt32 *actual) {
    HeaderBytes *h = client;

    *actual = 0;
    if (position < 0 || position >= h->size)
        return noErr;
    if (position + count > h->size)
        count = (UInt32)(h->size - position);
    memcpy(buffer, h->bytes + position, count);
    *actual = count;
    return noErr;
}

/* As large as a stream may be: the header's sizes are not cut to what has
   come so far. */
static SInt64 headerSize(void *client) {
    return (SInt64)1 << 50;
}

/* The start of a stream: its type, format and where its audio data starts
   (and how long it is, or -1). kAudioFileInvalidFileError while the header
   is not all there yet; another error when it never will be. */
OSStatus _AudioFileParseHeader(const void *bytes, UInt32 size, AudioFileTypeID *type,
                               AudioStreamBasicDescription *format, SInt64 *dataOffset, SInt64 *dataBytes) {
    HeaderBytes h = { bytes, size };
    struct OpaqueAudioFileID f;
    OSStatus status;

    memset(&f, 0, sizeof(f));
    f.magic = kFileMagic;
    f.fd = -1;
    f.client = &h;
    f.readProc = headerRead;
    f.getSizeProc = headerSize;
    status = parse(&f);
    free(f.cookie);
    if (status != noErr)
        return size < 12 ? kAudioFileInvalidFileError : status;
    *type = f.type;
    *format = f.format;
    *dataOffset = f.dataOffset;
    /* A stream written as it goes has no length (0 or all ones). */
    *dataBytes = f.dataBytes == 0 || f.dataBytes >= 0xFFFFFFFFLL ? -1 : f.dataBytes;
    return noErr;
}

/* ---- Writing the containers ---------------------------------------------- */

static Boolean writableFormat(AudioFileTypeID type, const AudioStreamBasicDescription *d) {
    Boolean big = (d->mFormatFlags & kAudioFormatFlagIsBigEndian) != 0;
    Boolean fl = (d->mFormatFlags & kAudioFormatFlagIsFloat) != 0;
    Boolean sgn = (d->mFormatFlags & kAudioFormatFlagIsSignedInteger) != 0;

    if (!_AUFormatIsSupported(d) || (d->mFormatFlags & kAudioFormatFlagIsNonInterleaved))
        return false;
    switch (type) {
    case kAudioFileWAVEType:
        /* Little-endian; 8-bit is unsigned, wider integers signed. */
        return !big && (fl || (d->mBitsPerChannel == 8 ? !sgn : sgn));
    case kAudioFileAIFFType:
        return big && sgn;
    case kAudioFileAIFCType:
        return (sgn && (big || d->mBitsPerChannel == 16)) || (fl && big);
    case kAudioFileCAFType:
        return fl || sgn;
    default:
        return false;
    }
}

/* Writes the header with the current sizes. */
/* An opened file's sizes, in its own fields. */
static OSStatus patchSizes(AudioFileID f) {
    unsigned char b[8];
    SInt64 total = f->dataOffset + f->dataBytes;

    if (f->dataSizeAt <= 0)
        return kAudioFileOperationNotSupportedError;    /* RF64's are elsewhere */
    switch (f->type) {
    case kAudioFileWAVEType:
        putLE32(b, (UInt32)f->dataBytes);
        writeAt(f, f->dataSizeAt, 4, b);
        putLE32(b, (UInt32)(total + (f->dataBytes & 1) - 8));
        return writeAt(f, 4, 4, b);
    case kAudioFileAIFFType:
    case kAudioFileAIFCType:
        putBE32(b, (UInt32)(f->dataBytes + 8 + (f->dataOffset - f->dataSizeAt - 12)));
        writeAt(f, f->dataSizeAt, 4, b);
        if (f->framesAt > 0) {
            putBE32(b, (UInt32)(f->dataBytes / f->format.mBytesPerFrame));
            writeAt(f, f->framesAt, 4, b);
        }
        putBE32(b, (UInt32)(total + (f->dataBytes & 1) - 8));
        return writeAt(f, 4, 4, b);
    case kAudioFileCAFType:
        putBE64(b, (UInt64)(f->dataBytes + 4));
        return writeAt(f, f->dataSizeAt, 8, b);
    default:
        return kAudioFileUnsupportedFileTypeError;
    }
}

static OSStatus writeHeader(AudioFileID f) {
    if (f->opened)
        return patchSizes(f);
    unsigned char h[80];
    const AudioStreamBasicDescription *d = &f->format;
    UInt32 frames = (UInt32)(f->dataBytes / d->mBytesPerFrame), n;

    memset(h, 0, sizeof(h));
    switch (f->type) {
    case kAudioFileWAVEType:
        memcpy(h, "RIFF", 4);
        putLE32(h + 4, (UInt32)(36 + f->dataBytes + (f->dataBytes & 1)));
        memcpy(h + 8, "WAVEfmt ", 8);
        putLE32(h + 16, 16);
        putLE16(h + 20, (d->mFormatFlags & kAudioFormatFlagIsFloat) ? 3 : 1);
        putLE16(h + 22, d->mChannelsPerFrame);
        putLE32(h + 24, (UInt32)d->mSampleRate);
        putLE32(h + 28, (UInt32)d->mSampleRate * d->mBytesPerFrame);
        putLE16(h + 32, d->mBytesPerFrame);
        putLE16(h + 34, d->mBitsPerChannel);
        memcpy(h + 36, "data", 4);
        putLE32(h + 40, (UInt32)f->dataBytes);
        n = 44;
        break;
    case kAudioFileAIFFType:
    case kAudioFileAIFCType: {
        Boolean c = f->type == kAudioFileAIFCType;
        UInt32 comm = c ? 24 : 18, at;
        memcpy(h, "FORM", 4);
        memcpy(h + 8, c ? "AIFC" : "AIFF", 4);
        at = 12;
        if (c) {
            memcpy(h + at, "FVER", 4);
            putBE32(h + at + 4, 4);
            putBE32(h + at + 8, 0xA2805140u);
            at += 12;
        }
        memcpy(h + at, "COMM", 4);
        putBE32(h + at + 4, comm);
        putBE16(h + at + 8, d->mChannelsPerFrame);
        putBE32(h + at + 10, frames);
        putBE16(h + at + 14, d->mBitsPerChannel);
        toExtended(h + at + 16, d->mSampleRate);
        if (c) {
            UInt32 code = (d->mFormatFlags & kAudioFormatFlagIsFloat)
                          ? (d->mBitsPerChannel == 64 ? 'fl64' : 'fl32')
                          : ((d->mFormatFlags & kAudioFormatFlagIsBigEndian) ? 'NONE' : 'sowt');
            putBE32(h + at + 26, code);
            /* An empty name: its count and a pad byte. */
        }
        at += 8 + comm;
        memcpy(h + at, "SSND", 4);
        putBE32(h + at + 4, (UInt32)(8 + f->dataBytes));
        at += 16;   /* offset and block size, 0 */
        putBE32(h + 4, (UInt32)(at - 8 + f->dataBytes + (f->dataBytes & 1)));
        n = at;
        break;
    }
    case kAudioFileCAFType: {
        double rate = d->mSampleRate;
        UInt64 bits;
        UInt32 flags = 0;
        memcpy(&bits, &rate, sizeof(bits));
        memcpy(h, "caff", 4);
        putBE16(h + 4, 1);
        memcpy(h + 8, "desc", 4);
        putBE64(h + 12, 32);
        putBE64(h + 20, bits);
        putBE32(h + 28, kAudioFormatLinearPCM);
        if (d->mFormatFlags & kAudioFormatFlagIsFloat)
            flags |= kCAFFloat;
        if (!(d->mFormatFlags & kAudioFormatFlagIsBigEndian))
            flags |= kCAFLittleEndian;
        putBE32(h + 32, flags);
        putBE32(h + 36, d->mBytesPerPacket);
        putBE32(h + 40, 1);
        putBE32(h + 44, d->mChannelsPerFrame);
        putBE32(h + 48, d->mBitsPerChannel);
        memcpy(h + 52, "data", 4);
        putBE64(h + 56, (UInt64)(f->dataBytes + 4));
        putBE32(h + 64, 0);     /* edit count */
        n = 68;
        break;
    }
    default:
        return kAudioFileUnsupportedFileTypeError;
    }
    f->dataOffset = n;
    return writeAt(f, 0, n, h);
}

/* ---- Opening and closing ------------------------------------------------------ */

static void keepChunksAfterData(AudioFileID f);

static AudioFileID newFile(void) {
    AudioFileID f = calloc(1, sizeof(*f));

    if (f != NULL) {
        f->magic = kFileMagic;
        f->fd = -1;
    }
    return f;
}

static void freeFile(AudioFileID f) {
    if (f->fd >= 0)
        close(f->fd);
    for (UInt32 i = 0; i < f->chunkCount; i++)
        free(f->chunks[i].data);
    free(f->chunks);
    free(f->cookie);
    f->magic = 0;
    free(f);
}

static Boolean pathOf(CFURLRef url, char *path, size_t size) {
    return url != NULL && CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, (CFIndex)size);
}

static OSStatus create(AudioFileID f, AudioFileTypeID type, const AudioStreamBasicDescription *format) {
    if (format == NULL)
        return kAudio_ParamError;
    if (type == kAudioFileWAVEType || type == kAudioFileAIFFType || type == kAudioFileAIFCType ||
        type == kAudioFileCAFType) {
        if (!writableFormat(type, format))
            return kAudioFileUnsupportedDataFormatError;
    } else {
        return kAudioFileUnsupportedFileTypeError;
    }
    f->type = type;
    f->format = *format;
    f->format.mFormatFlags |= kAudioFormatFlagIsPacked;
    f->writable = true;
    f->dataBytes = 0;
    f->dirty = true;
    return writeHeader(f);
}

OSStatus AudioFileCreateWithURL(CFURLRef url, AudioFileTypeID type, const AudioStreamBasicDescription *format,
                                AudioFileFlags flags, AudioFileID *out) {
    char path[1024];
    AudioFileID f;
    OSStatus status;
    int mode = O_RDWR | O_CREAT;

    if (out == NULL || !pathOf(url, path, sizeof(path)))
        return kAudio_ParamError;
    *out = NULL;
    if (!(flags & kAudioFileFlags_EraseFile) && access(path, F_OK) == 0)
        return kAudioFilePermissionsError;  /* it exists and is not to be erased */
    if (flags & kAudioFileFlags_EraseFile)
        mode |= O_TRUNC;
    f = newFile();
    if (f == NULL)
        return kAudio_MemFullError;
    f->fd = open(path, mode, 0644);
    if (f->fd < 0) {
        freeFile(f);
        return errno == EACCES ? kAudioFilePermissionsError : kAudioFileUnspecifiedError;
    }
    status = create(f, type, format);
    if (status != noErr) {
        freeFile(f);
        unlink(path);
        return status;
    }
    *out = f;
    return noErr;
}

OSStatus AudioFileOpenURL(CFURLRef url, AudioFilePermissions permissions, AudioFileTypeID hint, AudioFileID *out) {
    char path[1024];
    AudioFileID f;
    OSStatus status;
    int mode;

    if (out == NULL || !pathOf(url, path, sizeof(path)))
        return kAudio_ParamError;
    *out = NULL;
    mode = (permissions & kAudioFileWritePermission) ? ((permissions & kAudioFileReadPermission) ? O_RDWR : O_WRONLY)
                                                     : O_RDONLY;
    f = newFile();
    if (f == NULL)
        return kAudio_MemFullError;
    f->fd = open(path, mode);
    if (f->fd < 0) {
        int e = errno;
        freeFile(f);
        return e == ENOENT ? kAudioFileFileNotFoundError : (e == EACCES ? kAudioFilePermissionsError
                                                                          : kAudioFileUnspecifiedError);
    }
    f->writable = (permissions & kAudioFileWritePermission) != 0;
    status = parse(f);
    if (status != noErr) {
        freeFile(f);
        return status;
    }
    f->opened = true;
    keepChunksAfterData(f);
    *out = f;
    return noErr;
}

OSStatus AudioFileInitializeWithCallbacks(void *client, AudioFile_ReadProc readProc, AudioFile_WriteProc writeProc,
                                          AudioFile_GetSizeProc getSizeProc, AudioFile_SetSizeProc setSizeProc,
                                          AudioFileTypeID type, const AudioStreamBasicDescription *format,
                                          AudioFileFlags flags, AudioFileID *out) {
    AudioFileID f;
    OSStatus status;

    if (out == NULL || writeProc == NULL)
        return kAudio_ParamError;
    *out = NULL;
    f = newFile();
    if (f == NULL)
        return kAudio_MemFullError;
    f->client = client;
    f->readProc = readProc;
    f->writeProc = writeProc;
    f->getSizeProc = getSizeProc;
    f->setSizeProc = setSizeProc;
    if (setSizeProc != NULL)
        setSizeProc(client, 0);
    status = create(f, type, format);
    if (status != noErr) {
        freeFile(f);
        return status;
    }
    *out = f;
    return noErr;
}

OSStatus AudioFileOpenWithCallbacks(void *client, AudioFile_ReadProc readProc, AudioFile_WriteProc writeProc,
                                    AudioFile_GetSizeProc getSizeProc, AudioFile_SetSizeProc setSizeProc,
                                    AudioFileTypeID hint, AudioFileID *out) {
    AudioFileID f;
    OSStatus status;

    if (out == NULL || readProc == NULL || getSizeProc == NULL)
        return kAudio_ParamError;
    *out = NULL;
    f = newFile();
    if (f == NULL)
        return kAudio_MemFullError;
    f->client = client;
    f->readProc = readProc;
    f->writeProc = writeProc;
    f->getSizeProc = getSizeProc;
    f->setSizeProc = setSizeProc;
    f->writable = writeProc != NULL;
    status = parse(f);
    if (status != noErr) {
        freeFile(f);
        return status;
    }
    f->opened = true;
    keepChunksAfterData(f);
    *out = f;
    return noErr;
}

static OSStatus writeUserChunks(AudioFileID f);

/* The header's sizes; when the file is done (closed or optimized), the
   user data after the audio data too. */
static OSStatus updateSizesFinal(AudioFileID f, Boolean final) {
    OSStatus status;
    Boolean chunksAfter = false;

    for (UInt32 i = 0; i < f->chunkCount; i++)
        chunksAfter |= f->chunks[i].afterData || f->chunks[i].data != NULL;
    if (!f->dirty && !f->userDirty)
        return noErr;
    if (f->dirty) {
        if (f->dataBytes & 1) {
            /* RIFF and AIFF chunks are padded to an even size. */
            unsigned char zero = 0;
            if (f->type != kAudioFileCAFType)
                writeAt(f, f->dataOffset + f->dataBytes, 1, &zero);
        }
        status = writeHeader(f);
        if (status != noErr)
            return status;
    }
    f->dirty = false;
    if (final && (f->userDirty || chunksAfter)) {
        f->userDirty = false;
        return writeUserChunks(f);
    }
    return noErr;
}

static OSStatus updateSizes(AudioFileID f) {
    return updateSizesFinal(f, false);
}

/* A file opened to write: the chunks after its audio data are read now,
   before more data is written over them. */
static void keepChunksAfterData(AudioFileID f) {
    if (!f->writable)
        return;
    for (UInt32 i = 0; i < f->chunkCount; i++) {
        struct UserChunk *c = &f->chunks[i];
        if (c->afterData && c->data == NULL && c->offset >= 0) {
            c->data = malloc(c->size > 0 ? c->size : 1);
            if (c->data != NULL && !readExact(f, c->offset, c->size, c->data)) {
                free(c->data);
                c->data = NULL;
            }
        }
    }
}

OSStatus AudioFileClose(AudioFileID f) {
    OSStatus status = noErr;

    if (!alive(f))
        return kAudio_ParamError;
    if (f->writable)
        status = updateSizesFinal(f, true);
    freeFile(f);
    return status;
}

OSStatus AudioFileOptimize(AudioFileID f) {
    if (!alive(f))
        return kAudio_ParamError;
    return f->writable ? updateSizesFinal(f, true) : noErr;
}

/* ---- Data ----------------------------------------------------------------- */

OSStatus AudioFileReadBytes(AudioFileID f, Boolean useCache, SInt64 start, UInt32 *ioBytes, void *buffer) {
    UInt32 asked, want, got = 0;
    OSStatus status;

    if (!alive(f) || ioBytes == NULL || buffer == NULL || start < 0)
        return kAudio_ParamError;
    asked = want = *ioBytes;
    if (start >= f->dataBytes) {
        *ioBytes = 0;
        return kAudioFileEndOfFileError;
    }
    if ((SInt64)want > f->dataBytes - start)
        want = (UInt32)(f->dataBytes - start);
    status = readAt(f, f->dataOffset + start, want, buffer, &got);
    *ioBytes = got;
    if (status != noErr)
        return status;
    return got < asked ? kAudioFileEndOfFileError : noErr;
}

OSStatus AudioFileWriteBytes(AudioFileID f, Boolean useCache, SInt64 start, UInt32 *ioBytes, const void *buffer) {
    OSStatus status;

    if (!alive(f) || ioBytes == NULL || buffer == NULL || start < 0)
        return kAudio_ParamError;
    status = writeAt(f, f->dataOffset + start, *ioBytes, buffer);
    if (status != noErr) {
        *ioBytes = 0;
        return status;
    }
    if (start + *ioBytes > f->dataBytes)
        f->dataBytes = start + *ioBytes;
    f->dirty = true;
    return f->deferSizes ? noErr : updateSizes(f);
}

static OSStatus readPackets(AudioFileID f, UInt32 capacity, SInt64 start, UInt32 *ioPackets, void *buffer,
                            UInt32 *outBytes) {
    UInt32 bpp = f->format.mBytesPerPacket, n = *ioPackets, got = 0;
    SInt64 total = f->dataBytes / bpp;
    OSStatus status;

    if (start < 0)
        return kAudioFileInvalidPacketOffsetError;
    if (capacity / bpp < n)
        n = capacity / bpp;
    if (start >= total)
        n = 0;
    else if (n > total - start)
        n = (UInt32)(total - start);
    status = n > 0 ? readAt(f, f->dataOffset + start * bpp, n * bpp, buffer, &got) : noErr;
    *ioPackets = got / bpp;
    if (outBytes != NULL)
        *outBytes = *ioPackets * bpp;
    return status;
}

OSStatus AudioFileReadPacketData(AudioFileID f, Boolean useCache, UInt32 *ioBytes,
                                 AudioStreamPacketDescription *descriptions, SInt64 start, UInt32 *ioPackets,
                                 void *buffer) {
    if (!alive(f) || ioBytes == NULL || ioPackets == NULL || buffer == NULL)
        return kAudio_ParamError;
    return readPackets(f, *ioBytes, start, ioPackets, buffer, ioBytes);
}

OSStatus AudioFileReadPackets(AudioFileID f, Boolean useCache, UInt32 *outBytes,
                              AudioStreamPacketDescription *descriptions, SInt64 start, UInt32 *ioPackets,
                              void *buffer) {
    if (!alive(f) || ioPackets == NULL || buffer == NULL)
        return kAudio_ParamError;
    return readPackets(f, UINT32_MAX, start, ioPackets, buffer, outBytes);
}

OSStatus AudioFileWritePackets(AudioFileID f, Boolean useCache, UInt32 bytes,
                               const AudioStreamPacketDescription *descriptions, SInt64 start, UInt32 *ioPackets,
                               const void *buffer) {
    UInt32 bpp, n, size;
    OSStatus status;

    if (!alive(f) || ioPackets == NULL || buffer == NULL || start < 0)
        return kAudio_ParamError;
    bpp = f->format.mBytesPerPacket;
    n = *ioPackets;
    if (bytes / bpp < n)
        n = bytes / bpp;
    size = n * bpp;
    status = AudioFileWriteBytes(f, useCache, start * bpp, &size, buffer);
    *ioPackets = size / bpp;
    return status;
}

/* The index'th chunk of a type, or NULL. */
static struct UserChunk *userChunk(AudioFileID f, UInt32 id, UInt32 index) {
    for (UInt32 i = 0; i < f->chunkCount; i++)
        if (f->chunks[i].id == id && index-- == 0)
            return &f->chunks[i];
    return NULL;
}

OSStatus AudioFileCountUserData(AudioFileID f, UInt32 id, UInt32 *outCount) {
    UInt32 n = 0;

    if (!alive(f) || outCount == NULL)
        return kAudio_ParamError;
    for (UInt32 i = 0; i < f->chunkCount; i++)
        n += f->chunks[i].id == id;
    *outCount = n;
    return noErr;
}

OSStatus AudioFileGetUserDataSize(AudioFileID f, UInt32 id, UInt32 index, UInt32 *outSize) {
    struct UserChunk *c;

    if (!alive(f) || outSize == NULL)
        return kAudio_ParamError;
    c = userChunk(f, id, index);
    if (c == NULL)
        return kAudioFileInvalidChunkError;
    *outSize = c->size;
    return noErr;
}

OSStatus AudioFileGetUserData(AudioFileID f, UInt32 id, UInt32 index, UInt32 *ioSize, void *data) {
    struct UserChunk *c;
    UInt32 n;

    if (!alive(f) || ioSize == NULL || data == NULL)
        return kAudio_ParamError;
    c = userChunk(f, id, index);
    if (c == NULL)
        return kAudioFileInvalidChunkError;
    n = *ioSize < c->size ? *ioSize : c->size;
    if (c->data != NULL)
        memcpy(data, c->data, n);
    else if (!readExact(f, c->offset, n, data))
        return kAudioFileInvalidFileError;
    *ioSize = n;
    return noErr;
}

/* Sets the index'th chunk of a type (or adds one, at the next index); it
   is written after the audio data when the file is closed. */
OSStatus AudioFileSetUserData(AudioFileID f, UInt32 id, UInt32 index, UInt32 size, const void *data) {
    struct UserChunk *c;
    UInt32 count = 0;
    void *copy;

    if (!alive(f) || (data == NULL && size > 0))
        return kAudio_ParamError;
    if (!f->writable)
        return kAudioFilePermissionsError;
    if (structural(f->type, id))
        return kAudioFileInvalidChunkError;
    AudioFileCountUserData(f, id, &count);
    if (index > count)
        return kAudioFileInvalidChunkError;
    copy = malloc(size > 0 ? size : 1);
    if (copy == NULL)
        return kAudio_MemFullError;
    memcpy(copy, data, size);
    c = userChunk(f, id, index);
    if (c == NULL) {
        noteChunk(f, id, -1, 0, true);
        c = &f->chunks[f->chunkCount - 1];
    } else {
        free(c->data);
    }
    c->data = copy;
    c->size = size;
    f->userDirty = true;
    return noErr;
}

OSStatus AudioFileRemoveUserData(AudioFileID f, UInt32 id, UInt32 index) {
    struct UserChunk *c;

    if (!alive(f))
        return kAudio_ParamError;
    if (!f->writable)
        return kAudioFilePermissionsError;
    c = userChunk(f, id, index);
    if (c == NULL)
        return kAudioFileInvalidChunkError;
    if (c->offset >= 0 && !c->afterData) {
        /* Where it was, before the data, it becomes padding. */
        unsigned char tag[4];
        UInt32 pad = f->type == kAudioFileCAFType ? 'free' : 'JUNK';
        SInt64 at = c->offset - (f->type == kAudioFileCAFType ? 12 : 8);
        tag[0] = pad >> 24; tag[1] = (pad >> 16) & 0xff; tag[2] = (pad >> 8) & 0xff; tag[3] = pad & 0xff;
        writeAt(f, at, 4, tag);
    }
    free(c->data);
    memmove(c, c + 1, (f->chunkCount - (UInt32)(c - f->chunks) - 1) * sizeof(*c));
    f->chunkCount--;
    f->userDirty = true;
    return noErr;
}

/* After the audio data, the chunks that go there: those the app set and
   those that were there (read first, as the data may have grown over
   them). The file ends after them, and the container's size says so. */
static OSStatus writeUserChunks(AudioFileID f) {
    SInt64 end = f->dataOffset + f->dataBytes + (f->type != kAudioFileCAFType ? (f->dataBytes & 1) : 0);
    Boolean caf = f->type == kAudioFileCAFType, big = f->type != kAudioFileWAVEType;
    unsigned char h[12];
    SInt64 at = end;

    for (UInt32 i = 0; i < f->chunkCount; i++) {
        struct UserChunk *c = &f->chunks[i];
        if (c->data == NULL && c->offset >= 0 && c->afterData) {
            c->data = malloc(c->size > 0 ? c->size : 1);
            if (c->data == NULL || !readExact(f, c->offset, c->size, c->data))
                return kAudioFileInvalidFileError;
        }
    }
    for (UInt32 i = 0; i < f->chunkCount; i++) {
        struct UserChunk *c = &f->chunks[i];
        UInt32 n;
        if (c->data == NULL)
            continue;   /* before the data and unchanged: it stays where it is */
        if (c->offset >= 0 && !c->afterData) {
            /* A changed chunk from before the data: padding there now. */
            UInt32 pad = caf ? 'free' : 'JUNK';
            unsigned char tag[4] = { pad >> 24, (pad >> 16) & 0xff, (pad >> 8) & 0xff, pad & 0xff };
            writeAt(f, c->offset - (caf ? 12 : 8), 4, tag);
        }
        putBE32(h, c->id);
        if (caf) {
            putBE64(h + 4, c->size);
            n = 12;
        } else {
            if (big)
                putBE32(h + 4, c->size);
            else
                putLE32(h + 4, c->size);
            n = 8;
        }
        if (writeAt(f, at, n, h) != noErr || (c->size > 0 && writeAt(f, at + n, c->size, c->data) != noErr))
            return kAudioFilePositionError;
        c->offset = at + n;
        c->afterData = true;
        at += n + c->size;
        if (!caf && (c->size & 1)) {
            unsigned char zero = 0;
            writeAt(f, at, 1, &zero);
            at++;
        }
    }
    if (f->fd >= 0)
        ftruncate(f->fd, at);
    else if (f->setSizeProc != NULL)
        f->setSizeProc(f->client, at);
    if (!caf) {
        /* RIFF and FORM give the whole file's size, less their header. */
        if (big)
            putBE32(h, (UInt32)(at - 8));
        else
            putLE32(h, (UInt32)(at - 8));
        writeAt(f, 4, 4, h);
    }
    return noErr;
}

/* ---- Properties ------------------------------------------------------------- */

static UInt32 propertySize(AudioFileID f, AudioFilePropertyID id, UInt32 *writable) {
    *writable = 0;
    switch (id) {
    case kAudioFilePropertyFileFormat:
    case kAudioFilePropertyIsOptimized:
    case kAudioFilePropertyMaximumPacketSize:
    case kAudioFilePropertyPacketSizeUpperBound:
    case kAudioFilePropertyBitRate:
    case kAudioFilePropertySourceBitDepth:
    case kAudioFilePropertyAudioTrackCount:
        return sizeof(UInt32);
    case kAudioFilePropertyDeferSizeUpdates:
        *writable = 1;
        return sizeof(UInt32);
    case kAudioFilePropertyDataFormat:
        *writable = f->writable && f->dataBytes == 0;
        return sizeof(AudioStreamBasicDescription);
    case kAudioFilePropertyAudioDataByteCount:
    case kAudioFilePropertyAudioDataPacketCount:
        return sizeof(UInt64);
    case kAudioFilePropertyDataOffset:
        return sizeof(SInt64);
    case kAudioFilePropertyEstimatedDuration:
        return sizeof(Float64);
    case kAudioFilePropertyPacketTableInfo:
        return sizeof(AudioFilePacketTableInfo);
    case kAudioFilePropertyPacketToFrame:
    case kAudioFilePropertyFrameToPacket:
        return sizeof(AudioFramePacketTranslation);
    case kAudioFilePropertyPacketToByte:
    case kAudioFilePropertyByteToPacket:
        return sizeof(AudioBytePacketTranslation);
    case kAudioFilePropertyInfoDictionary:
        return sizeof(CFDictionaryRef);
    case kAudioFilePropertyMagicCookieData:
        *writable = f->writable;
        return f->cookieSize;
    case kAudioFilePropertyReserveDuration:
        *writable = 1;
        return sizeof(Float64);
    default:
        return 0;
    }
}

OSStatus AudioFileGetPropertyInfo(AudioFileID f, AudioFilePropertyID id, UInt32 *outSize, UInt32 *outWritable) {
    UInt32 writable, size;

    if (!alive(f))
        return kAudio_ParamError;
    size = propertySize(f, id, &writable);
    if (size == 0)
        return kAudioFileUnsupportedPropertyError;
    if (outSize != NULL)
        *outSize = size;
    if (outWritable != NULL)
        *outWritable = writable;
    return noErr;
}

OSStatus AudioFileGetProperty(AudioFileID f, AudioFilePropertyID id, UInt32 *ioSize, void *data) {
    UInt32 writable, size, bpp;
    SInt64 packets;

    if (!alive(f) || ioSize == NULL || data == NULL)
        return kAudio_ParamError;
    size = propertySize(f, id, &writable);
    if (size == 0)
        return kAudioFileUnsupportedPropertyError;
    if (*ioSize < size)
        return kAudioFileBadPropertySizeError;
    bpp = f->format.mBytesPerPacket;
    packets = f->dataBytes / bpp;
    switch (id) {
    case kAudioFilePropertyFileFormat:
        *(UInt32 *)data = f->type;
        break;
    case kAudioFilePropertyIsOptimized:
        *(UInt32 *)data = 1;
        break;
    case kAudioFilePropertyMaximumPacketSize:
    case kAudioFilePropertyPacketSizeUpperBound:
        *(UInt32 *)data = bpp;
        break;
    case kAudioFilePropertyBitRate:
        *(UInt32 *)data = (UInt32)(f->format.mSampleRate * bpp * 8);
        break;
    case kAudioFilePropertySourceBitDepth:
        *(UInt32 *)data = f->sourceBits != 0 ? f->sourceBits : f->format.mBitsPerChannel;
        break;
    case kAudioFilePropertyAudioTrackCount:
        *(UInt32 *)data = 1;
        break;
    case kAudioFilePropertyDeferSizeUpdates:
        *(UInt32 *)data = f->deferSizes;
        break;
    case kAudioFilePropertyDataFormat:
        *(AudioStreamBasicDescription *)data = f->format;
        break;
    case kAudioFilePropertyAudioDataByteCount:
        *(UInt64 *)data = (UInt64)f->dataBytes;
        break;
    case kAudioFilePropertyAudioDataPacketCount:
        *(UInt64 *)data = (UInt64)packets;
        break;
    case kAudioFilePropertyDataOffset:
        *(SInt64 *)data = f->dataOffset;
        break;
    case kAudioFilePropertyEstimatedDuration:
        *(Float64 *)data = packets / f->format.mSampleRate;
        break;
    case kAudioFilePropertyPacketTableInfo: {
        AudioFilePacketTableInfo info = { packets, 0, 0 };
        *(AudioFilePacketTableInfo *)data = info;
        break;
    }
    case kAudioFilePropertyPacketToFrame: {
        AudioFramePacketTranslation *t = data;
        t->mFrame = t->mPacket;
        t->mFrameOffsetInPacket = 0;
        break;
    }
    case kAudioFilePropertyFrameToPacket: {
        AudioFramePacketTranslation *t = data;
        t->mPacket = t->mFrame;
        t->mFrameOffsetInPacket = 0;
        break;
    }
    case kAudioFilePropertyPacketToByte: {
        AudioBytePacketTranslation *t = data;
        t->mByte = t->mPacket * bpp;
        t->mFlags = 0;
        break;
    }
    case kAudioFilePropertyByteToPacket: {
        AudioBytePacketTranslation *t = data;
        t->mPacket = t->mByte / bpp;
        t->mByteOffsetInPacket = (UInt32)(t->mByte % bpp);
        t->mFlags = 0;
        break;
    }
    case kAudioFilePropertyInfoDictionary: {
        CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
        CFStringRef duration = CFStringCreateWithFormat(NULL, NULL, CFSTR("%.3f"), packets / f->format.mSampleRate);
        CFDictionarySetValue(d, CFSTR("approximate duration in seconds"), duration);
        CFRelease(duration);
        *(CFDictionaryRef *)data = d;
        break;
    }
    case kAudioFilePropertyMagicCookieData:
        memcpy(data, f->cookie, f->cookieSize);
        break;
    case kAudioFilePropertyReserveDuration:
        *(Float64 *)data = 0;
        break;
    }
    *ioSize = size;
    return noErr;
}

OSStatus AudioFileSetProperty(AudioFileID f, AudioFilePropertyID id, UInt32 size, const void *data) {
    if (!alive(f) || data == NULL)
        return kAudio_ParamError;
    switch (id) {
    case kAudioFilePropertyDeferSizeUpdates:
        if (size < sizeof(UInt32))
            return kAudioFileBadPropertySizeError;
        f->deferSizes = *(const UInt32 *)data != 0;
        return f->deferSizes ? noErr : updateSizes(f);
    case kAudioFilePropertyDataFormat:
        if (size < sizeof(AudioStreamBasicDescription))
            return kAudioFileBadPropertySizeError;
        if (!f->writable || f->dataBytes != 0)
            return kAudioFileOperationNotSupportedError;
        if (!writableFormat(f->type, data))
            return kAudioFileUnsupportedDataFormatError;
        f->format = *(const AudioStreamBasicDescription *)data;
        return writeHeader(f);
    case kAudioFilePropertyMagicCookieData:
        if (!f->writable)
            return kAudioFilePermissionsError;
        return noErr;   /* linear PCM has none to keep */
    case kAudioFilePropertyReserveDuration:
        return noErr;
    default:
        return kAudioFileUnsupportedPropertyError;
    }
}

/* ---- What the files are ------------------------------------------------------ */

typedef struct {
    AudioFileTypeID type;
    const char *name, *extensions[3], *mime[3], *uti;
} TypeInfo;

static const TypeInfo types[] = {
    { kAudioFileWAVEType, "WAVE", { "wav", "wave", NULL }, { "audio/wav", "audio/x-wav", "audio/vnd.wave" },
      "com.microsoft.waveform-audio" },
    { kAudioFileAIFFType, "AIFF", { "aiff", "aif", NULL }, { "audio/aiff", "audio/x-aiff", NULL }, "public.aiff-audio" },
    { kAudioFileAIFCType, "AIFC", { "aifc", "aiff", "aif" }, { "audio/aiff", "audio/x-aiff", NULL },
      "public.aifc-audio" },
    { kAudioFileCAFType, "CAF", { "caf", NULL, NULL }, { "audio/x-caf", NULL, NULL }, "com.apple.coreaudio-format" },
    { kAudioFileRF64Type, "RF64", { "wav", NULL, NULL }, { "audio/wav", NULL, NULL }, "com.microsoft.waveform-audio" },
};
#define kTypeCount (sizeof(types) / sizeof(types[0]))

static const TypeInfo *typeInfo(AudioFileTypeID type) {
    for (size_t i = 0; i < kTypeCount; i++)
        if (types[i].type == type)
            return &types[i];
    return NULL;
}

static CFStringRef cfString(const char *s) {
    return CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8);
}

static CFArrayRef stringArray(const char *const *list, size_t count) {
    CFMutableArrayRef a = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (size_t i = 0; i < count; i++)
        if (list[i] != NULL) {
            CFStringRef s = cfString(list[i]);
            CFIndex n = CFArrayGetCount(a);
            if (!CFArrayContainsValue(a, CFRangeMake(0, n), s))
                CFArrayAppendValue(a, s);
            CFRelease(s);
        }
    return a;
}

/* Fills a UInt32 list of the types whose field matches the string. */
static UInt32 typesMatching(CFStringRef s, int field, UInt32 *out) {
    char buf[128];
    UInt32 n = 0;

    if (s == NULL || !CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8))
        return 0;
    for (size_t i = 0; i < kTypeCount - 1; i++) {   /* RF64 is found by its contents */
        Boolean hit = false;
        for (int k = 0; k < 3; k++) {
            const char *v = field == 0 ? types[i].extensions[k] : (field == 1 ? types[i].mime[k] : (k == 0 ? types[i].uti : NULL));
            if (v != NULL && strcasecmp(v, buf) == 0)
                hit = true;
        }
        if (hit)
            out[n++] = types[i].type;
    }
    return n;
}

static OSStatus globalInfo(AudioFilePropertyID id, UInt32 specSize, const void *spec, UInt32 *ioSize, void *out,
                           Boolean sizeOnly) {
    UInt32 list[16], n = 0;
    const TypeInfo *t;

    switch (id) {
    case kAudioFileGlobalInfo_ReadableTypes:
        for (size_t i = 0; i < kTypeCount; i++)
            list[n++] = types[i].type;
        goto uint32List;
    case kAudioFileGlobalInfo_WritableTypes:
        list[n++] = kAudioFileWAVEType;
        list[n++] = kAudioFileAIFFType;
        list[n++] = kAudioFileAIFCType;
        list[n++] = kAudioFileCAFType;
        goto uint32List;
    case kAudioFileGlobalInfo_AvailableFormatIDs:
        if (specSize < sizeof(UInt32) || typeInfo(*(const UInt32 *)spec) == NULL)
            return kAudioFileUnsupportedFileTypeError;
        list[n++] = kAudioFormatLinearPCM;
        goto uint32List;
    case kAudioFileGlobalInfo_TypesForExtension:
        n = typesMatching(specSize >= sizeof(CFStringRef) ? *(const CFStringRef *)spec : NULL, 0, list);
        goto uint32List;
    case kAudioFileGlobalInfo_TypesForMIMEType:
        n = typesMatching(specSize >= sizeof(CFStringRef) ? *(const CFStringRef *)spec : NULL, 1, list);
        goto uint32List;
    case kAudioFileGlobalInfo_TypesForUTI:
        n = typesMatching(specSize >= sizeof(CFStringRef) ? *(const CFStringRef *)spec : NULL, 2, list);
        goto uint32List;
    case kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat: {
        const AudioFileTypeAndFormatID *tf = spec;
        static const UInt32 bits[] = { 8, 16, 24, 32 };
        AudioStreamBasicDescription d[8];
        UInt32 count = 0;
        if (specSize < sizeof(*tf) || typeInfo(tf->mFileType) == NULL)
            return kAudioFileUnsupportedFileTypeError;
        if (tf->mFormatID != kAudioFormatLinearPCM)
            return kAudioFileUnsupportedDataFormatError;
        for (int i = 0; i < 4; i++) {
            UInt32 flags = bits[i] == 8 && tf->mFileType == kAudioFileWAVEType ? 0 : kAudioFormatFlagIsSignedInteger;
            if (tf->mFileType == kAudioFileAIFFType || tf->mFileType == kAudioFileAIFCType ||
                tf->mFileType == kAudioFileCAFType)
                flags |= kAudioFormatFlagIsBigEndian;
            memset(&d[count], 0, sizeof(d[count]));
            d[count].mFormatID = kAudioFormatLinearPCM;
            d[count].mFormatFlags = flags | kAudioFormatFlagIsPacked;
            d[count].mBitsPerChannel = bits[i];
            d[count].mFramesPerPacket = 1;
            count++;
        }
        if (tf->mFileType != kAudioFileAIFFType) {
            memset(&d[count], 0, sizeof(d[count]));
            d[count].mFormatID = kAudioFormatLinearPCM;
            d[count].mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked |
                                    (tf->mFileType == kAudioFileWAVEType ? 0 : kAudioFormatFlagIsBigEndian);
            d[count].mBitsPerChannel = 32;
            d[count].mFramesPerPacket = 1;
            count++;
        }
        if (!sizeOnly) {
            if (*ioSize < count * sizeof(d[0]))
                count = *ioSize / sizeof(d[0]);
            memcpy(out, d, count * sizeof(d[0]));
        }
        *ioSize = count * sizeof(d[0]);
        return noErr;
    }
    case kAudioFileGlobalInfo_FileTypeName:
    case kAudioFileGlobalInfo_ExtensionsForType:
    case kAudioFileGlobalInfo_MIMETypesForType:
    case kAudioFileGlobalInfo_UTIsForType: {
        CFTypeRef v;
        if (specSize < sizeof(UInt32) || (t = typeInfo(*(const UInt32 *)spec)) == NULL)
            return kAudioFileUnsupportedFileTypeError;
        if (!sizeOnly) {
            if (*ioSize < sizeof(CFTypeRef))
                return kAudioFileBadPropertySizeError;
            if (id == kAudioFileGlobalInfo_FileTypeName)
                v = cfString(t->name);
            else if (id == kAudioFileGlobalInfo_ExtensionsForType)
                v = stringArray(t->extensions, 3);
            else if (id == kAudioFileGlobalInfo_MIMETypesForType)
                v = stringArray(t->mime, 3);
            else
                v = stringArray(&t->uti, 1);
            *(CFTypeRef *)out = v;
        }
        *ioSize = sizeof(CFTypeRef);
        return noErr;
    }
    case kAudioFileGlobalInfo_AllExtensions:
    case kAudioFileGlobalInfo_AllMIMETypes:
    case kAudioFileGlobalInfo_AllUTIs: {
        const char *all[kTypeCount * 3];
        size_t count = 0;
        if (!sizeOnly) {
            if (*ioSize < sizeof(CFArrayRef))
                return kAudioFileBadPropertySizeError;
            for (size_t i = 0; i < kTypeCount; i++)
                for (int k = 0; k < 3; k++) {
                    const char *v = id == kAudioFileGlobalInfo_AllExtensions ? types[i].extensions[k]
                                    : id == kAudioFileGlobalInfo_AllMIMETypes ? types[i].mime[k]
                                    : (k == 0 ? types[i].uti : NULL);
                    if (v != NULL)
                        all[count++] = v;
                }
            *(CFArrayRef *)out = stringArray(all, count);
        }
        *ioSize = sizeof(CFArrayRef);
        return noErr;
    }
    default:
        return kAudioFileUnsupportedPropertyError;
    }

uint32List:
    if (!sizeOnly) {
        if (*ioSize < n * sizeof(UInt32))
            n = *ioSize / sizeof(UInt32);
        memcpy(out, list, n * sizeof(UInt32));
    }
    *ioSize = n * sizeof(UInt32);
    return noErr;
}

OSStatus AudioFileGetGlobalInfoSize(AudioFilePropertyID id, UInt32 specSize, void *spec, UInt32 *outSize) {
    UInt32 size = 0;
    OSStatus status;

    if (outSize == NULL)
        return kAudio_ParamError;
    status = globalInfo(id, specSize, spec, &size, NULL, true);
    if (status == noErr)
        *outSize = size;
    return status;
}

OSStatus AudioFileGetGlobalInfo(AudioFilePropertyID id, UInt32 specSize, void *spec, UInt32 *ioSize, void *out) {
    if (ioSize == NULL || out == NULL)
        return kAudio_ParamError;
    return globalInfo(id, specSize, spec, ioSize, out, false);
}
