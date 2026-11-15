/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * mediadecoder (AU5): one file's demuxer and decoders, FFmpeg's, in a
 * process of their own, for AVFoundation (Frameworks/AVFoundation/
 * MediaDecoder). The app opens the file and hands it over; the decoder
 * gives back its tracks' samples: video frames in IOSurfaces, converted
 * to the pixel format asked for, and audio as interleaved samples at the
 * rate and channels asked for. It ends when the app lets go of it.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CVPixelBuffer.h>
#include <IOSurface/IOSurfaceRef.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/display.h>
#include <libavutil/log.h>
#include <libavutil/pixdesc.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <mach/mach.h>
#include <mach/notify.h>
#include <servers/bootstrap.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#include "MediaDecoderTypes.h"
#include "MediaDecoderServer.h"

extern int fileport_makefd(mach_port_t port);

#define MAX_TRACKS	16
#define MAX_SURFACES	32
#define IO_BUFFER	65536

typedef struct {
	AVPacket **items;
	int count, capacity;
} PacketQueue;

typedef struct {
	int stream;			/* its index in the file */
	AVCodecContext *ctx;
	bool enabled, flushed, ended;
	PacketQueue queue;
	int64_t skipBefore;		/* after a seek, in the stream's time base */

	/* Video. */
	OSType pixelFormat;
	int width, height;
	struct SwsContext *sws;
	CVPixelBufferRef surfaces[MAX_SURFACES];
	int surfaceCount;

	/* Audio. */
	SwrContext *swr;
	int sampleRate, channels;
	bool isFloat, drained;		/* drained: the resampler's last samples given */
	int64_t nextOutputFrame;	/* in the output rate: when no time is known */
} Track;

static AVFormatContext *fmt;
static int fileDescriptor = -1;

/* A download still under way (open_stream): its pipe, readable or hung up
   once the app has closed its end, and the length it will have (-1 when
   not known). */
static int doneDescriptor = -1;
static bool streaming;
static int64_t expectedLength = -1;
static bool stalled;

#define STREAM_WAIT_MS	5
#define STREAM_STALL_MS	60000

static bool downloadOver(void)
{
	struct pollfd p = { doneDescriptor, POLLIN, 0 };
	return poll(&p, 1, 0) > 0;
}
static Track tracks[MAX_TRACKS];
static int trackCount;
static bool demuxEnded;
static AVFrame *frame;

/* ---- The file, through the descriptor --------------------------------- */

/* Reads; at the file's end while it is still being downloaded, waits for
   more (the app's download goes on in the file), until it is over. */
static int readFile(void *opaque, uint8_t *buf, int size)
{
	int waited = 0;
	for (;;) {
		ssize_t n = read(fileDescriptor, buf, size);
		if (n < 0)
			return AVERROR(errno);
		if (n > 0)
			return (int)n;
		if (!streaming)
			return AVERROR_EOF;
		off_t at = lseek(fileDescriptor, 0, SEEK_CUR);
		if (expectedLength >= 0 && at >= expectedLength)
			return AVERROR_EOF;
		if (downloadOver()) {
			/* What came last, then the end. */
			streaming = false;
			continue;
		}
		if (waited >= STREAM_STALL_MS) {
			stalled = true;
			return AVERROR(ETIMEDOUT);
		}
		poll(NULL, 0, STREAM_WAIT_MS);
		waited += STREAM_WAIT_MS;
	}
}

static int64_t seekFile(void *opaque, int64_t offset, int whence)
{
	if (whence & AVSEEK_SIZE) {
		struct stat st;
		if (streaming)
			return expectedLength >= 0 ? expectedLength : AVERROR(ENOSYS);
		return fstat(fileDescriptor, &st) == 0 ? st.st_size : AVERROR(errno);
	}
	whence &= ~AVSEEK_FORCE;
	/* Still downloading, the file's end is not where it is now: from the
	   end only with the length expected. */
	if (whence == SEEK_END && streaming) {
		if (expectedLength < 0)
			return AVERROR(ENOSYS);
		offset += expectedLength;
		whence = SEEK_SET;
	}
	off_t r = lseek(fileDescriptor, offset, whence);
	return r < 0 ? AVERROR(errno) : r;
}

/* ---- Property lists ----------------------------------------------------- */

