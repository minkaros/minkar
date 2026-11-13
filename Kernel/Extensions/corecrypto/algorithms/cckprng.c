/* Modifications Copyright (c) 2026 The Minkar contributors */
#include <stddef.h>
#include <corecrypto/cckprng.h>
#include <corecrypto/ccrng.h>
#include "yarrow/yarrow.h"
#if !KERNEL
#include <stdint.h>
#endif
#if KERNEL
#include <kern/locks.h>
#include <kern/debug.h>

/*
 * Minkar: Yarrow keeps one state and is not safe from several threads,
 * and the kernel asks for random bytes on every CPU at once. Two calls
 * at once could push its output index past its end; from then on every
 * prngOutput failed, unchecked, and the callers (getentropy,
 * /dev/random, arc4random) got whatever their buffer held. Each use of
 * a state now holds this lock, and a failure stops the system rather
 * than hand out bytes that are not random.
 */
static lck_grp_t *cc_prng_lock_group;
static lck_spin_t *cc_prng_lock;

void cckprng_lock_init(void);
void cckprng_lock_init(void) {
	if (cc_prng_lock == NULL) {
		cc_prng_lock_group = lck_grp_alloc_init("corecrypto prng", LCK_GRP_ATTR_NULL);
		cc_prng_lock = lck_spin_alloc_init(cc_prng_lock_group, LCK_ATTR_NULL);
	}
}

#define PRNG_LOCK() do { if (cc_prng_lock != NULL) lck_spin_lock(cc_prng_lock); } while (0)
#define PRNG_UNLOCK() do { if (cc_prng_lock != NULL) lck_spin_unlock(cc_prng_lock); } while (0)
#define PRNG_CHECK(e) do { prng_error_status _e = (e); if (_e != PRNG_SUCCESS) panic("corecrypto: the PRNG failed (%d)", (int)_e); } while (0)
#else
#define PRNG_LOCK() do { } while (0)
#define PRNG_UNLOCK() do { } while (0)
#define PRNG_CHECK(e) ((void)(e))
#endif

void cckprng_init(struct cckprng_ctx *ctx, unsigned max_ngens, size_t entropybuf_nbytes, const void *entropybuf,
				  const uint32_t *entropybuf_nsamples, size_t seed_nbytes, const void *seed, size_t nonce_nbytes,
				  const void *nonce) {
	prngInitialize(&ctx->prng);
	ctx->bytes_generated = ctx->bytes_since_entropy = 0;

	cckprng_reseed(ctx, seed_nbytes, seed);
	cckprng_reseed(ctx, nonce_nbytes, nonce);
	cckprng_reseed(ctx, entropybuf_nbytes, entropybuf);
}

void cckprng_init_with_getentropy(struct cckprng_ctx *ctx, unsigned max_ngens, size_t seed_nbytes, const void *seed,
								  size_t nonce_nbytes, const void *nonce, cckprng_getentropy getentropy,
								  void *getentropy_arg) {
	uint8_t entropy[32];
	size_t entropy_nbytes = sizeof(entropy);

	cckprng_init(ctx, max_ngens, 0, NULL, NULL, seed_nbytes, seed, nonce_nbytes, nonce);

	if (getentropy && getentropy(&entropy_nbytes, entropy, getentropy_arg) == 0) {
		if (entropy_nbytes > sizeof(entropy)) {
			entropy_nbytes = sizeof(entropy);
		}
		cckprng_reseed(ctx, entropy_nbytes, entropy);
	}
}

void cckprng_initgen(struct cckprng_ctx *ctx, unsigned gen_idx) {
	// Nothing is needed here.
}

void cckprng_reseed(struct cckprng_ctx *ctx, size_t nbytes, const void *seed) {
	if (nbytes == 0 || seed == NULL) {
		return;
	}

	PRNG_LOCK();
	prngInput(ctx->prng, (BYTE *)seed, (UINT)nbytes, 0, 0);
	prngAllowReseed(ctx->prng, 5000);
	ctx->bytes_since_entropy = 0;
	PRNG_UNLOCK();
}

void cckprng_refresh(struct cckprng_ctx *ctx) {
	// Nothing is needed here, either.
}

void cckprng_generate(struct cckprng_ctx *ctx, unsigned gen_idx, size_t nbytes, void *out) {
	BYTE *buffer = (BYTE *)out;
	PRNG_LOCK();
	while (nbytes > UINT32_MAX) {
		PRNG_CHECK(prngOutput(ctx->prng, buffer, UINT32_MAX));
		ctx->bytes_generated += UINT32_MAX;
		ctx->bytes_since_entropy += UINT32_MAX;

		buffer += UINT32_MAX;
		nbytes -= UINT32_MAX;
		nbytes = CC_MAX(nbytes, 0);
	}

	if (nbytes != 0) {
		PRNG_CHECK(prngOutput(ctx->prng, buffer, (UINT)nbytes));
		ctx->bytes_generated += nbytes;
		ctx->bytes_since_entropy += nbytes;
	}
	PRNG_UNLOCK();
}

// MARK: -

struct ccrng_impl {
	CCRNG_STATE_COMMON;
	PrngRef prng;
};

static int ccrng_generate_impl(struct ccrng_state *rng, size_t outlen, void *out) {
	struct ccrng_impl *impl = (struct ccrng_impl *)rng;
	PRNG_LOCK();
	PRNG_CHECK(prngOutput(impl->prng, out, (UINT)outlen));
	PRNG_UNLOCK();
	return 0;
}

struct ccrng_state *ccrng(int *error) {
	static struct ccrng_impl ccrng_impl = { ccrng_generate_impl, NULL };
	static bool initialized = false;

	if (!initialized) {
		prngInitialize(&ccrng_impl.prng);
		initialized = true;
	}

	return (struct ccrng_state *)&ccrng_impl;
}
