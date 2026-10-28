/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The Citrus modules, linked into libiconv. FreeBSD loads each one from
 * /usr/lib/i18n with dlopen and looks up _citrus_<module>_<interface>_getops
 * in it; here a module is an entry of this table and the handle points to
 * it. mapper_parallel is mapper_serial's source, as in FreeBSD.
 */

#include <sys/types.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "citrus_namespace.h"
#include "citrus_types.h"
#include "citrus_module.h"

void	*_citrus_BIG5_stdenc_getops(void);
void	*_citrus_DECHanyu_stdenc_getops(void);
void	*_citrus_EUC_stdenc_getops(void);
void	*_citrus_EUCTW_stdenc_getops(void);
void	*_citrus_GBK2K_stdenc_getops(void);
void	*_citrus_HZ_stdenc_getops(void);
void	*_citrus_ISO2022_stdenc_getops(void);
void	*_citrus_JOHAB_stdenc_getops(void);
void	*_citrus_MSKanji_stdenc_getops(void);
void	*_citrus_UES_stdenc_getops(void);
void	*_citrus_UTF1632_stdenc_getops(void);
void	*_citrus_UTF7_stdenc_getops(void);
void	*_citrus_UTF8_stdenc_getops(void);
void	*_citrus_VIQR_stdenc_getops(void);
void	*_citrus_ZW_stdenc_getops(void);
void	*_citrus_iconv_none_iconv_getops(void);
void	*_citrus_iconv_std_iconv_getops(void);
void	*_citrus_mapper_646_mapper_getops(void);
void	*_citrus_mapper_none_mapper_getops(void);
void	*_citrus_mapper_serial_mapper_getops(void);
void	*_citrus_mapper_parallel_mapper_getops(void);
void	*_citrus_mapper_std_mapper_getops(void);
void	*_citrus_mapper_zone_mapper_getops(void);

struct _citrus_module_rec {
	const char	*name;		/* the module, e.g. "UTF8" */
	const char	*interface;	/* "stdenc", "iconv" or "mapper" */
	void		*(*getops)(void);
};

static const struct _citrus_module_rec modules[] = {
	{ "BIG5", "stdenc", _citrus_BIG5_stdenc_getops },
	{ "DECHanyu", "stdenc", _citrus_DECHanyu_stdenc_getops },
	{ "EUC", "stdenc", _citrus_EUC_stdenc_getops },
	{ "EUCTW", "stdenc", _citrus_EUCTW_stdenc_getops },
	{ "GBK2K", "stdenc", _citrus_GBK2K_stdenc_getops },
	{ "HZ", "stdenc", _citrus_HZ_stdenc_getops },
	{ "ISO2022", "stdenc", _citrus_ISO2022_stdenc_getops },
	{ "JOHAB", "stdenc", _citrus_JOHAB_stdenc_getops },
	{ "MSKanji", "stdenc", _citrus_MSKanji_stdenc_getops },
	{ "UES", "stdenc", _citrus_UES_stdenc_getops },
	{ "UTF1632", "stdenc", _citrus_UTF1632_stdenc_getops },
	{ "UTF7", "stdenc", _citrus_UTF7_stdenc_getops },
	{ "UTF8", "stdenc", _citrus_UTF8_stdenc_getops },
	{ "VIQR", "stdenc", _citrus_VIQR_stdenc_getops },
	{ "ZW", "stdenc", _citrus_ZW_stdenc_getops },
	{ "iconv_none", "iconv", _citrus_iconv_none_iconv_getops },
	{ "iconv_std", "iconv", _citrus_iconv_std_iconv_getops },
	{ "mapper_646", "mapper", _citrus_mapper_646_mapper_getops },
	{ "mapper_none", "mapper", _citrus_mapper_none_mapper_getops },
	{ "mapper_serial", "mapper", _citrus_mapper_serial_mapper_getops },
	{ "mapper_parallel", "mapper", _citrus_mapper_parallel_mapper_getops },
	{ "mapper_std", "mapper", _citrus_mapper_std_mapper_getops },
	{ "mapper_zone", "mapper", _citrus_mapper_zone_mapper_getops },
};

#define	NMODULES	(sizeof(modules) / sizeof(modules[0]))

void *
_citrus_find_getops(_citrus_module_t handle, const char *modname,
    const char *ifname)
{
	size_t i;

	if (handle == NULL)
		return (NULL);
	/* The module was found by name; the interface picks its getops. */
	for (i = 0; i < NMODULES; i++)
		if (strcmp(modules[i].name, modname) == 0 &&
		    strcmp(modules[i].interface, ifname) == 0)
			return ((void *)modules[i].getops);
	return (NULL);
}

int
_citrus_load_module(_citrus_module_t *rhandle, const char *encname)
{
	size_t i;

	for (i = 0; i < NMODULES; i++)
		if (strcmp(modules[i].name, encname) == 0) {
			*rhandle = (_citrus_module_t)&modules[i];
			return (0);
		}
	return (EINVAL);
}

void
_citrus_unload_module(_citrus_module_t handle)
{

	(void)handle;
}