static void setString(CFMutableDictionaryRef d, const char *key, const char *value)
{
	if (value == NULL)
		return;
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	CFStringRef v = CFStringCreateWithCString(NULL, value, kCFStringEncodingUTF8);
	if (v != NULL)
		CFDictionarySetValue(d, k, v);
	CFRelease(k);
	if (v != NULL)
		CFRelease(v);
}

static void setDouble(CFMutableDictionaryRef d, const char *key, double value)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	CFNumberRef v = CFNumberCreate(NULL, kCFNumberDoubleType, &value);
	CFDictionarySetValue(d, k, v);
	CFRelease(k);
	CFRelease(v);
}

static void setInt(CFMutableDictionaryRef d, const char *key, long long value)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	CFNumberRef v = CFNumberCreate(NULL, kCFNumberLongLongType, &value);
	CFDictionarySetValue(d, k, v);
	CFRelease(k);
	CFRelease(v);
}

static void setValue(CFMutableDictionaryRef d, const char *key, CFTypeRef value)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	CFDictionarySetValue(d, k, value);
	CFRelease(k);
}

static CFMutableDictionaryRef newDictionary(void)
{
	return CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static CFDictionaryRef tags(AVDictionary *m)
{
	CFMutableDictionaryRef d = newDictionary();
	const AVDictionaryEntry *e = NULL;
	while ((e = av_dict_iterate(m, e)) != NULL)
		setString(d, e->key, e->value);
	return d;
}

static double number(CFDictionaryRef d, const char *key, double otherwise)
{
	if (d == NULL)
		return otherwise;
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	CFTypeRef v = CFDictionaryGetValue(d, k);
	double x = otherwise;
	CFRelease(k);
	if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
		CFNumberGetValue(v, kCFNumberDoubleType, &x);
	else if (v != NULL && CFGetTypeID(v) == CFBooleanGetTypeID())
		x = CFBooleanGetValue(v);
	return x;
}

static CFPropertyListRef readPlist(const char *bytes, mach_msg_type_number_t length)
{
	CFDataRef data = CFDataCreate(NULL, (const UInt8 *)bytes, length);
	CFPropertyListRef p = CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable, NULL, NULL);
	CFRelease(data);
	return p;
}

/* Bytes the reply carries away (and MIG deallocates after sending). */
static kern_return_t outOfLine(const void *bytes, size_t length, md_data_t *out, mach_msg_type_number_t *outCount)
{
	vm_address_t a = 0;
	*out = NULL;
	*outCount = 0;
	if (length == 0)
		return KERN_SUCCESS;
	if (vm_allocate(mach_task_self(), &a, length, VM_FLAGS_ANYWHERE) != KERN_SUCCESS)
		return KERN_RESOURCE_SHORTAGE;
	memcpy((void *)a, bytes, length);
	*out = (md_data_t)a;
	*outCount = (mach_msg_type_number_t)length;
	return KERN_SUCCESS;
}

/* ---- Opening ------------------------------------------------------------ */

static double rotationOf(AVStream *st)
{
	const AVPacketSideData *sd = av_packet_side_data_get(st->codecpar->coded_side_data,
	    st->codecpar->nb_coded_side_data, AV_PKT_DATA_DISPLAYMATRIX);
	if (sd == NULL || sd->size < 9 * 4)
		return 0;
	double r = -av_display_rotation_get((const int32_t *)sd->data);
	return r != r ? 0 : r;		/* not a number: none */
}

