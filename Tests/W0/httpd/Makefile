# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Apache httpd, with APR and APR-util built inside it, for libsoup's tests
# that talk to an Apache server (auth, connection, range, proxy; gate W0).
# A test tool, not part of the system: it goes to
# /usr/local/tests/w0-tools/httpd with the modules libsoup's httpd.conf
# loads.

SUBDIR =
EXTLIB =	httpd
EXTLIB_TEST =	yes
EXTLIB_TEST_PATH = /usr/local/tests/w0-tools/httpd
EXTLIB_SUBPROJECTS = apr apr-util
EXTLIB_SUBPROJECTS_DIR = srclib
EXTLIB_PATCHES = 0001-server-test-char-on-the-build-machine.patch
EXTLIB_SYSTEM =	autotools
HTTPD =		${EXTLIB_TEST_PATH}
# What configure cannot find out when it builds for another system.
EXTLIB_ENV =	ac_cv_file__dev_zero=yes ac_cv_func_setpgrp_void=yes \
		apr_cv_process_shared_works=yes apr_cv_mutex_robust_shared=no \
		apr_cv_tcp_nodelay_with_cork=no ac_cv_sizeof_struct_iovec=16 \
		apr_cv_mutex_recursive=yes ap_cv_void_ptr_lt_long=no \
		ac_cv_strerror_r_rc_int=yes ac_cv_struct_rlimit=yes
EXTLIB_OPTIONS = --prefix=${HTTPD} --libdir=${HTTPD}/lib --exec-prefix=${HTTPD} \
		--with-included-apr --with-pcre=${.CURDIR}/pcre2-config \
		--with-expat=${MINKAR_SDKROOT}/usr --with-ssl --with-nghttp2 \
		--enable-mpms-shared=event --enable-modules=none \
		--enable-mods-shared="alias auth_basic auth_digest authn_core authn_file \
		    authz_core authz_host authz_user dir mime proxy proxy_http proxy_connect \
		    ssl unixd http2 log_config" \
		--disable-static
EXTLIB_TEST_FILES = ${HTTPD:S|^/||}/bin ${HTTPD:S|^/||}/modules ${HTTPD:S|^/||}/lib \
		${HTTPD:S|^/||}/conf

.include <minkar.extlib.mk>
