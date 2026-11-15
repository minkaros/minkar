/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A media decoder session (MDClient.h): /usr/libexec/mediadecoder started
 * with a port of ours as its bootstrap port, given our own bootstrap port
 * once it says hello, then asked for samples over MediaDecoder.defs. A
 * decoder that dies leaves the session failing with kMDErrorNoService.
 */

#include "MDClient.h"
#include "MediaDecoderTypes.h"
#include "MediaDecoderUser.h"

#include <CoreMedia/CMBlockBuffer.h>
#include <CoreMedia/CMFormatDescription.h>
#include <CoreVideo/CVPixelBuffer.h>
#include <IOSurface/IOSurfaceRef.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern int fileport_makeport(int fd, mach_port_t *port);
extern char **environ;
void _CVPixelBufferSetFinalizeHook(CVPixelBufferRef buffer, void (*hook)(void *refcon), void *refcon);

#define MAX_TRACKS 16

typedef struct {
	bool isVideo;
	int sampleRate, channels;
	bool isFloat;
	CMFormatDescriptionRef format;	/* the last one made */
	CFDictionaryRef colorAttachments;
} TrackState;

struct MDSession {
	pthread_mutex_t lock;
	mach_port_t service;
	pid_t pid;
	bool dead;
	CFDictionaryRef info;
	int trackCount;
	TrackState tracks[MAX_TRACKS];
};

/* ---- Starting the decoder ----------------------------------------------- */

static void reapWhenDone(pid_t pid)
{
	dispatch_source_t s = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)pid, DISPATCH_PROC_EXIT,
	    dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
	if (s == NULL) {
		waitpid(pid, NULL, WNOHANG);
		return;
	}
	dispatch_source_set_event_handler(s, ^{
		waitpid(pid, NULL, 0);
		dispatch_source_cancel(s);
	});
	dispatch_source_set_cancel_handler(s, ^{
		dispatch_release(s);
	});
	dispatch_resume(s);
}

static mach_port_t startDecoder(pid_t *pidOut)
{
	mach_port_t hello = MACH_PORT_NULL, service = MACH_PORT_NULL;
	posix_spawnattr_t attr;
	pid_t pid;
	char *argv[] = { "mediadecoder", NULL };

	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &hello) != KERN_SUCCESS)
		return MACH_PORT_NULL;
	mach_port_insert_right(mach_task_self(), hello, hello, MACH_MSG_TYPE_MAKE_SEND);
	posix_spawnattr_init(&attr);
	posix_spawnattr_setspecialport_np(&attr, hello, TASK_BOOTSTRAP_PORT);
	int r = posix_spawn(&pid, kMDDecoderPath, NULL, &attr, argv, environ);
	posix_spawnattr_destroy(&attr);
	mach_port_deallocate(mach_task_self(), hello);	/* our send right */
	if (r != 0) {
		mach_port_mod_refs(mach_task_self(), hello, MACH_PORT_RIGHT_RECEIVE, -1);
		return MACH_PORT_NULL;
	}
	reapWhenDone(pid);

	struct {
		MDHello hello;
		mach_msg_max_trailer_t trailer;
	} in;
	memset(&in, 0, sizeof(in));
	if (mach_msg(&in.hello.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(in), hello, 10000,
	    MACH_PORT_NULL) == MACH_MSG_SUCCESS && in.hello.header.msgh_id == kMDHelloID &&
	    (in.hello.header.msgh_bits & MACH_MSGH_BITS_COMPLEX) && in.hello.body.msgh_descriptor_count == 1) {
		service = in.hello.service.name;
		MDWelcome w;
		memset(&w, 0, sizeof(w));
		w.header.msgh_bits = MACH_MSGH_BITS_COMPLEX | MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
		w.header.msgh_size = sizeof(w);
		w.header.msgh_remote_port = in.hello.header.msgh_remote_port;
		w.header.msgh_id = kMDWelcomeID;
		w.body.msgh_descriptor_count = 1;
		w.bootstrap.name = bootstrap_port;
		w.bootstrap.disposition = MACH_MSG_TYPE_COPY_SEND;
		w.bootstrap.type = MACH_MSG_PORT_DESCRIPTOR;
		if (mach_msg(&w.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(w), 0, MACH_PORT_NULL, 5000,
		    MACH_PORT_NULL) != MACH_MSG_SUCCESS) {
			mach_port_deallocate(mach_task_self(), service);
			service = MACH_PORT_NULL;
		}
	} else
		kill(pid, SIGKILL);
	mach_port_mod_refs(mach_task_self(), hello, MACH_PORT_RIGHT_RECEIVE, -1);
	*pidOut = pid;
	return service;
}