static CFDictionaryRef describeTrack(int id, AVStream *st)
{
	AVCodecParameters *par = st->codecpar;
	CFMutableDictionaryRef t = newDictionary();
	setInt(t, kMDKeyTrackID, id);
	setString(t, kMDKeyMediaType, par->codec_type == AVMEDIA_TYPE_VIDEO ? "vide" : "soun");
	setString(t, kMDKeyCodec, avcodec_get_name(par->codec_id));
	setInt(t, kMDKeyTimescale, st->time_base.den / (st->time_base.num > 0 ? st->time_base.num : 1));
	if (st->duration != AV_NOPTS_VALUE)
		setDouble(t, kMDKeyTrackDuration, st->duration * av_q2d(st->time_base));
	else if (fmt->duration != AV_NOPTS_VALUE)
		setDouble(t, kMDKeyTrackDuration, fmt->duration / (double)AV_TIME_BASE);
	if (par->bit_rate > 0)
		setInt(t, kMDKeyBitRate, par->bit_rate);
	AVDictionaryEntry *lang = av_dict_get(st->metadata, "language", NULL, 0);
	if (lang != NULL && strcmp(lang->value, "und") != 0)
		setString(t, kMDKeyLanguage, lang->value);
	CFDictionaryRef m = tags(st->metadata);
	setValue(t, kMDKeyMetadata, m);
	CFRelease(m);
	if (par->codec_type == AVMEDIA_TYPE_VIDEO) {
		setInt(t, kMDKeyWidth, par->width);
		setInt(t, kMDKeyHeight, par->height);
		AVRational rate = st->avg_frame_rate.num > 0 ? st->avg_frame_rate : st->r_frame_rate;
		if (rate.num > 0 && rate.den > 0)
			setDouble(t, kMDKeyFrameRate, av_q2d(rate));
		if (par->color_primaries != AVCOL_PRI_UNSPECIFIED)
			setString(t, kMDKeyColorPrimaries, av_color_primaries_name(par->color_primaries));
		if (par->color_trc != AVCOL_TRC_UNSPECIFIED)
			setString(t, kMDKeyTransfer, av_color_transfer_name(par->color_trc));
		if (par->color_space != AVCOL_SPC_UNSPECIFIED)
			setString(t, kMDKeyMatrix, av_color_space_name(par->color_space));
		setValue(t, kMDKeyFullRange, par->color_range == AVCOL_RANGE_JPEG ? kCFBooleanTrue : kCFBooleanFalse);
		double rot = rotationOf(st);
		if (rot != 0)
			setDouble(t, kMDKeyRotation, rot);
		AVRational sar = av_guess_sample_aspect_ratio(fmt, st, NULL);
		if (sar.num > 0 && sar.den > 0 && sar.num != sar.den) {
			int pair[2] = { sar.num, sar.den };
			CFNumberRef n[2] = { CFNumberCreate(NULL, kCFNumberIntType, &pair[0]),
			    CFNumberCreate(NULL, kCFNumberIntType, &pair[1]) };
			CFArrayRef a = CFArrayCreate(NULL, (const void **)n, 2, &kCFTypeArrayCallBacks);
			setValue(t, kMDKeySampleAspect, a);
			CFRelease(a);
			CFRelease(n[0]);
			CFRelease(n[1]);
		}
	} else {
		setInt(t, kMDKeySampleRate, par->sample_rate);
		setInt(t, kMDKeyChannels, par->ch_layout.nb_channels);
	}
	return t;
}

kern_return_t mds_open_stream(mach_port_t server, mach_port_t file, mach_port_t done, md_bytes_t name,
    mach_msg_type_number_t nameCount, int64_t expected, int32_t *result, md_data_t *info,
    mach_msg_type_number_t *infoCount)
{
	if (fmt == NULL && doneDescriptor < 0) {
		doneDescriptor = fileport_makefd(done);
		streaming = doneDescriptor >= 0;
		expectedLength = expected >= 0 ? expected : -1;
	}
	mach_port_deallocate(mach_task_self(), done);
	return mds_open(server, file, name, nameCount, result, info, infoCount);
}

