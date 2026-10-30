/*
 * Copyright (c) 2000-2019 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_FREE_COPYRIGHT@
 *
 */
/*
 * @APPLE_FREE_COPYRIGHT@
 */
/*
 *	NetBSD: ite.c,v 1.16 1995/07/17 01:24:34 briggs Exp
 *
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: ite.c 1.28 92/12/20$
 *
 *	@(#)ite.c	8.2 (Berkeley) 1/12/94
 */

/*
 * ite.c
 *
 * The ite module handles the system console; that is, stuff printed
 * by the kernel and by user programs while "desktop" and X aren't
 * running.  Some (very small) parts are based on hp300's 4.4 ite.c,
 * hence the above copyright.
 *
 *   -- Brad and Lawrence, June 26th, 1994
 *
 */

#include <console/video_console.h>
#include <console/serial_protos.h>

#include <kern/kern_types.h>
#include <kern/kalloc.h>
#include <kern/debug.h>
#include <kern/spl.h>
#include <kern/thread_call.h>

#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <machine/io_map_entries.h>
#include <machine/machine_cpu.h>

#include <pexpert/pexpert.h>
#include <sys/kdebug.h>

#include "iso_font.c"
#if !CONFIG_EMBEDDED
#include "progress_meter_data.c"
#endif

#include "sys/msgbuf.h"

/*
 * Generic Console (Front-End)
 * ---------------------------
 */

struct vc_info vinfo;

void noroot_icon_test(void);


extern int       disableConsoleOutput;
static boolean_t gc_enabled     = FALSE;
static boolean_t gc_initialized = FALSE;
static boolean_t vm_initialized = FALSE;

/*
 * Minkar (2026): what a console cell holds. Below 0x100, a character of
 * the ISO 8859-1 font; above, a Unicode character drawn by code (box
 * lines, blocks, Braille), see gc_glyph_bitmap.
 */
typedef uint16_t gc_glyph_t;

static struct {
	void (*initialize)(struct vc_info * info);
	void (*enable)(boolean_t enable);
	void (*paint_char)(unsigned int xx, unsigned int yy, gc_glyph_t ch,
			   int attrs, gc_glyph_t ch_previous,
			   int attrs_previous);
	void (*clear_screen)(unsigned int xx, unsigned int yy, unsigned int top,
			     unsigned int bottom, int which);
	void (*scroll_down)(int num, unsigned int top, unsigned int bottom);
	void (*scroll_up)(int num, unsigned int top, unsigned int bottom);
	void (*hide_cursor)(unsigned int xx, unsigned int yy);
	void (*show_cursor)(unsigned int xx, unsigned int yy);
	void (*update_color)(int color, boolean_t fore);
} gc_ops;

static unsigned char *gc_buffer_attributes;
static gc_glyph_t *gc_buffer_characters;
static unsigned char *gc_buffer_colorcodes;
static unsigned char *gc_buffer_tab_stops;
static uint32_t gc_buffer_columns;
static uint32_t gc_buffer_rows;
static uint32_t gc_buffer_size;

#if defined(__i386__) || defined(__x86_64__)
decl_simple_lock_data(static, vcputc_lock);

#define VCPUTC_LOCK_INIT()				\
MACRO_BEGIN						\
	simple_lock_init(&vcputc_lock, 0);		\
MACRO_END

#define VCPUTC_LOCK_LOCK()				\
MACRO_BEGIN						\
	boolean_t istate = ml_get_interrupts_enabled();	\
	while (!simple_lock_try(&vcputc_lock, LCK_GRP_NULL))	\
	{						\
		if (!istate)				\
			handle_pending_TLB_flushes();	\
		cpu_pause();				\
	}						\
MACRO_END

#define VCPUTC_LOCK_UNLOCK()				\
MACRO_BEGIN						\
	simple_unlock(&vcputc_lock);	\
MACRO_END
#else
static hw_lock_data_t vcputc_lock;

#define VCPUTC_LOCK_INIT()				\
MACRO_BEGIN						\
	hw_lock_init(&vcputc_lock);			\
MACRO_END

#define VCPUTC_LOCK_LOCK()				\
MACRO_BEGIN						\
	if (!hw_lock_to(&vcputc_lock, ~0U, LCK_GRP_NULL))\
	{						\
		panic("VCPUTC_LOCK_LOCK");		\
	}						\
MACRO_END

#define VCPUTC_LOCK_UNLOCK()				\
MACRO_BEGIN						\
	hw_lock_unlock(&vcputc_lock);	\
MACRO_END
#endif

/*
# Attribute codes:
# 00=none 01=bold 04=underscore 05=blink 07=reverse 08=concealed
# Text color codes:
# 30=black 31=red 32=green 33=yellow 34=blue 35=magenta 36=cyan 37=white
# Background color codes:
# 40=black 41=red 42=green 43=yellow 44=blue 45=magenta 46=cyan 47=white
*/

#define ATTR_NONE	0
#define ATTR_BOLD	1
#define ATTR_UNDER	2
#define ATTR_REVERSE	4

#define COLOR_BACKGROUND 0
#define COLOR_FOREGROUND 7

#define COLOR_CODE_GET(code, fore)        (((code) & ((fore) ? 0xF0 : 0x0F))            >> ((fore) ? 4 : 0))
#define COLOR_CODE_SET(code, color, fore) (((code) & ((fore) ? 0x0F : 0xF0)) | ((color) << ((fore) ? 4 : 0)))

static unsigned char gc_color_code;

/* VT100 state: */
#define MAXPARS	16
static unsigned int gc_x, gc_y, gc_savex, gc_savey;
static unsigned int gc_par[MAXPARS], gc_numpars, gc_hanging_cursor, gc_attr, gc_saveattr;

/* VT100 scroll region */
static unsigned int gc_scrreg_top, gc_scrreg_bottom;

enum vt100state_e {
	ESnormal,		/* Nothing yet                             */
	ESesc,			/* Got ESC                                 */
	ESsquare,		/* Got ESC [				   */
	ESgetpars,		/* About to get or getting the parameters  */
	ESgotpars,		/* Finished getting the parameters         */
	ESfunckey,		/* Function key                            */
	EShash,			/* DEC-specific stuff (screen align, etc.) */
	ESsetG0,		/* Specify the G0 character set            */
	ESsetG1,		/* Specify the G1 character set            */
	ESask,
	EScharsize,
	ESignore,		/* Ignore this sequence                    */
	ESstring		/* Minkar: OSC/DCS/APC/PM string, skipped  */
} gc_vt100state = ESnormal;

/*
 * Minkar (2026): sequences the console does not draw are skipped whole, as
 * terminals do: a control string (ESC ] ... BEL or ESC \, and ESC P, ESC _,
 * ESC ^) and a CSI sequence with a private marker (< = >) or intermediate
 * bytes (space to /), up to its final byte. vim, for one, asks xterm for
 * its colors (ESC ] 10 ; ? BEL) and sets keyboard modes (ESC [ > 4 ; m);
 * their parameters were drawn as text. A string longer than
 * GC_STRING_MAX ends anyway, so that a lost terminator cannot hide output.
 */
#define GC_STRING_MAX	512
static unsigned int gc_string_len;


enum
{
    /* secs */
    kProgressAcquireDelay   = 0,
#if CONFIG_EMBEDDED
    kProgressReacquireDelay = 5,
#else
    kProgressReacquireDelay = 5,
#endif
};

static int8_t vc_rotate_matr[4][2][2] = {
  { {  1,  0 },
    {  0,  1 } },
  { {  0,  1 },
    { -1,  0 } },
  { { -1,  0 },
    {  0, -1 } },
  { {  0, -1 },
    {  1,  0 } }
};

static int gc_wrap_mode = 1, gc_relative_origin = 0;
/*
 * Minkar (2026): the last character painted (for REP, CSI b) and the UTF-8
 * sequence being decoded (gc_utf8_need bytes still to come).
 */
static gc_glyph_t gc_last_char = ' ';
static uint32_t gc_utf8_cp;
static int gc_utf8_need;
static int gc_charset_select = 0, gc_save_charset_s = 0;
static int gc_charset[2] = { 0, 0 };
static int gc_charset_save[2] = { 0, 0 };

static void gc_clear_line(unsigned int xx, unsigned int yy, int which);
static void gc_clear_screen(unsigned int xx, unsigned int yy, int top,
		unsigned int bottom, int which);
static void gc_enable(boolean_t enable);
static void gc_hide_cursor(unsigned int xx, unsigned int yy);
static void gc_initialize(struct vc_info * info);
static boolean_t gc_is_tab_stop(unsigned int column);
static void gc_paint_char(unsigned int xx, unsigned int yy, gc_glyph_t ch,
		int attrs);
static void gc_putchar(char ch);
static void gc_putc_askcmd(unsigned char ch);
static void gc_putc_charsetcmd(int charset, unsigned char ch);
static void gc_putc_charsizecmd(unsigned char ch);
static void gc_putc_esc(unsigned char ch);
static void gc_putc_getpars(unsigned char ch);
static void gc_putc_gotpars(unsigned char ch);
static void gc_putc_normal(gc_glyph_t ch);
static void gc_putc_square(unsigned char ch);
static void gc_reset_screen(void);
static void gc_reset_tabs(void);
static void gc_reset_vt100(void);
static void gc_scroll_down(int num, unsigned int top, unsigned int bottom);
static void gc_scroll_up(int num, unsigned int top, unsigned int bottom);
static void gc_set_tab_stop(unsigned int column, boolean_t enabled);
static void gc_show_cursor(unsigned int xx, unsigned int yy);
static void gc_update_color(int color, boolean_t fore);

static void
gc_clear_line(unsigned int xx, unsigned int yy, int which)
{
	unsigned int start, end, i;

	/*
	 * This routine runs extremely slowly.  I don't think it's
	 * used all that often, except for To end of line.  I'll go
	 * back and speed this up when I speed up the whole vc
	 * module. --LK
	 */

	switch (which) {
	case 0:		/* To end of line	 */
		start = xx;
		end = vinfo.v_columns-1;
		break;
	case 1:		/* To start of line	 */
		start = 0;
		end = xx;
		break;
	case 2:		/* Whole line		 */
		start = 0;
		end = vinfo.v_columns-1;
		break;
	default:
		return;
	}

	for (i = start; i <= end; i++) {
		gc_paint_char(i, yy, ' ', ATTR_NONE);
	}
}

static void
gc_clear_screen(unsigned int xx, unsigned int yy, int top, unsigned int bottom,
		int which)
{
	if (!gc_buffer_size) return;

	if ( xx < gc_buffer_columns && yy < gc_buffer_rows && bottom <= gc_buffer_rows )
	{
		uint32_t start, end;

		switch (which) {
			case 0:		/* To end of screen	 */
				start = (yy * gc_buffer_columns) + xx;
				end = (bottom * gc_buffer_columns) - 1;
				break;
			case 1:		/* To start of screen	 */
				start = (top * gc_buffer_columns);
				end = (yy * gc_buffer_columns) + xx;
				break;
			case 2:		/* Whole screen		 */
				start = (top * gc_buffer_columns);
				end = (bottom * gc_buffer_columns) - 1;
				break;
			default:
				start = 0;
				end = 0;
				break;
		}

		memset(gc_buffer_attributes + start, ATTR_NONE, end - start + 1);
		for (uint32_t i = start; i <= end; i++) {
			gc_buffer_characters[i] = ' ';
		}
		memset(gc_buffer_colorcodes + start, gc_color_code, end - start + 1);
	}

	gc_ops.clear_screen(xx, yy, top, bottom, which);
}

static void
gc_enable( boolean_t enable )
{
	unsigned char *buffer_attributes = NULL;
	gc_glyph_t *buffer_characters = NULL;
	unsigned char *buffer_colorcodes = NULL;
	unsigned char *buffer_tab_stops  = NULL;
	uint32_t buffer_columns = 0;
	uint32_t buffer_rows = 0;
	uint32_t buffer_size = 0;
	spl_t s;

	if ( enable == FALSE )
	{
		// only disable console output if it goes to the graphics console
		if ( console_is_serial() == FALSE )
			disableConsoleOutput = TRUE;
		gc_enabled           = FALSE;
		gc_ops.enable(FALSE);
	}

	s = splhigh( );
	VCPUTC_LOCK_LOCK( );

	if ( gc_buffer_size )
	{
		buffer_attributes = gc_buffer_attributes;
		buffer_characters = gc_buffer_characters;
		buffer_colorcodes = gc_buffer_colorcodes;
		buffer_tab_stops  = gc_buffer_tab_stops;
		buffer_columns    = gc_buffer_columns;
		buffer_rows       = gc_buffer_rows;
		buffer_size       = gc_buffer_size;

		gc_buffer_attributes = NULL;
		gc_buffer_characters = NULL;
		gc_buffer_colorcodes = NULL;
		gc_buffer_tab_stops  = NULL;
		gc_buffer_columns    = 0;
		gc_buffer_rows       = 0;
		gc_buffer_size       = 0;

		VCPUTC_LOCK_UNLOCK( );
		splx( s );

		kfree( buffer_attributes, buffer_size );
		kfree( buffer_characters, buffer_size * sizeof(gc_glyph_t) );
		kfree( buffer_colorcodes, buffer_size );
		kfree( buffer_tab_stops,  buffer_columns );
	}
	else
	{
		VCPUTC_LOCK_UNLOCK( );
		splx( s );
	}

	if ( enable )
	{
		if ( vm_initialized )
		{
			buffer_columns = vinfo.v_columns;
			buffer_rows    = vinfo.v_rows;
			buffer_size    = buffer_columns * buffer_rows;

			if ( buffer_size )
			{
				buffer_attributes = (unsigned char *) kalloc( buffer_size );
				buffer_characters = (gc_glyph_t *) kalloc( buffer_size * sizeof(gc_glyph_t) );
				buffer_colorcodes = (unsigned char *) kalloc( buffer_size );
				buffer_tab_stops  = (unsigned char *) kalloc( buffer_columns );

				if ( buffer_attributes == NULL ||
				     buffer_characters == NULL ||
				     buffer_colorcodes == NULL ||
				     buffer_tab_stops  == NULL )
				{
					if ( buffer_attributes ) kfree( buffer_attributes, buffer_size );
					if ( buffer_characters ) kfree( buffer_characters, buffer_size * sizeof(gc_glyph_t) );
					if ( buffer_colorcodes ) kfree( buffer_colorcodes, buffer_size );
					if ( buffer_tab_stops  ) kfree( buffer_tab_stops,  buffer_columns );

					buffer_attributes = NULL;
					buffer_characters = NULL;
					buffer_colorcodes = NULL;
					buffer_tab_stops  = NULL;
					buffer_columns = 0;
					buffer_rows    = 0;
					buffer_size    = 0;
				}
				else
				{
					memset( buffer_attributes, ATTR_NONE, buffer_size );
					for (uint32_t i = 0; i < buffer_size; i++) {
						buffer_characters[i] = ' ';
					}
					memset( buffer_colorcodes, COLOR_CODE_SET( 0, COLOR_FOREGROUND, TRUE ), buffer_size );
					memset( buffer_tab_stops, 0, buffer_columns );
				}
			}
		}

		s = splhigh( );
		VCPUTC_LOCK_LOCK( );

		gc_buffer_attributes = buffer_attributes;
		gc_buffer_characters = buffer_characters;
		gc_buffer_colorcodes = buffer_colorcodes;
		gc_buffer_tab_stops  = buffer_tab_stops;
		gc_buffer_columns    = buffer_columns;
		gc_buffer_rows       = buffer_rows;
		gc_buffer_size       = buffer_size;

		gc_reset_screen();

		VCPUTC_LOCK_UNLOCK( );
		splx( s );

		gc_ops.clear_screen(gc_x, gc_y, 0, vinfo.v_rows, 2);
		gc_ops.show_cursor(gc_x, gc_y);

		gc_ops.enable(TRUE);
		gc_enabled           = TRUE;
		disableConsoleOutput = FALSE;
	}
}

static void
gc_hide_cursor(unsigned int xx, unsigned int yy)
{
	if ( xx < gc_buffer_columns && yy < gc_buffer_rows )
	{
		uint32_t index = (yy * gc_buffer_columns) + xx;
		unsigned char attribute = gc_buffer_attributes[index];
		gc_glyph_t character = gc_buffer_characters[index];
		unsigned char colorcode = gc_buffer_colorcodes[index];
		unsigned char colorcodesave = gc_color_code;

		gc_update_color(COLOR_CODE_GET(colorcode, TRUE ), TRUE );
		gc_update_color(COLOR_CODE_GET(colorcode, FALSE), FALSE);

		gc_ops.paint_char(xx, yy, character, attribute, 0, 0);

		gc_update_color(COLOR_CODE_GET(colorcodesave, TRUE ), TRUE );
		gc_update_color(COLOR_CODE_GET(colorcodesave, FALSE), FALSE);
	}
	else
	{
		gc_ops.hide_cursor(xx, yy);
	}
}

