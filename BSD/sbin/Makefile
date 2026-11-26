# Independent programs: built in parallel.
SUBDIR_PARALLEL =
SUBDIR = dmesg fsck fsck_hfs quotacheck rtsol pfctl fsck_msdos newfs_msdos mount_msdos mount_nfs ifconfig launchd mount mount_hfs newfs_hfs dynamic_pager disklabel emond nfsd md5 minkarinit reboot \
	shutdown umount ping route mknod nologin hfs_util msdos_util autodiskmount fsck_exfat newfs_exfat mount_exfat exfat_util

# Optimised: the programs were built without any -O (the top Makefile's
# CFLAGS kept sys.mk's -O2 away).
CFLAGS += -O2
.export CFLAGS
.include <bsd.subdir.mk>