kern_return_t mds_open(mach_port_t server, mach_port_t file, md_bytes_t name, mach_msg_type_number_t nameCount,
    int32_t *result, md_data_t *info, mach_msg_type_number_t *infoCount)
{
	char path[MAXPATHLEN];
	*info = NULL;
	*infoCount = 0;
	if (fmt != NULL) {
		mach_port_deallocate(mach_task_self(), file);
		*result = kMDErrorBadOptions;
		return KERN_SUCCESS;
	}
	fileDescriptor = fileport_makefd(file);
	mach_port_deallocate(mach_task_self(), file);
	snprintf(path, sizeof(path), "%.*s", (int)MIN(nameCount, sizeof(path) - 1), name);
	fmt = avformat_alloc_context();
	unsigned char *buf = av_malloc(IO_BUFFER);
	if (fileDescriptor < 0 || fmt == NULL || buf == NULL ||
	    (fmt->pb = avio_alloc_context(buf, IO_BUFFER, 0, NULL, readFile, NULL, seekFile)) == NULL ||
	    avformat_open_input(&fmt, path, NULL, NULL) < 0 || avformat_find_stream_info(fmt, NULL) < 0) {
		/* avformat_open_input frees the context when it fails. */
		fmt = NULL;
		*result = stalled ? kMDErrorStalled : kMDErrorCannotOpen;
		return KERN_SUCCESS;
	}

	CFMutableDictionaryRef d = newDictionary();
	CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	setString(d, kMDKeyFormat, fmt->iformat->name);
	if (fmt->duration != AV_NOPTS_VALUE)
		setDouble(d, kMDKeyDuration, fmt->duration / (double)AV_TIME_BASE);
	CFDictionaryRef m = tags(fmt->metadata);
	setValue(d, kMDKeyMetadata, m);
	CFRelease(m);
	for (unsigned i = 0; i < fmt->nb_streams && trackCount < MAX_TRACKS; i++) {
		AVStream *st = fmt->streams[i];
		enum AVMediaType type = st->codecpar->codec_type;
		if ((type != AVMEDIA_TYPE_VIDEO && type != AVMEDIA_TYPE_AUDIO) ||
		    (st->disposition & AV_DISPOSITION_ATTACHED_PIC))
			continue;
		tracks[trackCount].stream = i;
		trackCount++;
		CFDictionaryRef t = describeTrack(trackCount, st);
		CFArrayAppendValue(list, t);
		CFRelease(t);
	}
	setValue(d, kMDKeyTracks, list);
	CFRelease(list);
	CFDataRef data = CFPropertyListCreateData(NULL, d, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
	CFRelease(d);
	kern_return_t kr = outOfLine(CFDataGetBytePtr(data), CFDataGetLength(data), info, infoCount);
	CFRelease(data);
	*result = kMDSuccess;
	return kr;
}

/* ---- Configuring -------------------------------------------------------- */

static Track *trackWithID(int32_t id)
{
	return fmt != NULL && id >= 1 && id <= trackCount ? &tracks[id - 1] : NULL;
}

static void dropSurfaces(Track *t)
{
	for (int i = 0; i < t->surfaceCount; i++)
		CVPixelBufferRelease(t->surfaces[i]);
	t->surfaceCount = 0;
}

kern_return_t mds_configure(mach_port_t server, int32_t id, md_bytes_t options, mach_msg_type_number_t length,
    int32_t *result)
{
	Track *t = trackWithID(id);
	if (t == NULL) {
		*result = fmt == NULL ? kMDErrorNotOpen : kMDErrorNoSuchTrack;
		return KERN_SUCCESS;
	}
	CFPropertyListRef o = length > 0 ? readPlist(options, length) : NULL;
	CFDictionaryRef opts = o != NULL && CFGetTypeID(o) == CFDictionaryGetTypeID() ? o : NULL;
	AVCodecParameters *par = fmt->streams[t->stream]->codecpar;

	if (t->ctx == NULL) {
		const AVCodec *codec = avcodec_find_decoder(par->codec_id);
		t->ctx = codec != NULL ? avcodec_alloc_context3(codec) : NULL;
		if (t->ctx == NULL || avcodec_parameters_to_context(t->ctx, par) < 0) {
			avcodec_free_context(&t->ctx);
			*result = kMDErrorDecode;
			goto done;
		}
		t->ctx->pkt_timebase = fmt->streams[t->stream]->time_base;
		t->ctx->thread_count = 0;	/* as many as the processor has */
		if (avcodec_open2(t->ctx, codec, NULL) < 0) {
			avcodec_free_context(&t->ctx);
			*result = kMDErrorDecode;
			goto done;
		}
	}
	if (par->codec_type == AVMEDIA_TYPE_VIDEO) {
		OSType pf = (OSType)number(opts, kMDKeyOutputPixelFormat, kCVPixelFormatType_32BGRA);
		int w = (int)number(opts, kMDKeyOutputWidth, par->width);
		int h = (int)number(opts, kMDKeyOutputHeight, par->height);
		if ((pf != kCVPixelFormatType_32BGRA && pf != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange &&
		     pf != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) || w <= 0 || h <= 0 || w > 16384 || h > 16384) {
			*result = kMDErrorBadOptions;
			goto done;
		}
		if (pf != t->pixelFormat || w != t->width || h != t->height)
			dropSurfaces(t);
		t->pixelFormat = pf;
		t->width = w;
		t->height = h;
	} else {
		int rate = (int)number(opts, kMDKeyOutputSampleRate, par->sample_rate);
		int ch = (int)number(opts, kMDKeyOutputChannels, par->ch_layout.nb_channels);
		if (rate <= 0 || rate > 768000 || ch <= 0 || ch > 32) {
			*result = kMDErrorBadOptions;
			goto done;
		}
		if (rate != t->sampleRate || ch != t->channels)
			swr_free(&t->swr);
		t->sampleRate = rate;
		t->channels = ch;
		bool f = number(opts, kMDKeyOutputFloat, 1) != 0;
		if (f != t->isFloat)
			swr_free(&t->swr);
		t->isFloat = f;
	}
	t->enabled = true;
	*result = kMDSuccess;
done:
	if (o != NULL)
		CFRelease(o);
	return KERN_SUCCESS;
}

/* ---- Packets ------------------------------------------------------------ */

static void push(PacketQueue *q, AVPacket *p)
{
	if (q->count == q->capacity) {
		q->capacity = q->capacity ? q->capacity * 2 : 64;
		q->items = realloc(q->items, sizeof(*q->items) * q->capacity);
	}
	q->items[q->count++] = p;
}

static AVPacket *pop(PacketQueue *q)
{
	if (q->count == 0)
		return NULL;
	AVPacket *p = q->items[0];
	memmove(q->items, q->items + 1, sizeof(*q->items) * --q->count);
	return p;
}

static void clear(PacketQueue *q)
{
	AVPacket *p;
	while ((p = pop(q)) != NULL)
		av_packet_free(&p);
}

/* One packet from the file, to its track's queue (when it is read). */
static void demuxOne(void)
{
	AVPacket *p = av_packet_alloc();
	if (av_read_frame(fmt, p) < 0) {
		demuxEnded = true;
		av_packet_free(&p);
		return;
	}
	for (int i = 0; i < trackCount; i++)
		if (tracks[i].stream == p->stream_index && tracks[i].enabled) {
			push(&tracks[i].queue, p);
			return;
		}
	av_packet_free(&p);
}

/* ---- Seeking ------------------------------------------------------------ */

kern_return_t mds_seek(mach_port_t server, int64_t value, int32_t scale, int32_t *result)
{
	if (fmt == NULL || scale <= 0) {
		*result = fmt == NULL ? kMDErrorNotOpen : kMDErrorBadOptions;
		return KERN_SUCCESS;
	}
	int64_t target = av_rescale(value, AV_TIME_BASE, scale);
	if (fmt->start_time != AV_NOPTS_VALUE)
		target += fmt->start_time;
	if (avformat_seek_file(fmt, -1, INT64_MIN, target, target, AVSEEK_FLAG_BACKWARD) < 0 &&
	    av_seek_frame(fmt, -1, target, AVSEEK_FLAG_BACKWARD) < 0) {
		*result = kMDErrorDecode;
		return KERN_SUCCESS;
	}
	demuxEnded = false;
	for (int i = 0; i < trackCount; i++) {
		Track *t = &tracks[i];
		clear(&t->queue);
		if (t->ctx != NULL)
			avcodec_flush_buffers(t->ctx);
		if (t->swr != NULL)
			swr_free(&t->swr);
		t->flushed = t->ended = t->drained = false;
		t->skipBefore = av_rescale_q(target, AV_TIME_BASE_Q, fmt->streams[t->stream]->time_base);
		t->nextOutputFrame = av_rescale(value, t->sampleRate > 0 ? t->sampleRate : 1, scale);
	}
	*result = kMDSuccess;
	return KERN_SUCCESS;
}

/* ---- Reading ------------------------------------------------------------ */

/* The next decoded frame of the track: 1, or 0 at its end, or -1. */
static int nextFrame(Track *t)
{
	for (;;) {
		int r = avcodec_receive_frame(t->ctx, frame);
		if (r == 0) {
			int64_t pts = frame->best_effort_timestamp;
			/* After a seek: the frames that end before the time
			   (or, with no duration, start before it). */
			int64_t duration = frame->duration;
			if (duration <= 0 && t->ctx->codec_type == AVMEDIA_TYPE_AUDIO && frame->sample_rate > 0)
				duration = av_rescale_q(frame->nb_samples, (AVRational){ 1, frame->sample_rate },
				    fmt->streams[t->stream]->time_base);
			int64_t end = pts + (duration > 0 ? duration : 1);
			if (t->skipBefore != AV_NOPTS_VALUE && pts != AV_NOPTS_VALUE) {
				if (end <= t->skipBefore) {
					av_frame_unref(frame);
					continue;
				}
			}
			return 1;
		}
		if (r == AVERROR_EOF)
			return 0;
		if (r != AVERROR(EAGAIN))
			return -1;
		AVPacket *p = pop(&t->queue);
		if (p != NULL) {
			/* A damaged packet is skipped: the next may decode. */
			avcodec_send_packet(t->ctx, p);
			av_packet_free(&p);
		} else if (demuxEnded) {
			if (t->flushed)
				return 0;
			avcodec_send_packet(t->ctx, NULL);
			t->flushed = true;
		} else
			demuxOne();
	}
}

/* A surface the app is not using, or a new one. */
static CVPixelBufferRef freeSurface(Track *t)
{
	for (int i = 0; i < t->surfaceCount; i++)
		if (!IOSurfaceIsInUse(CVPixelBufferGetIOSurface(t->surfaces[i])))
			return t->surfaces[i];
	if (t->surfaceCount == MAX_SURFACES)
		return NULL;
	const void *k[] = { kCVPixelBufferIOSurfacePropertiesKey };
	CFDictionaryRef empty = CFDictionaryCreate(NULL, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	const void *v[] = { empty };
	CFDictionaryRef attrs = CFDictionaryCreate(NULL, k, v, 1, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CVPixelBufferRef b = NULL;
	CVPixelBufferCreate(NULL, t->width, t->height, t->pixelFormat, attrs, &b);
	CFRelease(attrs);
	CFRelease(empty);
	if (b == NULL || CVPixelBufferGetIOSurface(b) == NULL) {
		CVPixelBufferRelease(b);
		return NULL;
	}
	t->surfaces[t->surfaceCount++] = b;
	return b;
}

static int scaleFlagsFor(const AVFrame *f, int w, int h)
{
	return (f->width == w && f->height == h ? SWS_POINT : SWS_BICUBIC) | SWS_ACCURATE_RND | SWS_FULL_CHR_H_INT;
}

static int32_t convertVideo(Track *t, mach_port_t *surface)
{
	CVPixelBufferRef b = freeSurface(t);
	if (b == NULL)
		return kMDErrorNoBuffers;
	bool nv12 = t->pixelFormat != kCVPixelFormatType_32BGRA;
	enum AVPixelFormat dst = nv12 ? AV_PIX_FMT_NV12 : AV_PIX_FMT_BGRA;
	t->sws = sws_getCachedContext(t->sws, frame->width, frame->height, frame->format, t->width, t->height, dst,
	    scaleFlagsFor(frame, t->width, t->height), NULL, NULL, NULL);
	if (t->sws == NULL)
		return kMDErrorDecode;
	/* The frame's own matrix and range; the output's range as asked
	   (BGRA is always full range). */
	int cs = frame->colorspace != AVCOL_SPC_UNSPECIFIED ? frame->colorspace :
	    (frame->height >= 720 ? AVCOL_SPC_BT709 : AVCOL_SPC_SMPTE170M);
	int srcFull = frame->color_range == AVCOL_RANGE_JPEG ||
	    frame->format == AV_PIX_FMT_YUVJ420P || frame->format == AV_PIX_FMT_YUVJ422P ||
	    frame->format == AV_PIX_FMT_YUVJ444P;
	int dstFull = !nv12 || t->pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
	sws_setColorspaceDetails(t->sws, sws_getCoefficients(cs), srcFull, sws_getCoefficients(cs), dstFull,
	    0, 1 << 16, 1 << 16);

	CVPixelBufferLockBaseAddress(b, 0);
	uint8_t *planes[4] = { 0 };
	int strides[4] = { 0 };
	if (nv12) {
		for (int i = 0; i < 2; i++) {
			planes[i] = CVPixelBufferGetBaseAddressOfPlane(b, i);
			strides[i] = (int)CVPixelBufferGetBytesPerRowOfPlane(b, i);
		}
	} else {
		planes[0] = CVPixelBufferGetBaseAddress(b);
		strides[0] = (int)CVPixelBufferGetBytesPerRow(b);
	}
	sws_scale(t->sws, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height, planes, strides);
	CVPixelBufferUnlockBaseAddress(b, 0);

	IOSurfaceRef s = CVPixelBufferGetIOSurface(b);
	*surface = IOSurfaceCreateMachPort(s);
	if (*surface == MACH_PORT_NULL)
		return kMDErrorNoBuffers;
	IOSurfaceIncrementUseCount(s);	/* the app's, until it lets go */
	return kMDSuccess;
}

static int32_t convertAudio(Track *t, md_data_t *samples, mach_msg_type_number_t *count, int32_t *frames)
{
	if (t->swr == NULL) {
		AVChannelLayout out;
		if (t->channels == frame->ch_layout.nb_channels)
			av_channel_layout_copy(&out, &frame->ch_layout);
		else
			av_channel_layout_default(&out, t->channels);
		int r = swr_alloc_set_opts2(&t->swr, &out, t->isFloat ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16,
		    t->sampleRate, &frame->ch_layout, frame->format, frame->sample_rate, 0, NULL);
		av_channel_layout_uninit(&out);
		if (r < 0 || swr_init(t->swr) < 0) {
			swr_free(&t->swr);
			return kMDErrorDecode;
		}
	}
	int room = swr_get_out_samples(t->swr, frame->nb_samples);
	size_t bytesPerFrame = (size_t)t->channels * (t->isFloat ? 4 : 2);
	uint8_t *buf = malloc(room * bytesPerFrame + 1);
	uint8_t *dst[1] = { buf };
	int got = swr_convert(t->swr, dst, room, (const uint8_t **)frame->extended_data, frame->nb_samples);
	if (got < 0) {
		free(buf);
		return kMDErrorDecode;
	}
	*frames = got;
	kern_return_t kr = outOfLine(buf, got * bytesPerFrame, samples, count);
	free(buf);
	return kr == KERN_SUCCESS ? kMDSuccess : kMDErrorDecode;
}

kern_return_t mds_read(mach_port_t server, int32_t id, int32_t *result, int32_t *flags, int64_t *ptsValue,
    int32_t *ptsScale, int64_t *durationValue, int32_t *durationScale, mach_port_t *surface, int32_t *frames,
    md_data_t *samples, mach_msg_type_number_t *samplesCount)
{
	Track *t = trackWithID(id);
	*flags = 0;
	*ptsValue = *durationValue = 0;
	*ptsScale = *durationScale = 1;
	*surface = MACH_PORT_NULL;
	*frames = 0;
	*samples = NULL;
	*samplesCount = 0;
	if (t == NULL || !t->enabled || t->ctx == NULL) {
		*result = fmt == NULL ? kMDErrorNotOpen : t == NULL ? kMDErrorNoSuchTrack : kMDErrorBadOptions;
		return KERN_SUCCESS;
	}
	if (t->ended) {
		*flags = kMDFlagEndOfTrack;
		*result = kMDSuccess;
		return KERN_SUCCESS;
	}
	int r = nextFrame(t);
	if (stalled) {
		*result = kMDErrorStalled;
		return KERN_SUCCESS;
	}
	/* At the end, the samples the resampler still holds. */
	if (r == 0 && t->ctx->codec_type == AVMEDIA_TYPE_AUDIO && t->swr != NULL && !t->drained) {
		t->drained = true;
		size_t bytesPerFrame = (size_t)t->channels * (t->isFloat ? 4 : 2);
		int room = swr_get_out_samples(t->swr, 0);
		if (room > 0) {
			uint8_t *buf = malloc(room * bytesPerFrame);
			uint8_t *dst[1] = { buf };
			int got = swr_convert(t->swr, dst, room, NULL, 0);
			if (got > 0) {
				*frames = got;
				*result = outOfLine(buf, got * bytesPerFrame, samples, samplesCount) == KERN_SUCCESS ?
				    kMDSuccess : kMDErrorDecode;
				*ptsValue = t->nextOutputFrame;
				*ptsScale = *durationScale = t->sampleRate;
				*durationValue = got;
				t->nextOutputFrame += got;
				free(buf);
				return KERN_SUCCESS;
			}
			free(buf);
		}
	}
	if (r <= 0) {
		t->ended = r == 0;
		*flags = r == 0 ? kMDFlagEndOfTrack : 0;
		*result = r == 0 ? kMDSuccess : kMDErrorDecode;
		return KERN_SUCCESS;
	}
	AVRational tb = fmt->streams[t->stream]->time_base;
	int64_t start = fmt->start_time != AV_NOPTS_VALUE ? av_rescale_q(fmt->start_time, AV_TIME_BASE_Q, tb) : 0;
	int64_t pts = frame->best_effort_timestamp;
	if (frame->flags & AV_FRAME_FLAG_KEY)
		*flags |= kMDFlagKeyFrame;

	if (t->ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
		*result = convertVideo(t, surface);
		/* Times in the stream's own time base, from the file's start. */
		*ptsValue = pts != AV_NOPTS_VALUE ? (pts - start) * tb.num : 0;
		*ptsScale = tb.den;
		*durationValue = frame->duration > 0 ? frame->duration * tb.num : 0;
		*durationScale = tb.den;
		if (*durationValue == 0) {
			AVRational rate = fmt->streams[t->stream]->avg_frame_rate;
			if (rate.num > 0) {
				*durationValue = rate.den;
				*durationScale = rate.num;
			}
		}
	} else {
		*result = convertAudio(t, samples, samplesCount, frames);
		/* Times in the output's sample rate. */
		int64_t at = pts != AV_NOPTS_VALUE ? av_rescale_q(pts - start, tb, (AVRational){ 1, t->sampleRate }) :
		    t->nextOutputFrame;
		*ptsValue = at;
		*ptsScale = t->sampleRate;
		*durationValue = *frames;
		*durationScale = t->sampleRate;
		t->nextOutputFrame = at + *frames;
	}
	av_frame_unref(frame);
	return KERN_SUCCESS;
}

/* ---- Starting ----------------------------------------------------------- */

/* The app's port (our bootstrap port for now) gets our service port and
   answers with the bootstrap port to use. */
static mach_port_t checkIn(void)
{
	mach_port_t app = MACH_PORT_NULL, service = MACH_PORT_NULL, reply = MACH_PORT_NULL;
	if (task_get_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, &app) != KERN_SUCCESS || app == MACH_PORT_NULL)
		return MACH_PORT_NULL;
	mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &service);
	mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply);

	MDHello hello;
	memset(&hello, 0, sizeof(hello));
	hello.header.msgh_bits = MACH_MSGH_BITS_COMPLEX |
	    MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
	hello.header.msgh_size = sizeof(hello);
	hello.header.msgh_remote_port = app;
	hello.header.msgh_local_port = reply;
	hello.header.msgh_id = kMDHelloID;
	hello.body.msgh_descriptor_count = 1;
	hello.service.name = service;
	hello.service.disposition = MACH_MSG_TYPE_MAKE_SEND;
	hello.service.type = MACH_MSG_PORT_DESCRIPTOR;
	if (mach_msg(&hello.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(hello), 0, MACH_PORT_NULL, 5000,
	    MACH_PORT_NULL) != MACH_MSG_SUCCESS)
		return MACH_PORT_NULL;

	struct {
		MDWelcome welcome;
		mach_msg_max_trailer_t trailer;
	} in;
	memset(&in, 0, sizeof(in));
	if (mach_msg(&in.welcome.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(in), reply, 10000,
	    MACH_PORT_NULL) != MACH_MSG_SUCCESS || in.welcome.header.msgh_id != kMDWelcomeID ||
	    !(in.welcome.header.msgh_bits & MACH_MSGH_BITS_COMPLEX) || in.welcome.body.msgh_descriptor_count != 1)
		return MACH_PORT_NULL;
	mach_port_t bootstrap = in.welcome.bootstrap.name;
	task_set_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, bootstrap);
	bootstrap_port = bootstrap;
	mach_port_deallocate(mach_task_self(), app);
	mach_port_mod_refs(mach_task_self(), reply, MACH_PORT_RIGHT_RECEIVE, -1);

	/* When the app lets go of the service, we end. */
	mach_port_t previous = MACH_PORT_NULL;
	mach_port_request_notification(mach_task_self(), service, MACH_NOTIFY_NO_SENDERS, 1, service,
	    MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
	return service;
}