static void
gc_initialize(struct vc_info * info)
{
	if ( gc_initialized == FALSE )
	{
		/* Init our lock */
		VCPUTC_LOCK_INIT();

		gc_initialized = TRUE;
	}

	gc_ops.initialize(info);

	gc_reset_vt100();
	gc_x = gc_y = 0;
}

static void
gc_paint_char(unsigned int xx, unsigned int yy, gc_glyph_t ch, int attrs)
{
	if ( xx < gc_buffer_columns && yy < gc_buffer_rows )
	{
		uint32_t index = (yy * gc_buffer_columns) + xx;

		gc_buffer_attributes[index] = attrs;
		gc_buffer_characters[index] = ch;
		gc_buffer_colorcodes[index] = gc_color_code;
	}

	gc_ops.paint_char(xx, yy, ch, attrs, 0, 0);
}

/*
 * Minkar (2026): the console's font has the 256 characters of ISO 8859-1.
 * Box lines, blocks and Braille dots (graphs) are drawn by code
 * (gc_glyph_bitmap); other characters past the font are drawn as the
 * nearest ASCII one (arrows as ^ v < >, shapes as # or o), or as ?.
 */
static gc_glyph_t
gc_unicode_glyph(uint32_t cp)
{
	if (cp < 0x100)
		return (unsigned char)cp;
	/* Drawn by code: box lines, blocks, Braille, a few shapes and arrows. */
	if ((cp >= 0x2500 && cp <= 0x259F) || (cp >= 0x2800 && cp <= 0x28FF))
		return (gc_glyph_t)cp;
	switch (cp) {
	case 0x25A0: case 0x25AA: case 0x25B2: case 0x25BC: case 0x25CB:
	case 0x25CF: case 0x2191: case 0x2193:
		return (gc_glyph_t)cp;
	}
	if (cp >= 0x25A0 && cp <= 0x25FF)		/* geometric shapes */
		return cp == 0x25E6 ? 'o' : '#';
	switch (cp) {
	case 0x2190: return '<';
	case 0x2192: return '>';
	case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014:
	case 0x2212:
		return '-';
	case 0x2018: case 0x2019: return '\'';
	case 0x201C: case 0x201D: return '"';
	case 0x2022: case 0x2219: return 0xB7;	/* middle dot */
	case 0x2026: return '.';
	case 0x20AC: return 'E';
	default:
		return '?';
	}
}

static void
gc_putchar(char ch)
{
	unsigned char c = (unsigned char)ch;

	if (!ch) {
		return;	/* ignore null characters */
	}
	/*
	 * Minkar (2026): text is UTF-8. A sequence is decoded here and its
	 * character drawn with gc_unicode_glyph; a malformed one is drawn
	 * as ?. Escape sequences are ASCII and stay as they were.
	 */
	if (gc_vt100state == ESnormal) {
		if (gc_utf8_need > 0) {
			if ((c & 0xC0) == 0x80) {
				gc_utf8_cp = (gc_utf8_cp << 6) | (c & 0x3F);
				if (--gc_utf8_need == 0 && gc_utf8_cp >= 0xA0)
					gc_putc_normal(gc_unicode_glyph(gc_utf8_cp));
				return;
			}
			gc_utf8_need = 0;
			gc_putc_normal('?');
		}
		if (c >= 0xC2 && c <= 0xF4) {
			gc_utf8_need = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
			gc_utf8_cp = c & (c >= 0xF0 ? 0x07 : c >= 0xE0 ? 0x0F : 0x1F);
			return;
		}
		if (c >= 0x80) {
			gc_putc_normal('?');
			return;
		}
	}
	switch (gc_vt100state) {
		default:gc_vt100state = ESnormal;	/* FALLTHROUGH */
	case ESnormal:
		gc_putc_normal(ch);
		break;
	case ESesc:
		gc_putc_esc(ch);
		break;
	case ESsquare:
		gc_putc_square(ch);
		break;
	case ESgetpars:
		gc_putc_getpars(ch);
		break;
	case ESgotpars:
		gc_putc_gotpars(ch);
		break;
	case ESask:
		gc_putc_askcmd(ch);
		break;
	case EScharsize:
		gc_putc_charsizecmd(ch);
		break;
	case ESsetG0:
		gc_putc_charsetcmd(0, ch);
		break;
	case ESsetG1:
		gc_putc_charsetcmd(1, ch);
		break;
	case ESignore:		/* Minkar: up to a CSI final byte */
		if (ch == '\033')
			gc_vt100state = ESesc;
		else if (ch >= 0x40 && ch <= 0x7E)
			gc_vt100state = ESnormal;
		break;
	case ESstring:		/* Minkar: up to BEL or ST (ESC \) */
		if (ch == 0x07 || ++gc_string_len > GC_STRING_MAX)
			gc_vt100state = ESnormal;
		else if (ch == '\033')
			gc_vt100state = ESesc;
		break;
	}

	if (gc_x >= vinfo.v_columns) {
		if (0 == vinfo.v_columns)
			gc_x = 0;
		else
			gc_x = vinfo.v_columns - 1;
	}
	if (gc_y >= vinfo.v_rows) {
		if (0 == vinfo.v_rows)
			gc_y = 0;
		else
			gc_y = vinfo.v_rows - 1;
	}
}

static void
gc_putc_askcmd(unsigned char ch)
{
	unsigned int i;

	if (ch >= '0' && ch <= '9') {
		gc_par[gc_numpars] = (10*gc_par[gc_numpars]) + (ch-'0');
		return;
	}
	/* Minkar (2026): ESC [ ? n ; m h takes several modes. */
	if (ch == ';') {
		if (gc_numpars < MAXPARS - 1)
			gc_numpars++;
		return;
	}
	gc_vt100state = ESnormal;
	if (ch != 'h' && ch != 'l')
		return;

	for (i = 0; i <= gc_numpars; i++) {
		switch (gc_par[i]) {
		case 6:
			gc_relative_origin = ch == 'h';
			break;
		case 7:	/* wrap around mode h=1, l=0*/
			gc_wrap_mode = ch == 'h';
			break;
		case 47:
		case 1047:
		case 1049:
			/*
			 * Minkar (2026): the alternate screen (full-screen
			 * programs). There is one screen: entering and leaving
			 * clear it, and 1049 keeps the cursor.
			 */
			if (gc_par[i] == 1049 && ch == 'h') {
				gc_savex = gc_x;
				gc_savey = gc_y;
			}
			gc_clear_screen(gc_x, gc_y, 0, vinfo.v_rows, 2);
			if (gc_par[i] == 1049 && ch == 'l') {
				gc_x = gc_savex;
				gc_y = gc_savey;
			}
			break;
		default:
			break;
		}
	}
}

static void
gc_putc_charsetcmd(int charset, unsigned char ch)
{
	gc_vt100state = ESnormal;

	switch (ch) {
		case 'A' :
		case 'B' :
		default:
			gc_charset[charset] = 0;
			break;
		case '0' :	/* Graphic characters */
		case '2' :
			gc_charset[charset] = 0x21;
			break;
	}

}

static void
gc_putc_charsizecmd(unsigned char ch)
{
	gc_vt100state = ESnormal;

	switch (ch) {
		case '3' :
		case '4' :
		case '5' :
		case '6' :
			break;
		case '8' :	/* fill 'E's */
			{
				unsigned int xx, yy;
				for (yy = 0; yy < vinfo.v_rows; yy++)
					for (xx = 0; xx < vinfo.v_columns; xx++)
						gc_paint_char(xx, yy, 'E', ATTR_NONE);
			}
			break;
	}

}

static void 
gc_putc_esc(unsigned char ch)
{
	gc_vt100state = ESnormal;

	switch (ch) {
	case '[':
		gc_vt100state = ESsquare;
		break;
	case ']':		/* Minkar: control strings, skipped */
	case 'P':
	case '_':
	case '^':
		gc_vt100state = ESstring;
		gc_string_len = 0;
		break;
	case 'c':		/* Reset terminal 	 */
		gc_reset_vt100();
		gc_clear_screen(gc_x, gc_y, 0, vinfo.v_rows, 2);
		gc_x = gc_y = 0;
		break;
	case 'D':		/* Line feed		 */
	case 'E':
		if (gc_y >= gc_scrreg_bottom -1) {
			gc_scroll_up(1, gc_scrreg_top, gc_scrreg_bottom);
			gc_y = gc_scrreg_bottom - 1;
		} else {
			gc_y++;
		}
		if (ch == 'E') gc_x = 0;
		break;
	case 'H':		/* Set tab stop		 */
		gc_set_tab_stop(gc_x, TRUE);
		break;
	case 'M':		/* Cursor up		 */
		if (gc_y <= gc_scrreg_top) {
			gc_scroll_down(1, gc_scrreg_top, gc_scrreg_bottom);
			gc_y = gc_scrreg_top;
		} else {
			gc_y--;
		}
		break;
	case '>':
		gc_reset_vt100();
		break;
	case '7':		/* Save cursor		 */
		gc_savex = gc_x;
		gc_savey = gc_y;
		gc_saveattr = gc_attr;
		gc_save_charset_s = gc_charset_select;
		gc_charset_save[0] = gc_charset[0];
		gc_charset_save[1] = gc_charset[1];
		break;
	case '8':		/* Restore cursor	 */
		gc_x = gc_savex;
		gc_y = gc_savey;
		gc_attr = gc_saveattr;
		gc_charset_select = gc_save_charset_s;
		gc_charset[0] = gc_charset_save[0];
		gc_charset[1] = gc_charset_save[1];
		break;
	case 'Z':		/* return terminal ID */
		break;
	case '#':		/* change characters height */
		gc_vt100state = EScharsize;
		break;
	case '(':
		gc_vt100state = ESsetG0;
		break;
	case ')':		/* character set sequence */
		gc_vt100state = ESsetG1;
		break;
	case '=':
		break;
	default:
		/* Rest not supported */
		break;
	}

}

static void 
gc_putc_getpars(unsigned char ch)
{
	if (ch == '?') {
		gc_vt100state = ESask;
		return;
	}
	if (ch == '[') {
		gc_vt100state = ESnormal;
		/* Not supported */
		return;
	}
	/* Minkar: private markers and intermediate bytes: not supported, and
	 * the sequence is skipped up to its final byte. */
	if (ch == '<' || ch == '=' || ch == '>' || (ch >= 0x20 && ch <= 0x2F)) {
		gc_vt100state = ESignore;
		return;
	}
	if (ch == ';' && gc_numpars < MAXPARS - 1) {
		gc_numpars++;
	} else
		if (ch >= '0' && ch <= '9') {
			gc_par[gc_numpars] *= 10;
			gc_par[gc_numpars] += ch - '0';
		} else {
			gc_numpars++;
			gc_vt100state = ESgotpars;
			gc_putc_gotpars(ch);
		}
}

/*
 * Minkar (2026): copies the cell at column from to column to on row yy,
 * on the screen and in the buffer, with its colors.
 */
static void
gc_copy_cell(unsigned int to, unsigned int from, unsigned int yy)
{
	uint32_t src, dst;

	if (!gc_buffer_size || yy >= gc_buffer_rows ||
	    to >= gc_buffer_columns || from >= gc_buffer_columns)
		return;
	src = yy * gc_buffer_columns + from;
	dst = yy * gc_buffer_columns + to;
	if (gc_color_code != gc_buffer_colorcodes[src]) {
		gc_update_color(COLOR_CODE_GET(gc_buffer_colorcodes[src], TRUE), TRUE);
		gc_update_color(COLOR_CODE_GET(gc_buffer_colorcodes[src], FALSE), FALSE);
	}
	gc_ops.paint_char(to, yy, gc_buffer_characters[src],
	    gc_buffer_attributes[src], 0, 0);
	gc_buffer_attributes[dst] = gc_buffer_attributes[src];
	gc_buffer_characters[dst] = gc_buffer_characters[src];
	gc_buffer_colorcodes[dst] = gc_buffer_colorcodes[src];
}

/*
 * Minkar (2026): inserts (n > 0) or deletes (n < 0) |n| characters at the
 * cursor; the rest of the line moves right or left and blanks fill in.
 */
static void
gc_shift_line(int n)
{
	unsigned char colorcodesave = gc_color_code;
	unsigned int cols = vinfo.v_columns, i, count;

	if (gc_x >= cols)
		return;
	count = (unsigned int)(n < 0 ? -n : n);
	if (count > cols - gc_x)
		count = cols - gc_x;
	if (n > 0) {
		for (i = cols - 1; i >= gc_x + count; i--)
			gc_copy_cell(i, i - count, gc_y);
	} else {
		for (i = gc_x; i + count < cols; i++)
			gc_copy_cell(i, i + count, gc_y);
	}
	if (colorcodesave != gc_color_code) {
		gc_update_color(COLOR_CODE_GET(colorcodesave, TRUE), TRUE);
		gc_update_color(COLOR_CODE_GET(colorcodesave, FALSE), FALSE);
	}
	for (i = 0; i < count; i++)
		gc_paint_char(n > 0 ? gc_x + i : cols - 1 - i, gc_y, ' ', ATTR_NONE);
}

/*
 * Minkar (2026): one of the console's 8 colors for a color of the 256-color
 * palette (38;5;n) or for r, g, b (38;2;r;g;b). A background channel is on
 * at half its range or more, so dark backgrounds stay black. A foreground
 * goes by its hue instead: a channel is on at half the brightest one, and
 * only a color close to black is black, so that dim text and lines (a
 * theme's gray box lines) stay visible.
 */
static int
gc_rgb_to_color(unsigned int r, unsigned int g, unsigned int b, boolean_t fore)
{
	unsigned int half = 128;

	if (fore) {
		unsigned int max = r > g ? (r > b ? r : b) : (g > b ? g : b);
		if (max < 40)
			return 0;
		half = (max + 1) / 2;
	}
	return (r >= half ? 1 : 0) | (g >= half ? 2 : 0) | (b >= half ? 4 : 0);
}

static int
gc_256_to_color(unsigned int n, boolean_t fore)
{
	static const unsigned char cube[6] = { 0, 95, 135, 175, 215, 255 };

	if (n < 8)
		return (int)n;
	if (n < 16)
		return (int)(n - 8);
	if (n < 232) {
		n -= 16;
		return gc_rgb_to_color(cube[n / 36], cube[(n / 6) % 6], cube[n % 6], fore);
	}
	/* the grays, 8 to 238 */
	return gc_rgb_to_color(8 + 10 * (n - 232), 8 + 10 * (n - 232), 8 + 10 * (n - 232), fore);
}

