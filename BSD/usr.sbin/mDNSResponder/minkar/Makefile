# mDNSResponder for Minkar: the POSIX daemon of this mDNSResponder release
# (mDNSPosix/PosixDaemon.c and the shared core), unmodified, installed as
# /usr/sbin/mDNSResponder. It serves the dns_sd API (libsystem_dnssd) on
# /var/run/mDNSResponder, which libinfo's getaddrinfo() uses: unicast DNS
# from /etc/resolv.conf and multicast DNS on the local network.
# mDNSResponder is Apache-2.0 licensed (../LICENSE), approved for Minkar.
# Changed for Minkar, in mDNSPosix/mDNSPosix.c (marked "Minkar change"),
# so that unicast DNS works in the POSIX daemon:
# - mDNSPlatformSetDNSConfig() reports the servers of /etc/resolv.conf,
#   which uDNS_SetupDNSConfig() otherwise drops;
# - mDNSPlatformUDPSocket(), mDNSPlatformUDPClose() and
#   mDNSPlatformGetUDPPort() give each question its own UDP socket, which
#   mDNSPlatformSendUDP() sends from;
# - received packets reach the core with their real destination port
#   (always 5353 before, so no unicast answer matched its question).
# Who rewrites resolv.conf tells it: configd's IPMonitor posts
# com.apple.system.config.network_change.dns, which PosixDaemon.c registers
# to receive as SIGHUP (Minkar change); dhcpclient, without configd,
# sends SIGHUP.

SUBDIR =
MK_WERROR = no
PROG =	mDNSResponder
MAN =

MDNS =	${.CURDIR:H}
.PATH: ${MDNS}/mDNSPosix ${MDNS}/mDNSCore ${MDNS}/mDNSShared ${MDNS}/DSO

SRCS =	PosixDaemon.c mDNSPosix.c mDNSUNP.c posix_utilities.c \
	mDNS.c DNSDigest.c uDNS.c DNSCommon.c CryptoAlg.c \
	uds_daemon.c mDNSDebug.c dnssd_ipc.c GenLinkedList.c PlatformCommon.c \
	ClientRequests.c dso.c dso-transport.c

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${MDNS}/mDNSCore -I${MDNS}/mDNSShared -I${MDNS}/DSO \
	-I${MDNS}/ServiceRegistration -I${MDNS}/mDNSPosix -fwrapv \
	-DMDNS_UDS_SERVERPATH=\"/var/run/mDNSResponder\" \
	-DHAVE_IPV6=1 -DHAVE_STRLCPY=1 -D__APPLE_USE_RFC_3542 \
	-DMDNS_DEBUGMSGS=0 -D__OPEN_SOURCE__ -DPOSIX_BUILD \
	-Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/sbin/

.include <bsd.prog.mk>
