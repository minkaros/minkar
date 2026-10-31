/* Copyright (c) 2009 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

enum {
    // Files.
    NSFileNoSuchFileError = 4,
    NSFileLockingError = 255,
    NSFileReadUnknownError = 256,
    NSFileReadNoPermissionError = 257,
    NSFileReadInvalidFileNameError = 258,
    NSFileReadCorruptFileError = 259,
    NSFileReadNoSuchFileError = 260,
    NSFileReadInapplicableStringEncodingError = 261,
    NSFileReadUnsupportedSchemeError = 262,
    NSFileReadTooLargeError = 263,
    NSFileReadUnknownStringEncodingError = 264,
    NSFileWriteUnknownError = 512,
    NSFileWriteNoPermissionError = 513,
    NSFileWriteInvalidFileNameError = 514,
    NSFileWriteFileExistsError = 516,
    NSFileWriteInapplicableStringEncodingError = 517,
    NSFileWriteUnsupportedSchemeError = 518,
    NSFileWriteOutOfSpaceError = 640,
    NSFileWriteVolumeReadOnlyError = 642,
    NSFileManagerUnmountUnknownError = 768,
    NSFileManagerUnmountBusyError = 769,
    NSFileErrorMinimum = 0,
    NSFileErrorMaximum = 1023,

    NSKeyValueValidationError = 1024,
    NSValidationErrorMinimum = 1024,
    NSValidationErrorMaximum = 2047,

    NSUserCancelledError = 3072,
    NSFeatureUnsupportedError = 3328,

    // Bundles' executables.
    NSExecutableNotLoadableError = 3584,
    NSExecutableArchitectureMismatchError = 3585,
    NSExecutableRuntimeMismatchError = 3586,
    NSExecutableLoadError = 3587,
    NSExecutableLinkError = 3588,
    NSExecutableErrorMinimum = 3584,
    NSExecutableErrorMaximum = 3839,

    // NSXPCConnection.
    NSXPCConnectionInterrupted = 4097,
    NSXPCConnectionInvalid = 4099,
    NSXPCConnectionReplyInvalid = 4101,
    NSXPCConnectionErrorMinimum = 4096,
    NSXPCConnectionErrorMaximum = 4224,

    // A string that could not be parsed or formatted (an invalid regular
    // expression, for one).
    NSFormattingError = 2048,
    NSFormattingErrorMinimum = 2048,
    NSFormattingErrorMaximum = 2559,

    // Property lists, and JSON (NSJSONSerialization).
    NSPropertyListReadCorruptError = 3840,
    NSPropertyListReadUnknownVersionError = 3841,
    NSPropertyListReadStreamError = 3842,
    NSPropertyListWriteStreamError = 3851,
    NSPropertyListWriteInvalidError = 3852,
    NSPropertyListErrorMinimum = 3840,
    NSPropertyListErrorMaximum = 4095,

    // Keyed archives (NSKeyedArchiver, NSKeyedUnarchiver).
    NSCoderReadCorruptError = 4864,
    NSCoderValueNotFoundError = 4865,
    NSCoderInvalidValueError = 4866,
    NSCoderErrorMinimum = 4864,
    NSCoderErrorMaximum = 4991,
};
