#ifndef _SHIM_H_
#define _SHIM_H_

/*
 * The private interface between launchd and libxpc (event and process
 * routines, service types, the domain MIG routines). On macOS these come
 * from libxpc's private headers, which ravynOS's libxpc does not ship.
 * Everything else launchd needs comes from the Darwin headers.
 */
#include <mach/mach_types.h>
#include <spawn.h>

/* From <bsm/audit_session.h>, which arrives with libbsm (not built yet). */
#ifndef AU_SESSION_FLAG_IS_INITIAL
#define AU_SESSION_FLAG_IS_INITIAL	0x0001
#endif

typedef unsigned int xpc_service_type_t;

typedef int xpc_jetsam_band_t;

typedef char event_name_t[64];






#define XPC_EVENT_ROUTINE_KEY_STREAM "XPC key stream"
#define XPC_EVENT_ROUTINE_KEY_TOKEN "XPC key token"
#define XPC_EVENT_ROUTINE_KEY_NAME "XPC key name"
#define XPC_EVENT_ROUTINE_KEY_ENTITLEMENTS "XPC key entitlements"
#define XPC_EVENT_ROUTINE_KEY_EVENT "XPC key event"
#define XPC_EVENT_ROUTINE_KEY_EVENTS "XPC key events"
#define XPC_EVENT_ROUTINE_KEY_FLAGS "XPC key flags"
#define XPC_EVENT_ROUTINE_KEY_PORT "XPC key port"
#define XPC_EVENT_ROUTINE_KEY_STATE "XPC key state"
#define XPC_EVENT_ROUTINE_KEY_OP "XPC key op"
#define XPC_EVENT_ROUTINE_KEY_ERROR "XPC key error"
#define XPC_PROCESS_ROUTINE_KEY_LABEL "XPC process key label"
#define XPC_PROCESS_ROUTINE_KEY_ERROR "XPC process key error"
#define XPC_PROCESS_ROUTINE_KEY_HANDLE "XPC process key handle"
#define XPC_PROCESS_ROUTINE_KEY_NAME "XPC process key name"
#define XPC_PROCESS_ROUTINE_KEY_PATH "XPC process key path"
#define XPC_PROCESS_ROUTINE_KEY_ARGV "XPC process key argv"
#define XPC_PROCESS_ROUTINE_KEY_TYPE "XPC process key type"
#define XPC_PROCESS_ROUTINE_KEY_OP "XPC process key op"
#define XPC_PROCESS_ROUTINE_KEY_PID "XPC process key pid"
#define XPC_PROCESS_ROUTINE_KEY_RCDATA "XPC process key rcdata"
#define XPC_PROCESS_ROUTINE_KEY_SIGNAL "XPC process key signal"
#define XPC_PROCESS_ROUTINE_KEY_PRIORITY_BAND "XPC process key priority band"
#define XPC_PROCESS_ROUTINE_KEY_MEMORY_LIMIT "XPC process key memory limit"
#define XPC_SERVICE_ENTITLEMENT_ATTACH "XPC service entitlement attach"
#define XPC_SERVICE_RENDEZVOUS_TOKEN "XPC service rendezvous token"
#define XPC_PROCESS_ROUTINE_KEY_NEW_INSTANCE_PORT "XPC process key new instance port"
#define XPC_SERVICE_ENV_ATTACHED "XPC service env attached"

#ifndef _POSIX_SPAWN_DISABLE_ASLR
#define _POSIX_SPAWN_DISABLE_ASLR 0x8000
#endif

#define XPC_EVENT_FLAG_ENTITLEMENTS 0x000000000000

typedef enum {
	XPC_EVENT_GET_NAME,
	XPC_EVENT_SET,
	XPC_EVENT_COPY,
	XPC_EVENT_CHECK_IN,
	XPC_EVENT_LOOK_UP,
	XPC_EVENT_PROVIDER_CHECK_IN,
	XPC_EVENT_PROVIDER_SET_STATE,
	XPC_EVENT_COPY_ENTITLEMENTS
} xpc_event_t;

typedef enum {
	XPC_PROCESS_JETSAM_SET_BAND,
	XPC_PROCESS_JETSAM_SET_MEMORY_LIMIT,
	XPC_PROCESS_SERVICE_ATTACH,
	XPC_PROCESS_SERVICE_DETACH,
	XPC_PROCESS_SERVICE_GET_PROPERTIES,
	XPC_PROCESS_SERVICE_KILL
} xpc_jetsam_t;

/*
 * Jetsam bands, lowest priority first. Only launchd interprets them (it maps
 * each to a kernel JETSAM_PRIORITY_* in _launchd_priority_map), so the
 * values are ours; they only need to keep this order.
 */
#define XPC_JETSAM_BAND_SUSPENDED			10
#define XPC_JETSAM_BAND_BACKGROUND_OPPORTUNISTIC	20
#define XPC_JETSAM_BAND_BACKGROUND			30
#define XPC_JETSAM_BAND_MAIL				40
#define XPC_JETSAM_BAND_PHONE				50
#define XPC_JETSAM_BAND_UI_SUPPORT			60
#define XPC_JETSAM_BAND_FOREGROUND_SUPPORT		70
#define XPC_JETSAM_BAND_FOREGROUND			80
#define XPC_JETSAM_BAND_AUDIO				90
#define XPC_JETSAM_BAND_ACCESSORY			100
#define XPC_JETSAM_BAND_CRITICAL			110
#define XPC_JETSAM_BAND_TELEPHONY			120
#define XPC_JETSAM_BAND_LAST				130
#define XPC_JETSAM_PRIORITY_RESERVED 1000

#define XPC_SERVICE_TYPE_BUNDLED 0xBADDCAFE
#define XPC_SERVICE_TYPE_LAUNCHD 0xBABECAFE
#define XPC_SERVICE_TYPE_APP     0xDEADCAFE
#define XPC_SERVICE_TYPE_ENTITLEMENT_ATTACH     0x00DEAD00

/* domain.defs */
kern_return_t
xpc_domain_get_service_name(job_t j, event_name_t name);
kern_return_t
xpc_domain_load_services(job_t j, vm_offset_t services_buff, mach_msg_type_number_t services_sz);
kern_return_t
xpc_domain_check_in(job_t j, mach_port_t *bsport, mach_port_t *sbsport,
	mach_port_t *excport, mach_port_t *asport, uint32_t *uid, uint32_t *gid,
					int32_t *asid, vm_offset_t *ctx, mach_msg_type_number_t *ctx_sz);
kern_return_t
xpc_domain_set_environment(job_t j, mach_port_t rp, mach_port_t bsport, mach_port_t excport, vm_offset_t ctx, mach_msg_type_number_t ctx_sz);
kern_return_t
xpc_domain_import2(job_t j, mach_port_t reqport, mach_port_t dport);
kern_return_t
xpc_domain_add_services(job_t j, vm_offset_t services_buff, mach_msg_type_number_t services_sz);
#define XPC_LPI_VERSION 20141120


#endif