static void 
gc_putc_gotpars(unsigned char ch)
{
	unsigned int i;

	if (ch < ' ') {
		/* special case for vttest for handling cursor
		   movement in escape sequences */
		gc_putc_normal(ch);
		gc_vt100state = ESgotpars;
		return;
	}
	gc_vt100state = ESnormal;
	switch (ch) {
	case 'A':		/* Up			 */
		gc_y -= gc_par[0] ? gc_par[0] : 1;
		if (gc_y < gc_scrreg_top)
			gc_y = gc_scrreg_top;
		break;
	case 'B':		/* Down			 */
		gc_y += gc_par[0] ? gc_par[0] : 1;
		if (gc_y >= gc_scrreg_bottom)
			gc_y = gc_scrreg_bottom - 1;
		break;
	case 'C':		/* Right		 */
		gc_x += gc_par[0] ? gc_par[0] : 1;
		if (gc_x >= vinfo.v_columns)
			gc_x = vinfo.v_columns-1;
		break;
	case 'D':		/* Left			 */
		if (gc_par[0] > gc_x)
			gc_x = 0;
		else if (gc_par[0])
			gc_x -= gc_par[0];
		else if (gc_x)
			--gc_x;
		break;
	/*
	 * Minkar (2026): what xterm's terminfo entry uses and the console
	 * lacked. Line editors (sh's libedit, zsh) move with CHA after a
	 * carriage return: without it they redrew at column 0.
	 */
	case 'G':		/* Cursor to column (CHA) */
	case '`':		/* (HPA)		 */
		gc_x = gc_par[0] ? gc_par[0] - 1 : 0;
		gc_hanging_cursor = 0;
		break;
	case 'd':		/* Cursor to row (VPA)	 */
		gc_y = gc_par[0] ? gc_par[0] - 1 : 0;
		if (gc_relative_origin)
			gc_y += gc_scrreg_top;
		gc_hanging_cursor = 0;
		break;
	case 'a':		/* Right (HPR)		 */
		gc_x += gc_par[0] ? gc_par[0] : 1;
		break;
	case 'e':		/* Down (VPR)		 */
		gc_y += gc_par[0] ? gc_par[0] : 1;
		if (gc_y >= gc_scrreg_bottom)
			gc_y = gc_scrreg_bottom - 1;
		break;
	case 'E':		/* Down, column 1 (CNL)	 */
	case 'F':		/* Up, column 1 (CPL)	 */
		i = gc_par[0] ? gc_par[0] : 1;
		if (ch == 'E')
			gc_y = (gc_y + i >= gc_scrreg_bottom) ? gc_scrreg_bottom - 1 : gc_y + i;
		else
			gc_y = (gc_y < gc_scrreg_top + i) ? gc_scrreg_top : gc_y - i;
		gc_x = 0;
		gc_hanging_cursor = 0;
		break;
	case '@':		/* Insert characters (ICH) */
		gc_shift_line((int)(gc_par[0] ? gc_par[0] : 1));
		break;
	case 'P':		/* Delete characters (DCH) */
		gc_shift_line(-(int)(gc_par[0] ? gc_par[0] : 1));
		break;
	case 'L':		/* Insert lines (IL)	 */
	case 'M':		/* Delete lines (DL)	 */
		if (gc_y >= gc_scrreg_top && gc_y < gc_scrreg_bottom) {
			i = gc_par[0] ? gc_par[0] : 1;
			if (i > gc_scrreg_bottom - gc_y)
				i = gc_scrreg_bottom - gc_y;
			if (ch == 'L')
				gc_scroll_down(i, gc_y, gc_scrreg_bottom);
			else
				gc_scroll_up(i, gc_y, gc_scrreg_bottom);
			gc_x = 0;
		}
		break;
	case 'S':		/* Scroll up (SU)	 */
		gc_scroll_up(gc_par[0] ? gc_par[0] : 1, gc_scrreg_top, gc_scrreg_bottom);
		break;
	case 'T':		/* Scroll down (SD)	 */
		gc_scroll_down(gc_par[0] ? gc_par[0] : 1, gc_scrreg_top, gc_scrreg_bottom);
		break;
	case 'b':		/* Repeat the last character (REP) */
		for (i = gc_par[0] ? gc_par[0] : 1; i > 0; i--)
			gc_putc_normal(gc_last_char);
		break;
	case 's':		/* Save cursor		 */
		gc_savex = gc_x;
		gc_savey = gc_y;
		break;
	case 'u':		/* Restore cursor	 */
		gc_x = gc_savex;
		gc_y = gc_savey;
		gc_hanging_cursor = 0;
		break;
	case 'H':		/* Set cursor position	 */
	case 'f':
		gc_x = gc_par[1] ? gc_par[1] - 1 : 0;
		gc_y = gc_par[0] ? gc_par[0] - 1 : 0;
		if (gc_relative_origin)
			gc_y += gc_scrreg_top;
		gc_hanging_cursor = 0;
		break;
	case 'X':		/* clear p1 characters */
		if (gc_numpars) {
			for (i = gc_x; i < gc_x + gc_par[0]; i++)
				gc_paint_char(i, gc_y, ' ', ATTR_NONE);
		}
		break;
	case 'J':		/* Clear part of screen	 */
		gc_clear_screen(gc_x, gc_y, 0, vinfo.v_rows, gc_par[0]);
		break;
	case 'K':		/* Clear part of line	 */
		gc_clear_line(gc_x, gc_y, gc_par[0]);
		break;
	case 'g':		/* tab stops	 	 */
		switch (gc_par[0]) {
			case 1:
			case 2:	/* reset tab stops */
				/* gc_reset_tabs(); */
				break;				
			case 3:	/* Clear every tabs */
				{
					for (i = 0; i <= vinfo.v_columns; i++)
						gc_set_tab_stop(i, FALSE);
				}
				break;
			case 0:
				gc_set_tab_stop(gc_x, FALSE);
				break;
		}
		break;
	case 'm':		/* Set attribute	 */
		for (i = 0; i < gc_numpars; i++) {
			switch (gc_par[i]) {
			case 0:
				gc_attr = ATTR_NONE;
				gc_update_color(COLOR_BACKGROUND, FALSE);
				gc_update_color(COLOR_FOREGROUND, TRUE );	
				break;
			case 1:
				gc_attr |= ATTR_BOLD;
				break;
			case 4:
				gc_attr |= ATTR_UNDER;
				break;
			case 7:
				gc_attr |= ATTR_REVERSE;
				break;
			case 22:
				gc_attr &= ~ATTR_BOLD;
				break;
			case 24:
				gc_attr &= ~ATTR_UNDER;
				break;
			case 27:
				gc_attr &= ~ATTR_REVERSE;
				break;
			case 5:
			case 25:	/* blink/no blink */
				break;
			/*
			 * Minkar (2026): default colors, bright colors, and
			 * 256-color and RGB ones, as the nearest of the 8.
			 */
			case 39:
				gc_update_color(COLOR_FOREGROUND, TRUE);
				break;
			case 49:
				gc_update_color(COLOR_BACKGROUND, FALSE);
				break;
			case 38:
			case 48:
				if (i + 2 < gc_numpars && gc_par[i + 1] == 5) {
					gc_update_color(gc_256_to_color(gc_par[i + 2],
					    gc_par[i] == 38), gc_par[i] == 38);
					i += 2;
				} else if (i + 4 < gc_numpars && gc_par[i + 1] == 2) {
					gc_update_color(gc_rgb_to_color(gc_par[i + 2],
					    gc_par[i + 3], gc_par[i + 4], gc_par[i] == 38),
					    gc_par[i] == 38);
					i += 4;
				} else
					i = gc_numpars;
				break;
			default:
				if (gc_par[i] >= 30 && gc_par[i] <= 37)
					gc_update_color(gc_par[i] - 30, TRUE);
				if (gc_par[i] >= 40 && gc_par[i] <= 47)
					gc_update_color(gc_par[i] - 40, FALSE);
				if (gc_par[i] >= 90 && gc_par[i] <= 97)
					gc_update_color(gc_par[i] - 90, TRUE);
				if (gc_par[i] >= 100 && gc_par[i] <= 107)
					gc_update_color(gc_par[i] - 100, FALSE);
				break;
			}
		}
		break;
	case 'r':		/* Set scroll region	 */
		gc_x = gc_y = 0;
		/* ensure top < bottom, and both within limits */
		if ((gc_numpars > 0) && (gc_par[0] < vinfo.v_rows)) {
			gc_scrreg_top = gc_par[0] ? gc_par[0] - 1 : 0;
		} else {
			gc_scrreg_top = 0;
		}
		if ((gc_numpars > 1) && (gc_par[1] <= vinfo.v_rows) && (gc_par[1] > gc_par[0])) {
			gc_scrreg_bottom = gc_par[1];
			if (gc_scrreg_bottom > vinfo.v_rows)
				gc_scrreg_bottom = vinfo.v_rows;
		} else {
			gc_scrreg_bottom = vinfo.v_rows;
		}
		if (gc_relative_origin)
			gc_y = gc_scrreg_top;
		break;
	}

}

static void 
gc_putc_normal(gc_glyph_t ch)
{
	switch (ch) {
	case '\a':		/* Beep			 */
        break;
	case 127:		/* Delete		 */
	case '\b':		/* Backspace		 */
		if (gc_hanging_cursor) {
			gc_hanging_cursor = 0;
		} else
			if (gc_x > 0) {
				gc_x--;
			}
		break;
	case '\t':		/* Tab			 */
		if (gc_buffer_tab_stops) while (gc_x < vinfo.v_columns && !gc_is_tab_stop(++gc_x));

		if (gc_x >= vinfo.v_columns)
			gc_x = vinfo.v_columns-1;
		break;
	case 0x0b:
	case 0x0c:
	case '\n':		/* Line feed		 */
		if (gc_y >= gc_scrreg_bottom -1 ) {
			gc_scroll_up(1, gc_scrreg_top, gc_scrreg_bottom);
			gc_y = gc_scrreg_bottom - 1;
		} else {
			gc_y++;
		}
		break;
	case '\r':		/* Carriage return	 */
		gc_x = 0;
		gc_hanging_cursor = 0;
		break;
	case 0x0e:  /* Select G1 charset (Control-N) */
		gc_charset_select = 1;
		break;
	case 0x0f:  /* Select G0 charset (Control-O) */
		gc_charset_select = 0;
		break;
	case 0x18 : /* CAN : cancel */
	case 0x1A : /* like cancel */
			/* well, i do nothing here, may be later */
		break;
	case '\033':		/* Escape		 */
		gc_vt100state = ESesc;
		gc_hanging_cursor = 0;
		break;
	default:
		if (ch >= ' ') {
			if (gc_hanging_cursor) {
				gc_x = 0;
				if (gc_y >= gc_scrreg_bottom -1 ) {
					gc_scroll_up(1, gc_scrreg_top, gc_scrreg_bottom);
					gc_y = gc_scrreg_bottom - 1;
				} else {
					gc_y++;
				}
				gc_hanging_cursor = 0;
			}
			gc_paint_char(gc_x, gc_y, (ch >= 0x60 && ch <= 0x7f) ? ch + gc_charset[gc_charset_select]
								: ch, gc_attr);
			gc_last_char = ch;	/* Minkar (2026): for REP */
			if (gc_x == vinfo.v_columns - 1) {
				gc_hanging_cursor = gc_wrap_mode;
			} else {
				gc_x++;
			}
		}
		break;
	}

}

static void 
gc_putc_square(unsigned char ch)
{
	int     i;

	for (i = 0; i < MAXPARS; i++) {
		gc_par[i] = 0;
	}

	gc_numpars = 0;
	gc_vt100state = ESgetpars;

	gc_putc_getpars(ch);

}

static void
gc_reset_screen(void)
{
	gc_reset_vt100();
	gc_x = gc_y = 0;
}

static void
gc_reset_tabs(void)
{
	unsigned int i;
	
	if (!gc_buffer_tab_stops) return;

	for (i = 0; i < vinfo.v_columns; i++) {
		gc_buffer_tab_stops[i] = ((i % 8) == 0);
	}

}

static void
gc_set_tab_stop(unsigned int column, boolean_t enabled)
{
	if (gc_buffer_tab_stops && (column < vinfo.v_columns)) {
		gc_buffer_tab_stops[column] = enabled;
	}
}

static boolean_t gc_is_tab_stop(unsigned int column)
{
	if (gc_buffer_tab_stops == NULL)
		return ((column % 8) == 0);
	if (column < vinfo.v_columns)
		return gc_buffer_tab_stops[column];
	else
		return FALSE;
}

static void
gc_reset_vt100(void)
{
	gc_reset_tabs();
	gc_scrreg_top    = 0;
	gc_scrreg_bottom = vinfo.v_rows;
	gc_attr = ATTR_NONE;
	gc_charset[0] = gc_charset[1] = 0;
	gc_charset_select = 0;
	gc_wrap_mode = 1;
	gc_relative_origin = 0;
	gc_update_color(COLOR_BACKGROUND, FALSE);
	gc_update_color(COLOR_FOREGROUND, TRUE);
}

static void 
gc_scroll_down(int num, unsigned int top, unsigned int bottom)
{
	if (!gc_buffer_size) return;

	if ( bottom <= gc_buffer_rows )
	{
		unsigned char colorcodesave = gc_color_code;
		uint32_t column, row;
		uint32_t index, jump;

		jump = num * gc_buffer_columns;

		for ( row = bottom - 1 ; row >= top + num ; row-- )
		{
			index = row * gc_buffer_columns;

			for ( column = 0 ; column < gc_buffer_columns ; index++, column++ )
			{
				if ( gc_buffer_attributes[index] != gc_buffer_attributes[index - jump] || 
				     gc_buffer_characters[index] != gc_buffer_characters[index - jump] || 
				     gc_buffer_colorcodes[index] != gc_buffer_colorcodes[index - jump] )
				{
					if ( gc_color_code != gc_buffer_colorcodes[index - jump] )
					{
						gc_update_color(COLOR_CODE_GET(gc_buffer_colorcodes[index - jump], TRUE ), TRUE );
						gc_update_color(COLOR_CODE_GET(gc_buffer_colorcodes[index - jump], FALSE), FALSE);
					}

					if ( gc_buffer_colorcodes[index] != gc_buffer_colorcodes[index - jump] )
					{
						gc_ops.paint_char( /* xx             */ column,
						                   /* yy             */ row,
						                   /* ch             */ gc_buffer_characters[index - jump],
						                   /* attrs          */ gc_buffer_attributes[index - jump],
						                   /* ch_previous    */ 0,
						                   /* attrs_previous */ 0 );
					}
					else
					{
						gc_ops.paint_char( /* xx             */ column,
						                   /* yy             */ row,
						                   /* ch             */ gc_buffer_characters[index - jump],
						                   /* attrs          */ gc_buffer_attributes[index - jump],
						                   /* ch_previous    */ gc_buffer_characters[index],
						                   /* attrs_previous */ gc_buffer_attributes[index] );
					}

					gc_buffer_attributes[index] = gc_buffer_attributes[index - jump];
					gc_buffer_characters[index] = gc_buffer_characters[index - jump];
					gc_buffer_colorcodes[index] = gc_buffer_colorcodes[index - jump];
				}
			}
		}

		if ( colorcodesave != gc_color_code )
		{
			gc_update_color(COLOR_CODE_GET(colorcodesave, TRUE ), TRUE );
			gc_update_color(COLOR_CODE_GET(colorcodesave, FALSE), FALSE);
		}

		/* Now set the freed up lines to the background colour */

		for ( row = top ; row < top + num ; row++ )
		{
			index = row * gc_buffer_columns;

			for ( column = 0 ; column < gc_buffer_columns ; index++, column++ )
			{
				if ( gc_buffer_attributes[index] != ATTR_NONE     || 
				     gc_buffer_characters[index] != ' '           || 
				     gc_buffer_colorcodes[index] != gc_color_code )
				{
					if ( gc_buffer_colorcodes[index] != gc_color_code )
					{
						gc_ops.paint_char( /* xx             */ column,
						                   /* yy             */ row,
						                   /* ch             */ ' ',
						                   /* attrs          */ ATTR_NONE,
						                   /* ch_previous    */ 0,
						                   /* attrs_previous */ 0 );
					}
					else
					{
						gc_ops.paint_char( /* xx             */ column,
						                   /* yy             */ row,
						                   /* ch             */ ' ',
						                   /* attrs          */ ATTR_NONE,
						                   /* ch_previous    */ gc_buffer_characters[index],
						                   /* attrs_previous */ gc_buffer_attributes[index] );
					}

					gc_buffer_attributes[index] = ATTR_NONE;
					gc_buffer_characters[index] = ' ';
					gc_buffer_colorcodes[index] = gc_color_code;
				}
			}
		}
	}
	else
	{
		gc_ops.scroll_down(num, top, bottom);

		/* Now set the freed up lines to the background colour */

		gc_clear_screen(vinfo.v_columns - 1, top + num - 1, top, bottom, 1);
	}
}

static void 
gc_scroll_up(int num, unsigned int top, unsigned int bottom)
{
	if (!gc_buffer_size) return;

	if ( bottom <= gc_buffer_rows )
	{
		unsigned char colorcodesave = gc_color_code;
		uint32_t column, row;
		uint32_t index, jump;

		jump = num * gc_buffer_columns;

		for ( row = top ; row < bottom - num ; row++ )
		{
			index = row * gc_buffer_columns;

			for ( column = 0 ; column < gc_buffer_columns ; index++, column++ )
			{
				if ( gc_buffer_attributes[index] != gc_buffer_attributes[index + jump] || 
				     gc_buffer_characters[index] != gc_buffer_characters[index + jump] || 
				     gc_buffer_colorcodes[index] != gc_buffer_colorcodes[index + jump] )
				{
					if ( gc_color_code != gc_buffer_colorcodes[index + jump] )
					{
						gc_update_color(COLOR_CODE_GET(gc_buffer_colorcodes[index + jump], TRUE ), TRUE );
						gc_update_color(COLOR_CODE_GET(gc_buffer_colorcodes[index + jump], FALSE), FALSE);
					}

					if ( gc_buffer_colorcodes[index] != gc_buffer_colorcodes[index + jump] )
					{
						gc_ops.paint_char( /* xx             */ column,
						                   /* yy             */ row,
						                   /* ch             */ gc_buffer_characters[index + jump],
						                   /* attrs          */ gc_buffer_attributes[index + jump],
						                   /* ch_previous    */ 0,
						                   /* attrs_previous */ 0 );
					}
					else
					{
						gc_ops.paint_char( /* xx             */ column,
						                   /* yy             */ row,
						                   /* ch             */ gc_buffer_characters[index + jump],
						                   /* attrs          */ gc_buffer_attributes[index + jump],
						                   /* ch_previous    */ gc_buffer_characters[index],
						                   /* attrs_previous */ gc_buffer_attributes[index] );
					}

					gc_buffer_attributes[index] = gc_buffer_attributes[index + jump];
					gc_buffer_characters[index] = gc_buffer_characters[index + jump];
					gc_buffer_colorcodes[index] = gc_buffer_colorcodes[index + jump];
				}
			}
		}

		if ( colorcodesave != gc_color_code )
		{
			gc_update_color(COLOR_CODE_GET(colorcodesave, TRUE ), TRUE );
			gc_update_color(COLOR_CODE_GET(colorcodesave, FALSE), FALSE);
		}

		/* Now set the freed up lines to the background colour */

		for ( row = bottom - num ; row < bottom ; row++ )
		{
			index = row * gc_buffer_columns;

			for ( column = 0 ; column < gc_buffer_columns ; index++, column++ )
			{
				if ( gc_buffer_attributes[index] != ATTR_NONE     || 
				     gc_buffer_characters[index] != ' '           || 
				     gc_buffer_colorcodes[index] != gc_color_code )
				{
					if ( gc_buffer_colorcodes[index] != gc_color_code )
					{
						gc_ops.paint_char( /* xx             */ column,
						                   /* yy             */ row,
						                   /* ch             */ ' ',
						                   /* attrs          */ ATTR_NONE,
						                   /* ch_previous    */ 0,
						                   /* attrs_previous */ 0 );
					}
					else
					{
						gc_ops.paint_char( /* xx             */ column,
						                   /* yy             */ row,
						                   /* ch             */ ' ',
						                   /* attrs          */ ATTR_NONE,
						                   /* ch_previous    */ gc_buffer_characters[index],
						                   /* attrs_previous */ gc_buffer_attributes[index] );
					}

					gc_buffer_attributes[index] = ATTR_NONE;
					gc_buffer_characters[index] = ' ';
					gc_buffer_colorcodes[index] = gc_color_code;
				}
			}
		}
	}
	else
	{
		gc_ops.scroll_up(num, top, bottom);

		/* Now set the freed up lines to the background colour */

		gc_clear_screen(0, bottom - num, top, bottom, 0);
	}
}