static double number(CFDictionaryRef d, const char *key, double otherwise)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	CFTypeRef v = d != NULL ? CFDictionaryGetValue(d, k) : NULL;
	double x = otherwise;
	CFRelease(k);
	if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
		CFNumberGetValue(v, kCFNumberDoubleType, &x);
	else if (v != NULL && CFGetTypeID(v) == CFBooleanGetTypeID())
		x = CFBooleanGetValue(v);
	return x;
}

static CFStringRef string(CFDictionaryRef d, const char *key)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	CFTypeRef v = d != NULL ? CFDictionaryGetValue(d, k) : NULL;
	CFRelease(k);
	return v != NULL && CFGetTypeID(v) == CFStringGetTypeID() ? v : NULL;
}

static bool named(CFStringRef s, const char *name)
{
	if (s == NULL)
		return false;
	CFStringRef n = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
	bool same = CFEqual(s, n);
	CFRelease(n);
	return same;
}

/* A video track's colors as pixel buffer attachments (FFmpeg's names). */
static CFDictionaryRef colorAttachments(CFDictionaryRef track)
{
	CFMutableDictionaryRef a = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFStringRef pri = string(track, kMDKeyColorPrimaries), trc = string(track, kMDKeyTransfer),
	    mat = string(track, kMDKeyMatrix);
	CFStringRef p = named(pri, "bt2020") ? kCVImageBufferColorPrimaries_ITU_R_2020 :
	    named(pri, "smpte432") ? kCVImageBufferColorPrimaries_P3_D65 :
	    named(pri, "smpte431") ? kCVImageBufferColorPrimaries_DCI_P3 :
	    named(pri, "bt470bg") ? kCVImageBufferColorPrimaries_EBU_3213 :
	    named(pri, "smpte170m") || named(pri, "smpte240m") ? kCVImageBufferColorPrimaries_SMPTE_C :
	    kCVImageBufferColorPrimaries_ITU_R_709_2;
	CFStringRef t = named(trc, "smpte2084") ? kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ :
	    named(trc, "arib-std-b67") ? kCVImageBufferTransferFunction_ITU_R_2100_HLG :
	    named(trc, "iec61966-2-1") ? kCVImageBufferTransferFunction_sRGB :
	    named(trc, "linear") ? kCVImageBufferTransferFunction_Linear :
	    named(trc, "smpte240m") ? kCVImageBufferTransferFunction_SMPTE_240M_1995 :
	    named(trc, "bt2020-10") || named(trc, "bt2020-12") ? kCVImageBufferTransferFunction_ITU_R_2020 :
	    kCVImageBufferTransferFunction_ITU_R_709_2;
	double height = number(track, kMDKeyHeight, 0);
	CFStringRef m = named(mat, "bt2020nc") || named(mat, "bt2020c") ? kCVImageBufferYCbCrMatrix_ITU_R_2020 :
	    named(mat, "smpte170m") || named(mat, "bt470bg") ? kCVImageBufferYCbCrMatrix_ITU_R_601_4 :
	    named(mat, "smpte240m") ? kCVImageBufferYCbCrMatrix_SMPTE_240M_1995 :
	    named(mat, "bt709") ? kCVImageBufferYCbCrMatrix_ITU_R_709_2 :
	    height >= 720 ? kCVImageBufferYCbCrMatrix_ITU_R_709_2 : kCVImageBufferYCbCrMatrix_ITU_R_601_4;
	CFDictionarySetValue(a, kCVImageBufferColorPrimariesKey, p);
	CFDictionarySetValue(a, kCVImageBufferTransferFunctionKey, t);
	CFDictionarySetValue(a, kCVImageBufferYCbCrMatrixKey, m);
	return a;
}

