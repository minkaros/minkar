/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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
 * SCNetworkReachability for Minkar. Apple's implementation is built on
 * Network.framework's private API, which Minkar does not have; this one
 * asks the kernel instead:
 *
 * - an address is reachable if the routing table has a route to it
 *   through an interface that is up and running (RTM_GET); without a
 *   gateway it is direct, and an address of this host is local;
 * - a name is resolved with getaddrinfo(3), and is reachable if one of
 *   its addresses is;
 * - an address pair is judged by its remote address, or by its local
 *   address alone (which must belong to an interface).
 *
 * Changes are noticed on a routing socket (routes, addresses and links
 * change there), from a run loop or a dispatch queue, and reported to the
 * callback when the flags change.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <dispatch/dispatch.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum reachType { kReachAddress, kReachPair, kReachName };

typedef struct __SCNetworkReachability {
	CFRuntimeBase			cfBase;
	pthread_mutex_t			lock;

	enum reachType			type;
	struct sockaddr_storage		local, remote;		/* ss_len 0: none */
	char				*name;
	char				ifname[IFNAMSIZ];	/* "" or the interface it must use */

	SCNetworkReachabilityCallBack	callout;
	SCNetworkReachabilityContext	context;

	/* Monitoring: a routing socket, from a run loop or a dispatch queue. */
	CFSocketRef			socket;
	CFRunLoopSourceRef		rls;
	CFMutableArrayRef		runLoops;		/* [runLoop, mode] pairs */
	dispatch_queue_t		queue;
	dispatch_source_t		source;
	SCNetworkReachabilityFlags	lastFlags;
	Boolean				haveLastFlags;
} SCNetworkReachabilityPrivate, *SCNetworkReachabilityPrivateRef;

static CFTypeID __kSCNetworkReachabilityTypeID = _kCFRuntimeNotATypeID;

static void		unschedule(SCNetworkReachabilityPrivateRef target);

static __inline__ CFTypeRef
isA_SCNetworkReachability(CFTypeRef obj)
{
	return isA_CFType(obj, SCNetworkReachabilityGetTypeID());
}

#pragma mark Object

static CFStringRef
__SCNetworkReachabilityCopyDescription(CFTypeRef cf)
{
	SCNetworkReachabilityPrivateRef	target	= (SCNetworkReachabilityPrivateRef)cf;
	char				buf[NI_MAXHOST];

	switch (target->type) {
	case kReachName:
		return CFStringCreateWithFormat(NULL, NULL,
		    CFSTR("<SCNetworkReachability %p> {name = %s}"), cf, target->name);
	default: {
		const struct sockaddr_storage *ss = target->remote.ss_len != 0 ?
		    &target->remote : &target->local;

		if (getnameinfo((const struct sockaddr *)ss, ss->ss_len, buf, sizeof(buf),
		    NULL, 0, NI_NUMERICHOST) != 0) {
			strlcpy(buf, "?", sizeof(buf));
		}
		return CFStringCreateWithFormat(NULL, NULL,
		    CFSTR("<SCNetworkReachability %p> {%s = %s}"), cf,
		    target->remote.ss_len != 0 ? "address" : "local address", buf);
	}
	}
}

static void
__SCNetworkReachabilityDeallocate(CFTypeRef cf)
{
	SCNetworkReachabilityPrivateRef	target	= (SCNetworkReachabilityPrivateRef)cf;

	unschedule(target);
	if (target->context.release != NULL && target->context.info != NULL) {
		target->context.release(target->context.info);
	}
	if (target->runLoops != NULL) {
		CFRelease(target->runLoops);
	}
	free(target->name);
	pthread_mutex_destroy(&target->lock);
}

static const CFRuntimeClass __SCNetworkReachabilityClass = {
	0,					// version
	"SCNetworkReachability",		// className
	NULL,					// init
	NULL,					// copy
	__SCNetworkReachabilityDeallocate,	// dealloc
	NULL,					// equal
	NULL,					// hash
	NULL,					// copyFormattingDesc
	__SCNetworkReachabilityCopyDescription	// copyDebugDesc
};

CFTypeID
SCNetworkReachabilityGetTypeID(void)
{
	static dispatch_once_t	once;

	dispatch_once(&once, ^{
		__kSCNetworkReachabilityTypeID = _CFRuntimeRegisterClass(&__SCNetworkReachabilityClass);
	});
	return __kSCNetworkReachabilityTypeID;
}

