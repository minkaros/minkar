MK_TESTS = no
# Optimised: the programs were built without any -O (the top Makefile's
# CFLAGS kept sys.mk's -O2 away).
CFLAGS = -O2 --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -fuse-ld=${LD}
.export CFLAGS LDFLAGS MK_TESTS

# Independent programs: built in parallel.
SUBDIR_PARALLEL =
SUBDIR = cat chmod cp csh dash df echo wait4path hostname launchctl ln ls mkdir mv ps pwd realpath \
	stty date dd ed expr kill pax pkill \
	rm rmdir sleep sh shlauncher test sync

.include <bsd.subdir.mk>