static void
gc_show_cursor(unsigned int xx, unsigned int yy)
{
	if ( xx < gc_buffer_columns && yy < gc_buffer_rows )
	{
		uint32_t index = (yy * gc_buffer_columns) + xx;
		unsigned char attribute = gc_buffer_attributes[index];
		gc_glyph_t character = gc_buffer_characters[index];
		unsigned char colorcode = gc_buffer_colorcodes[index];
		unsigned char colorcodesave = gc_color_code;

		gc_update_color(COLOR_CODE_GET(colorcode, FALSE), TRUE );
		gc_update_color(COLOR_CODE_GET(colorcode, TRUE ), FALSE);

		gc_ops.paint_char(xx, yy, character, attribute, 0, 0);

		gc_update_color(COLOR_CODE_GET(colorcodesave, TRUE ), TRUE );
		gc_update_color(COLOR_CODE_GET(colorcodesave, FALSE), FALSE);
	}
	else
	{
		gc_ops.show_cursor(xx, yy);
	}
}

static void
gc_update_color(int color, boolean_t fore)
{
	assert(gc_ops.update_color);

	gc_color_code = COLOR_CODE_SET(gc_color_code, color, fore);
	gc_ops.update_color(color, fore);
}

void
vcputc(__unused int l, __unused int u, int c)
{
	if ( gc_initialized && gc_enabled )
	{
		spl_t s;

		s = splhigh();
#if	defined(__i386__) || defined(__x86_64__)
		x86_filter_TLB_coherency_interrupts(TRUE);
#endif
		VCPUTC_LOCK_LOCK();
		if ( gc_enabled )
		{
			gc_hide_cursor(gc_x, gc_y);
			gc_putchar(c);
			gc_show_cursor(gc_x, gc_y);
		}
		VCPUTC_LOCK_UNLOCK();
#if	defined(__i386__) || defined(__x86_64__)
		x86_filter_TLB_coherency_interrupts(FALSE);
#endif
		splx(s);
	}
}

/*
 * Video Console (Back-End)
 * ------------------------
 */
 
/*
 * For the color support (Michel Pollet)
 */
static unsigned char vc_color_index_table[33] = 
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 2 };

static uint32_t vc_colors[8][4] = {
	{ 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 },	/* black */
	{ 0x23232323, 0x7C007C00, 0x00FF0000, 0x3FF00000 },	/* red	*/
	{ 0xb9b9b9b9, 0x03e003e0, 0x0000FF00, 0x000FFC00 },	/* green */
	{ 0x05050505, 0x7FE07FE0, 0x00FFFF00, 0x3FFFFC00 },	/* yellow */
	{ 0xd2d2d2d2, 0x001f001f, 0x000000FF, 0x000003FF },	/* blue	 */
//	{ 0x80808080, 0x31933193, 0x00666699, 0x00000000 },	/* blue	 */
	{ 0x18181818, 0x7C1F7C1F, 0x00FF00FF, 0x3FF003FF },	/* magenta */
	{ 0xb4b4b4b4, 0x03FF03FF, 0x0000FFFF, 0x000FFFFF },	/* cyan	*/
	{ 0x00000000, 0x7FFF7FFF, 0x00FFFFFF, 0x3FFFFFFF }	/* white */
};

static uint32_t vc_color_fore = 0;
static uint32_t vc_color_back = 0;

/* 
 * New Rendering code from Michel Pollet
 */

/* Rendered Font Buffer */
static unsigned char *vc_rendered_font = NULL;

/* Rendered Font Size */
static uint32_t vc_rendered_font_size = 0;

/* Size of a character in the table (bytes) */
static int vc_rendered_char_size = 0;

#define REN_MAX_DEPTH	32
static unsigned char vc_rendered_char[ISO_CHAR_HEIGHT * ((REN_MAX_DEPTH / 8) * ISO_CHAR_WIDTH)];

#if !CONFIG_EMBEDDED
static void
internal_set_progressmeter(int new_value);
static void
internal_enable_progressmeter(int new_value);

enum
{
    kProgressMeterOff    = FALSE,
    kProgressMeterUser   = TRUE,
    kProgressMeterKernel = 3,
};
enum
{
    kProgressMeterMax    = 1024,
    kProgressMeterEnd    = 512,
};

#endif	/* !CONFIG_EMBEDDED */

static boolean_t vc_progress_white = 
#ifdef CONFIG_VC_PROGRESS_WHITE
                                     TRUE;
#else /* !CONFIG_VC_PROGRESS_WHITE */
                                     FALSE;
#endif /* !CONFIG_VC_PROGRESS_WHITE */

static int vc_acquire_delay = kProgressAcquireDelay;

static void 
vc_clear_screen(unsigned int xx, unsigned int yy, unsigned int scrreg_top,
		unsigned int scrreg_bottom, int which)
{
	uint32_t *p, *endp, *row;
	int      linelongs, col;
	int      rowline, rowlongs;

	if(!vinfo.v_depth)
		return;

	linelongs = vinfo.v_rowbytes * (ISO_CHAR_HEIGHT >> 2);
	rowline = vinfo.v_rowscanbytes >> 2;
	rowlongs = vinfo.v_rowbytes >> 2;

	p = (uint32_t*) vinfo.v_baseaddr;
	endp = (uint32_t*) vinfo.v_baseaddr;

	switch (which) {
	case 0:		/* To end of screen	 */
		gc_clear_line(xx, yy, 0);
		if (yy < scrreg_bottom - 1) {
			p += (yy + 1) * linelongs;
			endp += scrreg_bottom * linelongs;
		}
		break;
	case 1:		/* To start of screen	 */
		gc_clear_line(xx, yy, 1);
		if (yy > scrreg_top) {
			p += scrreg_top * linelongs;
			endp += yy * linelongs;
		}
		break;
	case 2:		/* Whole screen		 */
		p += scrreg_top * linelongs;
		if (scrreg_bottom == vinfo.v_rows) {
			endp += rowlongs * vinfo.v_height;
		} else {
			endp += scrreg_bottom * linelongs;
		}
		break;
	}

	for (row = p ; row < endp ; row += rowlongs) {
		for (col = 0; col < rowline; col++) 
			*(row+col) = vc_color_back;
	}
}

/*
 * Minkar (2026): characters drawn by code, so that full-screen programs
 * (btop, htop, dialog boxes) show real lines, blocks and graphs. The cell
 * is 8x16; bit x of a row is pixel x, as in iso_font.
 *
 * Box drawing (U+2500-U+257F): each character is its four arms, up, down,
 * left and right, 2 bits each: 0 none, 1 light, 2 heavy, 3 double.
 */
static const uint8_t gc_box_arms[128] = {
	0x05, 0x0a, 0x50, 0xa0, 0x05, 0x0a, 0x50, 0xa0,	/* U+2500 */
	0x05, 0x0a, 0x50, 0xa0, 0x11, 0x12, 0x21, 0x22,	/* U+2508 */
	0x14, 0x18, 0x24, 0x28, 0x41, 0x42, 0x81, 0x82,	/* U+2510 */
	0x44, 0x48, 0x84, 0x88, 0x51, 0x52, 0x91, 0x61,	/* U+2518 */
	0xa1, 0x92, 0x62, 0xa2, 0x54, 0x58, 0x94, 0x64,	/* U+2520 */
	0xa4, 0x98, 0x68, 0xa8, 0x15, 0x19, 0x16, 0x1a,	/* U+2528 */
	0x25, 0x29, 0x26, 0x2a, 0x45, 0x49, 0x46, 0x4a,	/* U+2530 */
	0x85, 0x89, 0x86, 0x8a, 0x55, 0x59, 0x56, 0x5a,	/* U+2538 */
	0x95, 0x65, 0xa5, 0x99, 0x96, 0x69, 0x66, 0x9a,	/* U+2540 */
	0x6a, 0xa9, 0xa6, 0xaa, 0x05, 0x0a, 0x50, 0xa0,	/* U+2548 */
	0x0f, 0xf0, 0x13, 0x31, 0x33, 0x1c, 0x34, 0x3c,	/* U+2550 */
	0x43, 0xc1, 0xc3, 0x4c, 0xc4, 0xcc, 0x53, 0xf1,	/* U+2558 */
	0xf3, 0x5c, 0xf4, 0xfc, 0x1f, 0x35, 0x3f, 0x4f,	/* U+2560 */
	0xc5, 0xcf, 0x5f, 0xf5, 0xff, 0x11, 0x14, 0x44,	/* U+2568 */
	0x41, 0x00, 0x00, 0x00, 0x04, 0x40, 0x01, 0x10,	/* U+2570 */
	0x08, 0x80, 0x02, 0x20, 0x06, 0x60, 0x09, 0x90,	/* U+2578 */
};

static void
gc_fill(unsigned char bits[ISO_CHAR_HEIGHT], int x0, int x1, int y0, int y1)
{
	for (int y = y0; y <= y1; y++) {
		for (int x = x0; x <= x1; x++) {
			bits[y] |= (unsigned char)(1 << x);
		}
	}
}

/* One arm: from the cell's edge to just past its middle. */
static void
gc_box_arm(unsigned char bits[ISO_CHAR_HEIGHT], int arm, int weight)
{
	/* The vertical line's columns, the horizontal line's rows. */
	static const int vx[4][2] = { {0, 0}, {3, 3}, {3, 4}, {2, 5} };
	static const int hy[4][2] = { {0, 0}, {7, 7}, {7, 8}, {5, 9} };
	int lines = weight == 3 ? 2 : 1;

	for (int k = 0; k < lines; k++) {
		int x0 = vx[weight][k], x1 = weight == 3 ? x0 : vx[weight][1];
		int y0 = hy[weight][k], y1 = weight == 3 ? y0 : hy[weight][1];
		switch (arm) {
		case 0: gc_fill(bits, x0, x1, 0, 9); break;		/* up */
		case 1: gc_fill(bits, x0, x1, 5, ISO_CHAR_HEIGHT - 1); break;	/* down */
		case 2: gc_fill(bits, 0, 5, y0, y1); break;		/* left */
		case 3: gc_fill(bits, 2, ISO_CHAR_WIDTH - 1, y0, y1); break;	/* right */
		}
	}
}

static void
gc_glyph_bitmap(gc_glyph_t ch, unsigned char bits[ISO_CHAR_HEIGHT])
{
	memset(bits, 0, ISO_CHAR_HEIGHT);
	if (ch >= 0x2571 && ch <= 0x2573) {		/* diagonals */
		for (int y = 0; y < ISO_CHAR_HEIGHT; y++) {
			if (ch != 0x2572) {
				bits[y] |= (unsigned char)(1 << (7 - y / 2));	/* / */
			}
			if (ch != 0x2571) {
				bits[y] |= (unsigned char)(1 << (y / 2));	/* backslash */
			}
		}
	} else if (ch >= 0x2500 && ch <= 0x257F) {
		uint8_t arms = gc_box_arms[ch - 0x2500];
		for (int arm = 0; arm < 4; arm++) {
			int weight = (arms >> (6 - 2 * arm)) & 3;
			if (weight != 0) {
				gc_box_arm(bits, arm, weight);
			}
		}
	} else if (ch == 0x2580) {			/* upper half */
		gc_fill(bits, 0, 7, 0, 7);
	} else if (ch >= 0x2581 && ch <= 0x2588) {	/* lower eighths, full */
		gc_fill(bits, 0, 7, ISO_CHAR_HEIGHT - 2 * (ch - 0x2580), ISO_CHAR_HEIGHT - 1);
	} else if (ch >= 0x2589 && ch <= 0x258F) {	/* left eighths */
		gc_fill(bits, 0, 0x2590 - ch - 1, 0, ISO_CHAR_HEIGHT - 1);
	} else if (ch == 0x2590) {			/* right half */
		gc_fill(bits, 4, 7, 0, ISO_CHAR_HEIGHT - 1);
	} else if (ch >= 0x2591 && ch <= 0x2593) {	/* shades */
		for (int y = 0; y < ISO_CHAR_HEIGHT; y++) {
			unsigned char light = (y % 2) ? 0x22 : 0x88;
			bits[y] = ch == 0x2591 ? light : ch == 0x2592 ?
			    ((y % 2) ? 0x55 : 0xAA) : (unsigned char)~light;
		}
	} else if (ch == 0x2594) {			/* upper eighth */
		gc_fill(bits, 0, 7, 0, 1);
	} else if (ch == 0x2595) {			/* right eighth */
		gc_fill(bits, 7, 7, 0, ISO_CHAR_HEIGHT - 1);
	} else if (ch >= 0x2596 && ch <= 0x259F) {	/* quadrants */
		/* upper left, upper right, lower left, lower right */
		static const uint8_t quads[10] = {
			0x4, 0x8, 0x1, 0xD, 0x9, 0x7, 0xB, 0x2, 0x6, 0xE,
		};
		uint8_t q = quads[ch - 0x2596];
		if (q & 1) gc_fill(bits, 0, 3, 0, 7);
		if (q & 2) gc_fill(bits, 4, 7, 0, 7);
		if (q & 4) gc_fill(bits, 0, 3, 8, 15);
		if (q & 8) gc_fill(bits, 4, 7, 8, 15);
	} else if (ch >= 0x2800 && ch <= 0x28FF) {	/* Braille: 2 by 4 dots */
		static const uint8_t col[8] = { 0, 0, 0, 1, 1, 1, 0, 1 };
		static const uint8_t row[8] = { 0, 1, 2, 0, 1, 2, 3, 3 };
		for (int dot = 0; dot < 8; dot++) {
			if ((ch - 0x2800) & (1 << dot)) {
				gc_fill(bits, col[dot] * 4, col[dot] * 4 + 2,
				    row[dot] * 4, row[dot] * 4 + 2);
			}
		}
	} else if (ch == 0x25A0) {			/* black square */
		gc_fill(bits, 1, 6, 4, 11);
	} else if (ch == 0x25AA) {			/* small black square */
		gc_fill(bits, 2, 5, 6, 9);
	} else if (ch == 0x25B2 || ch == 0x25BC) {	/* triangles up, down */
		for (int i = 0; i < 4; i++) {
			int y = ch == 0x25B2 ? 5 + i * 2 : 10 - i * 2;
			gc_fill(bits, 3 - i, 4 + i, y, y + 1);
		}
	} else if (ch == 0x25CF || ch == 0x25CB) {	/* circles, filled and not */
		static const uint8_t disc[8] = { 0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C };
		static const uint8_t ring[8] = { 0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C };
		for (int i = 0; i < 8; i++) {
			bits[4 + i] = ch == 0x25CF ? disc[i] : ring[i];
		}
	} else if (ch == 0x2191 || ch == 0x2193) {	/* arrows up, down */
		gc_fill(bits, 3, 4, 3, 12);
		for (int i = 1; i <= 3; i++) {
			int y = ch == 0x2191 ? 3 + i : 12 - i;
			gc_fill(bits, 3 - i, 4 + i, y, y);
		}
	} else {
		/* Not one of the characters drawn here. */
		gc_fill(bits, 2, 5, 6, 9);
	}
}