static SCNetworkReachabilityPrivateRef
createPrivate(CFAllocatorRef allocator, enum reachType type)
{
	SCNetworkReachabilityPrivateRef	target;
	uint32_t			size;

	size = sizeof(SCNetworkReachabilityPrivate) - sizeof(CFRuntimeBase);
	target = (SCNetworkReachabilityPrivateRef)_CFRuntimeCreateInstance(allocator,
	    SCNetworkReachabilityGetTypeID(), size, NULL);
	if (target == NULL) {
		return NULL;
	}
	memset((char *)target + sizeof(CFRuntimeBase), 0, size);
	pthread_mutex_init(&target->lock, NULL);
	target->type = type;
	return target;
}

static Boolean
copyAddress(struct sockaddr_storage *dst, const struct sockaddr *sa)
{
	if (sa == NULL) {
		return TRUE;
	}
	if ((sa->sa_family != AF_INET && sa->sa_family != AF_INET6) ||
	    sa->sa_len > sizeof(*dst) || sa->sa_len < sizeof(struct sockaddr_in)) {
		return FALSE;
	}
	memcpy(dst, sa, sa->sa_len);
	return TRUE;
}

SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithAddress(CFAllocatorRef allocator, const struct sockaddr *address)
{
	SCNetworkReachabilityPrivateRef	target;

	if (address == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}
	target = createPrivate(allocator, kReachAddress);
	if (target != NULL && !copyAddress(&target->remote, address)) {
		CFRelease(target);
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}
	return (SCNetworkReachabilityRef)target;
}

SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithAddressPair(CFAllocatorRef allocator,
    const struct sockaddr *localAddress, const struct sockaddr *remoteAddress)
{
	SCNetworkReachabilityPrivateRef	target;

	if (localAddress == NULL && remoteAddress == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}
	target = createPrivate(allocator, kReachPair);
	if (target != NULL &&
	    (!copyAddress(&target->local, localAddress) ||
	     !copyAddress(&target->remote, remoteAddress))) {
		CFRelease(target);
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}
	return (SCNetworkReachabilityRef)target;
}

SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithName(CFAllocatorRef allocator, const char *nodename)
{
	SCNetworkReachabilityPrivateRef	target;
	struct addrinfo			hints, *res;

	if (nodename == NULL || nodename[0] == '\0') {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	/* A numeric name is an address. */
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(nodename, NULL, &hints, &res) == 0) {
		target = (SCNetworkReachabilityPrivateRef)
		    SCNetworkReachabilityCreateWithAddress(allocator, res->ai_addr);
		freeaddrinfo(res);
		return (SCNetworkReachabilityRef)target;
	}

	target = createPrivate(allocator, kReachName);
	if (target != NULL && (target->name = strdup(nodename)) == NULL) {
		CFRelease(target);
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}
	return (SCNetworkReachabilityRef)target;
}

static Boolean
optionAddress(CFDictionaryRef options, CFStringRef key, struct sockaddr_storage *dst)
{
	CFDataRef	data	= CFDictionaryGetValue(options, key);

	if (data == NULL) {
		return TRUE;
	}
	if (!isA_CFData(data) || (size_t)CFDataGetLength(data) < sizeof(struct sockaddr_in)) {
		return FALSE;
	}
	return copyAddress(dst, (const struct sockaddr *)(const void *)CFDataGetBytePtr(data));
}

SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithOptions(CFAllocatorRef allocator, CFDictionaryRef options)
{
	CFStringRef			nodename;
	CFStringRef			interface;
	SCNetworkReachabilityPrivateRef	target;
	char				name[NI_MAXHOST];

	if (!isA_CFDictionary(options)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}
	nodename = CFDictionaryGetValue(options, kSCNetworkReachabilityOptionNodeName);
	interface = CFDictionaryGetValue(options, kSCNetworkReachabilityOptionInterface);
	if (nodename != NULL) {
		if (!isA_CFString(nodename) ||
		    !CFStringGetCString(nodename, name, sizeof(name), kCFStringEncodingUTF8)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}
		target = (SCNetworkReachabilityPrivateRef)SCNetworkReachabilityCreateWithName(allocator, name);
	} else {
		target = createPrivate(allocator, kReachPair);
		if (target != NULL &&
		    (!optionAddress(options, kSCNetworkReachabilityOptionLocalAddress, &target->local) ||
		     !optionAddress(options, kSCNetworkReachabilityOptionRemoteAddress, &target->remote) ||
		     (target->local.ss_len == 0 && target->remote.ss_len == 0))) {
			CFRelease(target);
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}
	}
	if (target != NULL && interface != NULL &&
	    (!isA_CFString(interface) ||
	     !CFStringGetCString(interface, target->ifname, sizeof(target->ifname), kCFStringEncodingASCII))) {
		CFRelease(target);
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}
	return (SCNetworkReachabilityRef)target;
}

