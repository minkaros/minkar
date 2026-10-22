/* ccrng() mínimo para ravynOS: entropía vía getentropy(). */
#include <stddef.h>
int getentropy(void *buffer, size_t size);  /* libsystem_kernel */

struct ccrng_state {
    int (*generate)(struct ccrng_state *rng, size_t outlen, void *out);
};

static int ravyn_ccrng_generate(struct ccrng_state *rng, size_t outlen, void *out)
{
    unsigned char *p = out;
    (void)rng;
    while (outlen > 0) {
        size_t n = outlen > 256 ? 256 : outlen;   /* límite de getentropy */
        if (getentropy(p, n) != 0)
            return -1;
        p += n;
        outlen -= n;
    }
    return 0;
}

static struct ccrng_state ravyn_ccrng_state = { ravyn_ccrng_generate };

struct ccrng_state *ccrng(int *error)
{
    if (error)
        *error = 0;
    return &ravyn_ccrng_state;
}