static void
vc_render_char(gc_glyph_t ch, unsigned char *renderptr, short newdepth)
{
	union {
		unsigned char  *charptr;
		unsigned short *shortptr;
		uint32_t  *longptr;
	} current; 	/* current place in rendered font, multiple types. */
	const unsigned char *theChar;	/* current char in iso_font */
	unsigned char drawn[ISO_CHAR_HEIGHT];
	int line;

	current.charptr = renderptr;
	if (ch < 0x100) {
		theChar = iso_font + (ch * ISO_CHAR_HEIGHT);
	} else {
		gc_glyph_bitmap(ch, drawn);
		theChar = drawn;
	}

	for (line = 0; line < ISO_CHAR_HEIGHT; line++) {
		unsigned char mask = 1;
		do {
			switch (newdepth) {
			case 8: 
				*current.charptr++ = (*theChar & mask) ? 0xFF : 0;
				break;
			case 16:
				*current.shortptr++ = (*theChar & mask) ? 0xFFFF : 0;
				break;

			case 30: 
			case 32: 
				*current.longptr++ = (*theChar & mask) ? 0xFFFFFFFF : 0;
				break;
			}
			mask <<= 1;
		} while (mask);	/* while the single bit drops to the right */
		theChar++;
	}
}

static void
vc_paint_char_8(unsigned int xx, unsigned int yy, gc_glyph_t ch, int attrs,
		__unused gc_glyph_t ch_previous, __unused int attrs_previous)
{
	uint32_t *theChar;
	uint32_t *where;
	int i;
	
	if (vc_rendered_font && ch < 0x100) {
		theChar = (uint32_t*)(vc_rendered_font + (ch * vc_rendered_char_size));
	} else {
		vc_render_char(ch, vc_rendered_char, 8);
		theChar = (uint32_t*)(vc_rendered_char);
	}
	where = (uint32_t*)(vinfo.v_baseaddr + 
					(yy * ISO_CHAR_HEIGHT * vinfo.v_rowbytes) + 
					(xx * ISO_CHAR_WIDTH));

	if (!attrs) for (i = 0; i < ISO_CHAR_HEIGHT; i++) {	/* No attr? FLY !*/
		uint32_t *store = where;
		int x;
		for (x = 0; x < 2; x++) {
			uint32_t val = *theChar++;
			val = (vc_color_back & ~val) | (vc_color_fore & val);
			*store++ = val;
		}
		
		where = (uint32_t*)(((unsigned char*)where)+vinfo.v_rowbytes);
	} else for (i = 0; i < ISO_CHAR_HEIGHT; i++) {	/* a little slower */
		uint32_t *store = where, lastpixel = 0;
		int x;
		for (x = 0 ; x < 2; x++) {
			uint32_t val = *theChar++, save = val;
			if (attrs & ATTR_BOLD) {	/* bold support */
				if (lastpixel && !(save & 0xFF000000))
					val |= 0xff000000;
				if ((save & 0xFFFF0000) == 0xFF000000)
					val |= 0x00FF0000;
				if ((save & 0x00FFFF00) == 0x00FF0000)
					val |= 0x0000FF00;
				if ((save & 0x0000FFFF) == 0x0000FF00)
					val |= 0x000000FF;
			}
			if (attrs & ATTR_REVERSE) val = ~val;
			if (attrs & ATTR_UNDER &&  i == ISO_CHAR_HEIGHT-1) val = ~val;

			val = (vc_color_back & ~val) | (vc_color_fore & val);
			*store++ = val;
			lastpixel = save & 0xff;
		}
		
		where = (uint32_t*)(((unsigned char*)where)+vinfo.v_rowbytes);		
	}

}

static void
vc_paint_char_16(unsigned int xx, unsigned int yy, gc_glyph_t ch, int attrs,
		 __unused gc_glyph_t ch_previous,
		 __unused int attrs_previous) 
{
	uint32_t *theChar;
	uint32_t *where;
	int i;
	
	if (vc_rendered_font && ch < 0x100) {
		theChar = (uint32_t*)(vc_rendered_font + (ch * vc_rendered_char_size));
	} else {
		vc_render_char(ch, vc_rendered_char, 16);
		theChar = (uint32_t*)(vc_rendered_char);
	}
	where = (uint32_t*)(vinfo.v_baseaddr + 
				 (yy * ISO_CHAR_HEIGHT * vinfo.v_rowbytes) + 
				 (xx * ISO_CHAR_WIDTH * 2));

	if (!attrs) for (i = 0; i < ISO_CHAR_HEIGHT; i++) {	/* No attrs ? FLY ! */
		uint32_t *store = where;
		int x;
		for (x = 0; x < 4; x++) {
			uint32_t val = *theChar++;
			val = (vc_color_back & ~val) | (vc_color_fore & val);
			*store++ = val;
		}
		
		where = (uint32_t*)(((unsigned char*)where)+vinfo.v_rowbytes);
	} else for (i = 0; i < ISO_CHAR_HEIGHT; i++) { /* a little bit slower */
		uint32_t *store = where, lastpixel = 0;
		int x;
		for (x = 0 ; x < 4; x++) {
			uint32_t val = *theChar++, save = val;
			if (attrs & ATTR_BOLD) {	/* bold support */
				if (save == 0xFFFF0000) val |= 0xFFFF;
				else if (lastpixel && !(save & 0xFFFF0000))
					val |= 0xFFFF0000;
			}
			if (attrs & ATTR_REVERSE) val = ~val;
			if (attrs & ATTR_UNDER &&  i == ISO_CHAR_HEIGHT-1) val = ~val;

			val = (vc_color_back & ~val) | (vc_color_fore & val);

			*store++ = val;
			lastpixel = save & 0x7fff;
		}
		
		where = (uint32_t*)(((unsigned char*)where)+vinfo.v_rowbytes);		
	}

}

static void
vc_paint_char_32(unsigned int xx, unsigned int yy, gc_glyph_t ch, int attrs,
		 gc_glyph_t ch_previous, int attrs_previous) 
{
	uint32_t *theChar;
	uint32_t *theCharPrevious;
	uint32_t *where;
	int i;
	
	if (vc_rendered_font && ch < 0x100 && ch_previous < 0x100) {
		theChar = (uint32_t*)(vc_rendered_font + (ch * vc_rendered_char_size));
		theCharPrevious = (uint32_t*)(vc_rendered_font + (ch_previous * vc_rendered_char_size));
	} else if (vc_rendered_font && ch < 0x100) {
		theChar = (uint32_t*)(vc_rendered_font + (ch * vc_rendered_char_size));
		theCharPrevious = NULL;
	} else {
		vc_render_char(ch, vc_rendered_char, 32);
		theChar = (uint32_t*)(vc_rendered_char);
		theCharPrevious = NULL;
	}
	if (!ch_previous) {
		theCharPrevious = NULL;
	}
	if (attrs_previous) {
		theCharPrevious = NULL;
	}
	where = (uint32_t*)(vinfo.v_baseaddr + 
					(yy * ISO_CHAR_HEIGHT * vinfo.v_rowbytes) + 
					(xx * ISO_CHAR_WIDTH * 4));

	if (!attrs) for (i = 0; i < ISO_CHAR_HEIGHT; i++) {	/* No attrs ? FLY ! */
		uint32_t *store = where;
		int x;
		for (x = 0; x < 8; x++) {
			uint32_t val = *theChar++;
			if (theCharPrevious == NULL || val != *theCharPrevious++ ) {
				val = (vc_color_back & ~val) | (vc_color_fore & val);
				*store++ = val;
			} else {
				store++;
			}
		}
		
		where = (uint32_t *)(((unsigned char*)where)+vinfo.v_rowbytes);
	} else for (i = 0; i < ISO_CHAR_HEIGHT; i++) {	/* a little slower */
		uint32_t *store = where, lastpixel = 0;
		int x;
		for (x = 0 ; x < 8; x++) {
			uint32_t val = *theChar++, save = val;
			if (attrs & ATTR_BOLD) {	/* bold support */
				if (lastpixel && !save)
					val = 0xFFFFFFFF;
			}
			if (attrs & ATTR_REVERSE) val = ~val;
			if (attrs & ATTR_UNDER &&  i == ISO_CHAR_HEIGHT-1) val = ~val;

			val = (vc_color_back & ~val) | (vc_color_fore & val);
			*store++ = val;
			lastpixel = save;
		}
		
		where = (uint32_t*)(((unsigned char*)where)+vinfo.v_rowbytes);		
	}

}

static void
vc_paint_char(unsigned int xx, unsigned int yy, gc_glyph_t ch, int attrs,
	      gc_glyph_t ch_previous, int attrs_previous)
{
	if(!vinfo.v_depth)
		return;

	switch(vinfo.v_depth) {
	case 8:
		vc_paint_char_8(xx, yy, ch, attrs, ch_previous, attrs_previous);
		break;
	case 16:
		vc_paint_char_16(xx, yy, ch, attrs, ch_previous,
				 attrs_previous);
		break;
	case 30:
	case 32:
		vc_paint_char_32(xx, yy, ch, attrs, ch_previous,
				 attrs_previous);
		break;
	}
}

static void
vc_render_font(short newdepth)
{
	static short olddepth = 0;

	int charindex;	/* index in ISO font */
	unsigned char *rendered_font;
	unsigned int rendered_font_size;
	int rendered_char_size;
	spl_t s;

	if (vm_initialized == FALSE) {
		return;	/* nothing to do */
	}
	if (olddepth == newdepth && vc_rendered_font) {
		return;	/* nothing to do */
	}

	s = splhigh();
	VCPUTC_LOCK_LOCK();

	rendered_font      = vc_rendered_font;
	rendered_font_size = vc_rendered_font_size;
	rendered_char_size = vc_rendered_char_size;

	vc_rendered_font      = NULL;
	vc_rendered_font_size = 0;
	vc_rendered_char_size = 0;

	VCPUTC_LOCK_UNLOCK();
	splx(s);

	if (rendered_font) {
		kfree(rendered_font, rendered_font_size);
		rendered_font = NULL;
	}

	if (newdepth) {
		rendered_char_size = ISO_CHAR_HEIGHT * (((newdepth + 7) / 8) * ISO_CHAR_WIDTH);
		rendered_font_size = (ISO_CHAR_MAX-ISO_CHAR_MIN+1) * rendered_char_size;
		rendered_font = (unsigned char *) kalloc(rendered_font_size);
	}

	if (rendered_font == NULL) {
		return;
	}

	for (charindex = ISO_CHAR_MIN; charindex <= ISO_CHAR_MAX; charindex++) {
		vc_render_char(charindex, rendered_font + (charindex * rendered_char_size), newdepth);
	}

	olddepth = newdepth;

	s = splhigh();
	VCPUTC_LOCK_LOCK();

	vc_rendered_font      = rendered_font;
	vc_rendered_font_size = rendered_font_size;
	vc_rendered_char_size = rendered_char_size;

	VCPUTC_LOCK_UNLOCK();
	splx(s);
}

static void
vc_enable(boolean_t enable)
{
	vc_render_font(enable ? vinfo.v_depth : 0);
}

static void
vc_reverse_cursor(unsigned int xx, unsigned int yy)
{
	uint32_t *where;
	int line, col;

	if(!vinfo.v_depth)
		return;

	where = (uint32_t*)(vinfo.v_baseaddr + 
			(yy * ISO_CHAR_HEIGHT * vinfo.v_rowbytes) + 
			(xx /** ISO_CHAR_WIDTH*/ * vinfo.v_depth));
	for (line = 0; line < ISO_CHAR_HEIGHT; line++) {
		switch (vinfo.v_depth) {
			case 8:
				where[0] = ~where[0];
				where[1] = ~where[1];
				break;
			case 16:
				for (col = 0; col < 4; col++)
					where[col] = ~where[col];
				break;
			case 32:
				for (col = 0; col < 8; col++)
					where[col] = ~where[col];
				break;
		}
		where = (uint32_t*)(((unsigned char*)where)+vinfo.v_rowbytes);
	}
}

static void 
vc_scroll_down(int num, unsigned int scrreg_top, unsigned int scrreg_bottom)
{
	uint32_t *from, *to,  linelongs, i, line, rowline, rowscanline;

	if(!vinfo.v_depth)
		return;

	linelongs = vinfo.v_rowbytes * (ISO_CHAR_HEIGHT >> 2);
	rowline = vinfo.v_rowbytes >> 2;
	rowscanline = vinfo.v_rowscanbytes >> 2;

	to = (uint32_t *) vinfo.v_baseaddr + (linelongs * scrreg_bottom)
		- (rowline - rowscanline);
	from = to - (linelongs * num);	/* handle multiple line scroll (Michel Pollet) */

	i = (scrreg_bottom - scrreg_top) - num;

	while (i-- > 0) {
		for (line = 0; line < ISO_CHAR_HEIGHT; line++) {
			/*
			 * Only copy what is displayed
			 */
			video_scroll_down(from, 
					(from-(vinfo.v_rowscanbytes >> 2)), 
					to);

			from -= rowline;
			to -= rowline;
		}
	}
}

static void 
vc_scroll_up(int num, unsigned int scrreg_top, unsigned int scrreg_bottom)
{
	uint32_t *from, *to, linelongs, i, line, rowline, rowscanline;

	if(!vinfo.v_depth)
		return;

	linelongs = vinfo.v_rowbytes * (ISO_CHAR_HEIGHT >> 2);
	rowline = vinfo.v_rowbytes >> 2;
	rowscanline = vinfo.v_rowscanbytes >> 2;

	to = (uint32_t *) vinfo.v_baseaddr + (scrreg_top * linelongs);
	from = to + (linelongs * num);	/* handle multiple line scroll (Michel Pollet) */

	i = (scrreg_bottom - scrreg_top) - num;

	while (i-- > 0) {
		for (line = 0; line < ISO_CHAR_HEIGHT; line++) {
			/*
			 * Only copy what is displayed
			 */
			video_scroll_up(from, 
					(from+(vinfo.v_rowscanbytes >> 2)), 
					to);

			from += rowline;
			to += rowline;
		}
	}
}

static void
vc_update_color(int color, boolean_t fore)
{
	if (!vinfo.v_depth)
		return;
	if (fore) {
        	vc_color_fore = vc_colors[color][vc_color_index_table[vinfo.v_depth]];
	} else {
		vc_color_back = vc_colors[color][vc_color_index_table[vinfo.v_depth]];
	}
}

/*
 * Video Console (Back-End): Icon Control
 * --------------------------------------
 */

static vc_progress_element *	vc_progress;
enum { kMaxProgressData = 3 };
static const unsigned char *    vc_progress_data[kMaxProgressData];
static const unsigned char *    vc_progress_alpha;
static boolean_t		vc_progress_enable;
static const unsigned char *    vc_clut;
static const unsigned char *    vc_clut8;
static unsigned char            vc_revclut8[256];
static uint32_t            	vc_progress_interval;
static uint32_t            	vc_progress_count;
static uint32_t            	vc_progress_angle;
static uint64_t			vc_progress_deadline;
static thread_call_data_t	vc_progress_call;
static boolean_t		vc_needsave;
static void *			vc_saveunder;
static vm_size_t		vc_saveunder_len;
static int8_t			vc_uiscale = 1;
vc_progress_user_options        vc_progress_options;
vc_progress_user_options        vc_user_options;

decl_simple_lock_data(,vc_progress_lock);

#if !CONFIG_EMBEDDED
static int           		vc_progress_withmeter = 3;
int                             vc_progressmeter_enable;
static int                      vc_progressmeter_drawn;
int            	                vc_progressmeter_value;
static uint32_t            	vc_progressmeter_count;
static uint32_t            	vc_progress_meter_start;
static uint32_t            	vc_progress_meter_end;
static uint64_t            	vc_progressmeter_interval;
static uint64_t            	vc_progressmeter_deadline;
static thread_call_data_t	vc_progressmeter_call;
static void *                   vc_progressmeter_backbuffer;
static boolean_t                vc_progressmeter_hold;
static uint32_t                 vc_progressmeter_diskspeed = 256;

#endif  /* !CONFIG_EMBEDDED */

enum {
    kSave          = 0x10,
    kDataIndexed   = 0x20,
    kDataAlpha     = 0x40,
    kDataBack      = 0x80,
    kDataRotate    = 0x03,
};

static void vc_blit_rect(int x, int y, int bx,
                            int width, int height,
			    int sourceWidth, int sourceHeight,
			    int sourceRow, int backRow,
			    const unsigned char * dataPtr,
			    void * backBuffer,
			    unsigned int flags);
static void vc_blit_rect_8(int x, int y, int bx,
                            int width, int height,
			    int sourceWidth, int sourceHeight,
			    int sourceRow, int backRow,
			    const unsigned char * dataPtr,
			    unsigned char * backBuffer,
			    unsigned int flags);
static void vc_blit_rect_16(int x, int y, int bx,
                            int width, int height,
			    int sourceWidth, int sourceHeight,
			    int sourceRow, int backRow,
			    const unsigned char * dataPtr,
			    unsigned short * backBuffer,
			    unsigned int flags);
