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
 * Core OpenGL (CGL) types and constants, source compatible with the
 * macOS CGL API. Written from the public CGL reference documentation.
 */

#ifndef _OPENGL_CGLTYPES_H_
#define _OPENGL_CGLTYPES_H_

#include <OpenGL/gltypes.h>

#ifdef __cplusplus
#define CGL_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define CGL_EXPORT extern __attribute__((visibility("default")))
#endif

#define CGL_DEPRECATED_ATTRIBUTE __attribute__((deprecated))

/* Opaque object types. */
typedef struct _CGLContextObject       *CGLContextObj;
typedef struct _CGLPixelFormatObject   *CGLPixelFormatObj;
typedef struct _CGLRendererInfoObject  *CGLRendererInfoObj;
typedef struct _CGLPBufferObject       *CGLPBufferObj;

/* Pixel format attributes (CGLChoosePixelFormat, CGLDescribePixelFormat). */
typedef enum _CGLPixelFormatAttribute {
    kCGLPFAAllRenderers                        =   1,
    kCGLPFATripleBuffer                        =   3,
    kCGLPFADoubleBuffer                        =   5,
    kCGLPFAColorSize                           =   8,
    kCGLPFAAlphaSize                           =  11,
    kCGLPFADepthSize                           =  12,
    kCGLPFAStencilSize                         =  13,
    kCGLPFAMinimumPolicy                       =  51,
    kCGLPFAMaximumPolicy                       =  52,
    kCGLPFASampleBuffers                       =  55,
    kCGLPFASamples                             =  56,
    kCGLPFAColorFloat                          =  58,
    kCGLPFAMultisample                         =  59,
    kCGLPFASupersample                         =  60,
    kCGLPFASampleAlpha                         =  61,
    kCGLPFARendererID                          =  70,
    kCGLPFANoRecovery                          =  72,
    kCGLPFAAccelerated                         =  73,
    kCGLPFAClosestPolicy                       =  74,
    kCGLPFABackingStore                        =  76,
    kCGLPFABackingVolatile                     =  77,
    kCGLPFADisplayMask                         =  84,
    kCGLPFAAllowOfflineRenderers               =  96,
    kCGLPFAAcceleratedCompute                  =  97,
    kCGLPFAOpenGLProfile                       =  99,
    kCGLPFASupportsAutomaticGraphicsSwitching  = 101,
    kCGLPFAVirtualScreenCount                  = 128,

    /* Deprecated attributes, still accepted. */
    kCGLPFAAuxBuffers                          =   7,
    kCGLPFAAccumSize                           =  14,
    kCGLPFAAuxDepthStencil                     =  57,
    kCGLPFAStereo                              =   6,
    kCGLPFAOffScreen                           =  53,
    kCGLPFAWindow                              =  80,
    kCGLPFACompliant                           =  83,
    kCGLPFAPBuffer                             =  90,
    kCGLPFARemotePBuffer                       =  91,
    kCGLPFASingleRenderer                      =  71,
    kCGLPFARobust                              =  75,
    kCGLPFAMPSafe                              =  78,
    kCGLPFAMultiScreen                         =  81,
    kCGLPFAFullScreen                          =  54,
} CGLPixelFormatAttribute;