#pragma mark Reachability

static Boolean
sameAddress(const struct sockaddr *a, const struct sockaddr *b)
{
	if (a->sa_family != b->sa_family) {
		return FALSE;
	}
	if (a->sa_family == AF_INET) {
		return ((const struct sockaddr_in *)(const void *)a)->sin_addr.s_addr ==
		       ((const struct sockaddr_in *)(const void *)b)->sin_addr.s_addr;
	}
	if (a->sa_family == AF_INET6) {
		struct in6_addr	x	= ((const struct sockaddr_in6 *)(const void *)a)->sin6_addr;
		struct in6_addr	y	= ((const struct sockaddr_in6 *)(const void *)b)->sin6_addr;

		/* The kernel keeps a link-local address's scope in its second word. */
		if (IN6_IS_ADDR_LINKLOCAL(&x)) {
			x.s6_addr[2] = x.s6_addr[3] = 0;
		}
		if (IN6_IS_ADDR_LINKLOCAL(&y)) {
			y.s6_addr[2] = y.s6_addr[3] = 0;
		}
		return IN6_ARE_ADDR_EQUAL(&x, &y);
	}
	return FALSE;
}

/* Whether sa is an address of an interface that is up; its name in ifname. */
static Boolean
isLocalAddress(const struct sockaddr *sa, char ifname[IFNAMSIZ])
{
	struct ifaddrs	*list, *ifa;
	Boolean		found	= FALSE;

	if (getifaddrs(&list) != 0) {
		return FALSE;
	}
	for (ifa = list; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr != NULL && (ifa->ifa_flags & IFF_UP) &&
		    sameAddress(ifa->ifa_addr, sa)) {
			strlcpy(ifname, ifa->ifa_name, IFNAMSIZ);
			found = TRUE;
			break;
		}
	}
	freeifaddrs(list);
	return found;
}

static unsigned int
interfaceFlags(const char *ifname)
{
	struct ifreq	ifr;
	int		s;
	unsigned int	flags	= 0;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		return 0;
	}
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0) {
		flags = (unsigned short)ifr.ifr_flags;
	}
	close(s);
	return flags;
}

#define ROUNDUP(a)	((a) > 0 ? (1 + (((a) - 1) | (sizeof(uint32_t) - 1))) : sizeof(uint32_t))

/*
 * The route to sa: its flags and interface name. FALSE if there is none
 * (or no answer).
 */
