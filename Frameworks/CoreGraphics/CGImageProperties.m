/* Copyright (c) 2008 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/NSString.h>

const CFStringRef kCGImagePropertyDPIWidth=(CFStringRef)@"DPIWidth";
const CFStringRef kCGImagePropertyDPIHeight=(CFStringRef)@"DPIHeight";
const CFStringRef kCGImagePropertyPixelHeight=(CFStringRef)@"PixelHeight";
const CFStringRef kCGImagePropertyPixelWidth=(CFStringRef)@"PixelWidth";
const CFStringRef kCGImagePropertyOrientation=(CFStringRef)@"Orientation";

const CFStringRef kCGImagePropertyTIFFDictionary=(CFStringRef)@"{TIFF}";
const CFStringRef kCGImagePropertyExifDictionary=(CFStringRef)@"{Exif}";

const CFStringRef kCGImagePropertyTIFFXResolution=(CFStringRef)@"XResolution";
const CFStringRef kCGImagePropertyTIFFYResolution=(CFStringRef)@"YResolution";
const CFStringRef kCGImagePropertyTIFFOrientation=(CFStringRef)@"Orientation";

const CFStringRef kCGImagePropertyFileSize=(CFStringRef)@"FileSize";
const CFStringRef kCGImagePropertyDepth=(CFStringRef)@"Depth";
const CFStringRef kCGImagePropertyHasAlpha=(CFStringRef)@"HasAlpha";
const CFStringRef kCGImagePropertyIsFloat=(CFStringRef)@"IsFloat";
const CFStringRef kCGImagePropertyIsIndexed=(CFStringRef)@"IsIndexed";
const CFStringRef kCGImagePropertyColorModel=(CFStringRef)@"ColorModel";
const CFStringRef kCGImagePropertyProfileName=(CFStringRef)@"ProfileName";
const CFStringRef kCGImagePropertyColorModelRGB=(CFStringRef)@"RGB";
const CFStringRef kCGImagePropertyColorModelGray=(CFStringRef)@"Gray";
const CFStringRef kCGImagePropertyColorModelCMYK=(CFStringRef)@"CMYK";
const CFStringRef kCGImagePropertyColorModelLab=(CFStringRef)@"Lab";
const CFStringRef kCGImagePropertyPNGDictionary=(CFStringRef)@"{PNG}";
const CFStringRef kCGImagePropertyGIFDictionary=(CFStringRef)@"{GIF}";
const CFStringRef kCGImagePropertyJFIFDictionary=(CFStringRef)@"{JFIF}";
const CFStringRef kCGImagePropertyIPTCDictionary=(CFStringRef)@"{IPTC}";
const CFStringRef kCGImagePropertyGPSDictionary=(CFStringRef)@"{GPS}";
const CFStringRef kCGImagePropertyExifAuxDictionary=(CFStringRef)@"{ExifAux}";
const CFStringRef kCGImagePropertyMakerAppleDictionary=(CFStringRef)@"{MakerApple}";
const CFStringRef kCGImagePropertyPNGGamma=(CFStringRef)@"Gamma";
const CFStringRef kCGImagePropertyPNGInterlaceType=(CFStringRef)@"InterlaceType";
const CFStringRef kCGImagePropertyGIFLoopCount=(CFStringRef)@"LoopCount";
const CFStringRef kCGImagePropertyGIFDelayTime=(CFStringRef)@"DelayTime";
const CFStringRef kCGImagePropertyGIFUnclampedDelayTime=(CFStringRef)@"UnclampedDelayTime";
const CFStringRef kCGImagePropertyGIFHasGlobalColorMap=(CFStringRef)@"HasGlobalColorMap";
const CFStringRef kCGImagePropertyExifDateTimeOriginal=(CFStringRef)@"DateTimeOriginal";
const CFStringRef kCGImagePropertyExifPixelXDimension=(CFStringRef)@"PixelXDimension";
const CFStringRef kCGImagePropertyExifPixelYDimension=(CFStringRef)@"PixelYDimension";
const CFStringRef kCGImagePropertyTIFFMake=(CFStringRef)@"Make";
const CFStringRef kCGImagePropertyTIFFModel=(CFStringRef)@"Model";
const CFStringRef kCGImagePropertyTIFFDateTime=(CFStringRef)@"DateTime";
const CFStringRef kCGImagePropertyTIFFSoftware=(CFStringRef)@"Software";
const CFStringRef kCGImagePropertyTIFFCompression=(CFStringRef)@"Compression";
