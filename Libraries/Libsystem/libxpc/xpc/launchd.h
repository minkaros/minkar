
#ifndef XPC_LAUNCHD_H_
#define XPC_LAUNCHD_H_
#include <errno.h>
#include <xpc/xpc.h>

#define EXNOERROR	0
#define EXNOMEM		1
#define EXINVAL		2
#define EXSRCH      3
#define EXMAX		EXSRCH

/* These are almost certainly the wrong values
 * FIXME: print out the values from Catalina using a test app
 */
enum {
    XPC_PIPE_PRIVILEGED = (1ULL << 0),
    XPC_PIPE_USE_SYNC_IPC_OVERRIDE = (1ULL << 1),
};
/* The names libinfo uses: a service only a privileged (root) job holds. */
#define XPC_PIPE_FLAG_PRIVILEGED XPC_PIPE_PRIVILEGED

typedef xpc_object_t xpc_pipe_t;

xpc_pipe_t xpc_pipe_create(const char *name, uint64_t flags);
void xpc_pipe_invalidate(xpc_pipe_t pipe);
int xpc_pipe_routine(xpc_pipe_t pipe, xpc_object_t msg, xpc_object_t *reply);
xpc_pipe_t xpc_pipe_create_from_port(mach_port_t port, uint64_t flags);

/* Minkar: a pipe's server side. A request (a dictionary) and where to
   answer it; the reply port's send right is used up by the answer. */
int xpc_pipe_receive_request(mach_port_t port, xpc_object_t *request, mach_port_t *reply_port, uint64_t *id);
int xpc_pipe_send_reply(xpc_object_t reply, mach_port_t reply_port, uint64_t id);

const char *xpc_strerror(int error);
xpc_object_t xpc_copy_entitlement_for_token(const char *, audit_token_t *);
int xpc_pipe_routine_reply(xpc_object_t);
int xpc_pipe_try_receive(mach_port_t, xpc_object_t *, mach_port_t *,
    boolean_t (*)(mach_msg_header_t *, mach_msg_header_t *), mach_msg_size_t, int);
kern_return_t xpc_call_wakeup(mach_port_t, int);
void xpc_dictionary_get_audit_token(xpc_object_t, audit_token_t *);
void xpc_dictionary_set_mach_recv(xpc_object_t, const char *, mach_port_t);
void xpc_dictionary_set_mach_send(xpc_object_t, const char *, mach_port_t);
mach_port_t xpc_dictionary_copy_mach_send(xpc_object_t, const char *);
mach_port_t xpc_dictionary_extract_mach_recv(xpc_object_t, const char *);
xpc_object_t xpc_copy_entitlements_for_pid(pid_t);
xpc_object_t ld2xpc(launch_data_t);

xpc_object_t xpc_create_from_plist(uint8_t *buf, size_t length);

#endif
