// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// The common types: made when the framework loads.

/* Defined here without the header, whose declarations are const: they
   are set once, as the framework loads. */
#import <UniformTypeIdentifiers/UTType.h>

__attribute__((visibility("default"))) UTType *UTTypeItem;
__attribute__((visibility("default"))) UTType *UTTypeContent;
__attribute__((visibility("default"))) UTType *UTTypeCompositeContent;
__attribute__((visibility("default"))) UTType *UTTypeData;
__attribute__((visibility("default"))) UTType *UTTypeDirectory;
__attribute__((visibility("default"))) UTType *UTTypeFolder;
__attribute__((visibility("default"))) UTType *UTTypeVolume;
__attribute__((visibility("default"))) UTType *UTTypePackage;
__attribute__((visibility("default"))) UTType *UTTypeBundle;
__attribute__((visibility("default"))) UTType *UTTypeApplication;
__attribute__((visibility("default"))) UTType *UTTypeApplicationBundle;
__attribute__((visibility("default"))) UTType *UTTypeExecutable;
__attribute__((visibility("default"))) UTType *UTTypeUnixExecutable;
__attribute__((visibility("default"))) UTType *UTTypeSymbolicLink;
__attribute__((visibility("default"))) UTType *UTTypeAliasFile;
__attribute__((visibility("default"))) UTType *UTTypeURL;
__attribute__((visibility("default"))) UTType *UTTypeFileURL;
__attribute__((visibility("default"))) UTType *UTTypeText;
__attribute__((visibility("default"))) UTType *UTTypePlainText;
__attribute__((visibility("default"))) UTType *UTTypeUTF8PlainText;
__attribute__((visibility("default"))) UTType *UTTypeRTF;
__attribute__((visibility("default"))) UTType *UTTypeHTML;
__attribute__((visibility("default"))) UTType *UTTypeXML;
__attribute__((visibility("default"))) UTType *UTTypeJSON;
__attribute__((visibility("default"))) UTType *UTTypeSourceCode;
__attribute__((visibility("default"))) UTType *UTTypeCSource;
__attribute__((visibility("default"))) UTType *UTTypeObjectiveCSource;
__attribute__((visibility("default"))) UTType *UTTypeSwiftSource;
__attribute__((visibility("default"))) UTType *UTTypeScript;
__attribute__((visibility("default"))) UTType *UTTypeShellScript;
__attribute__((visibility("default"))) UTType *UTTypePythonScript;
__attribute__((visibility("default"))) UTType *UTTypePropertyList;
__attribute__((visibility("default"))) UTType *UTTypeImage;
__attribute__((visibility("default"))) UTType *UTTypePNG;
__attribute__((visibility("default"))) UTType *UTTypeJPEG;
__attribute__((visibility("default"))) UTType *UTTypeGIF;
__attribute__((visibility("default"))) UTType *UTTypeTIFF;
__attribute__((visibility("default"))) UTType *UTTypeBMP;
__attribute__((visibility("default"))) UTType *UTTypeICNS;
__attribute__((visibility("default"))) UTType *UTTypeSVG;
__attribute__((visibility("default"))) UTType *UTTypePDF;
__attribute__((visibility("default"))) UTType *UTTypeAudio;
__attribute__((visibility("default"))) UTType *UTTypeMovie;
__attribute__((visibility("default"))) UTType *UTTypeVideo;
__attribute__((visibility("default"))) UTType *UTTypeArchive;
__attribute__((visibility("default"))) UTType *UTTypeZIP;
__attribute__((visibility("default"))) UTType *UTTypeGZIP;
__attribute__((visibility("default"))) UTType *UTTypeDiskImage;
__attribute__((visibility("default"))) UTType *UTTypeFont;
__attribute__((visibility("default"))) UTType *UTTypeLog;