MDSessionRef _MDSessionCreate(int fd, const char *name, int32_t *error)
{
	pid_t pid = 0;
	int32_t result = kMDErrorNoService;
	mach_port_t file = MACH_PORT_NULL;
	md_data_t info = NULL;
	mach_msg_type_number_t infoCount = 0;

	mach_port_t service = startDecoder(&pid);
	if (service == MACH_PORT_NULL)
		goto fail;
	if (fileport_makeport(fd, &file) != 0) {
		result = kMDErrorCannotOpen;
		goto fail;
	}
	size_t nameLength = name != NULL ? strlen(name) : 0;
	if (nameLength > 4095)
		nameLength = 4095;
	kern_return_t kr = mdc_open(service, file, (md_bytes_t)(name != NULL ? name : ""),
	    (mach_msg_type_number_t)nameLength, &result, &info, &infoCount);
	if (kr != KERN_SUCCESS) {
		result = kMDErrorNoService;
		goto fail;
	}
	if (result != kMDSuccess)
		goto fail;
	CFDataRef data = CFDataCreate(NULL, (const UInt8 *)info, infoCount);
	vm_deallocate(mach_task_self(), (vm_address_t)info, infoCount);
	CFPropertyListRef plist = CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable, NULL, NULL);
	CFRelease(data);
	if (plist == NULL || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
		if (plist != NULL)
			CFRelease(plist);
		result = kMDErrorDecode;
		goto fail;
	}
	struct MDSession *s = calloc(1, sizeof(*s));
	pthread_mutex_init(&s->lock, NULL);
	s->service = service;
	s->pid = pid;
	s->info = plist;
	CFStringRef tk = CFStringCreateWithCString(NULL, kMDKeyTracks, kCFStringEncodingUTF8);
	CFArrayRef tracks = CFDictionaryGetValue(plist, tk);
	CFRelease(tk);
	CFIndex n = tracks != NULL && CFGetTypeID(tracks) == CFArrayGetTypeID() ? CFArrayGetCount(tracks) : 0;
	for (CFIndex i = 0; i < n && i < MAX_TRACKS; i++) {
		CFDictionaryRef t = CFArrayGetValueAtIndex(tracks, i);
		TrackState *ts = &s->tracks[i];
		ts->isVideo = named(string(t, kMDKeyMediaType), "vide");
		ts->sampleRate = (int)number(t, kMDKeySampleRate, 0);
		ts->channels = (int)number(t, kMDKeyChannels, 0);
		ts->isFloat = true;
		if (ts->isVideo)
			ts->colorAttachments = colorAttachments(t);
		s->trackCount++;
	}
	if (error != NULL)
		*error = kMDSuccess;
	return s;

fail:
	if (service != MACH_PORT_NULL) {
		mach_port_deallocate(mach_task_self(), service);
		kill(pid, SIGKILL);
	}
	if (error != NULL)
		*error = result;
	return NULL;
}

CFDictionaryRef _MDSessionGetInfo(MDSessionRef s)
{
	return s != NULL ? s->info : NULL;
}

pid_t _MDSessionGetProcessID(MDSessionRef s)
{
	return s != NULL ? s->pid : 0;
}

/* A call's outcome: a transport failure means the decoder is gone. */
static int32_t outcome(MDSessionRef s, kern_return_t kr, int32_t result)
{
	if (kr == KERN_SUCCESS)
		return result;
	s->dead = true;
	return kMDErrorNoService;
}