static Boolean
lookupRoute(const struct sockaddr *sa, int *rtflags, char ifname[IFNAMSIZ])
{
	struct {
		struct rt_msghdr	hdr;
		char			space[512];
	} msg;
	static int		seq;
	int			s, mySeq;
	ssize_t			n;
	pid_t			pid	= getpid();
	char			*cp;

	s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0) {
		return FALSE;
	}
	memset(&msg, 0, sizeof(msg));
	msg.hdr.rtm_type = RTM_GET;
	msg.hdr.rtm_version = RTM_VERSION;
	msg.hdr.rtm_flags = RTF_UP | RTF_HOST;
	msg.hdr.rtm_addrs = RTA_DST | RTA_IFP;
	msg.hdr.rtm_seq = mySeq = __sync_add_and_fetch(&seq, 1);
	cp = msg.space;
	memcpy(cp, sa, sa->sa_len);
	cp += ROUNDUP(sa->sa_len);
	{
		/* RTA_IFP: an empty link address asks for the interface. */
		struct sockaddr_dl	sdl;

		memset(&sdl, 0, sizeof(sdl));
		sdl.sdl_len = sizeof(sdl);
		sdl.sdl_family = AF_LINK;
		memcpy(cp, &sdl, sizeof(sdl));
		cp += ROUNDUP(sizeof(sdl));
	}
	msg.hdr.rtm_msglen = (u_short)(cp - (char *)&msg);
	if (write(s, &msg, msg.hdr.rtm_msglen) < 0) {
		close(s);
		return FALSE;		/* ESRCH: no route */
	}
	do {
		n = read(s, &msg, sizeof(msg));
	} while (n > 0 && (msg.hdr.rtm_seq != mySeq || msg.hdr.rtm_pid != pid));
	close(s);
	if (n <= 0 || msg.hdr.rtm_errno != 0 || !(msg.hdr.rtm_flags & RTF_UP)) {
		return FALSE;
	}

	*rtflags = msg.hdr.rtm_flags;
	ifname[0] = '\0';
	cp = msg.space;
	for (int i = 0; i < RTAX_MAX && cp < (char *)&msg + n; i++) {
		const struct sockaddr	*rsa;

		if (!(msg.hdr.rtm_addrs & (1 << i))) {
			continue;
		}
		rsa = (const struct sockaddr *)(const void *)cp;
		if (i == RTAX_IFP && rsa->sa_family == AF_LINK) {
			const struct sockaddr_dl *sdl = (const struct sockaddr_dl *)(const void *)rsa;

			if (sdl->sdl_nlen > 0 && sdl->sdl_nlen < IFNAMSIZ) {
				memcpy(ifname, sdl->sdl_data, sdl->sdl_nlen);
				ifname[sdl->sdl_nlen] = '\0';
			} else if (sdl->sdl_index != 0) {
				if_indextoname(sdl->sdl_index, ifname);
			}
		}
		cp += ROUNDUP(rsa->sa_len);
	}
	if (ifname[0] == '\0' && msg.hdr.rtm_index != 0) {
		if_indextoname(msg.hdr.rtm_index, ifname);
	}
	return TRUE;
}

static SCNetworkReachabilityFlags
addressFlagsIf(const struct sockaddr *sa, const char *requiredIf, char ifname[IFNAMSIZ])
{
	int	rtflags	= 0;

	ifname[0] = '\0';

	if (isLocalAddress(sa, ifname)) {
		if (requiredIf[0] != '\0' && strcmp(requiredIf, ifname) != 0) {
			return 0;
		}
		return kSCNetworkReachabilityFlagsReachable |
		       kSCNetworkReachabilityFlagsIsLocalAddress |
		       kSCNetworkReachabilityFlagsIsDirect;
	}
	if (!lookupRoute(sa, &rtflags, ifname)) {
		return 0;
	}
	if (ifname[0] != '\0') {
		unsigned int ifflags = interfaceFlags(ifname);

		if ((ifflags & (IFF_UP | IFF_RUNNING)) != (IFF_UP | IFF_RUNNING)) {
			return 0;
		}
		if (requiredIf[0] != '\0' && strcmp(requiredIf, ifname) != 0) {
			return 0;
		}
	}
	if (rtflags & (RTF_REJECT | RTF_BLACKHOLE)) {
		return 0;
	}
	return kSCNetworkReachabilityFlagsReachable |
	       ((rtflags & RTF_GATEWAY) ? 0 : kSCNetworkReachabilityFlagsIsDirect);
}

static SCNetworkReachabilityFlags
addressFlags(const struct sockaddr *sa, const char *requiredIf)
{
	char	ifname[IFNAMSIZ];

	return addressFlagsIf(sa, requiredIf, ifname);
}