static void vc_blit_rect_32(int x, int y, int bx,
                            int width, int height,
			    int sourceWidth, int sourceHeight,
			    int sourceRow, int backRow,
			    const unsigned char * dataPtr,
			    unsigned int * backBuffer,
			    unsigned int flags);
static void vc_blit_rect_30(int x, int y, int bx,
                            int width, int height,
			    int sourceWidth, int sourceHeight,
			    int sourceRow, int backRow,
			    const unsigned char * dataPtr,
			    unsigned int * backBuffer,
			    unsigned int flags);
static void vc_progress_task( void * arg0, void * arg );
#if !CONFIG_EMBEDDED
static void vc_progressmeter_task( void * arg0, void * arg );
#endif	/* !CONFIG_EMBEDDED */

static void vc_blit_rect(int x, int y, int bx,
			    int width, int height,
			    int sourceWidth, int sourceHeight,
			    int sourceRow, int backRow,
			    const unsigned char * dataPtr,
			    void * backBuffer,
			    unsigned int flags)
{
    if (!vinfo.v_depth)                                return;
    if (((unsigned int)(x + width))  > vinfo.v_width)  return;
    if (((unsigned int)(y + height)) > vinfo.v_height) return;

    switch( vinfo.v_depth) {
	case 8:
            if( vc_clut8 == vc_clut)
                vc_blit_rect_8( x, y, bx, width, height, sourceWidth, sourceHeight, sourceRow, backRow, dataPtr, (unsigned char *) backBuffer, flags );
	    break;
	case 16:
	    vc_blit_rect_16( x, y, bx, width, height, sourceWidth, sourceHeight, sourceRow, backRow, dataPtr, (unsigned short *) backBuffer, flags );
	    break;
	case 32:
	    vc_blit_rect_32( x, y, bx, width, height, sourceWidth, sourceHeight, sourceRow, backRow, dataPtr, (unsigned int *) backBuffer, flags );
	    break;
	case 30:
	    vc_blit_rect_30( x, y, bx, width, height, sourceWidth, sourceHeight, sourceRow, backRow, dataPtr, (unsigned int *) backBuffer, flags );
	    break;
    }
}

static void
vc_blit_rect_8(int x, int y, __unused int bx,
	       int width, int height,
	       int sourceWidth, int sourceHeight,
	       int sourceRow, __unused int backRow,
	       const unsigned char * dataPtr,
	       __unused unsigned char * backBuffer,
	       __unused unsigned int flags)
{
    volatile unsigned short * dst;
    int line, col;
    unsigned int data = 0, out = 0;
    int sx, sy, a, b, c, d;
    int scale = 0x10000;

    a = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][0] * scale;
    b = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][1] * scale;
    c = vc_rotate_matr[kDataRotate & flags][1][0] * scale;
    d = vc_rotate_matr[kDataRotate & flags][1][1] * scale;

    sx = ((a + b) < 0) ? ((sourceWidth * scale)  - 0x8000) : 0;
    sy = ((c + d) < 0) ? ((sourceHeight * scale) - 0x8000) : 0;

    if (!sourceRow) data = (unsigned int)(uintptr_t)dataPtr;

    dst = (volatile unsigned short *) (vinfo.v_baseaddr +
                                    (y * vinfo.v_rowbytes) +
                                    (x * 4));

    for( line = 0; line < height; line++)
    {
        for( col = 0; col < width; col++)
	{
	    if (sourceRow) data = dataPtr[((sx + (col * a) + (line * b)) >> 16)
				+ sourceRow * (((sy + (col * c) + (line * d)) >> 16))];
	    if (kDataAlpha & flags)
		out = vc_revclut8[data];
            else
		out = data;
            *(dst + col) = out;
	}
        dst = (volatile unsigned short *) (((volatile char*)dst) + vinfo.v_rowbytes);
    }
}

/* For ARM, 16-bit is 565 (RGB); it is 1555 (XRGB) on other platforms */

#ifdef __arm__
#define CLUT_MASK_R	0xf8
#define CLUT_MASK_G	0xfc
#define CLUT_MASK_B	0xf8
#define CLUT_SHIFT_R	<< 8
#define CLUT_SHIFT_G	<< 3
#define CLUT_SHIFT_B	>> 3
#define MASK_R		0xf800
#define MASK_G		0x07e0
#define MASK_B		0x001f
#define MASK_R_8	0x7f800
#define MASK_G_8	0x01fe0
#define MASK_B_8	0x000ff
#else
#define CLUT_MASK_R	0xf8
#define CLUT_MASK_G	0xf8
#define CLUT_MASK_B	0xf8
#define CLUT_SHIFT_R	<< 7
#define CLUT_SHIFT_G	<< 2
#define CLUT_SHIFT_B	>> 3
#define MASK_R		0x7c00
#define MASK_G		0x03e0
#define MASK_B		0x001f
#define MASK_R_8	0x3fc00
#define MASK_G_8	0x01fe0
#define MASK_B_8	0x000ff
#endif

static void vc_blit_rect_16( int x, int y, int bx,
			     int width, int height,
			     int sourceWidth, int sourceHeight,
			     int sourceRow, int backRow,
			     const unsigned char * dataPtr,
			     unsigned short * backPtr,
			     unsigned int flags)
{
    volatile unsigned short * dst;
    int line, col;
    unsigned int data = 0, out = 0, back = 0;
    int sx, sy, a, b, c, d;
    int scale = 0x10000;

    a = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][0] * scale;
    b = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][1] * scale;
    c = vc_rotate_matr[kDataRotate & flags][1][0] * scale;
    d = vc_rotate_matr[kDataRotate & flags][1][1] * scale;

    sx = ((a + b) < 0) ? ((sourceWidth * scale)  - 0x8000) : 0;
    sy = ((c + d) < 0) ? ((sourceHeight * scale) - 0x8000) : 0;

    if (!sourceRow) data = (unsigned int)(uintptr_t)dataPtr;

    if (backPtr)
	backPtr += bx;
    dst = (volatile unsigned short *) (vinfo.v_baseaddr +
                                    (y * vinfo.v_rowbytes) +
                                    (x * 2));

    for( line = 0; line < height; line++)
    {
        for (col = 0; col < width; col++)
	{
	    if (sourceRow) data = dataPtr[((sx + (col * a) + (line * b)) >> 16)
				+ sourceRow * (((sy + (col * c) + (line * d)) >> 16))];
	    if (backPtr) {
		if (kSave & flags) {
		    back = *(dst + col);
		    *backPtr++ = back;
		} else
		    back = *backPtr++;
	    }
	    if (kDataIndexed & flags) {
		out = ( (CLUT_MASK_R & (vc_clut[data*3 + 0])) CLUT_SHIFT_R)
		       | ( (CLUT_MASK_G & (vc_clut[data*3 + 1])) CLUT_SHIFT_G)
		       | ( (CLUT_MASK_B & (vc_clut[data*3 + 2])) CLUT_SHIFT_B);
	    } else if (kDataAlpha & flags) {
		out = (((((back & MASK_R) * data) + MASK_R_8) >> 8) & MASK_R)
		     | (((((back & MASK_G) * data) + MASK_G_8) >> 8) & MASK_G)
		     | (((((back & MASK_B) * data) + MASK_B_8) >> 8) & MASK_B);
		if (vc_progress_white) out += (((0xff - data) & CLUT_MASK_R) CLUT_SHIFT_R)
					    | (((0xff - data) & CLUT_MASK_G) CLUT_SHIFT_G)
					    | (((0xff - data) & CLUT_MASK_B) CLUT_SHIFT_B);
            } else if (kDataBack & flags)
		out = back;
	    else
		out = data;
            *(dst + col) = out;
	}
        dst = (volatile unsigned short *) (((volatile char*)dst) + vinfo.v_rowbytes);
	if (backPtr)
	    backPtr += backRow - width;
    }
}


static void vc_blit_rect_32(int x, int y, int bx, 
                            int width, int height,
			    int sourceWidth, int sourceHeight,
			    int sourceRow, int backRow,
			    const unsigned char * dataPtr,
			    unsigned int * backPtr,
			    unsigned int flags)
{
    volatile unsigned int * dst;
    int line, col;
    unsigned int data = 0, out = 0, back = 0;
    int sx, sy, a, b, c, d;
    int scale = 0x10000;

    a = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][0] * scale;
    b = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][1] * scale;
    c = vc_rotate_matr[kDataRotate & flags][1][0] * scale;
    d = vc_rotate_matr[kDataRotate & flags][1][1] * scale;

    sx = ((a + b) < 0) ? ((sourceWidth * scale)  - 0x8000) : 0;
    sy = ((c + d) < 0) ? ((sourceHeight * scale) - 0x8000) : 0;
        
    if (!sourceRow) data = (unsigned int)(uintptr_t)dataPtr;

    if (backPtr)
	backPtr += bx;
    dst = (volatile unsigned int *) (vinfo.v_baseaddr +
                                    (y * vinfo.v_rowbytes) +
                                    (x * 4));

    for( line = 0; line < height; line++)
    {
        for( col = 0; col < width; col++)
	{
	    if (sourceRow) data = dataPtr[((sx + (col * a) + (line * b)) >> 16)
				+ sourceRow * (((sy + (col * c) + (line * d)) >> 16))];
	    if (backPtr) {
		if (kSave & flags) {
		    back = *(dst + col);
		    *backPtr++ = back;
		} else
		    back = *backPtr++;
	    }
	    if (kDataIndexed & flags) {
		out =     (vc_clut[data*3 + 0] << 16)
			| (vc_clut[data*3 + 1] << 8)
			| (vc_clut[data*3 + 2]);
	    } else if (kDataAlpha & flags) {
		out = (((((back & 0x00ff00ff) * data) + 0x00ff00ff) >> 8) & 0x00ff00ff)
		     | (((((back & 0x0000ff00) * data) + 0x0000ff00) >> 8) & 0x0000ff00);
		if (vc_progress_white) out += ((0xff - data) << 16)
					    | ((0xff - data) << 8)
					    |  (0xff - data);
            } else if (kDataBack & flags)
		out = back;
	    else
		out = data;
            *(dst + col) = out;
	}
        dst = (volatile unsigned int *) (((volatile char*)dst) + vinfo.v_rowbytes);
	if (backPtr)
	    backPtr += backRow - width;
    }
}

static void vc_blit_rect_30(int x, int y, int bx, 
                            int width, int height,
			    int sourceWidth, int sourceHeight,
			    int sourceRow, int backRow,
			    const unsigned char * dataPtr,
			    unsigned int * backPtr,
			    unsigned int flags)
{
    volatile unsigned int * dst;
    int line, col;
    unsigned int data = 0, out = 0, back = 0;
    unsigned long long exp;
    int sx, sy, a, b, c, d;
    int scale = 0x10000;

    a = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][0] * scale;
    b = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][1] * scale;
    c = vc_rotate_matr[kDataRotate & flags][1][0] * scale;
    d = vc_rotate_matr[kDataRotate & flags][1][1] * scale;

    sx = ((a + b) < 0) ? ((sourceWidth * scale)  - 0x8000) : 0;
    sy = ((c + d) < 0) ? ((sourceHeight * scale) - 0x8000) : 0;
        
    if (!sourceRow) data = (unsigned int)(uintptr_t)dataPtr;

    if (backPtr)
	backPtr += bx;
    dst = (volatile unsigned int *) (vinfo.v_baseaddr +
                                    (y * vinfo.v_rowbytes) +
                                    (x * 4));

    for( line = 0; line < height; line++)
    {
        for( col = 0; col < width; col++)
	{
	    if (sourceRow) data = dataPtr[((sx + (col * a) + (line * b)) >> 16)
				+ sourceRow * (((sy + (col * c) + (line * d)) >> 16))];
	    if (backPtr) {
		if (kSave & flags) {
		    back = *(dst + col);
		    *backPtr++ = back;
		} else
		    back = *backPtr++;
	    }
	    if (kDataIndexed & flags) {
		out =     (vc_clut[data*3 + 0] << 22)
			| (vc_clut[data*3 + 1] << 12)
			| (vc_clut[data*3 + 2] << 2);
	    } else if (kDataAlpha & flags) {
		exp = back;
		exp =  (((((exp & 0x3FF003FF) * data) + 0x0FF000FF) >> 8) & 0x3FF003FF)
		     | (((((exp & 0x000FFC00) * data) + 0x0003FC00) >> 8) & 0x000FFC00);
		out = (unsigned int)exp;
		if (vc_progress_white) out += ((0xFF - data) << 22)
					    | ((0xFF - data) << 12)
					    | ((0xFF - data) << 2);
            } else if (kDataBack & flags)
		out = back;
	    else
		out = data;
            *(dst + col) = out;
	}
        dst = (volatile unsigned int *) (((volatile char*)dst) + vinfo.v_rowbytes);
	if (backPtr)
	    backPtr += backRow - width;
    }
}

static void vc_clean_boot_graphics(void)
{
#if !CONFIG_EMBEDDED
    // clean up possible FDE login graphics
    vc_progress_set(FALSE, 0);
    const unsigned char *
    color = (typeof(color))(uintptr_t)(vc_progress_white ? 0x00000000 : 0xBFBFBFBF);
    vc_blit_rect(0, 0, 0, vinfo.v_width, vinfo.v_height, vinfo.v_width, vinfo.v_height, 0, 0, color, NULL, 0);
#endif
}

/*
 * Routines to render the lzss image format
 */

struct lzss_image_state {
	uint32_t col;
	uint32_t row;
	uint32_t width;
	uint32_t height;
	uint32_t bytes_per_row;
	volatile uint32_t * row_start;
	const uint8_t* clut;
};
typedef struct lzss_image_state lzss_image_state;

// returns 0 if OK, 1 if error
static inline int 
vc_decompress_lzss_next_pixel (int next_data, lzss_image_state* state) 
{
    uint32_t palette_index = 0;
    uint32_t pixel_value   = 0;

    palette_index = next_data * 3;

    pixel_value = ( (uint32_t) state->clut[palette_index + 0] << 16) 
                | ( (uint32_t) state->clut[palette_index + 1] << 8) 
                | ( (uint32_t) state->clut[palette_index + 2]);

    *(state->row_start + state->col) = pixel_value;

    if (++state->col >= state->width) {
        state->col = 0;
        if (++state->row >= state->height) {
            return 1;
        }
        state->row_start = (volatile uint32_t *) (((uintptr_t)state->row_start) + state->bytes_per_row);
    }
    return 0;
}


/*
 * Blit an lzss compressed image to the framebuffer
 * Assumes 32 bit screen (which is everything we ship at the moment)
 * The function vc_display_lzss_icon was copied from libkern/mkext.c, then modified.
 */

/* 
 * TODO: Does lzss use too much stack? 4096 plus bytes... 
 * 	Can probably chop it down by 1/2.
 */

/**************************************************************
 LZSS.C -- A Data Compression Program
***************************************************************
    4/6/1989 Haruhiko Okumura
    Use, distribute, and modify this program freely.
    Please send me your improved versions.
        PC-VAN      SCIENCE
        NIFTY-Serve PAF01022
        CompuServe  74050,1022

**************************************************************/

#define N         4096  /* size of ring buffer - must be power of 2 */
#define F         18    /* upper limit for match_length */
#define THRESHOLD 2     /* encode string into position and length
                           if match_length is greater than this */

// returns 0 if OK, 1 if error
// x and y indicate upper left corner of image location on screen
int
vc_display_lzss_icon(uint32_t dst_x,       uint32_t dst_y,
                     uint32_t image_width, uint32_t image_height,
                     const uint8_t *compressed_image,
                     uint32_t       compressed_size, 
                     const uint8_t *clut)
{
    uint32_t* image_start;
    uint32_t bytes_per_pixel = 4;
    uint32_t bytes_per_row = vinfo.v_rowbytes;

    vc_clean_boot_graphics();

    image_start = (uint32_t *) (vinfo.v_baseaddr + (dst_y * bytes_per_row) + (dst_x * bytes_per_pixel));
    
    lzss_image_state state = {0, 0, image_width, image_height, bytes_per_row, image_start, clut};

    int rval = 0;

    const uint8_t *src = compressed_image;
    uint32_t srclen = compressed_size;

    /* ring buffer of size N, with extra F-1 bytes to aid string comparison */
    uint8_t text_buf[N + F - 1];
    const uint8_t *srcend = src + srclen;
    int  i, j, k, r, c;
    unsigned int flags;

    srcend = src + srclen;
    for (i = 0; i < N - F; i++)
        text_buf[i] = ' ';
    r = N - F;
    flags = 0;
    for ( ; ; ) {
        if (((flags >>= 1) & 0x100) == 0) {
            if (src < srcend) c = *src++; else break;
            flags = c | 0xFF00;  /* uses higher byte cleverly */
        }   /* to count eight */
        if (flags & 1) {
            if (src < srcend) c = *src++; else break;
            rval = vc_decompress_lzss_next_pixel(c, &state);
            if (rval != 0)
                return rval;
            text_buf[r++] = c;
            r &= (N - 1);
        } else {
            if (src < srcend) i = *src++; else break;
            if (src < srcend) j = *src++; else break;
            i |= ((j & 0xF0) << 4);
            j  =  (j & 0x0F) + THRESHOLD;
            for (k = 0; k <= j; k++) {
                c = text_buf[(i + k) & (N - 1)];
                rval = vc_decompress_lzss_next_pixel(c, &state);
                if (rval != 0 )
                    return rval;
                text_buf[r++] = c;
                r &= (N - 1);
            }
        }
    }
    return 0;
}