int32_t _MDSessionConfigure(MDSessionRef s, int32_t trackID, CFDictionaryRef options)
{
	int32_t result = kMDErrorNoSuchTrack;
	if (s == NULL || trackID < 1 || trackID > s->trackCount)
		return result;
	pthread_mutex_lock(&s->lock);
	if (s->dead) {
		pthread_mutex_unlock(&s->lock);
		return kMDErrorNoService;
	}
	CFDataRef data = options != NULL ?
	    CFPropertyListCreateData(NULL, options, kCFPropertyListBinaryFormat_v1_0, 0, NULL) : NULL;
	if (data != NULL && CFDataGetLength(data) > 4096) {
		CFRelease(data);
		pthread_mutex_unlock(&s->lock);
		return kMDErrorBadOptions;
	}
	kern_return_t kr = mdc_configure(s->service, trackID,
	    data != NULL ? (md_bytes_t)CFDataGetBytePtr(data) : (md_bytes_t)"",
	    data != NULL ? (mach_msg_type_number_t)CFDataGetLength(data) : 0, &result);
	result = outcome(s, kr, result);
	if (result == kMDSuccess) {
		TrackState *t = &s->tracks[trackID - 1];
		if (!t->isVideo) {
			t->sampleRate = (int)number(options, kMDKeyOutputSampleRate, t->sampleRate);
			t->channels = (int)number(options, kMDKeyOutputChannels, t->channels);
			t->isFloat = number(options, kMDKeyOutputFloat, 1) != 0;
		}
		if (t->format != NULL) {
			CFRelease(t->format);
			t->format = NULL;
		}
	}
	if (data != NULL)
		CFRelease(data);
	pthread_mutex_unlock(&s->lock);
	return result;
}

int32_t _MDSessionSeek(MDSessionRef s, CMTime time)
{
	int32_t result = kMDErrorNotOpen;
	if (s == NULL || !CMTIME_IS_NUMERIC(time))
		return kMDErrorBadOptions;
	pthread_mutex_lock(&s->lock);
	if (s->dead)
		result = kMDErrorNoService;
	else
		result = outcome(s, mdc_seek(s->service, time.value, time.timescale, &result), result);
	pthread_mutex_unlock(&s->lock);
	return result;
}

/* ---- Samples ------------------------------------------------------------ */

static void surfaceReturned(void *refcon)
{
	IOSurfaceRef surface = refcon;
	IOSurfaceDecrementUseCount(surface);	/* the decoder may write it again */
	CFRelease(surface);
}

static void freeOutOfLine(void *refCon, void *block, size_t size)
{
	vm_deallocate(mach_task_self(), (vm_address_t)block, size);
}

static int32_t videoSample(TrackState *t, mach_port_t port, CMSampleTimingInfo *timing, CMSampleBufferRef *out)
{
	IOSurfaceRef surface = IOSurfaceLookupFromMachPort(port);
	mach_port_deallocate(mach_task_self(), port);
	if (surface == NULL)
		return kMDErrorNoBuffers;
	CVPixelBufferRef pb = NULL;
	if (CVPixelBufferCreateWithIOSurface(NULL, surface, NULL, &pb) != kCVReturnSuccess) {
		IOSurfaceDecrementUseCount(surface);
		CFRelease(surface);
		return kMDErrorNoBuffers;
	}
	_CVPixelBufferSetFinalizeHook(pb, surfaceReturned, (void *)surface);	/* takes our reference */
	if (t->colorAttachments != NULL)
		CVBufferSetAttachments(pb, t->colorAttachments, kCVAttachmentMode_ShouldPropagate);
	if (t->format == NULL || !CMVideoFormatDescriptionMatchesImageBuffer(t->format, pb)) {
		if (t->format != NULL)
			CFRelease(t->format);
		t->format = NULL;
		CMVideoFormatDescriptionCreateForImageBuffer(NULL, pb, &t->format);
	}
	OSStatus st = CMSampleBufferCreateReadyWithImageBuffer(NULL, pb, t->format, timing, out);
	CVPixelBufferRelease(pb);
	return st == noErr ? kMDSuccess : kMDErrorDecode;
}

