/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The public constants of CFOpenDirectory (record types, attribute types,
 * authentication types...), as constants.c defines them: the published
 * sources have their definitions, not this header. Generated from
 * constants.c; the types and codes after it.
 */

#ifndef __CFOPENDIRECTORY_CONSTANTS_H
#define __CFOPENDIRECTORY_CONSTANTS_H

#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN

/* Strings: NSString in Objective-C (what apps pass), CFStringRef in C;
   the same pointer either way. */
#if defined(__OBJC__)
@class NSString;
typedef NSString *ODRecordType;
typedef NSString *ODAttributeType;
typedef NSString *ODAuthenticationType;
typedef NSString *ODPolicyType;
#else
typedef CFStringRef ODRecordType;
typedef CFStringRef ODAttributeType;
typedef CFStringRef ODAuthenticationType;
typedef CFStringRef ODPolicyType;
#endif

/* Node types and match types: the values of DirectoryService's
   tDirPatternMatch (DirServicesTypes.h), which the reference system takes
   too (observed on it, 22 Nov 2026: ODNodeCreateWithNodeType's nodes and
   ODQuery's results for each value). */
typedef uint32_t ODNodeType;
enum {
	kODNodeTypeAuthentication	= 0x2201,
	kODNodeTypeContacts		= 0x2204,
	kODNodeTypeNetwork		= 0x2205,
	kODNodeTypeLocalNodes		= 0x2200,
	kODNodeTypeConfigure		= 0x2202,
};

typedef uint32_t ODMatchType;
enum {
	kODMatchAny			= 0x0001,
	kODMatchEqualTo			= 0x2001,
	kODMatchBeginsWith		= 0x2002,
	kODMatchEndsWith		= 0x2003,
	kODMatchContains		= 0x2004,
	kODMatchGreaterThan		= 0x2005,
	kODMatchLessThan		= 0x2006,
};

/* The framework's error codes, observed on the reference system (22 Nov
   2026): the codes ODConvertToLegacyErrorCode converts, matched to their
   DirectoryService codes through errors.c, and the codes its calls give
   back (an unknown node type, a NULL record name, a query synchronized).
   The ones marked "by sequence" were not seen; they take the free value
   of their group. */
enum ODFrameworkErrors {
	kODErrorSuccess					= 0,

	kODErrorSessionLocalOnlyDaemonInUse		= 1000,
	kODErrorSessionNormalDaemonInUse		= 1001,
	kODErrorSessionDaemonNotRunning			= 1002,
	kODErrorSessionDaemonRefused			= 1003,
	kODErrorSessionProxyCommunicationError		= 1100,
	kODErrorSessionProxyVersionMismatch		= 1101,
	kODErrorSessionProxyIPUnreachable		= 1102,
	kODErrorSessionProxyUnknownHost			= 1103,

	kODErrorNodeUnknownName				= 2000,
	kODErrorNodeUnknownType				= 2001,
	kODErrorNodeDisabled				= 2002,
	kODErrorNodeConnectionFailed			= 2100,
	kODErrorNodeUnknownHost				= 2200,

	kODErrorQuerySynchronize			= 3000,
	kODErrorQueryInvalidMatchType			= 3100,
	kODErrorQueryUnsupportedMatchType		= 3101,
	kODErrorQueryTimeout				= 3102,	/* by sequence */

	kODErrorRecordReadOnlyNode			= 4000,
	kODErrorRecordPermissionError			= 4001,
	kODErrorRecordParameterError			= 4100,
	kODErrorRecordInvalidType			= 4101,
	kODErrorRecordAlreadyExists			= 4102,
	kODErrorRecordTypeDisabled			= 4103,
	kODErrorRecordNoLongerExists			= 4104,
	kODErrorRecordAttributeUnknownType		= 4200,
	kODErrorRecordAttributeNotFound			= 4201,
	kODErrorRecordAttributeValueSchemaError		= 4202,
	kODErrorRecordAttributeValueNotFound		= 4203,

