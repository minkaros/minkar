MK_TESTS = no
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -fuse-ld=${LD}
.export CFLAGS LDFLAGS MK_TESTS

# Independent programs: built in parallel.
SUBDIR_PARALLEL =
SUBDIR = notifyd configd configd/scutil.tproj chown ipconfig mDNSResponder/minkar sysctl traceroute \
	syslogd aslmanager cron/cron cron/crontab lsof zic zdump ac arp ndp traceroute6 newsyslog \
	mtree vipw pwd_mkdb accton sa fstyp vifs quotaon edquota repquota tcpdump fdisk \
	tzlink purge dot_clean ioreg nvram

.include <bsd.subdir.mk>