/* Renderer properties (CGLDescribeRenderer). */
typedef enum _CGLRendererProperty {
    kCGLRPOffScreen                  =  53,
    kCGLRPFullScreen                 =  54,
    kCGLRPRendererID                 =  70,
    kCGLRPAccelerated                =  73,
    kCGLRPBackingStore               =  76,
    kCGLRPWindow                     =  80,
    kCGLRPCompliant                  =  83,
    kCGLRPDisplayMask                =  84,
    kCGLRPBufferModes                = 100,
    kCGLRPColorModes                 = 103,
    kCGLRPAccumModes                 = 104,
    kCGLRPDepthModes                 = 105,
    kCGLRPStencilModes               = 106,
    kCGLRPMaxAuxBuffers              = 107,
    kCGLRPMaxSampleBuffers           = 108,
    kCGLRPMaxSamples                 = 109,
    kCGLRPSampleModes                = 110,
    kCGLRPSampleAlpha                = 111,
    kCGLRPVideoMemory                = 120,
    kCGLRPTextureMemory              = 121,
    kCGLRPGPUVertProcCapable         = 122,
    kCGLRPGPUFragProcCapable         = 123,
    kCGLRPRendererCount              = 128,
    kCGLRPOnline                     = 129,
    kCGLRPAcceleratedCompute         = 130,
    kCGLRPVideoMemoryMegabytes       = 131,
    kCGLRPTextureMemoryMegabytes     = 132,
    kCGLRPMajorGLVersion             = 133,
    kCGLRPRegistryIDLow              = 140,
    kCGLRPRegistryIDHigh             = 141,
    kCGLRPRemovable                  = 142,

    /* Deprecated properties. */
    kCGLRPRobust                     =  75,
    kCGLRPMPSafe                     =  78,
    kCGLRPMultiScreen                =  81,
} CGLRendererProperty;

/* Boolean context options (CGLEnable, CGLDisable, CGLIsEnabled). */
typedef enum _CGLContextEnable {
    kCGLCESwapRectangle              = 201,
    kCGLCESwapLimit                  = 203,
    kCGLCERasterization              = 221,
    kCGLCEStateValidation            = 301,
    kCGLCESurfaceBackingSize         = 305,
    kCGLCEDisplayListOptimization    = 307,
    kCGLCEMPEngine                   = 313,
    kCGLCECrashOnRemovedFunctions    = 316,
} CGLContextEnable;

/* GPU restart status values (kCGLCPGPURestartStatus). */
typedef enum _CGLGPURestartStatus {
    kCGLCPGPURestartStatusNone       = 0,
    kCGLCPGPURestartStatusCaused     = 1,
    kCGLCPGPURestartStatusBlacklisted = 2,
} CGLGPURestartStatus;

/* Context parameters (CGLSetParameter, CGLGetParameter). */
typedef enum _CGLContextParameter {
    kCGLCPSwapRectangle                      = 200,
    kCGLCPSwapInterval                       = 222,
    kCGLCPDispatchTableSize                  = 224,
    kCGLCPClientStorage                      = 226,
    kCGLCPSurfaceTexture                     = 228,
    kCGLCPSurfaceOrder                       = 235,
    kCGLCPSurfaceOpacity                     = 236,
    kCGLCPSurfaceBackingSize                 = 304,
    kCGLCPSurfaceSurfaceVolatile             = 306,
    kCGLCPReclaimResources                   = 308,
    kCGLCPCurrentRendererID                  = 309,
    kCGLCPGPUVertexProcessing                = 310,
    kCGLCPGPUFragmentProcessing              = 311,
    kCGLCPHasDrawable                        = 314,
    kCGLCPMPSwapsInFlight                    = 315,
    kCGLCPGPURestartStatus                   = 317,
    kCGLCPAbortOnGPURestartStatusBlacklisted = 318,
    kCGLCPSupportGPURestart                  = 319,
    kCGLCPSupportSeparateAddressSpace        = 320,
    kCGLCPContextPriorityRequest             = 608,
} CGLContextParameter;

/* Values for kCGLCPContextPriorityRequest. */
enum {
    kCGLCPContextPriorityRequestHigh   = 0,
    kCGLCPContextPriorityRequestNormal = 1,
    kCGLCPContextPriorityRequestLow    = 2,
};

/* Library-wide options (CGLSetGlobalOption, CGLGetGlobalOption). */
typedef enum _CGLGlobalOption {
    kCGLGOFormatCacheSize            = 501,
    kCGLGOClearFormatCache           = 502,
    kCGLGORetainRenderers            = 503,
    kCGLGOUseBuildCache              = 506,

    /* Deprecated options. */
    kCGLGOResetLibrary               = 504,
    kCGLGOUseErrorHandler            = 505,
} CGLGlobalOption;