	kODErrorCredentialsInvalid			= 5000,
	kODErrorCredentialsMethodNotSupported		= 5100,
	kODErrorCredentialsNotAuthorized		= 5101,
	kODErrorCredentialsParameterError		= 5102,
	kODErrorCredentialsOperationFailed		= 5103,
	kODErrorCredentialsServerUnreachable		= 5200,
	kODErrorCredentialsServerNotFound		= 5201,
	kODErrorCredentialsServerError			= 5202,
	kODErrorCredentialsServerTimeout		= 5203,
	kODErrorCredentialsContactMaster		= 5204,
	kODErrorCredentialsServerCommunicationError	= 5205,
	kODErrorCredentialsAccountNotFound		= 5300,
	kODErrorCredentialsAccountDisabled		= 5301,
	kODErrorCredentialsAccountExpired		= 5302,
	kODErrorCredentialsAccountInactive		= 5303,
	kODErrorCredentialsPasswordExpired		= 5400,
	kODErrorCredentialsPasswordChangeRequired	= 5401,
	kODErrorCredentialsPasswordQualityFailed	= 5402,
	kODErrorCredentialsPasswordTooShort		= 5403,
	kODErrorCredentialsPasswordTooLong		= 5404,
	kODErrorCredentialsPasswordNeedsLetter		= 5405,
	kODErrorCredentialsPasswordNeedsDigit		= 5406,
	kODErrorCredentialsPasswordChangeTooSoon	= 5407,
	kODErrorCredentialsPasswordUnrecoverable	= 5408,
	kODErrorCredentialsInvalidLogonHours		= 5500,
	kODErrorCredentialsInvalidComputer		= 5501,

	kODErrorPluginOperationNotSupported		= 10000,
	kODErrorPluginError				= 10001,	/* by sequence */
	kODErrorDaemonError				= 10002,
};

/* The framework's own codes, between it and Minkar's opendirectoryd (no
   app sees them as such: the reference has its own, unpublished). */
enum {
	kODErrorSessionInvalid				= 10100,
	kODErrorSessionFailed				= 10101,
	kODErrorNodeInvalid				= 10102,
	kODErrorQueryInvalid				= 10103,
	kODErrorContextInvalid				= 10104,
};

CF_EXPORT const CFStringRef kODErrorDomainFramework;