static SCNetworkReachabilityFlags
computeFlags(SCNetworkReachabilityPrivateRef target)
{
	SCNetworkReachabilityFlags	flags	= 0;

	switch (target->type) {
	case kReachAddress:
		flags = addressFlags((struct sockaddr *)&target->remote, target->ifname);
		break;

	case kReachPair:
		if (target->remote.ss_len != 0) {
			flags = addressFlags((struct sockaddr *)&target->remote, target->ifname);
		} else {
			char ifname[IFNAMSIZ];

			if (isLocalAddress((struct sockaddr *)&target->local, ifname)) {
				flags = kSCNetworkReachabilityFlagsReachable |
					kSCNetworkReachabilityFlagsIsLocalAddress |
					kSCNetworkReachabilityFlagsIsDirect;
			}
		}
		break;

	case kReachName: {
		struct addrinfo	hints, *res, *ai;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_ADDRCONFIG;
		if (getaddrinfo(target->name, NULL, &hints, &res) != 0) {
			break;
		}
		/* The best of its addresses: reachable, and direct if one is. */
		for (ai = res; ai != NULL; ai = ai->ai_next) {
			SCNetworkReachabilityFlags f = addressFlags(ai->ai_addr, target->ifname);

			if ((f & kSCNetworkReachabilityFlagsReachable) &&
			    (!(flags & kSCNetworkReachabilityFlagsReachable) ||
			     (f & kSCNetworkReachabilityFlagsIsDirect))) {
				/* A name is never a "local address" to the caller. */
				flags = f & ~kSCNetworkReachabilityFlagsIsLocalAddress;
			}
		}
		freeaddrinfo(res);
		break;
	}
	}
	return flags;
}

Boolean
SCNetworkReachabilityGetFlags(SCNetworkReachabilityRef target, SCNetworkReachabilityFlags *flags)
{
	if (!isA_SCNetworkReachability(target) || flags == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}
	*flags = computeFlags((SCNetworkReachabilityPrivateRef)target);
	return TRUE;
}

#pragma mark Notifications

/* Recomputes the flags; calls the callback if they changed. */
static void
reachabilityChanged(SCNetworkReachabilityPrivateRef target)
{
	SCNetworkReachabilityFlags	flags	= computeFlags(target);
	SCNetworkReachabilityCallBack	callout;
	void				*info;

	pthread_mutex_lock(&target->lock);
	if (target->haveLastFlags && flags == target->lastFlags) {
		pthread_mutex_unlock(&target->lock);
		return;
	}
	target->lastFlags = flags;
	target->haveLastFlags = TRUE;
	callout = target->callout;
	info = target->context.info;
	pthread_mutex_unlock(&target->lock);
	if (callout != NULL) {
		callout((SCNetworkReachabilityRef)target, flags, info);
	}
}

/* Reads what the routing socket has; returns whether anything was relevant. */
static Boolean
drainRoutingSocket(int s)
{
	char	buf[2048];
	Boolean	relevant	= FALSE;
	ssize_t	n;

	while ((n = recv(s, buf, sizeof(buf), MSG_DONTWAIT)) > 0) {
		const struct rt_msghdr *rtm = (const struct rt_msghdr *)(const void *)buf;

		switch (rtm->rtm_type) {
		case RTM_ADD: case RTM_DELETE: case RTM_CHANGE:
		case RTM_NEWADDR: case RTM_DELADDR: case RTM_IFINFO:
			relevant = TRUE;
			break;
		default:
			break;
		}
	}
	return relevant;
}

static void
socketCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info)
{
#pragma unused(type, address, data)
	if (drainRoutingSocket(CFSocketGetNative(s))) {
		reachabilityChanged((SCNetworkReachabilityPrivateRef)info);
	}
}

static int
openRoutingSocket(void)
{
	int	s	= socket(PF_ROUTE, SOCK_RAW, 0);

	if (s >= 0) {
		(void)fcntl(s, F_SETFL, O_NONBLOCK);
		(void)fcntl(s, F_SETFD, FD_CLOEXEC);
	}
	return s;
}

static void
unschedule(SCNetworkReachabilityPrivateRef target)
{
	if (target->source != NULL) {
		dispatch_source_cancel(target->source);
		dispatch_release(target->source);
		target->source = NULL;
	}
	if (target->queue != NULL) {
		dispatch_release(target->queue);
		target->queue = NULL;
	}
	if (target->rls != NULL) {
		CFRunLoopSourceInvalidate(target->rls);
		CFRelease(target->rls);
		target->rls = NULL;
	}
	if (target->socket != NULL) {
		CFSocketInvalidate(target->socket);
		CFRelease(target->socket);
		target->socket = NULL;
	}
	target->haveLastFlags = FALSE;
}