static int32_t audioSample(TrackState *t, md_data_t bytes, mach_msg_type_number_t length, int32_t frames,
    CMTime pts, CMSampleBufferRef *out)
{
	if (t->format == NULL) {
		AudioStreamBasicDescription asbd = { 0 };
		asbd.mSampleRate = t->sampleRate;
		asbd.mFormatID = kAudioFormatLinearPCM;
		asbd.mFormatFlags = (t->isFloat ? kAudioFormatFlagIsFloat : kAudioFormatFlagIsSignedInteger) |
		    kAudioFormatFlagIsPacked;
		asbd.mChannelsPerFrame = t->channels;
		asbd.mBitsPerChannel = t->isFloat ? 32 : 16;
		asbd.mBytesPerFrame = asbd.mBytesPerPacket = t->channels * asbd.mBitsPerChannel / 8;
		asbd.mFramesPerPacket = 1;
		CMAudioFormatDescriptionCreate(NULL, &asbd, 0, NULL, 0, NULL, NULL, &t->format);
	}
	if (frames <= 0 || length == 0) {
		if (bytes != NULL)
			vm_deallocate(mach_task_self(), (vm_address_t)bytes, length);
		*out = NULL;
		return kMDSuccess;
	}
	CMBlockBufferCustomBlockSource source = { kCMBlockBufferCustomBlockSourceVersion, NULL, freeOutOfLine, NULL };
	CMBlockBufferRef block = NULL;
	OSStatus st = CMBlockBufferCreateWithMemoryBlock(NULL, bytes, length, kCFAllocatorNull, &source, 0, length, 0,
	    &block);
	if (st != noErr) {
		vm_deallocate(mach_task_self(), (vm_address_t)bytes, length);
		return kMDErrorDecode;
	}
	st = CMAudioSampleBufferCreateReadyWithPacketDescriptions(NULL, block, t->format, frames, pts, NULL, out);
	CFRelease(block);
	return st == noErr ? kMDSuccess : kMDErrorDecode;
}

int32_t _MDSessionCopyNextSample(MDSessionRef s, int32_t trackID, CMSampleBufferRef *sample, Boolean *ended)
{
	int32_t result = kMDErrorNoSuchTrack, flags = 0, ptsScale = 1, durationScale = 1, frames = 0;
	int64_t ptsValue = 0, durationValue = 0;
	mach_port_t surface = MACH_PORT_NULL;
	md_data_t bytes = NULL;
	mach_msg_type_number_t length = 0;

	*sample = NULL;
	if (ended != NULL)
		*ended = false;
	if (s == NULL || trackID < 1 || trackID > s->trackCount)
		return result;
	pthread_mutex_lock(&s->lock);
	TrackState *t = &s->tracks[trackID - 1];
	for (;;) {
		if (s->dead) {
			result = kMDErrorNoService;
			break;
		}
		kern_return_t kr = mdc_read(s->service, trackID, &result, &flags, &ptsValue, &ptsScale, &durationValue,
		    &durationScale, &surface, &frames, &bytes, &length);
		result = outcome(s, kr, result);
		if (result != kMDSuccess) {
			if (surface != MACH_PORT_NULL)
				mach_port_deallocate(mach_task_self(), surface);
			if (bytes != NULL)
				vm_deallocate(mach_task_self(), (vm_address_t)bytes, length);
			break;
		}
		if (flags & kMDFlagEndOfTrack) {
			if (ended != NULL)
				*ended = true;
			break;
		}
		CMSampleTimingInfo timing = { CMTimeMake(durationValue, durationScale), CMTimeMake(ptsValue, ptsScale),
		    kCMTimeInvalid };
		if (t->isVideo)
			result = surface != MACH_PORT_NULL ? videoSample(t, surface, &timing, sample) : kMDErrorNoBuffers;
		else
			result = audioSample(t, bytes, length, frames, timing.presentationTimeStamp, sample);
		/* An audio frame the resampler kept back: ask again. */
		if (result != kMDSuccess || *sample != NULL)
			break;
	}
	pthread_mutex_unlock(&s->lock);
	return result;
}

void _MDSessionRelease(MDSessionRef s)
{
	if (s == NULL)
		return;
	if (s->service != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), s->service);	/* the decoder ends */
	for (int i = 0; i < s->trackCount; i++) {
		if (s->tracks[i].format != NULL)
			CFRelease(s->tracks[i].format);
		if (s->tracks[i].colorAttachments != NULL)
			CFRelease(s->tracks[i].colorAttachments);
	}
	if (s->info != NULL)
		CFRelease(s->info);
	pthread_mutex_destroy(&s->lock);
	free(s);
}
