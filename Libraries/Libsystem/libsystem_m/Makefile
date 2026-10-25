# libsystem_m: the math library, FreeBSD's msun (BSD/lib/msun: BSD-2,
# BSD-3, MIT and Sun's "freely granted" notice), with the amd64 assembly
# and 80-bit long double sources, FreeBSD headers it needs (include/:
# complex.h, ieeefp.h, fpmath.h, x86 fpu; from git history, BSD), and glue.c (original, BSD-2) for the
# names ravynOS's <math.h> and Darwin have that msun does not. The
# previous implementation called itself for most functions.
# msun_compat.h and include/machine/asm.h supply FreeBSD's cdefs and asm
# macros; the assembly loses its ELF-only .note.GNU-stack section here.

SUBDIR =
MK_WERROR = no
LIB = system_m
MSUN = ${ROOT_SOURCE_DIR}/BSD/lib/msun

.PATH: ${.OBJDIR} ${MSUN}/amd64 ${MSUN}/x86 ${MSUN}/ld80 ${MSUN}/bsdsrc ${MSUN}/src

MSUN_ASM = e_fmod.S e_fmodf.S e_fmodl.S e_remainder.S e_remainderf.S e_remainderl.S \
	e_sqrt.S e_sqrtf.S e_sqrtl.S s_llrint.S s_llrintf.S s_llrintl.S \
	s_logbl.S s_lrint.S s_lrintf.S s_lrintl.S s_remquo.S s_remquof.S \
	s_remquol.S s_rintl.S s_scalbn.S s_scalbnf.S s_scalbnl.S

SRCS =	b_tgamma.c b_tgammal.c catrig.c catrigf.c catrigl.c e_acos.c \
	e_acosf.c e_acosh.c e_acoshf.c e_acoshl.c e_acosl.c e_asin.c \
	e_asinf.c e_asinl.c e_atan2.c e_atan2f.c e_atan2l.c e_atanh.c \
	e_atanhf.c e_atanhl.c e_cosh.c e_coshf.c e_coshl.c e_exp.c \
	e_expf.c e_gamma.c e_gamma_r.c e_gammaf.c e_gammaf_r.c e_hypot.c \
	e_hypotf.c e_hypotl.c e_j0.c e_j0f.c e_j1.c e_j1f.c \
	e_jn.c e_jnf.c e_lgamma.c e_lgamma_r.c e_lgammaf.c e_lgammaf_r.c \
	e_lgammal.c e_lgammal_r.c e_log.c e_log10.c e_log10f.c e_log2.c \
	e_log2f.c e_logf.c e_pow.c e_powf.c e_powl.c e_rem_pio2.c \
	e_rem_pio2f.c e_scalb.c e_scalbf.c e_sinh.c e_sinhf.c e_sinhl.c \
	fenv.c invtrig.c k_cos.c k_cosf.c k_cosl.c k_exp.c \
	k_expf.c k_rem_pio2.c k_sin.c k_sinf.c k_sinl.c k_tan.c \
	k_tanf.c k_tanl.c s_asinh.c s_asinhf.c s_asinhl.c s_atan.c \
	s_atanf.c s_atanl.c s_carg.c s_cargf.c s_cargl.c s_cbrt.c \
	s_cbrtf.c s_cbrtl.c s_ccosh.c s_ccoshf.c s_ceil.c s_ceilf.c \
	s_ceill.c s_cexp.c s_cexpf.c s_cexpl.c s_cimag.c s_cimagf.c \
	s_cimagl.c s_clog.c s_clogf.c s_clogl.c s_conj.c s_conjf.c \
	s_conjl.c s_copysign.c s_copysignf.c s_copysignl.c s_cos.c s_cosf.c \
	s_cosl.c s_cospi.c s_cospif.c s_cospil.c s_cpow.c s_cpowf.c \
	s_cpowl.c s_cproj.c s_cprojf.c s_cprojl.c s_creal.c s_crealf.c \
	s_creall.c s_csinh.c s_csinhf.c s_csqrt.c s_csqrtf.c s_csqrtl.c \
	s_ctanh.c s_ctanhf.c s_erf.c s_erff.c s_erfl.c s_exp2.c \
	s_exp2f.c s_exp2l.c s_expl.c s_expm1.c s_expm1f.c s_fabs.c \
	s_fabsf.c s_fabsl.c s_fdim.c s_finite.c s_finitef.c s_floor.c \
	s_floorf.c s_floorl.c s_fma.c s_fmaf.c s_fmal.c s_fmax.c \
	s_fmaxf.c s_fmaxl.c s_fmin.c s_fminf.c s_fminl.c s_frexp.c \
	s_frexpf.c s_frexpl.c s_ilogb.c s_ilogbf.c s_ilogbl.c s_isfinite.c \
	s_isnan.c s_isnormal.c s_llround.c s_llroundf.c s_llroundl.c s_log1p.c \
	s_log1pf.c s_logb.c s_logbf.c s_logl.c s_lround.c s_lroundf.c \
	s_lroundl.c s_modf.c s_modff.c s_modfl.c s_nan.c s_nanl.c \
	s_nearbyint.c s_nextafter.c s_nextafterf.c s_nextafterl.c s_nexttoward.c s_nexttowardf.c \
	s_rint.c s_rintf.c s_round.c s_roundf.c s_roundl.c s_scalbln.c \
	s_signbit.c s_signgam.c s_significand.c s_significandf.c s_sin.c s_sincos.c \
	s_sincosf.c s_sincosl.c s_sinf.c s_sinl.c s_sinpi.c s_sinpif.c \
	s_sinpil.c s_tan.c s_tanf.c s_tanh.c s_tanhf.c s_tanhl.c \
	s_tanl.c s_tanpi.c s_tanpif.c s_tanpil.c s_tgammaf.c s_trunc.c \
	s_truncf.c s_truncl.c w_cabs.c w_cabsf.c w_cabsl.c w_drem.c \
	w_dremf.c \
	${MSUN_ASM:S/^/rv_/} glue.c