CF_EXPORT const ODRecordType kODRecordTypeAttributeTypes;
CF_EXPORT const ODRecordType kODRecordTypeAFPServer;
CF_EXPORT const ODRecordType kODRecordTypeAliases;
CF_EXPORT const ODRecordType kODRecordTypeAugments;
CF_EXPORT const ODRecordType kODRecordTypeAutomount;
CF_EXPORT const ODRecordType kODRecordTypeAutomountMap;
CF_EXPORT const ODRecordType kODRecordTypeAutoServerSetup;
CF_EXPORT const ODRecordType kODRecordTypeBootp;
CF_EXPORT const ODRecordType kODRecordTypeCertificateAuthorities;
CF_EXPORT const ODRecordType kODRecordTypeComputerLists;
CF_EXPORT const ODRecordType kODRecordTypeComputerGroups;
CF_EXPORT const ODRecordType kODRecordTypeComputers;
CF_EXPORT const ODRecordType kODRecordTypeConfiguration;
CF_EXPORT const ODRecordType kODRecordTypeEthernets;
CF_EXPORT const ODRecordType kODRecordTypeFileMakerServers;
CF_EXPORT const ODRecordType kODRecordTypeFTPServer;
CF_EXPORT const ODRecordType kODRecordTypeGroups;
CF_EXPORT const ODRecordType kODRecordTypeHostServices;
CF_EXPORT const ODRecordType kODRecordTypeHosts;
CF_EXPORT const ODRecordType kODRecordTypeLDAPServer;
CF_EXPORT const ODRecordType kODRecordTypeLocations;
CF_EXPORT const ODRecordType kODRecordTypeMounts;
CF_EXPORT const ODRecordType kODRecordTypeNFS;
CF_EXPORT const ODRecordType kODRecordTypeNetDomains;
CF_EXPORT const ODRecordType kODRecordTypeNetGroups;
CF_EXPORT const ODRecordType kODRecordTypeNetworks;
CF_EXPORT const ODRecordType kODRecordTypePasswordServer;
CF_EXPORT const ODRecordType kODRecordTypePeople;
CF_EXPORT const ODRecordType kODRecordTypePlugins;
CF_EXPORT const ODRecordType kODRecordTypePresetComputers;
CF_EXPORT const ODRecordType kODRecordTypePresetComputerGroups;
CF_EXPORT const ODRecordType kODRecordTypePresetComputerLists;
CF_EXPORT const ODRecordType kODRecordTypePresetGroups;
CF_EXPORT const ODRecordType kODRecordTypePresetUsers;
CF_EXPORT const ODRecordType kODRecordTypePrintService;
CF_EXPORT const ODRecordType kODRecordTypePrintServiceUser;
CF_EXPORT const ODRecordType kODRecordTypePrinters;
CF_EXPORT const ODRecordType kODRecordTypeProtocols;
CF_EXPORT const ODRecordType kODRecordTypeQTSServer;
CF_EXPORT const ODRecordType kODRecordTypeRecordTypes;
CF_EXPORT const ODRecordType kODRecordTypeResources;
CF_EXPORT const ODRecordType kODRecordTypeRPC;
CF_EXPORT const ODRecordType kODRecordTypeSMBServer;
CF_EXPORT const ODRecordType kODRecordTypeServer;
CF_EXPORT const ODRecordType kODRecordTypeServices;
CF_EXPORT const ODRecordType kODRecordTypeSharePoints;
CF_EXPORT const ODRecordType kODRecordTypeUsers;
CF_EXPORT const ODRecordType kODRecordTypeWebServer;
CF_EXPORT const ODAttributeType kODAttributeTypeAllAttributes;
CF_EXPORT const ODAttributeType kODAttributeTypeStandardOnly;
CF_EXPORT const ODAttributeType kODAttributeTypeNativeOnly;
CF_EXPORT const ODAttributeType kODAttributeTypeMetaAmbiguousName;
CF_EXPORT const ODAttributeType kODAttributeTypeMetaAugmentedAttributes;
CF_EXPORT const ODAttributeType kODAttributeTypeMetaRecordName;
CF_EXPORT const ODAttributeType kODAttributeTypeAdminLimits;
CF_EXPORT const ODAttributeType kODAttributeTypeAuthenticationHint;
CF_EXPORT const ODAttributeType kODAttributeTypeAllTypes;
CF_EXPORT const ODAttributeType kODAttributeTypeAuthorityRevocationList;
CF_EXPORT const ODAttributeType kODAttributeTypeBirthday;
CF_EXPORT const ODAttributeType kODAttributeTypeCACertificate;
CF_EXPORT const ODAttributeType kODAttributeTypeCapacity;
CF_EXPORT const ODAttributeType kODAttributeTypeCategory;
CF_EXPORT const ODAttributeType kODAttributeTypeCertificateRevocationList;
CF_EXPORT const ODAttributeType kODAttributeTypeComment;
CF_EXPORT const ODAttributeType kODAttributeTypeContactGUID;
CF_EXPORT const ODAttributeType kODAttributeTypeContactPerson;
CF_EXPORT const ODAttributeType kODAttributeTypeCreationTimestamp;
CF_EXPORT const ODAttributeType kODAttributeTypeCrossCertificatePair;
CF_EXPORT const ODAttributeType kODAttributeTypeDataStamp;
CF_EXPORT const ODAttributeType kODAttributeTypeFullName;
CF_EXPORT const ODAttributeType kODAttributeTypeDNSDomain;
CF_EXPORT const ODAttributeType kODAttributeTypeDNSNameServer;
CF_EXPORT const ODAttributeType kODAttributeTypeENetAddress;
CF_EXPORT const ODAttributeType kODAttributeTypeExpire;
CF_EXPORT const ODAttributeType kODAttributeTypeFirstName;
CF_EXPORT const ODAttributeType kODAttributeTypeGUID;
CF_EXPORT const ODAttributeType kODAttributeTypeHomeDirectoryQuota;
CF_EXPORT const ODAttributeType kODAttributeTypeHomeDirectorySoftQuota;
CF_EXPORT const ODAttributeType kODAttributeTypeHomeLocOwner;
CF_EXPORT const ODAttributeType kODAttributeTypeInternetAlias;
CF_EXPORT const ODAttributeType kODAttributeTypeKDCConfigData;
CF_EXPORT const ODAttributeType kODAttributeTypeLastName;
CF_EXPORT const ODAttributeType kODAttributeTypeLDAPSearchBaseSuffix;
CF_EXPORT const ODAttributeType kODAttributeTypeLocation;
CF_EXPORT const ODAttributeType kODAttributeTypeMapGUID;
CF_EXPORT const ODAttributeType kODAttributeTypeMCXFlags;
CF_EXPORT const ODAttributeType kODAttributeTypeMCXSettings;
CF_EXPORT const ODAttributeType kODAttributeTypeMailAttribute;
CF_EXPORT const ODAttributeType kODAttributeTypeMetaAutomountMap;
CF_EXPORT const ODAttributeType kODAttributeTypeMiddleName;
CF_EXPORT const ODAttributeType kODAttributeTypeModificationTimestamp;
CF_EXPORT const ODAttributeType kODAttributeTypeNFSHomeDirectory;
CF_EXPORT const ODAttributeType kODAttributeTypeNote;
CF_EXPORT const ODAttributeType kODAttributeTypeOwner;
CF_EXPORT const ODAttributeType kODAttributeTypeOwnerGUID;
CF_EXPORT const ODAttributeType kODAttributeTypePassword;
CF_EXPORT const ODAttributeType kODAttributeTypePasswordPlus;
CF_EXPORT const ODAttributeType kODAttributeTypePasswordPolicyOptions;
CF_EXPORT const ODAttributeType kODAttributeTypePasswordServerList;
CF_EXPORT const ODAttributeType kODAttributeTypePasswordServerLocation;
CF_EXPORT const ODAttributeType kODAttributeTypePicture;
CF_EXPORT const ODAttributeType kODAttributeTypePort;
CF_EXPORT const ODAttributeType kODAttributeTypePresetUserIsAdmin;
CF_EXPORT const ODAttributeType kODAttributeTypePrimaryComputerGUID;
CF_EXPORT const ODAttributeType kODAttributeTypePrimaryComputerList;
CF_EXPORT const ODAttributeType kODAttributeTypePrimaryGroupID;
CF_EXPORT const ODAttributeType kODAttributeTypePrinter1284DeviceID;
CF_EXPORT const ODAttributeType kODAttributeTypePrinterLPRHost;
CF_EXPORT const ODAttributeType kODAttributeTypePrinterLPRQueue;
CF_EXPORT const ODAttributeType kODAttributeTypePrinterMakeAndModel;
CF_EXPORT const ODAttributeType kODAttributeTypePrinterType;
CF_EXPORT const ODAttributeType kODAttributeTypePrinterURI;
CF_EXPORT const ODAttributeType kODAttributeTypePrinterXRISupported;
CF_EXPORT const ODAttributeType kODAttributeTypePrintServiceInfoText;
CF_EXPORT const ODAttributeType kODAttributeTypePrintServiceInfoXML;
CF_EXPORT const ODAttributeType kODAttributeTypePrintServiceUserData;
CF_EXPORT const ODAttributeType kODAttributeTypeRealUserID;
CF_EXPORT const ODAttributeType kODAttributeTypeRelativeDNPrefix;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBAcctFlags;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBGroupRID;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBHome;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBHomeDrive;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBKickoffTime;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBLogoffTime;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBLogonTime;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBPrimaryGroupSID;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBPWDLastSet;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBProfilePath;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBRID;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBScriptPath;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBSID;
CF_EXPORT const ODAttributeType kODAttributeTypeSMBUserWorkstations;
CF_EXPORT const ODAttributeType kODAttributeTypeServiceType;
CF_EXPORT const ODAttributeType kODAttributeTypeSetupAdvertising;
CF_EXPORT const ODAttributeType kODAttributeTypeSetupAutoRegister;
CF_EXPORT const ODAttributeType kODAttributeTypeSetupLocation;
CF_EXPORT const ODAttributeType kODAttributeTypeSetupOccupation;
CF_EXPORT const ODAttributeType kODAttributeTypeTimeToLive;
CF_EXPORT const ODAttributeType kODAttributeTypeTrustInformation;
CF_EXPORT const ODAttributeType kODAttributeTypeUniqueID;
CF_EXPORT const ODAttributeType kODAttributeTypeUserCertificate;
CF_EXPORT const ODAttributeType kODAttributeTypeUserPKCS12Data;
CF_EXPORT const ODAttributeType kODAttributeTypeUserShell;
CF_EXPORT const ODAttributeType kODAttributeTypeUserSMIMECertificate;
CF_EXPORT const ODAttributeType kODAttributeTypeVFSDumpFreq;
CF_EXPORT const ODAttributeType kODAttributeTypeVFSLinkDir;
CF_EXPORT const ODAttributeType kODAttributeTypeVFSPassNo;
CF_EXPORT const ODAttributeType kODAttributeTypeVFSType;
CF_EXPORT const ODAttributeType kODAttributeTypeWeblogURI;
CF_EXPORT const ODAttributeType kODAttributeTypeXMLPlist;
CF_EXPORT const ODAttributeType kODAttributeTypeProtocolNumber;
CF_EXPORT const ODAttributeType kODAttributeTypeRPCNumber;
CF_EXPORT const ODAttributeType kODAttributeTypeNetworkNumber;
CF_EXPORT const ODAttributeType kODAttributeTypeAccessControlEntry;
CF_EXPORT const ODAttributeType kODAttributeTypeAddressLine1;
CF_EXPORT const ODAttributeType kODAttributeTypeAddressLine2;
CF_EXPORT const ODAttributeType kODAttributeTypeAddressLine3;
CF_EXPORT const ODAttributeType kODAttributeTypeAreaCode;
CF_EXPORT const ODAttributeType kODAttributeTypeAuthenticationAuthority;
CF_EXPORT const ODAttributeType kODAttributeTypeAutomountInformation;
CF_EXPORT const ODAttributeType kODAttributeTypeBootParams;
CF_EXPORT const ODAttributeType kODAttributeTypeBuilding;
CF_EXPORT const ODAttributeType kODAttributeTypeServicesLocator;
CF_EXPORT const ODAttributeType kODAttributeTypeCity;
CF_EXPORT const ODAttributeType kODAttributeTypeCompany;
CF_EXPORT const ODAttributeType kODAttributeTypeComputers;
CF_EXPORT const ODAttributeType kODAttributeTypeCountry;
CF_EXPORT const ODAttributeType kODAttributeTypeDepartment;
CF_EXPORT const ODAttributeType kODAttributeTypeDNSName;
CF_EXPORT const ODAttributeType kODAttributeTypeEMailAddress;
CF_EXPORT const ODAttributeType kODAttributeTypeEMailContacts;
CF_EXPORT const ODAttributeType kODAttributeTypeFaxNumber;
CF_EXPORT const ODAttributeType kODAttributeTypeGroup;
CF_EXPORT const ODAttributeType kODAttributeTypeGroupMembers;
CF_EXPORT const ODAttributeType kODAttributeTypeGroupMembership;
CF_EXPORT const ODAttributeType kODAttributeTypeGroupServices;
CF_EXPORT const ODAttributeType kODAttributeTypeHomePhoneNumber;
CF_EXPORT const ODAttributeType kODAttributeTypeHTML;
CF_EXPORT const ODAttributeType kODAttributeTypeHomeDirectory;
CF_EXPORT const ODAttributeType kODAttributeTypeIMHandle;
CF_EXPORT const ODAttributeType kODAttributeTypeIPAddress;
CF_EXPORT const ODAttributeType kODAttributeTypeIPAddressAndENetAddress;
CF_EXPORT const ODAttributeType kODAttributeTypeIPv6Address;
CF_EXPORT const ODAttributeType kODAttributeTypeJPEGPhoto;
CF_EXPORT const ODAttributeType kODAttributeTypeJobTitle;
CF_EXPORT const ODAttributeType kODAttributeTypeKDCAuthKey;
CF_EXPORT const ODAttributeType kODAttributeTypeKeywords;
CF_EXPORT const ODAttributeType kODAttributeTypeLDAPReadReplicas;
CF_EXPORT const ODAttributeType kODAttributeTypeLDAPWriteReplicas;
CF_EXPORT const ODAttributeType kODAttributeTypeMapCoordinates;
CF_EXPORT const ODAttributeType kODAttributeTypeMapURI;
CF_EXPORT const ODAttributeType kODAttributeTypeMIME;
CF_EXPORT const ODAttributeType kODAttributeTypeMobileNumber;
CF_EXPORT const ODAttributeType kODAttributeTypeNestedGroups;
CF_EXPORT const ODAttributeType kODAttributeTypeNetGroups;
CF_EXPORT const ODAttributeType kODAttributeTypeNickName;
CF_EXPORT const ODAttributeType kODAttributeTypeOrganizationInfo;
CF_EXPORT const ODAttributeType kODAttributeTypeOrganizationName;
CF_EXPORT const ODAttributeType kODAttributeTypePagerNumber;
CF_EXPORT const ODAttributeType kODAttributeTypePhoneContacts;
CF_EXPORT const ODAttributeType kODAttributeTypePhoneNumber;
CF_EXPORT const ODAttributeType kODAttributeTypePGPPublicKey;
CF_EXPORT const ODAttributeType kODAttributeTypePostalAddress;
CF_EXPORT const ODAttributeType kODAttributeTypePostalAddressContacts;
CF_EXPORT const ODAttributeType kODAttributeTypePostalCode;
CF_EXPORT const ODAttributeType kODAttributeTypeNamePrefix;
CF_EXPORT const ODAttributeType kODAttributeTypeProtocols;
CF_EXPORT const ODAttributeType kODAttributeTypeRecordName;
CF_EXPORT const ODAttributeType kODAttributeTypeRelationships;
CF_EXPORT const ODAttributeType kODAttributeTypeResourceInfo;
CF_EXPORT const ODAttributeType kODAttributeTypeResourceType;
CF_EXPORT const ODAttributeType kODAttributeTypeState;
CF_EXPORT const ODAttributeType kODAttributeTypeStreet;
CF_EXPORT const ODAttributeType kODAttributeTypeNameSuffix;
CF_EXPORT const ODAttributeType kODAttributeTypeURL;
CF_EXPORT const ODAttributeType kODAttributeTypeVFSOpts;
CF_EXPORT const ODAttributeType kODAttributeTypeAlias;
CF_EXPORT const ODAttributeType kODAttributeTypeAuthCredential;
CF_EXPORT const ODAttributeType kODAttributeTypeCopyTimestamp;
CF_EXPORT const ODAttributeType kODAttributeTypeDateRecordCreated;
CF_EXPORT const ODAttributeType kODAttributeTypeKerberosRealm;
CF_EXPORT const ODAttributeType kODAttributeTypeKerberosServices;
CF_EXPORT const ODAttributeType kODAttributeTypeNTDomainComputerAccount;
CF_EXPORT const ODAttributeType kODAttributeTypeOriginalHomeDirectory;
CF_EXPORT const ODAttributeType kODAttributeTypeOriginalNFSHomeDirectory;
CF_EXPORT const ODAttributeType kODAttributeTypeOriginalNodeName;
CF_EXPORT const ODAttributeType kODAttributeTypePrimaryNTDomain;
CF_EXPORT const ODAttributeType kODAttributeTypePwdAgingPolicy;
CF_EXPORT const ODAttributeType kODAttributeTypeReadOnlyNode;
CF_EXPORT const ODAttributeType kODAttributeTypeTimePackage;
CF_EXPORT const ODAttributeType kODAttributeTypeTotalSize;
CF_EXPORT const ODAttributeType kODAttributeTypeAuthMethod;
CF_EXPORT const ODAttributeType kODAttributeTypeMetaNodeLocation;
CF_EXPORT const ODAttributeType kODAttributeTypeNodePath;
CF_EXPORT const ODAttributeType kODAttributeTypePlugInInfo;
CF_EXPORT const ODAttributeType kODAttributeTypeRecordType;
CF_EXPORT const ODAttributeType kODAttributeTypeSchema;
CF_EXPORT const ODAttributeType kODAttributeTypeSubNodes;
CF_EXPORT const ODAttributeType kODAttributeTypeNetGroupTriplet;
CF_EXPORT const ODAttributeType kODAttributeTypeSearchPath;
CF_EXPORT const ODAttributeType kODAttributeTypeSearchPolicy;
CF_EXPORT const ODAttributeType kODAttributeTypeAutomaticSearchPath;
CF_EXPORT const ODAttributeType kODAttributeTypeLocalOnlySearchPath;
CF_EXPORT const ODAttributeType kODAttributeTypeCustomSearchPath;
CF_EXPORT const ODAttributeType kODAttributeTypeAdvertisedServices;
CF_EXPORT const ODAttributeType kODAttributeTypeLocaleRelay;
CF_EXPORT const ODAttributeType kODAttributeTypeLocaleSubnets;
CF_EXPORT const ODAttributeType kODAttributeTypeNetworkInterfaces;
CF_EXPORT const ODAttributeType kODAttributeTypeParentLocales;
CF_EXPORT const ODAttributeType kODAttributeTypePrimaryLocale;
CF_EXPORT const ODAttributeType kODAttributeTypeBuildVersion;
CF_EXPORT const ODAttributeType kODAttributeTypeConfigAvailable;
CF_EXPORT const ODAttributeType kODAttributeTypeConfigFile;
CF_EXPORT const ODAttributeType kODAttributeTypeCoreFWVersion;
CF_EXPORT const ODAttributeType kODAttributeTypeFunctionalState;
CF_EXPORT const ODAttributeType kODAttributeTypeFWVersion;
CF_EXPORT const ODAttributeType kODAttributeTypePluginIndex;
CF_EXPORT const ODAttributeType kODAttributeTypeNumTableList;
CF_EXPORT const ODAttributeType kODAttributeTypeVersion;
CF_EXPORT const ODAttributeType kODAttributeTypePIDValue;
CF_EXPORT const ODAttributeType kODAttributeTypeProcessName;
CF_EXPORT const ODAttributeType kODAttributeTypeTotalRefCount;
CF_EXPORT const ODAttributeType kODAttributeTypeDirRefCount;
CF_EXPORT const ODAttributeType kODAttributeTypeNodeRefCount;
CF_EXPORT const ODAttributeType kODAttributeTypeRecRefCount;
CF_EXPORT const ODAttributeType kODAttributeTypeAttrListRefCount;
CF_EXPORT const ODAttributeType kODAttributeTypeAttrListValueRefCount;
CF_EXPORT const ODAttributeType kODAttributeTypeDirRefs;
CF_EXPORT const ODAttributeType kODAttributeTypeNodeRefs;
CF_EXPORT const ODAttributeType kODAttributeTypeRecRefs;
CF_EXPORT const ODAttributeType kODAttributeTypeAttrListRefs;
CF_EXPORT const ODAttributeType kODAttributeTypeAttrListValueRefs;
CF_EXPORT const ODAttributeType kODAttributeTypeOperatingSystem;
CF_EXPORT const ODAttributeType kODAttributeTypeOperatingSystemVersion;
CF_EXPORT const ODAttributeType kODAttributeTypeAltSecurityIdentities;
CF_EXPORT const ODAttributeType kODAttributeTypeHardwareUUID;
CF_EXPORT const ODAuthenticationType kODAuthenticationType2WayRandom;
CF_EXPORT const ODAuthenticationType kODAuthenticationType2WayRandomChangePasswd;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeAPOP;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeCRAM_MD5;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeChangePasswd;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeClearText;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeCrypt;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeDIGEST_MD5;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeDeleteUser;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeGetEffectivePolicy;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeGetGlobalPolicy;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeGetKerberosPrincipal;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeGetPolicy;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeGetUserData;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeGetUserName;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeKerberosTickets;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeMPPEMasterKeys;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeMSCHAP2;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeNTLMv2;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeNTLMv2WithSessionKey;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeNewUser;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeNewUserWithPolicy;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeNodeNativeClearTextOK;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeNodeNativeNoClearText;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeNodeNativeRetainCredential;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeNodePPS;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeReadSecureHash;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSMBNTv2UserSessionKey;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSMBWorkstationCredentialSessionKey;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSMB_LM_Key;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSMB_NT_Key;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSMB_NT_UserSessionKey;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSMB_NT_WithUserSessionKey;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSecureHash;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSetCertificateHashAsCurrent;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSetGlobalPolicy;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSetLMHash;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSetNTHash;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSetPassword;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSetPasswordAsCurrent;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSetPolicy;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSetPolicyAsCurrent;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSetUserData;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSetUserName;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeSetWorkstationPassword;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeWithAuthorizationRef;
CF_EXPORT const ODAuthenticationType kODAuthenticationTypeWriteSecureHash;

CF_EXTERN_C_END

#endif
