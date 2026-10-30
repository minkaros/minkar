MK_TESTS = no
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -fuse-ld=${LD}
.export CFLAGS LDFLAGS MK_TESTS

# Independent programs: built in parallel.
SUBDIR_PARALLEL =
# BIND links the OpenSSL that is staged there.
SUBDIR_DEPEND_bind =	openssl
SUBDIR = apply basename chroot cut dirname env false find getopt grep 
SUBDIR += head killall login make patch pfetch printf sed sw_vers 
SUBDIR += tail tee time touch true uname users whereis which xargs yes
SUBDIR += dscacheutil id
SUBDIR += cmp du gzip logger mktemp seq stat tr uniq uptime w wc
SUBDIR += awk btop less sort tar who
SUBDIR += diff nc netstat
SUBDIR += curl fetch file su top sudo
SUBDIR += defaults plutil ditto syslog
SUBDIR += printenv hexdump paste nohup xinstall
SUBDIR += m4 cpio bc zsh
SUBDIR += rev column comm join fold fmt split csplit tsort nl expand unexpand tty ncal getconf locale
SUBDIR += mandoc man
SUBDIR += jot lam rs col colrm look logname mkfifo renice nice lockf cksum bintrans what vis unvis xattr chflags units locate iconv git \
	calendar pr sdiff diff3 script whois ul mesg wall write banner compress \
	gencat cap_mkdb last indent ctags timeout passwd \
	uuidgen asa c99 finger newgrp pagesize unzip zip at \
	ipcs ipcrm htop builtin xxd nano bash rsync vim ksh llvm-tools chpass shlock mail lex telnet quota openssl bind \
	ftp rlogin arch vm_stat caffeinate lsbom talk localedef heimdal csrutil hdiutil fs_usage

.include <bsd.subdir.mk>
