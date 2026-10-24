MK_TESTS = no
CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -Wno-nullability-completeness
LDFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -fuse-ld=${LD}
.export CFLAGS LDFLAGS MK_TESTS

SUBDIR = apply basename chroot cut date dirname env false find getopt grep 
SUBDIR += head kill killall login patch pfetch printf sed sw_vers 
SUBDIR += tail tee time touch true uname users whereis which xargs yes
SUBDIR += dscacheutil id
SUBDIR += cmp du stat tr uniq wc
SUBDIR += awk less sort tar vi who
SUBDIR += diff nc netstat
SUBDIR += curl fetch file su top sudo
SUBDIR += defaults plutil ditto syslog

.include <bsd.subdir.mk>
