/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The keys of a print settings object (PMPrintSettingsGetValue), and of
 * the print-job dictionaries made from it.
 */

#ifndef PRINTCORE_PMPRINTSETTINGSKEYS_H
#define PRINTCORE_PMPRINTSETTINGSKEYS_H

#include <CoreFoundation/CoreFoundation.h>

#define kPMCopiesStr                    "com.apple.print.PrintSettings.PMCopies"
#define kPMCopiesKey                    CFSTR(kPMCopiesStr)
#define kPMCopyCollateStr               "com.apple.print.PrintSettings.PMCopyCollate"
#define kPMCopyCollateKey               CFSTR(kPMCopyCollateStr)
#define kPMOutputOrderStr               "OutputOrder"
#define kPMOutputOrderKey               CFSTR(kPMOutputOrderStr)
#define kPMPageSetStr                   "page-set"
#define kPMPageSetKey                   CFSTR(kPMPageSetStr)
#define kPMMirrorStr                    "mirror"
#define kPMMirrorKey                    CFSTR(kPMMirrorStr)
#define kPMPrintSelectionOnlyStr        "com.apple.print.PrintSettings.PMPrintSelectionOnly"
#define kPMPrintSelectionOnlyKey        CFSTR(kPMPrintSelectionOnlyStr)
#define kPMBorderStr                    "com.apple.print.PrintSettings.PMBorder"
#define kPMBorderKey                    CFSTR(kPMBorderStr)
#define kPMBorderTypeStr                "com.apple.print.PrintSettings.PMBorderType"
#define kPMBorderTypeKey                CFSTR(kPMBorderTypeStr)
#define kPMLayoutNUpStr                 "com.apple.print.PrintSettings.PMLayoutNUp"
#define kPMLayoutNUpKey                 CFSTR(kPMLayoutNUpStr)
#define kPMLayoutRowsStr                "com.apple.print.PrintSettings.PMLayoutRows"
#define kPMLayoutRowsKey                CFSTR(kPMLayoutRowsStr)
#define kPMLayoutColumnsStr             "com.apple.print.PrintSettings.PMLayoutColumns"
#define kPMLayoutColumnsKey             CFSTR(kPMLayoutColumnsStr)
#define kPMLayoutDirectionStr           "com.apple.print.PrintSettings.PMLayoutDirection"
#define kPMLayoutDirectionKey           CFSTR(kPMLayoutDirectionStr)
#define kPMLayoutTileOrientationStr     "com.apple.print.PrintSettings.PMLayoutTileOrientation"
#define kPMLayoutTileOrientationKey     CFSTR(kPMLayoutTileOrientationStr)
#define kPMJobStateStr                  "com.apple.print.PrintSettings.PMJobState"
#define kPMJobStateKey                  CFSTR(kPMJobStateStr)
#define kPMJobHoldUntilTimeStr          "com.apple.print.PrintSettings.PMJobHoldUntilTime"
#define kPMJobHoldUntilTimeKey          CFSTR(kPMJobHoldUntilTimeStr)
#define kPMJobPriorityStr               "com.apple.print.PrintSettings.PMJobPriority"
#define kPMJobPriorityKey               CFSTR(kPMJobPriorityStr)
#define kPMDuplexingStr                 "com.apple.print.PrintSettings.PMDuplexing"
#define kPMDuplexingKey                 CFSTR(kPMDuplexingStr)
#define kPMColorSyncProfileIDStr        "com.apple.print.PrintSettings.PMColorSyncProfileID"
#define kPMColorSyncProfileIDKey        CFSTR(kPMColorSyncProfileIDStr)
#define kPMPrimaryPaperFeedStr          "com.apple.print.PrintSettings.PMPrimaryPaperFeed"
#define kPMPrimaryPaperFeedKey          CFSTR(kPMPrimaryPaperFeedStr)
#define kPMSecondaryPaperFeedStr        "com.apple.print.PrintSettings.PMSecondaryPaperFeed"
#define kPMSecondaryPaperFeedKey        CFSTR(kPMSecondaryPaperFeedStr)
#define kPMPSErrorHandlerStr            "com.apple.print.PrintSettings.PMPSErrorHandler"
#define kPMPSErrorHandlerKey            CFSTR(kPMPSErrorHandlerStr)
#define kPMPSTraySwitchStr              "com.apple.print.PrintSettings.PMPSTraySwitch"
#define kPMPSTraySwitchKey              CFSTR(kPMPSTraySwitchStr)
#define kPMTotalBeginPagesStr           "com.apple.print.PrintSettings.PMTotalBeginPages"
#define kPMTotalBeginPagesKey           CFSTR(kPMTotalBeginPagesStr)
#define kPMTotalSidesImagedStr          "com.apple.print.PrintSettings.PMTotalSidesImaged"
#define kPMTotalSidesImagedKey          CFSTR(kPMTotalSidesImagedStr)
#define kPMFaxNumberStr                 "phone"
#define kPMFaxNumberKey                 CFSTR(kPMFaxNumberStr)
#define kPMFaxToStr                     "faxTo"
#define kPMFaxToKey                     CFSTR(kPMFaxToStr)
#define kPMFaxPrefixStr                 "faxPrefix"
#define kPMFaxPrefixKey                 CFSTR(kPMFaxPrefixStr)
#define kPMFaxSubjectStr                "faxSubject"
#define kPMFaxSubjectKey                CFSTR(kPMFaxSubjectStr)
#define kPMFaxCoverSheetStr             "cover-sheet"
#define kPMFaxCoverSheetKey             CFSTR(kPMFaxCoverSheetStr)
#define kPMFaxCoverSheetMessageStr      "faxCoverSheetMessage"
#define kPMFaxCoverSheetMessageKey      CFSTR(kPMFaxCoverSheetMessageStr)
#define kPMFaxToneDialingStr            "faxToneDialing"
#define kPMFaxToneDialingKey            CFSTR(kPMFaxToneDialingStr)
#define kPMFaxUseSoundStr               "faxUseSound"
#define kPMFaxUseSoundKey               CFSTR(kPMFaxUseSoundStr)
#define kPMFaxWaitForDialToneStr        "faxWaitForDialTone"
#define kPMFaxWaitForDialToneKey        CFSTR(kPMFaxWaitForDialToneStr)
#define kPMFaxToLabelStr                "faxToLabel"
#define kPMFaxToLabelKey                CFSTR(kPMFaxToLabelStr)
#define kPMFaxFromLabelStr              "faxFromLabel"
#define kPMFaxFromLabelKey              CFSTR(kPMFaxFromLabelStr)
#define kPMFaxDateLabelStr              "faxDateLabel"
#define kPMFaxDateLabelKey              CFSTR(kPMFaxDateLabelStr)
#define kPMFaxSubjectLabelStr           "faxSubjectLabel"
#define kPMFaxSubjectLabelKey           CFSTR(kPMFaxSubjectLabelStr)
#define kPMFaxSheetsLabelStr            "faxSheetsLabel"
#define kPMFaxSheetsLabelKey            CFSTR(kPMFaxSheetsLabelStr)
#define kPMCoverPageStr                 "com.apple.print.PrintSettings.PMCoverPage"
#define kPMCoverPageKey                 CFSTR(kPMCoverPageStr)
#define kPMCoverPageSourceStr           "com.apple.print.PrintSettings.PMCoverPageSource"
#define kPMCoverPageSourceKey           CFSTR(kPMCoverPageSourceStr)
#define kPMDestinationPrinterIDStr      "DestinationPrinterID"
#define kPMDestinationPrinterIDKey      CFSTR(kPMDestinationPrinterIDStr)
#define kPMInlineWorkflowStr            "inlineWorkflow"
#define kPMInlineWorkflowKey            CFSTR(kPMInlineWorkflowStr)
#define kPMPageToPaperMappingTypeStr    "com.apple.print.PageToPaperMappingType"
#define kPMPageToPaperMappingTypeKey    CFSTR(kPMPageToPaperMappingTypeStr)
#define kPMPageToPaperMediaNameStr      "com.apple.print.PageToPaperMappingMediaName"
#define kPMPageToPaperMediaNameKey      CFSTR(kPMPageToPaperMediaNameStr)
#define kPMPageToPaperMappingAllowScalingUpStr "com.apple.print.PageToPaperMappingAllowScalingUp"
#define kPMPageToPaperMappingAllowScalingUpKey CFSTR(kPMPageToPaperMappingAllowScalingUpStr)
#define kPMCustomProfilePathStr         "PMCustomProfilePath"
#define kPMCustomProfilePathKey         CFSTR(kPMCustomProfilePathStr)
#define kPMVendorColorMatchingStr       "AP_VendorColorMatching"
#define kPMVendorColorMatching          CFSTR(kPMVendorColorMatchingStr)
#define kPMApplicationColorMatchingStr  "AP_ApplicationColorMatching"
#define kPMApplicationColorMatching     CFSTR(kPMApplicationColorMatchingStr)
#define kPMColorMatchingModeStr         "AP_ColorMatchingMode"
#define kPMColorMatchingModeKey         CFSTR(kPMColorMatchingModeStr)
#define kPMDesiredFormatStr             "com.apple.print.PrintSettings.PMDesiredFormat"
#define kPMDesiredFormatKey             CFSTR(kPMDesiredFormatStr)
#define kPMDestinationTypeStr           "com.apple.print.PrintSettings.PMDestinationType"
#define kPMDestinationTypeKey           CFSTR(kPMDestinationTypeStr)
#define kPMOutputFilenameStr            "com.apple.print.PrintSettings.PMOutputFilename"
#define kPMOutputFilenameKey            CFSTR(kPMOutputFilenameStr)

#endif