/* The first report, after scheduling: the flags as they are. */
static void
initialReport(SCNetworkReachabilityPrivateRef target, dispatch_queue_t queue)
{
	CFRetain(target);
	dispatch_async(queue != NULL ? queue : dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		if (queue != NULL) {
			reachabilityChanged(target);
		} else {
			SCNetworkReachabilityFlags flags = computeFlags(target);

			pthread_mutex_lock(&target->lock);
			if (!target->haveLastFlags) {
				target->lastFlags = flags;
				target->haveLastFlags = TRUE;
			}
			pthread_mutex_unlock(&target->lock);
		}
		CFRelease(target);
	});
}

Boolean
SCNetworkReachabilitySetCallback(SCNetworkReachabilityRef target, SCNetworkReachabilityCallBack callout,
    SCNetworkReachabilityContext *context)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;
	SCNetworkReachabilityContext	old;

	if (!isA_SCNetworkReachability(target)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}
	pthread_mutex_lock(&targetPrivate->lock);
	old = targetPrivate->context;
	targetPrivate->callout = callout;
	memset(&targetPrivate->context, 0, sizeof(targetPrivate->context));
	if (context != NULL) {
		targetPrivate->context = *context;
		if (context->retain != NULL && context->info != NULL) {
			targetPrivate->context.info = (void *)context->retain(context->info);
		}
	}
	pthread_mutex_unlock(&targetPrivate->lock);
	if (old.release != NULL && old.info != NULL) {
		old.release(old.info);
	}
	return TRUE;
}

Boolean
SCNetworkReachabilityScheduleWithRunLoop(SCNetworkReachabilityRef target, CFRunLoopRef runLoop,
    CFStringRef runLoopMode)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;
	Boolean				first		= FALSE;

	if (!isA_SCNetworkReachability(target) || runLoop == NULL || runLoopMode == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}
	pthread_mutex_lock(&targetPrivate->lock);
	if (targetPrivate->queue != NULL) {
		/* Either a dispatch queue or run loops, not both. */
		pthread_mutex_unlock(&targetPrivate->lock);
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}
	if (targetPrivate->rls == NULL) {
		CFSocketContext	ctx	= { 0, (void *)target, NULL, NULL, NULL };
		int		s	= openRoutingSocket();

		if (s < 0) {
			pthread_mutex_unlock(&targetPrivate->lock);
			_SCErrorSet(errno);
			return FALSE;
		}
		targetPrivate->socket = CFSocketCreateWithNative(NULL, s, kCFSocketReadCallBack,
		    socketCallback, &ctx);
		if (targetPrivate->socket == NULL) {
			close(s);
			pthread_mutex_unlock(&targetPrivate->lock);
			_SCErrorSet(kSCStatusFailed);
			return FALSE;
		}
		targetPrivate->rls = CFSocketCreateRunLoopSource(NULL, targetPrivate->socket, 0);
		if (targetPrivate->runLoops == NULL) {
			targetPrivate->runLoops = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		}
		first = TRUE;
	}
	CFRunLoopAddSource(runLoop, targetPrivate->rls, runLoopMode);
	CFArrayAppendValue(targetPrivate->runLoops, runLoop);
	CFArrayAppendValue(targetPrivate->runLoops, runLoopMode);
	pthread_mutex_unlock(&targetPrivate->lock);
	if (first) {
		initialReport(targetPrivate, NULL);
	}
	return TRUE;
}

Boolean
SCNetworkReachabilityUnscheduleFromRunLoop(SCNetworkReachabilityRef target, CFRunLoopRef runLoop,
    CFStringRef runLoopMode)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;
	Boolean				found		= FALSE;

	if (!isA_SCNetworkReachability(target) || runLoop == NULL || runLoopMode == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}
	pthread_mutex_lock(&targetPrivate->lock);
	if (targetPrivate->rls != NULL && targetPrivate->runLoops != NULL) {
		for (CFIndex i = 0; i + 1 < CFArrayGetCount(targetPrivate->runLoops); i += 2) {
			if (CFArrayGetValueAtIndex(targetPrivate->runLoops, i) == runLoop &&
			    CFEqual(CFArrayGetValueAtIndex(targetPrivate->runLoops, i + 1), runLoopMode)) {
				CFRunLoopRemoveSource(runLoop, targetPrivate->rls, runLoopMode);
				CFArrayRemoveValueAtIndex(targetPrivate->runLoops, i + 1);
				CFArrayRemoveValueAtIndex(targetPrivate->runLoops, i);
				found = TRUE;
				break;
			}
		}
		if (found && CFArrayGetCount(targetPrivate->runLoops) == 0) {
			unschedule(targetPrivate);
		}
	}
	pthread_mutex_unlock(&targetPrivate->lock);
	if (!found) {
		_SCErrorSet(kSCStatusInvalidArgument);
	}
	return found;
}