__attribute__((constructor)) static void UTMakeCoreTypes(void) {
    @autoreleasepool {
        UTTypeItem = [UTType typeWithIdentifier:@"public.item"];
        UTTypeContent = [UTType typeWithIdentifier:@"public.content"];
        UTTypeCompositeContent = [UTType typeWithIdentifier:@"public.composite-content"];
        UTTypeData = [UTType typeWithIdentifier:@"public.data"];
        UTTypeDirectory = [UTType typeWithIdentifier:@"public.directory"];
        UTTypeFolder = [UTType typeWithIdentifier:@"public.folder"];
        UTTypeVolume = [UTType typeWithIdentifier:@"public.volume"];
        UTTypePackage = [UTType typeWithIdentifier:@"com.apple.package"];
        UTTypeBundle = [UTType typeWithIdentifier:@"com.apple.bundle"];
        UTTypeApplication = [UTType typeWithIdentifier:@"com.apple.application"];
        UTTypeApplicationBundle = [UTType typeWithIdentifier:@"com.apple.application-bundle"];
        UTTypeExecutable = [UTType typeWithIdentifier:@"public.executable"];
        UTTypeUnixExecutable = [UTType typeWithIdentifier:@"public.unix-executable"];
        UTTypeSymbolicLink = [UTType typeWithIdentifier:@"public.symlink"];
        UTTypeAliasFile = [UTType typeWithIdentifier:@"com.apple.alias-file"];
        UTTypeURL = [UTType typeWithIdentifier:@"public.url"];
        UTTypeFileURL = [UTType typeWithIdentifier:@"public.file-url"];
        UTTypeText = [UTType typeWithIdentifier:@"public.text"];
        UTTypePlainText = [UTType typeWithIdentifier:@"public.plain-text"];
        UTTypeUTF8PlainText = [UTType typeWithIdentifier:@"public.utf8-plain-text"];
        UTTypeRTF = [UTType typeWithIdentifier:@"public.rtf"];
        UTTypeHTML = [UTType typeWithIdentifier:@"public.html"];
        UTTypeXML = [UTType typeWithIdentifier:@"public.xml"];
        UTTypeJSON = [UTType typeWithIdentifier:@"public.json"];
        UTTypeSourceCode = [UTType typeWithIdentifier:@"public.source-code"];
        UTTypeCSource = [UTType typeWithIdentifier:@"public.c-source"];
        UTTypeObjectiveCSource = [UTType typeWithIdentifier:@"public.objective-c-source"];
        UTTypeSwiftSource = [UTType typeWithIdentifier:@"public.swift-source"];
        UTTypeScript = [UTType typeWithIdentifier:@"public.script"];
        UTTypeShellScript = [UTType typeWithIdentifier:@"public.shell-script"];
        UTTypePythonScript = [UTType typeWithIdentifier:@"public.python-script"];
        UTTypePropertyList = [UTType typeWithIdentifier:@"com.apple.property-list"];
        UTTypeImage = [UTType typeWithIdentifier:@"public.image"];
        UTTypePNG = [UTType typeWithIdentifier:@"public.png"];
        UTTypeJPEG = [UTType typeWithIdentifier:@"public.jpeg"];
        UTTypeGIF = [UTType typeWithIdentifier:@"com.compuserve.gif"];
        UTTypeTIFF = [UTType typeWithIdentifier:@"public.tiff"];
        UTTypeBMP = [UTType typeWithIdentifier:@"com.microsoft.bmp"];
        UTTypeICNS = [UTType typeWithIdentifier:@"com.apple.icns"];
        UTTypeSVG = [UTType typeWithIdentifier:@"public.svg-image"];
        UTTypePDF = [UTType typeWithIdentifier:@"com.adobe.pdf"];
        UTTypeAudio = [UTType typeWithIdentifier:@"public.audio"];
        UTTypeMovie = [UTType typeWithIdentifier:@"public.movie"];
        UTTypeVideo = [UTType typeWithIdentifier:@"public.video"];
        UTTypeArchive = [UTType typeWithIdentifier:@"public.archive"];
        UTTypeZIP = [UTType typeWithIdentifier:@"public.zip-archive"];
        UTTypeGZIP = [UTType typeWithIdentifier:@"org.gnu.gnu-zip-archive"];
        UTTypeDiskImage = [UTType typeWithIdentifier:@"public.disk-image"];
        UTTypeFont = [UTType typeWithIdentifier:@"public.font"];
        UTTypeLog = [UTType typeWithIdentifier:@"public.log"];
    }
}
