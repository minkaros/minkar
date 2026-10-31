MK_WERROR = no
SUBDIR =
KEXT = IOACPIFamily
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = com.apple.iokit.IOACPIFamily
BUNDLE_VERSION = 1.0.0
KERNEL_PRIVATE = yes

SRCS = ACPIRTC.cpp SMBIOS.cpp \
	EFINVRAM.cpp \
	ACPICPU.cpp \
	ACPIPCIBridge.cpp \
	ACPIPlatformExpert.cpp \
	IOACPIPlatformDevice.cpp \
	util.cpp \
	ACPIOSLayer.cpp \
	ACPIInterpreter.cpp \
	ACPIPowerButton.cpp

# ACPICA (Intel, BSD-3 option; restored from this repository's history):
# the AML interpreter, without its debugger and disassembler. See
# acpica/README.minkar.
ACPICA =	${.CURDIR}/acpica/contrib/dev/acpica
.PATH: ${ACPICA}/components/dispatcher ${ACPICA}/components/events ${ACPICA}/components/executer ${ACPICA}/components/hardware ${ACPICA}/components/namespace ${ACPICA}/components/parser ${ACPICA}/components/resources ${ACPICA}/components/tables ${ACPICA}/components/utilities
SRCS +=	dsargs.c dscontrol.c dsdebug.c dsfield.c dsinit.c dsmethod.c \
	dsmthdat.c dsobject.c dsopcode.c dspkginit.c dsutils.c dswexec.c \
	dswload.c dswload2.c dswscope.c dswstate.c evevent.c evglock.c \
	evgpe.c evgpeblk.c evgpeinit.c evgpeutil.c evhandler.c evmisc.c \
	evregion.c evrgnini.c evsci.c evxface.c evxfevnt.c evxfgpe.c \
	evxfregn.c exconcat.c exconfig.c exconvrt.c excreate.c exdebug.c \
	exdump.c exfield.c exfldio.c exmisc.c exmutex.c exnames.c \
	exoparg1.c exoparg2.c exoparg3.c exoparg6.c exprep.c exregion.c \
	exresnte.c exresolv.c exresop.c exserial.c exstore.c exstoren.c \
	exstorob.c exsystem.c extrace.c exutils.c hwacpi.c hwesleep.c \
	hwgpe.c hwpci.c hwregs.c hwsleep.c hwtimer.c hwvalid.c \
	hwxface.c hwxfsleep.c nsaccess.c nsalloc.c nsarguments.c nsconvert.c \
	nsdump.c nsdumpdv.c nseval.c nsinit.c nsload.c nsnames.c \
	nsobject.c nsparse.c nspredef.c nsprepkg.c nsrepair.c nsrepair2.c \
	nssearch.c nsutils.c nswalk.c nsxfeval.c nsxfname.c nsxfobj.c \
	psargs.c psloop.c psobject.c psopcode.c psopinfo.c psparse.c \
	psscope.c pstree.c psutils.c pswalk.c psxface.c rsaddr.c \
	rscalc.c rscreate.c rsdumpinfo.c rsinfo.c rsio.c rsirq.c \
	rslist.c rsmemory.c rsmisc.c rsserial.c rsutils.c rsxface.c \
	tbdata.c tbfadt.c tbfind.c tbinstal.c tbprint.c tbutils.c \
	tbxface.c tbxfload.c tbxfroot.c utaddress.c utalloc.c utascii.c \
	utbuffer.c utcache.c utcksum.c utcopy.c utdebug.c utdecode.c \
	utdelete.c uterror.c uteval.c utexcep.c utglobal.c uthex.c \
	utids.c utinit.c utlock.c utmath.c utmisc.c utmutex.c \
	utnonansi.c utobject.c utosi.c utownerid.c utpredef.c utresdecode.c \
	utresrc.c utstate.c utstring.c utstrsuppt.c utstrtoul64.c uttrack.c \
	utuuid.c utxface.c utxferror.c utxfinit.c utxfmutex.c

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-I${.CURDIR}/include \
	-I${.CURDIR}/acpica \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/EXTERNAL_HEADERS \
	-I${.CURDIR:H}/IOPCIFamily \
	-I${.CURDIR:H}/AppleAPIC \
	-I${.CURDIR:H}/MinkarAHCIPort \
	-I${.CURDIR:H}/ApplePS2Controller

LDFLAGS = -mmacos-version-min=${MACOS_VERSION_MIN} --sysroot=${MINKAR_SDKROOT} \
	-fuse-ld=${LD}

.include <minkar.kext.mk>