CFLAGS = -mmacos-version-min=${MACOS_VERSION_MIN} --sysroot=${RAVYN_SDKROOT} \
	-Wno-nullability-completeness -fno-math-errno -fno-builtin \
	-I${.CURDIR}/include -I${MSUN}/src -I${MSUN}/x86 -I${MSUN}/ld80 \
	-I${MSUN}/amd64 -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include \
	-include ${.CURDIR}/msun_compat.h
ACFLAGS = -mmacos-version-min=${MACOS_VERSION_MIN} --sysroot=${RAVYN_SDKROOT} \
	-I${.CURDIR}/include
LDFLAGS = -mmacos-version-min=${MACOS_VERSION_MIN} --sysroot=${RAVYN_SDKROOT} \
	-Wl,-install_name,/usr/lib/system/libsystem_m.dylib \
	-Wl,-current_version,3178 -Wl,-compatibility_version,1 \
	-Wl,-all_load -umbrella System

.for s in ${MSUN_ASM}
rv_${s}: ${MSUN}/amd64/${s}
	sed -e '/note.GNU-stack/d' -e 's/^#include "s_/#include "rv_s_/' \
	    ${.ALLSRC} > ${.TARGET}
.endfor

# These two #include their generated lrint counterparts.
rv_s_llrint.o: rv_s_lrint.S
rv_s_llrintf.o: rv_s_lrintf.S

all: ${.OBJDIR}/lib${LIB}.dylib

${.OBJDIR}/lib${LIB}.dylib: lib${LIB}.a
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
	  -Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
	cp -f ${.TARGET} ${RAVYN_SDKROOT}/usr/lib/system/
	cp -f ${.TARGET} ${SYSROOT_DIR}/usr/lib/system/

.include <bsd.lib.mk>
