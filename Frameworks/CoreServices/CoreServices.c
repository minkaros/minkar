#include "CoreServices.h"

/* Metadata attribute names: the strings NSMetadataItem keeps them under. */
const CFStringRef kMDItemPath = CFSTR("kMDItemPath");
const CFStringRef kMDItemWhereFroms = CFSTR("kMDItemWhereFroms");
const CFStringRef kMDItemFSName = CFSTR("kMDItemFSName");
const CFStringRef kMDItemDisplayName = CFSTR("kMDItemDisplayName");
const CFStringRef kMDItemFSSize = CFSTR("kMDItemFSSize");
const CFStringRef kMDItemFSCreationDate = CFSTR("kMDItemFSCreationDate");
const CFStringRef kMDItemFSContentChangeDate = CFSTR("kMDItemFSContentChangeDate");
const CFStringRef kMDItemContentModificationDate = CFSTR("kMDItemContentModificationDate");
const CFStringRef kMDItemContentType = CFSTR("kMDItemContentType");
const CFStringRef kMDItemContentTypeTree = CFSTR("kMDItemContentTypeTree");
const CFStringRef kMDItemKind = CFSTR("kMDItemKind");
const CFStringRef kMDItemContentCreationDate = CFSTR("kMDItemContentCreationDate");
const CFStringRef kMDItemLastUsedDate = CFSTR("kMDItemLastUsedDate");
const CFStringRef kMDItemFSOwnerUserID = CFSTR("kMDItemFSOwnerUserID");
const CFStringRef kMDItemFSOwnerGroupID = CFSTR("kMDItemFSOwnerGroupID");
const CFStringRef kMDItemFSIsExtensionHidden = CFSTR("kMDItemFSIsExtensionHidden");
const CFStringRef kMDItemFSInvisible = CFSTR("kMDItemFSInvisible");
const CFStringRef kMDItemCFBundleIdentifier = CFSTR("kMDItemCFBundleIdentifier");
const CFStringRef kMDItemVersion = CFSTR("kMDItemVersion");
const CFStringRef kMDItemLogicalSize = CFSTR("kMDItemLogicalSize");
const CFStringRef kMDItemPhysicalSize = CFSTR("kMDItemPhysicalSize");
const CFStringRef kMDItemDateAdded = CFSTR("kMDItemDateAdded");
const CFStringRef kMDItemFSNodeCount = CFSTR("kMDItemFSNodeCount");
const CFStringRef kMDItemFSLabel = CFSTR("kMDItemFSLabel");
const CFStringRef kMDItemFSFinderFlags = CFSTR("kMDItemFSFinderFlags");
const CFStringRef kMDItemFSCreatorCode = CFSTR("kMDItemFSCreatorCode");
const CFStringRef kMDItemFSTypeCode = CFSTR("kMDItemFSTypeCode");
const CFStringRef kMDItemExecutableArchitectures = CFSTR("kMDItemExecutableArchitectures");
const CFStringRef kMDItemCopyright = CFSTR("kMDItemCopyright");
const CFStringRef kMDItemAlternateNames = CFSTR("kMDItemAlternateNames");
const CFStringRef kMDItemDocumentIdentifier = CFSTR("kMDItemDocumentIdentifier");
