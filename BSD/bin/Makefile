MK_TESTS = no
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -fuse-ld=${LD}
.export CFLAGS LDFLAGS MK_TESTS

# Independent programs: built in parallel.
SUBDIR_PARALLEL =
SUBDIR = cat chmod cp df echo hostname launchctl ln ls mkdir mv ps pwd realpath \
	stty date dd ed expr kill pax pkill \
	rm rmdir sleep sh test sync

.include <bsd.subdir.mk>