/* OpenGL profiles (kCGLPFAOpenGLProfile). */
typedef enum _CGLOpenGLProfile {
    kCGLOGLPVersion_Legacy           = 0x1000,
    kCGLOGLPVersion_3_2_Core         = 0x3200,
    kCGLOGLPVersion_GL3_Core         = 0x3200,
    kCGLOGLPVersion_GL4_Core         = 0x4100,
} CGLOpenGLProfile;

/* Error codes returned by every CGL call. */
typedef enum _CGLError {
    kCGLNoError            = 0,
    kCGLBadAttribute       = 10000,
    kCGLBadProperty        = 10001,
    kCGLBadPixelFormat     = 10002,
    kCGLBadRendererInfo    = 10003,
    kCGLBadContext         = 10004,
    kCGLBadDrawable        = 10005,
    kCGLBadDisplay         = 10006,
    kCGLBadState           = 10007,
    kCGLBadValue           = 10008,
    kCGLBadMatch           = 10009,
    kCGLBadEnumeration     = 10010,
    kCGLBadOffScreen       = 10011,
    kCGLBadFullScreen      = 10012,
    kCGLBadWindow          = 10013,
    kCGLBadAddress         = 10014,
    kCGLBadCodeModule      = 10015,
    kCGLBadAlloc           = 10016,
    kCGLBadConnection      = 10017,
} CGLError;

/* Buffer mode bits (kCGLRPBufferModes). */
#define kCGLMonoscopicBit       0x00000001
#define kCGLStereoscopicBit     0x00000002
#define kCGLSingleBufferBit     0x00000004
#define kCGLDoubleBufferBit     0x00000008
#define kCGLTripleBufferBit     0x00000010

/* Depth, stencil and accumulation buffer bit depths. */
#define kCGL0Bit                0x00000001
#define kCGL1Bit                0x00000002
#define kCGL2Bit                0x00000004
#define kCGL3Bit                0x00000008
#define kCGL4Bit                0x00000010
#define kCGL5Bit                0x00000020
#define kCGL6Bit                0x00000040
#define kCGL8Bit                0x00000080
#define kCGL10Bit               0x00000100
#define kCGL12Bit               0x00000200
#define kCGL16Bit               0x00000400
#define kCGL24Bit               0x00000800
#define kCGL32Bit               0x00001000
#define kCGL48Bit               0x00002000
#define kCGL64Bit               0x00004000
#define kCGL96Bit               0x00008000
#define kCGL128Bit              0x00010000

/* Color and accumulation buffer formats (kCGLRPColorModes). */
#define kCGLRGB444Bit           0x00000040
#define kCGLARGB4444Bit         0x00000080
#define kCGLRGB444A8Bit         0x00000100
#define kCGLRGB555Bit           0x00000200
#define kCGLARGB1555Bit         0x00000400
#define kCGLRGB555A8Bit         0x00000800
#define kCGLRGB565Bit           0x00001000
#define kCGLRGB565A8Bit         0x00002000
#define kCGLRGB888Bit           0x00004000
#define kCGLARGB8888Bit         0x00008000
#define kCGLRGB888A8Bit         0x00010000
#define kCGLRGB101010Bit        0x00020000
#define kCGLARGB2101010Bit      0x00040000
#define kCGLRGB101010_A8Bit     0x00080000
#define kCGLRGB121212Bit        0x00100000
#define kCGLARGB12121212Bit     0x00200000
#define kCGLRGB161616Bit        0x00400000
#define kCGLRGBA16161616Bit     0x00800000
#define kCGLRGBFloat64Bit       0x01000000
#define kCGLRGBAFloat64Bit      0x02000000
#define kCGLRGBFloat128Bit      0x04000000
#define kCGLRGBAFloat128Bit     0x08000000
#define kCGLRGBFloat256Bit      0x10000000
#define kCGLRGBAFloat256Bit     0x20000000

/* Sampling modes (kCGLRPSampleModes). */
#define kCGLSupersampleBit      0x00000001
#define kCGLMultisampleBit      0x00000002

#endif /* _OPENGL_CGLTYPES_H_ */