void noroot_icon_test(void) {
    boolean_t o_vc_progress_enable = vc_progress_enable;

    vc_progress_enable = 1;

    PE_display_icon( 0, "noroot");

    vc_progress_enable = o_vc_progress_enable;
}


void vc_display_icon( vc_progress_element * desc,
			const unsigned char * data )
{
    int			x, y, width, height;

    if( vc_progress_enable && vc_clut) {

	vc_clean_boot_graphics();

	width = desc->width;
	height = desc->height;
	x = desc->dx;
	y = desc->dy;
	if( 1 & desc->flags) {
	    x += ((vinfo.v_width - width) / 2);
	    y += ((vinfo.v_height - height) / 2);
	}
	vc_blit_rect( x, y, 0, width, height, width, height, 0, 0, data, NULL, kDataIndexed );
    }
}

void
vc_progress_initialize( vc_progress_element * desc,
			const unsigned char * data1x,
			const unsigned char * data2x,
			const unsigned char * data3x,
			const unsigned char * clut )
{
    uint64_t	abstime;

    if( (!clut) || (!desc) || (!data1x))
	return;
    vc_clut = clut;
    vc_clut8 = clut;

    vc_progress = desc;
    vc_progress_data[0] = data1x;
    vc_progress_data[1] = data2x;
    vc_progress_data[2] = data3x;
    if( 2 & vc_progress->flags)
        vc_progress_alpha = data1x
                            + vc_progress->count * vc_progress->width * vc_progress->height;
    else
        vc_progress_alpha = NULL;

    thread_call_setup(&vc_progress_call, vc_progress_task, NULL);
    clock_interval_to_absolutetime_interval(vc_progress->time, 1000 * 1000, &abstime);
    vc_progress_interval = (uint32_t)abstime;

#if !CONFIG_EMBEDDED
    thread_call_setup(&vc_progressmeter_call, vc_progressmeter_task, NULL);
    clock_interval_to_absolutetime_interval(1000 / 8, 1000 * 1000, &abstime);
    vc_progressmeter_interval = (uint32_t)abstime;
#endif	/* !CONFIG_EMBEDDED */

}

void
vc_progress_set(boolean_t enable, uint32_t vc_delay)
{
    spl_t	     s;
    void             *saveBuf = NULL;
    vm_size_t        saveLen = 0;
    unsigned int     count;
    unsigned int     index;
    unsigned char    pdata8;
    unsigned short   pdata16;
    unsigned short * buf16;
    unsigned int     pdata32;
    unsigned int *   buf32;

#if !CONFIG_EMBEDDED

    if (kBootArgsFlagBlack & ((boot_args *) PE_state.bootArgs)->flags) return;

    if (1 & vc_progress_withmeter)
    {
	if (enable) internal_enable_progressmeter(kProgressMeterKernel);

	s = splhigh();
	simple_lock(&vc_progress_lock, LCK_GRP_NULL);

	if( vc_progress_enable != enable) {
	    vc_progress_enable = enable;
	    if( enable)
	    {
		vc_progressmeter_count = 0;
		clock_interval_to_deadline(vc_delay,
					   1000 * 1000 * 1000 /*second scale*/,
					   &vc_progressmeter_deadline);
		thread_call_enter_delayed(&vc_progressmeter_call, vc_progressmeter_deadline);
	    } 
	    else thread_call_cancel(&vc_progressmeter_call);
	}

	simple_unlock(&vc_progress_lock);
	splx(s);

	if (!enable) internal_enable_progressmeter(kProgressMeterOff);
	return;
    }

#endif /* !CONFIG_EMBEDDED */

    if(!vc_progress) return;

    if( enable) {
        saveLen = (vc_progress->width * vc_uiscale) * (vc_progress->height * vc_uiscale) * ((vinfo.v_depth + 7) / 8);
        saveBuf = kalloc( saveLen );

	switch( vinfo.v_depth) {
	    case 8 :
		for( count = 0; count < 256; count++) {
		    vc_revclut8[count] = vc_clut[0x01 * 3];
		    pdata8 = (vc_clut[0x01 * 3] * count + 0x0ff) >> 8;
		    for( index = 0; index < 256; index++) {
			if( (pdata8 == vc_clut[index * 3 + 0]) &&
			    (pdata8 == vc_clut[index * 3 + 1]) &&
			    (pdata8 == vc_clut[index * 3 + 2])) {
			    vc_revclut8[count] = index;
			    break;
			}
		    }
		}
		memset( saveBuf, 0x01, saveLen );
		break;

	    case 16 :
		buf16 = (unsigned short *) saveBuf;
		pdata16 = ((vc_clut[0x01 * 3 + 0] & CLUT_MASK_R) CLUT_SHIFT_R)
		       | ((vc_clut[0x01 * 3 + 0] & CLUT_MASK_G) CLUT_SHIFT_G)
		       | ((vc_clut[0x01 * 3 + 0] & CLUT_MASK_B) CLUT_SHIFT_B);
		for( count = 0; count < saveLen / 2; count++)
		    buf16[count] = pdata16;
		break;

	    case 32 :
		buf32 = (unsigned int *) saveBuf;
		pdata32 = ((vc_clut[0x01 * 3 + 0] & 0xff) << 16)
		       | ((vc_clut[0x01 * 3 + 1] & 0xff) << 8)
		       | ((vc_clut[0x01 * 3 + 2] & 0xff) << 0);
		for( count = 0; count < saveLen / 4; count++)
		    buf32[count] = pdata32;
		break;
	}
    }

    s = splhigh();
    simple_lock(&vc_progress_lock, LCK_GRP_NULL);

    if( vc_progress_enable != enable) {
        vc_progress_enable = enable;
        if( enable) {
            vc_needsave      = TRUE;
            vc_saveunder     = saveBuf;
            vc_saveunder_len = saveLen;
            saveBuf	          = NULL;
            saveLen 	      = 0;
            vc_progress_count = 0;
	    vc_progress_angle = 0;

            clock_interval_to_deadline(vc_delay,
				       1000 * 1000 * 1000 /*second scale*/,
				       &vc_progress_deadline);
            thread_call_enter_delayed(&vc_progress_call, vc_progress_deadline);

        } else {
            if( vc_saveunder) {
                saveBuf      = vc_saveunder;
                saveLen      = vc_saveunder_len;
                vc_saveunder = NULL;
                vc_saveunder_len = 0;
            }

            thread_call_cancel(&vc_progress_call);
        }
    }

    simple_unlock(&vc_progress_lock);
    splx(s);

    if( saveBuf)
        kfree( saveBuf, saveLen );
}

#if !CONFIG_EMBEDDED

static uint32_t vc_progressmeter_range(uint32_t pos)
{
    uint32_t ret;

    if (pos > kProgressMeterEnd) pos = kProgressMeterEnd;
    ret = vc_progress_meter_start 
    	+ ((pos * (vc_progress_meter_end - vc_progress_meter_start)) / kProgressMeterEnd);

    return (ret);
}

static void
vc_progressmeter_task(__unused void *arg0, __unused void *arg)
{
    spl_t    s;
    uint64_t interval;

    s = splhigh();
    simple_lock(&vc_progress_lock, LCK_GRP_NULL);
    if (vc_progressmeter_enable)
    {
	uint32_t pos = (vc_progressmeter_count >> 13);
	internal_set_progressmeter(vc_progressmeter_range(pos));
	if (pos < kProgressMeterEnd)
	{
            static uint16_t incr[8] = { 10000, 10000, 8192, 4096, 2048, 384, 384, 64 };
	    vc_progressmeter_count += incr[(pos * 8) / kProgressMeterEnd];

	    interval = vc_progressmeter_interval;
	    interval = ((interval * 256) / vc_progressmeter_diskspeed);

	    clock_deadline_for_periodic_event(interval, mach_absolute_time(), &vc_progressmeter_deadline);
	    thread_call_enter_delayed(&vc_progressmeter_call, vc_progressmeter_deadline);
	}
    }
    simple_unlock(&vc_progress_lock);
    splx(s);
}

void vc_progress_setdiskspeed(uint32_t speed)
{
    vc_progressmeter_diskspeed = speed;
}

#endif	/* !CONFIG_EMBEDDED */

static void
vc_progress_task(__unused void *arg0, __unused void *arg)
{
    spl_t     s;
    int	      x, y, width, height;
    uint64_t  x_pos, y_pos;
    const unsigned char * data;

    s = splhigh();
    simple_lock(&vc_progress_lock, LCK_GRP_NULL);

    if( vc_progress_enable) do {
    
	vc_progress_count++;
	if( vc_progress_count >= vc_progress->count) {
	    vc_progress_count = 0;
	    vc_progress_angle++;
	}

	width  = (vc_progress->width * vc_uiscale);
	height = (vc_progress->height * vc_uiscale);
	data   = vc_progress_data[vc_uiscale - 1];
	if (!data) break;

	if (kVCUsePosition & vc_progress_options.options) {
	    /* Rotation: 0:normal, 1:right 90, 2:left 180, 3:left 90 */
	    switch (3 & vinfo.v_rotate) {
		case kDataRotate0:
		    x_pos = vc_progress_options.x_pos;
		    y_pos = vc_progress_options.y_pos;
		    break;
		case kDataRotate180:
		    x_pos = 0xFFFFFFFF - vc_progress_options.x_pos;
		    y_pos = 0xFFFFFFFF - vc_progress_options.y_pos;
		    break;
		case kDataRotate90:
		    x_pos = 0xFFFFFFFF - vc_progress_options.y_pos;
		    y_pos = vc_progress_options.x_pos;
		    break;
		case kDataRotate270:
		    x_pos = vc_progress_options.y_pos;
		    y_pos = 0xFFFFFFFF - vc_progress_options.x_pos;
		    break;
	    }
	    x = (uint32_t)((x_pos * (uint64_t) vinfo.v_width) / 0xFFFFFFFFULL);
	    y = (uint32_t)((y_pos * (uint64_t) vinfo.v_height) / 0xFFFFFFFFULL);
	    x -= (width / 2);
	    y -= (height / 2);
	} else {
	    x = (vc_progress->dx * vc_uiscale);
	    y = (vc_progress->dy * vc_uiscale);
	    if( 1 & vc_progress->flags) {
		x += ((vinfo.v_width - width) / 2);
		y += ((vinfo.v_height - height) / 2);
	    }
	}

	if ((x + width)  > (int)vinfo.v_width)  break;
	if ((y + height) > (int)vinfo.v_height) break;

	data += vc_progress_count * width * height;

	vc_blit_rect( x, y, 0,
		      width, height, width, height, width, width,
		      data, vc_saveunder,
		      kDataAlpha
		      | (vc_progress_angle & kDataRotate) 
		      | (vc_needsave ? kSave : 0) );
	vc_needsave = FALSE;

	clock_deadline_for_periodic_event(vc_progress_interval, mach_absolute_time(), &vc_progress_deadline);
	thread_call_enter_delayed(&vc_progress_call, vc_progress_deadline);
    }
    while (FALSE);
    simple_unlock(&vc_progress_lock);
    splx(s);
}

/*
 * Generic Console (Front-End): Master Control
 * -------------------------------------------
 */

#if defined (__i386__) || defined (__x86_64__)
#include <pexpert/i386/boot.h>
#endif

static boolean_t gc_acquired      = FALSE;
static boolean_t gc_graphics_boot = FALSE;
static boolean_t gc_desire_text   = FALSE;
static boolean_t gc_paused_progress;

static vm_offset_t  lastVideoVirt    = 0;
static vm_size_t    lastVideoMapSize = 0;
static boolean_t    lastVideoMapKmap = FALSE;

static void
gc_pause( boolean_t pause, boolean_t graphics_now )
{
	spl_t s;

	s = splhigh( );
	VCPUTC_LOCK_LOCK( );

    disableConsoleOutput = (pause && !console_is_serial());
    gc_enabled           = (!pause && graphics_now);

    VCPUTC_LOCK_UNLOCK( );

    simple_lock(&vc_progress_lock, LCK_GRP_NULL);

    if (pause) 
    {
        gc_paused_progress = vc_progress_enable;
	vc_progress_enable = FALSE;
    }
    else vc_progress_enable = gc_paused_progress;

    if (vc_progress_enable)
    {
#if !CONFIG_EMBEDDED
	if (1 & vc_progress_withmeter) thread_call_enter_delayed(&vc_progressmeter_call, vc_progressmeter_deadline);
	else                           
#endif /* !CONFIG_EMBEDDED */
	thread_call_enter_delayed(&vc_progress_call, vc_progress_deadline);
    }

    simple_unlock(&vc_progress_lock);
    splx(s);
}

static void
vc_initialize(__unused struct vc_info * vinfo_p)
{
#ifdef __arm__
	unsigned long cnt, data16, data32;

	if (vinfo.v_depth == 16) {
		for (cnt = 0; cnt < 8; cnt++) {
			data32 = vc_colors[cnt][2];
			data16  = (data32 & 0x0000F8) <<  8;
			data16 |= (data32 & 0x00FC00) >>  5;
			data16 |= (data32 & 0xF80000) >> 19;
			data16 |= data16 << 16;
			vc_colors[cnt][1] = data16;
		}
	}
#endif

	vinfo.v_rows = vinfo.v_height / ISO_CHAR_HEIGHT;
	vinfo.v_columns = vinfo.v_width / ISO_CHAR_WIDTH;
	vinfo.v_rowscanbytes = ((vinfo.v_depth + 7) / 8) * vinfo.v_width;
	vc_uiscale = vinfo.v_scale;
	if (vc_uiscale > kMaxProgressData) vc_uiscale = kMaxProgressData;
	else if (!vc_uiscale)              vc_uiscale = 1;
}

