/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The type identifiers of the system (the Cocoa API's UTCoreTypes.h).
 */

#ifndef LAUNCHSERVICES_UTCORETYPES_H
#define LAUNCHSERVICES_UTCORETYPES_H

#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN

extern const CFStringRef kUTTypeItem;	/* public.item */
extern const CFStringRef kUTTypeContent;	/* public.content */
extern const CFStringRef kUTTypeCompositeContent;	/* public.composite-content */
extern const CFStringRef kUTTypeMessage;	/* public.message */
extern const CFStringRef kUTTypeContact;	/* public.contact */
extern const CFStringRef kUTTypeArchive;	/* public.archive */
extern const CFStringRef kUTTypeDiskImage;	/* public.disk-image */
extern const CFStringRef kUTTypeData;	/* public.data */
extern const CFStringRef kUTTypeDirectory;	/* public.directory */
extern const CFStringRef kUTTypeResolvable;	/* com.apple.resolvable */
extern const CFStringRef kUTTypeSymLink;	/* public.symlink */
extern const CFStringRef kUTTypeExecutable;	/* public.executable */
extern const CFStringRef kUTTypeMountPoint;	/* com.apple.mount-point */
extern const CFStringRef kUTTypeAliasFile;	/* com.apple.alias-file */
extern const CFStringRef kUTTypeURL;	/* public.url */
extern const CFStringRef kUTTypeFileURL;	/* public.file-url */
extern const CFStringRef kUTTypeText;	/* public.text */
extern const CFStringRef kUTTypePlainText;	/* public.plain-text */
extern const CFStringRef kUTTypeUTF8PlainText;	/* public.utf8-plain-text */
extern const CFStringRef kUTTypeUTF16PlainText;	/* public.utf16-plain-text */
extern const CFStringRef kUTTypeRTF;	/* public.rtf */
extern const CFStringRef kUTTypeHTML;	/* public.html */
extern const CFStringRef kUTTypeXML;	/* public.xml */
extern const CFStringRef kUTTypeSourceCode;	/* public.source-code */
extern const CFStringRef kUTTypeAssemblyLanguageSource;	/* public.assembly-source */
extern const CFStringRef kUTTypeCSource;	/* public.c-source */
extern const CFStringRef kUTTypeObjectiveCSource;	/* public.objective-c-source */
extern const CFStringRef kUTTypeSwiftSource;	/* public.swift-source */
extern const CFStringRef kUTTypeCPlusPlusSource;	/* public.c-plus-plus-source */
extern const CFStringRef kUTTypeObjectiveCPlusPlusSource;	/* public.objective-c-plus-plus-source */
extern const CFStringRef kUTTypeCHeader;	/* public.c-header */
extern const CFStringRef kUTTypeCPlusPlusHeader;	/* public.c-plus-plus-header */
extern const CFStringRef kUTTypeScript;	/* public.script */
extern const CFStringRef kUTTypeShellScript;	/* public.shell-script */
extern const CFStringRef kUTTypePythonScript;	/* public.python-script */
extern const CFStringRef kUTTypePerlScript;	/* public.perl-script */
extern const CFStringRef kUTTypeRubyScript;	/* public.ruby-script */
extern const CFStringRef kUTTypeJavaScript;	/* com.netscape.javascript-source */
extern const CFStringRef kUTTypeJSON;	/* public.json */
extern const CFStringRef kUTTypePropertyList;	/* com.apple.property-list */
extern const CFStringRef kUTTypeXMLPropertyList;	/* com.apple.xml-property-list */
extern const CFStringRef kUTTypeBinaryPropertyList;	/* com.apple.binary-property-list */
extern const CFStringRef kUTTypePDF;	/* com.adobe.pdf */
extern const CFStringRef kUTTypeRTFD;	/* com.apple.rtfd */
extern const CFStringRef kUTTypeLog;	/* public.log */
extern const CFStringRef kUTTypeCommaSeparatedText;	/* public.comma-separated-values-text */
extern const CFStringRef kUTTypeTabSeparatedText;	/* public.tab-separated-values-text */
extern const CFStringRef kUTTypeURLBookmarkData;	/* com.apple.bookmark */
extern const CFStringRef kUTTypeImage;	/* public.image */
extern const CFStringRef kUTTypeJPEG;	/* public.jpeg */
extern const CFStringRef kUTTypeTIFF;	/* public.tiff */
extern const CFStringRef kUTTypeGIF;	/* com.compuserve.gif */
extern const CFStringRef kUTTypePNG;	/* public.png */
extern const CFStringRef kUTTypeBMP;	/* com.microsoft.bmp */
extern const CFStringRef kUTTypeICO;	/* com.microsoft.ico */
extern const CFStringRef kUTTypeAppleICNS;	/* com.apple.icns */
extern const CFStringRef kUTTypeScalableVectorGraphics;	/* public.svg-image */
extern const CFStringRef kUTTypeAudiovisualContent;	/* public.audiovisual-content */
extern const CFStringRef kUTTypeMovie;	/* public.movie */
extern const CFStringRef kUTTypeVideo;	/* public.video */
extern const CFStringRef kUTTypeAudio;	/* public.audio */
extern const CFStringRef kUTTypeQuickTimeMovie;	/* com.apple.quicktime-movie */
extern const CFStringRef kUTTypeMPEG;	/* public.mpeg */
extern const CFStringRef kUTTypeMPEG4;	/* public.mpeg-4 */
extern const CFStringRef kUTTypeMP3;	/* public.mp3 */
extern const CFStringRef kUTTypeMPEG4Audio;	/* public.mpeg-4-audio */
extern const CFStringRef kUTTypeWaveformAudio;	/* com.microsoft.waveform-audio */
extern const CFStringRef kUTTypeFolder;	/* public.folder */
extern const CFStringRef kUTTypeVolume;	/* public.volume */
extern const CFStringRef kUTTypePackage;	/* com.apple.package */
extern const CFStringRef kUTTypeBundle;	/* com.apple.bundle */
extern const CFStringRef kUTTypePluginBundle;	/* com.apple.plugin */
extern const CFStringRef kUTTypeFramework;	/* com.apple.framework */
extern const CFStringRef kUTTypeApplication;	/* com.apple.application */
extern const CFStringRef kUTTypeApplicationBundle;	/* com.apple.application-bundle */
extern const CFStringRef kUTTypeApplicationFile;	/* com.apple.application-file */
extern const CFStringRef kUTTypeUnixExecutable;	/* public.unix-executable */
extern const CFStringRef kUTTypeFont;	/* public.font */
extern const CFStringRef kUTTypeVCard;	/* public.vcard */
extern const CFStringRef kUTTypeGNUZipArchive;	/* org.gnu.gnu-zip-archive */
extern const CFStringRef kUTTypeBzip2Archive;	/* public.bzip2-archive */
extern const CFStringRef kUTTypeZipArchive;	/* public.zip-archive */
extern const CFStringRef kUTTypeSpreadsheet;	/* public.spreadsheet */
extern const CFStringRef kUTTypePresentation;	/* public.presentation */
extern const CFStringRef kUTTypeDatabase;	/* public.database */
extern const CFStringRef kUTTypeEmailMessage;	/* public.email-message */

CF_EXTERN_C_END

#endif