static boolean_t demux(mach_msg_header_t *in, mach_msg_header_t *out)
{
	if (in->msgh_id == MACH_NOTIFY_NO_SENDERS)
		exit(0);
	return MediaDecoder_server(in, out);
}

int main(void)
{
	/* MEDIADECODER_LOG=1 (in the app's environment, which the decoder
	   inherits): FFmpeg's messages on the app's standard error. */
	av_log_set_level(getenv("MEDIADECODER_LOG") != NULL ? AV_LOG_VERBOSE : AV_LOG_QUIET);
	mach_port_t service = checkIn();
	if (service == MACH_PORT_NULL) {
		syslog(LOG_ERR, "mediadecoder: started by no app");
		return 1;
	}
	frame = av_frame_alloc();
	for (int i = 0; i < MAX_TRACKS; i++)
		tracks[i].skipBefore = AV_NOPTS_VALUE;
	/* Room for the largest request (the subsystem's maxsize is its
	   largest reply) and for the trailer. */
	for (;;)
		mach_msg_server_once(demux,
		    (mach_msg_size_t)MAX(sizeof(union __RequestUnion__mds_MediaDecoder_subsystem),
		    mds_MediaDecoder_subsystem.maxsize) + MAX_TRAILER_SIZE, service, 0);
}