Boolean
SCNetworkReachabilitySetDispatchQueue(SCNetworkReachabilityRef target, dispatch_queue_t queue)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;
	int				s;

	if (!isA_SCNetworkReachability(target)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}
	pthread_mutex_lock(&targetPrivate->lock);
	if (queue == NULL) {
		if (targetPrivate->queue != NULL) {
			unschedule(targetPrivate);
		}
		pthread_mutex_unlock(&targetPrivate->lock);
		return TRUE;
	}
	if (targetPrivate->rls != NULL || targetPrivate->queue != NULL) {
		pthread_mutex_unlock(&targetPrivate->lock);
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}
	s = openRoutingSocket();
	if (s < 0) {
		pthread_mutex_unlock(&targetPrivate->lock);
		_SCErrorSet(errno);
		return FALSE;
	}
	dispatch_retain(queue);
	targetPrivate->queue = queue;
	targetPrivate->source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)s, 0, queue);
	dispatch_source_set_event_handler(targetPrivate->source, ^{
		if (drainRoutingSocket(s)) {
			reachabilityChanged(targetPrivate);
		}
	});
	dispatch_source_set_cancel_handler(targetPrivate->source, ^{
		close(s);
	});
	dispatch_resume(targetPrivate->source);
	pthread_mutex_unlock(&targetPrivate->lock);
	initialReport(targetPrivate, queue);
	return TRUE;
}

#pragma mark SPI

/*
 * The addresses a name target resolves to, as CFData sockaddrs; NULL, with
 * the getaddrinfo(3) error in *error_num, if it does not resolve (or is not
 * a name target).
 */
CFArrayRef
SCNetworkReachabilityCopyResolvedAddress(SCNetworkReachabilityRef target, int *error_num)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;
	struct addrinfo			hints, *res, *ai;
	CFMutableArrayRef		list;
	int				err;

	if (error_num != NULL) {
		*error_num = 0;
	}
	if (!isA_SCNetworkReachability(target) || targetPrivate->type != kReachName) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG;
	err = getaddrinfo(targetPrivate->name, NULL, &hints, &res);
	if (err != 0) {
		if (error_num != NULL) {
			*error_num = err;
		}
		return NULL;
	}
	list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	for (ai = res; ai != NULL; ai = ai->ai_next) {
		CFDataRef	data;

		data = CFDataCreate(NULL, (const UInt8 *)ai->ai_addr, ai->ai_addrlen);
		CFArrayAppendValue(list, data);
		CFRelease(data);
	}
	freeaddrinfo(res);
	return list;
}

/* The index of the interface that reaches the target; -1 if it is not reachable. */
int
SCNetworkReachabilityGetInterfaceIndex(SCNetworkReachabilityRef target)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;
	char				ifname[IFNAMSIZ];
	int				index		= -1;

	if (!isA_SCNetworkReachability(target)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return -1;
	}
	if (targetPrivate->type == kReachName) {
		struct addrinfo	hints, *res, *ai;

		memset(&hints, 0, sizeof(hints));
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_ADDRCONFIG;
		if (getaddrinfo(targetPrivate->name, NULL, &hints, &res) != 0) {
			return -1;
		}
		for (ai = res; ai != NULL && index < 0; ai = ai->ai_next) {
			if ((addressFlagsIf(ai->ai_addr, targetPrivate->ifname, ifname) &
			     kSCNetworkReachabilityFlagsReachable) && ifname[0] != '\0') {
				index = (int)if_nametoindex(ifname);
			}
		}
		freeaddrinfo(res);
	} else {
		const struct sockaddr_storage *ss = targetPrivate->remote.ss_len != 0 ?
		    &targetPrivate->remote : &targetPrivate->local;

		if ((addressFlagsIf((const struct sockaddr *)ss, targetPrivate->ifname, ifname) &
		     kSCNetworkReachabilityFlagsReachable) && ifname[0] != '\0') {
			index = (int)if_nametoindex(ifname);
		}
	}
	return index;
}