void
initialize_screen(PE_Video * boot_vinfo, unsigned int op)
{
	unsigned int newMapSize = 0;
	vm_offset_t newVideoVirt = 0;
	boolean_t graphics_now;
	uint32_t delay;

	if ( boot_vinfo )
	{
		struct vc_info new_vinfo = vinfo;
		boolean_t makeMapping = FALSE;
		/*
		 * Minkar: a driver that hands the console the mode it already
		 * has (the GOP framebuffer) must not start it over: the cursor
		 * would go back to the top and the next lines would be drawn
		 * over the old ones.
		 */
		boolean_t sameMode = gc_initialized &&
		    vinfo.v_width == (unsigned int)boot_vinfo->v_width &&
		    vinfo.v_height == (unsigned int)boot_vinfo->v_height &&
		    vinfo.v_depth == (unsigned int)boot_vinfo->v_depth &&
		    vinfo.v_rowbytes == (unsigned int)boot_vinfo->v_rowBytes;

		/* 
		 *	Copy parameters
		 */
		if (kPEBaseAddressChange != op)
		{
		    new_vinfo.v_width    = (unsigned int)boot_vinfo->v_width;
		    new_vinfo.v_height   = (unsigned int)boot_vinfo->v_height;
		    new_vinfo.v_depth    = (unsigned int)boot_vinfo->v_depth;
		    new_vinfo.v_rowbytes = (unsigned int)boot_vinfo->v_rowBytes;
		    if (kernel_map == VM_MAP_NULL) {
				// only booter supplies HW rotation
				new_vinfo.v_rotate   = (unsigned int)boot_vinfo->v_rotate;
		    }
#if defined(__i386__) || defined(__x86_64__)
		    new_vinfo.v_type     = (unsigned int)boot_vinfo->v_display;
#else
		    new_vinfo.v_type = 0;
#endif
            unsigned int scale   = (unsigned int)boot_vinfo->v_scale;
            if (scale == kPEScaleFactor1x )
                new_vinfo.v_scale = kPEScaleFactor1x;
            else if (scale == kPEScaleFactor2x)
                new_vinfo.v_scale = kPEScaleFactor2x;
            else /* Scale factor not set, default to 1x */
                new_vinfo.v_scale = kPEScaleFactor1x;
		}
		new_vinfo.v_name[0]  = 0;
		new_vinfo.v_physaddr = 0;

		/*
		 *  Check if we are have to map the framebuffer
		 *  If VM is up, we are given a virtual address, unless b0 is set to indicate physical.
		 */
		newVideoVirt = boot_vinfo->v_baseAddr;
		makeMapping = (kernel_map == VM_MAP_NULL) || (0 != (1 & newVideoVirt));
		if (makeMapping)
		{
			newVideoVirt = 0;
			new_vinfo.v_physaddr = boot_vinfo->v_baseAddr & ~3UL;		/* Get the physical address */
#ifndef __LP64__
			new_vinfo.v_physaddr |= (((uint64_t) boot_vinfo->v_baseAddrHigh) << 32);
#endif
			kprintf("initialize_screen: b=%08llX, w=%08X, h=%08X, r=%08X, d=%08X\n",                  /* (BRINGUP) */
			    new_vinfo.v_physaddr, new_vinfo.v_width,  new_vinfo.v_height,  new_vinfo.v_rowbytes, new_vinfo.v_type);     /* (BRINGUP) */
		}
     
		if (!newVideoVirt && !new_vinfo.v_physaddr)							/* Check to see if we have a framebuffer */
		{
			kprintf("initialize_screen: No video - forcing serial mode\n");		/* (BRINGUP) */
			new_vinfo.v_depth = 0;						/* vc routines are nop */
			(void)switch_to_serial_console();				/* Switch into serial mode */
			gc_graphics_boot = FALSE;					/* Say we are not in graphics mode */
			disableConsoleOutput = FALSE;					/* Allow printfs to happen */
			gc_acquired = TRUE;
		}
		else
		{
		    if (makeMapping)
		    {
			    unsigned int flags = VM_WIMG_IO;
				if (boot_vinfo->v_length != 0)
					newMapSize = (unsigned int) round_page(boot_vinfo->v_length);
				else
					newMapSize = (unsigned int) round_page(new_vinfo.v_height * new_vinfo.v_rowbytes);			/* Remember size */
			    newVideoVirt = io_map_spec((vm_map_offset_t)new_vinfo.v_physaddr, newMapSize, flags);	/* Allocate address space for framebuffer */
			}
		    new_vinfo.v_baseaddr = newVideoVirt + boot_vinfo->v_offset;				/* Set the new framebuffer address */
		}

#if defined(__x86_64__)
		// Adjust the video buffer pointer to point to where it is in high virtual (above the hole)
		new_vinfo.v_baseaddr |= (VM_MIN_KERNEL_ADDRESS & ~LOW_4GB_MASK);
#endif

		/* Update the vinfo structure atomically with respect to the vc_progress task if running */
		if (vc_progress)
		{
		    simple_lock(&vc_progress_lock, LCK_GRP_NULL);
		    vinfo = new_vinfo;
		    simple_unlock(&vc_progress_lock);
		}
		else
		{
		    vinfo = new_vinfo;
		}

		// If we changed the virtual address, remove the old mapping
		if (newVideoVirt != 0)
		{
			if (lastVideoVirt && lastVideoMapSize)							/* Was the framebuffer mapped before? */
			{
				/* XXX why only !4K? */
				if (!TEST_PAGE_SIZE_4K && lastVideoMapSize)
				{
					pmap_remove(kernel_pmap, trunc_page_64(lastVideoVirt),
						round_page_64(lastVideoVirt + lastVideoMapSize));	/* Toss mappings */
				}
				/* Was this not a special pre-VM mapping? */
				if (lastVideoMapKmap)
				{
					kmem_free(kernel_map, lastVideoVirt, lastVideoMapSize);	/* Toss kernel addresses */
				}
			}
			lastVideoMapKmap = (NULL != kernel_map);		/* Remember how mapped */
			lastVideoMapSize = newMapSize;					/* Remember the size */
			lastVideoVirt    = newVideoVirt;				/* Remember the virtual framebuffer address */
		}

        if (kPEBaseAddressChange != op)
        {
			// Graphics mode setup by the booter.

			gc_ops.initialize   = vc_initialize;
			gc_ops.enable       = vc_enable;
			gc_ops.paint_char   = vc_paint_char;
			gc_ops.scroll_down  = vc_scroll_down;
			gc_ops.scroll_up    = vc_scroll_up;
			gc_ops.clear_screen = vc_clear_screen;
			gc_ops.hide_cursor  = vc_reverse_cursor;
			gc_ops.show_cursor  = vc_reverse_cursor;
			gc_ops.update_color = vc_update_color;
			if (!sameMode) {
				gc_initialize(&vinfo);
			}
		}
	}

    graphics_now = gc_graphics_boot && !gc_desire_text;
	switch ( op )
	{
		case kPEGraphicsMode:
			gc_graphics_boot = TRUE;
			gc_desire_text = FALSE;
			break;

		case kPETextMode:
			gc_graphics_boot = FALSE;
			break;

		case kPEAcquireScreen:
			if ( gc_acquired ) break;

			vc_progress_options = vc_user_options;
			bzero(&vc_user_options, sizeof(vc_user_options));

			if (kVCAcquireImmediate & vc_progress_options.options) delay = 0;
			else if (kVCDarkReboot & vc_progress_options.options)  delay = 120;
			else                                                   delay = vc_acquire_delay;

			if (kVCDarkBackground & vc_progress_options.options)       vc_progress_white = TRUE;
			else if (kVCLightBackground & vc_progress_options.options) vc_progress_white = FALSE;

#if !defined(XNU_TARGET_OS_BRIDGE)
			vc_progress_set( graphics_now, delay );
#endif /* !defined(XNU_TARGET_OS_BRIDGE) */
			gc_enable( !graphics_now );
			gc_acquired = TRUE;
			gc_desire_text = FALSE;
			break;

		case kPEDisableScreen:
            if (gc_acquired) 
            {
                gc_pause( TRUE, graphics_now );
            }
			break;

		case kPEEnableScreen:
            if (gc_acquired) 
            {
                gc_pause( FALSE, graphics_now );
            }
			break;

		case kPETextScreen:
			if ( console_is_serial() ) break;

			if ( gc_acquired == FALSE )
			{
				gc_desire_text = TRUE;
				break;
			}
			if ( gc_graphics_boot == FALSE ) break;

			vc_progress_set( FALSE, 0 );
#if !CONFIG_EMBEDDED
			vc_enable_progressmeter( FALSE );
#endif
			gc_enable( TRUE );
			break;

		case kPEReleaseScreen:
			gc_acquired = FALSE;
			gc_desire_text = FALSE;
			gc_enable( FALSE );
			if ( gc_graphics_boot == FALSE ) break;

			vc_progress_set( FALSE, 0 );
			vc_acquire_delay = kProgressReacquireDelay;
			vc_progress_white      = TRUE;
#if !CONFIG_EMBEDDED
			vc_enable_progressmeter(FALSE);
			vc_progress_withmeter &= ~1;
#endif
			vc_clut8 = NULL;
			break;


#if !CONFIG_EMBEDDED
		case kPERefreshBootGraphics:
		{
		    spl_t     s;
		    boolean_t save;

		    if (kBootArgsFlagBlack & ((boot_args *) PE_state.bootArgs)->flags) break;

		    save = vc_progress_white;
		    vc_progress_white = (0 != (kBootArgsFlagBlackBg & ((boot_args *) PE_state.bootArgs)->flags));

		    internal_enable_progressmeter(kProgressMeterKernel);

		    s = splhigh();
		    simple_lock(&vc_progress_lock, LCK_GRP_NULL);

		    vc_progressmeter_drawn = 0;
		    internal_set_progressmeter(vc_progressmeter_range(vc_progressmeter_count >> 13));

		    simple_unlock(&vc_progress_lock);
		    splx(s);

		    internal_enable_progressmeter(kProgressMeterOff);
		    vc_progress_white = save;
		}
#endif
	}
}

void vcattach(void); /* XXX gcc 4 warning cleanup */

void
vcattach(void)
{
	vm_initialized = TRUE;

#if !CONFIG_EMBEDDED
        const boot_args * bootargs  = (typeof(bootargs)) PE_state.bootArgs;

	vc_progress_white = (0 != ((kBootArgsFlagBlackBg | kBootArgsFlagLoginUI) 
					  & bootargs->flags));
	PE_parse_boot_argn("meter", &vc_progress_withmeter, sizeof(vc_progress_withmeter));

	if (kBootArgsFlagInstallUI & bootargs->flags)
	{
	    vc_progress_meter_start = (bootargs->bootProgressMeterStart * kProgressMeterMax) / 65535;
	    vc_progress_meter_end   = (bootargs->bootProgressMeterEnd   * kProgressMeterMax) / 65535;
	}
	else
	{
	    vc_progress_meter_start = 0;
	    vc_progress_meter_end   = kProgressMeterMax;
	}
#endif
	simple_lock_init(&vc_progress_lock, 0);

	if ( gc_graphics_boot == FALSE )
	{
		long index;

		if ( gc_acquired )
		{
			initialize_screen(NULL, kPEReleaseScreen);
		}

		initialize_screen(NULL, kPEAcquireScreen);

		for ( index = 0 ; index < msgbufp->msg_bufx ; index++ )
		{
			if (msgbufp->msg_bufc[index] == '\0') {
				continue;
			}

			vcputc( 0, 0, msgbufp->msg_bufc[index] );

			if ( msgbufp->msg_bufc[index] == '\n' )
			{
				vcputc( 0, 0,'\r' );
			}
		}
	}
}

#if !CONFIG_EMBEDDED

// redraw progress meter between pixels start, end, position at pos,
// options (including rotation) passed in flags
static void
vc_draw_progress_meter(unsigned int flags, int start, int end, int pos)
{
    const unsigned char *data;
    int i, width, bx, srcRow, backRow;
    int rectX, rectY, rectW, rectH;
    int endCapPos, endCapStart;
    int barWidth  = kProgressBarWidth * vc_uiscale;
    int barHeight = kProgressBarHeight * vc_uiscale;
    int capWidth  = kProgressBarCapWidth * vc_uiscale;
    // 1 rounded fill, 0 square end
    int style = (0 == (2 & vc_progress_withmeter));
    // 1 white, 0 greyed out
    int onoff;

    for (i = start; i < end; i += width)
    {
	onoff       = (i < pos);
	endCapPos   = ((style && onoff) ? pos : barWidth);
	endCapStart = endCapPos - capWidth;
	if (flags & kDataBack) { // restore back bits
	    width   = end; // loop done after this iteration
	    data    = NULL;
	    srcRow  = 0;
	} else if (i < capWidth) { // drawing the left cap
	    width   = (end < capWidth) ? (end - i) : (capWidth - i);
	    data    = progressmeter_leftcap[vc_uiscale >= 2][onoff];
	    data    += i;
	    srcRow  = capWidth;
	} else if (i < endCapStart) { // drawing the middle
	    width   = (end < endCapStart) ? (end - i) : (endCapStart - i);
	    data    = progressmeter_middle[vc_uiscale >= 2][onoff];
	    srcRow  = 1;
	} else { // drawing the right cap
	    width   = endCapPos - i;
	    data    = progressmeter_rightcap[vc_uiscale >= 2][onoff];
	    data    += i - endCapStart;
	    srcRow  = capWidth;
	}

	switch (flags & kDataRotate) {
	  case kDataRotate90: // left middle, bar goes down
	    rectW   = barHeight;
	    rectH   = width;
	    rectX   = ((vinfo.v_width / 3) - (barHeight / 2));
	    rectY   = ((vinfo.v_height - barWidth) / 2) + i;
	    bx      = i * barHeight;
	    backRow = barHeight;
	    break;
	  case kDataRotate180: // middle upper, bar goes left
	    rectW   = width;
	    rectH   = barHeight;
	    rectX   = ((vinfo.v_width - barWidth) / 2) + barWidth - width - i;
	    rectY   = (vinfo.v_height / 3) - (barHeight / 2);
	    bx      = barWidth - width - i;
	    backRow = barWidth;
	    break;
	  case kDataRotate270: // right middle, bar goes up
	    rectW   = barHeight;
	    rectH   = width;
	    rectX   = (vinfo.v_width - (vinfo.v_width / 3) - (barHeight / 2));
	    rectY   = ((vinfo.v_height - barWidth) / 2) + barWidth - width - i;
	    bx      = (barWidth - width - i) * barHeight;
	    backRow = barHeight;
	    break;
	  default:
	  case kDataRotate0: // middle lower, bar goes right
	    rectW   = width;
	    rectH   = barHeight;
	    rectX   = ((vinfo.v_width - barWidth) / 2) + i;
	    rectY   = vinfo.v_height - (vinfo.v_height / 3) - (barHeight / 2);
	    bx      = i;
	    backRow = barWidth;
	    break;
	}
	vc_blit_rect(rectX, rectY, bx, rectW, rectH, width, barHeight,
		     srcRow, backRow, data, vc_progressmeter_backbuffer, flags);
    }
}

extern void IORecordProgressBackbuffer(void * buffer, size_t size, uint32_t theme);

static void
internal_enable_progressmeter(int new_value)
{
    spl_t     s;
    void    * new_buffer;
    boolean_t stashBackbuffer;
    int flags = vinfo.v_rotate;

    stashBackbuffer = FALSE;
    new_buffer = NULL;
    if (new_value)
    {
	new_buffer = kalloc((kProgressBarWidth * vc_uiscale) 
		            * (kProgressBarHeight * vc_uiscale) * sizeof(int));
    }

    s = splhigh();
    simple_lock(&vc_progress_lock, LCK_GRP_NULL);

    if (kProgressMeterUser == new_value)
    {
	if (gc_enabled || !gc_acquired || !gc_graphics_boot
	 || (kProgressMeterKernel == vc_progressmeter_enable)) new_value = vc_progressmeter_enable;
    }

    if (new_value != vc_progressmeter_enable)
    {
	if (new_value)
	{
	    if (kProgressMeterOff == vc_progressmeter_enable)
	    {
		vc_progressmeter_backbuffer = new_buffer;
		vc_draw_progress_meter(kDataAlpha | kSave | flags, 0, (kProgressBarWidth * vc_uiscale), 0);
		new_buffer = NULL;
		vc_progressmeter_drawn = 0;
	    }
	    vc_progressmeter_enable = new_value;
	}
	else if (vc_progressmeter_backbuffer)
	{
	    if (kProgressMeterUser == vc_progressmeter_enable)
	    {
		vc_draw_progress_meter(kDataBack | flags, 0, (kProgressBarWidth * vc_uiscale), vc_progressmeter_drawn);
	    }
	    else stashBackbuffer = TRUE;
	    new_buffer = vc_progressmeter_backbuffer;
	    vc_progressmeter_backbuffer = NULL;
	    vc_progressmeter_enable = FALSE;
	}
    }

    simple_unlock(&vc_progress_lock);
    splx(s);

    if (new_buffer)
    {
	if (stashBackbuffer) IORecordProgressBackbuffer(new_buffer, 
	    		                                (kProgressBarWidth * vc_uiscale) 
	    		                                * (kProgressBarHeight * vc_uiscale) 
	    		                                * sizeof(int),
	    		                                vc_progress_white);
	kfree(new_buffer, (kProgressBarWidth * vc_uiscale) 
			* (kProgressBarHeight * vc_uiscale) * sizeof(int));
    }
}

static void
internal_set_progressmeter(int new_value)
{
    int x1, x3;
    int capRedraw;
    // 1 rounded fill, 0 square end
    int style = (0 == (2 & vc_progress_withmeter));
    int flags = kDataAlpha | vinfo.v_rotate;

    if ((new_value < 0) || (new_value > kProgressMeterMax)) return;

    if (vc_progressmeter_enable)
    {
	vc_progressmeter_value = new_value;

	capRedraw = (style ? (kProgressBarCapWidth * vc_uiscale) : 0);
	x3 = (((kProgressBarWidth * vc_uiscale) - 2 * capRedraw) * vc_progressmeter_value) / kProgressMeterMax;
	x3 += (2 * capRedraw);

	if (x3 > vc_progressmeter_drawn)
	{
	    x1 = capRedraw;
	    if (x1 > vc_progressmeter_drawn) x1 = vc_progressmeter_drawn;
	    vc_draw_progress_meter(flags, vc_progressmeter_drawn - x1, x3, x3);
        }
	else
	{
	    vc_draw_progress_meter(flags, x3 - capRedraw, vc_progressmeter_drawn, x3);
	}
	vc_progressmeter_drawn = x3;
    }
}

void
vc_enable_progressmeter(int new_value)
{
    if (kProgressMeterKernel == vc_progressmeter_enable)
    {
    	vc_progressmeter_hold = new_value;
    }
    else
    {
	internal_enable_progressmeter(new_value ? kProgressMeterUser : kProgressMeterOff);
    }
}

void
vc_set_progressmeter(int new_value)
{
    spl_t s;

    s = splhigh();
    simple_lock(&vc_progress_lock, LCK_GRP_NULL);

    if (vc_progressmeter_enable && (kProgressMeterKernel != vc_progressmeter_enable))
    {
	internal_set_progressmeter((new_value * kProgressMeterMax) / 100);
    }

    simple_unlock(&vc_progress_lock);
    splx(s);
}

#endif /* !CONFIG_EMBEDDED */
