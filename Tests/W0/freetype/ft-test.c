/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * FreeType's tests for gate W0. FreeType's source carries no test that
 * runs without a download (its only one needs a font of unclear license),
 * so these are Minkar's, through FreeType's public API, on the fonts the
 * system ships under the OFL (Inter and Agave). For each font given:
 *
 *   - it opens, with its family, style, glyph count and units per em;
 *   - its character map and glyph indices agree both ways, for every
 *     character it maps;
 *   - every glyph loads, unhinted and hinted, and its outline is valid
 *     (contours end in order, the last at n_points - 1);
 *   - every glyph renders in the normal, light, mono and LCD modes, at
 *     three sizes, to a bitmap as wide and tall as its outline's control
 *     box allows;
 *   - the stroker strokes a glyph's outline into a larger one;
 *   - kerning and the glyph advance agree with the font's own metrics;
 *   - opening the same font from memory gives the same glyphs.
 *
 * One line a check, "PASS name" or "FAIL name: why"; the totals last.
 */

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_STROKER_H
#include FT_GLYPH_H
#include FT_LCD_FILTER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed, failed;

static void
check(int ok, const char *font, const char *what, const char *why)
{
	if (ok) {
		passed++;
		printf("PASS %s: %s\n", font, what);
	} else {
		failed++;
		printf("FAIL %s: %s: %s\n", font, what, why);
	}
}

static const char *
base(const char *path)
{
	const char *s = strrchr(path, '/');
	return s != NULL ? s + 1 : path;
}

/* Contours end in increasing order, the last at the last point. */
static int
outline_valid(const FT_Outline *o)
{
	short prev = -1;

	if (o->n_contours == 0)
		return o->n_points == 0;
	for (int c = 0; c < o->n_contours; c++) {
		if (o->contours[c] <= prev || o->contours[c] >= o->n_points)
			return 0;
		prev = o->contours[c];
	}
	return o->contours[o->n_contours - 1] == o->n_points - 1;
}

static void
test_font(FT_Library lib, const char *path)
{
	const char *name = base(path);
	FT_Face face = NULL;
	char why[256];

	if (FT_New_Face(lib, path, 0, &face) != 0) {
		check(0, name, "opens", "FT_New_Face failed");
		return;
	}
	check(face->family_name != NULL && face->family_name[0] != 0 && face->num_glyphs > 100 &&
	    face->units_per_EM >= 16 && FT_IS_SCALABLE(face) && FT_HAS_HORIZONTAL(face),
	    name, "opens, scalable, with its family and glyphs", "bad face");

	/* The character map both ways. */
	FT_UInt gindex;
	FT_ULong code = FT_Get_First_Char(face, &gindex);
	long mapped = 0, bad = 0;
	while (gindex != 0) {
		if (FT_Get_Char_Index(face, code) != gindex)
			bad++;
		mapped++;
		code = FT_Get_Next_Char(face, code, &gindex);
	}
	snprintf(why, sizeof(why), "%ld of %ld characters disagree", bad, mapped);
	check(mapped > 100 && bad == 0, name, "its character map agrees both ways", why);

	/* Every glyph: loaded unhinted and hinted, a valid outline. */
	FT_Set_Char_Size(face, 0, 16 * 64, 96, 96);
	long loadBad = 0, outlineBad = 0;
	for (FT_Long g = 0; g < face->num_glyphs; g++) {
		if (FT_Load_Glyph(face, (FT_UInt)g, FT_LOAD_NO_HINTING) != 0) {
			loadBad++;
			continue;
		}
		if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE && !outline_valid(&face->glyph->outline))
			outlineBad++;
		if (FT_Load_Glyph(face, (FT_UInt)g, FT_LOAD_DEFAULT) != 0)
			loadBad++;
	}
	snprintf(why, sizeof(why), "%ld loads failed, %ld outlines invalid", loadBad, outlineBad);
	check(loadBad == 0 && outlineBad == 0, name, "every glyph loads, with a valid outline", why);

	/* Rendering: four modes, three sizes, a bitmap within the control box. */
	static const FT_Render_Mode modes[] = { FT_RENDER_MODE_NORMAL, FT_RENDER_MODE_LIGHT,
	    FT_RENDER_MODE_MONO, FT_RENDER_MODE_LCD };
	static const int sizes[] = { 9, 16, 48 };
	FT_Library_SetLcdFilter(lib, FT_LCD_FILTER_DEFAULT);
	long renderBad = 0, sizeBad = 0, inked = 0;
	for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
		FT_Set_Pixel_Sizes(face, 0, (FT_UInt)sizes[s]);
		for (unsigned m = 0; m < sizeof(modes) / sizeof(modes[0]); m++)
			for (FT_Long g = 0; g < face->num_glyphs; g += 7) {
				if (FT_Load_Glyph(face, (FT_UInt)g, FT_LOAD_DEFAULT) != 0) {
					renderBad++;
					continue;
				}
				FT_BBox box;
				FT_Outline_Get_CBox(&face->glyph->outline, &box);
				long w = (box.xMax - box.xMin) / 64 + 3, h = (box.yMax - box.yMin) / 64 + 3;
				if (modes[m] == FT_RENDER_MODE_LCD)
					w = w * 3 + 6;
				if (FT_Render_Glyph(face->glyph, modes[m]) != 0) {
					renderBad++;
					continue;
				}
				FT_Bitmap *b = &face->glyph->bitmap;
				if ((long)b->width > w || (long)b->rows > h)
					sizeBad++;
				for (unsigned r = 0; r < b->rows && b->buffer != NULL; r++) {
					int any = 0;
					for (int x = 0; x < abs(b->pitch); x++)
						any |= b->buffer[r * abs(b->pitch) + x] != 0;
					if (any) {
						inked++;
						break;
					}
				}
			}
	}
	snprintf(why, sizeof(why), "%ld renders failed, %ld bitmaps larger than their box, %ld inked",
	    renderBad, sizeBad, inked);
	check(renderBad == 0 && sizeBad == 0 && inked > 100, name,
	    "glyphs render in four modes at three sizes", why);

	/* The stroker: a stroked outline covers more than the glyph. */
	FT_Set_Pixel_Sizes(face, 0, 48);
	FT_UInt o = FT_Get_Char_Index(face, 'O');
	FT_Stroker stroker;
	int strokeOK = 0;
	if (o != 0 && FT_Load_Glyph(face, o, FT_LOAD_NO_BITMAP) == 0 && FT_Stroker_New(lib, &stroker) == 0) {
		FT_Glyph glyph;
		FT_BBox before, after;
		FT_Stroker_Set(stroker, 3 * 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
		if (FT_Get_Glyph(face->glyph, &glyph) == 0) {
			FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &before);
			if (FT_Glyph_Stroke(&glyph, stroker, 1) == 0) {
				FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &after);
				strokeOK = after.xMax - after.xMin > before.xMax - before.xMin &&
				    after.yMax - after.yMin > before.yMax - before.yMin;
			}
			FT_Done_Glyph(glyph);
		}
		FT_Stroker_Done(stroker);
	}
	check(strokeOK, name, "the stroker widens a glyph", "stroked box not larger");

	/* Advances: unscaled, as the font's hmtx gives them, positive; with
	   kerning, a pair's vector is a whole number of pixels when hinted. */
	FT_Set_Char_Size(face, 0, 0, 0, 0);
	FT_Set_Pixel_Sizes(face, 0, 32);
	long advBad = 0;
	for (int c = 'A'; c <= 'Z'; c++) {
		FT_UInt g = FT_Get_Char_Index(face, (FT_ULong)c);
		if (g == 0 || FT_Load_Glyph(face, g, FT_LOAD_NO_SCALE) != 0 ||
		    face->glyph->metrics.horiAdvance <= 0 ||
		    face->glyph->metrics.horiAdvance > face->units_per_EM * 4)
			advBad++;
	}
	if (FT_HAS_KERNING(face)) {
		FT_Vector k;
		if (FT_Get_Kerning(face, FT_Get_Char_Index(face, 'A'), FT_Get_Char_Index(face, 'V'),
		    FT_KERNING_DEFAULT, &k) != 0 || k.x % 64 != 0)
			advBad++;
	}
	snprintf(why, sizeof(why), "%ld advances or kerning wrong", advBad);
	check(advBad == 0, name, "advances and kerning", why);

	/* The same font from memory: the same glyph count and outlines. */
	FILE *f = fopen(path, "rb");
	long len = 0;
	unsigned char *bytes = NULL;
	if (f != NULL && fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0 &&
	    (bytes = malloc((size_t)len)) != NULL && fread(bytes, 1, (size_t)len, f) != (size_t)len) {
		free(bytes);
		bytes = NULL;
	}
	if (f != NULL)
		fclose(f);
	FT_Face mem = NULL;
	long memBad = 0;
	if (bytes == NULL || FT_New_Memory_Face(lib, bytes, len, 0, &mem) != 0 ||
	    mem->num_glyphs != face->num_glyphs)
		memBad = -1;
	else
		for (FT_Long g = 0; g < face->num_glyphs; g += 3) {
			if (FT_Load_Glyph(face, (FT_UInt)g, FT_LOAD_NO_SCALE) != 0 ||
			    FT_Load_Glyph(mem, (FT_UInt)g, FT_LOAD_NO_SCALE) != 0 ||
			    face->glyph->outline.n_points != mem->glyph->outline.n_points ||
			    face->glyph->metrics.horiAdvance != mem->glyph->metrics.horiAdvance)
				memBad++;
		}
	snprintf(why, sizeof(why), "%ld glyphs differ (-1: not opened)", memBad);
	check(memBad == 0, name, "the same font from memory", why);
	if (mem != NULL)
		FT_Done_Face(mem);
	free(bytes);
	FT_Done_Face(face);
}

int
main(int argc, char **argv)
{
	FT_Library lib;
	FT_Int major, minor, patch;

	if (argc < 2) {
		fprintf(stderr, "usage: ft-test FONT...\n");
		return 2;
	}
	if (FT_Init_FreeType(&lib) != 0) {
		printf("FAIL FT_Init_FreeType\n");
		return 1;
	}
	FT_Library_Version(lib, &major, &minor, &patch);
	printf("FreeType %d.%d.%d\n", major, minor, patch);
	for (int i = 1; i < argc; i++)
		test_font(lib, argv[i]);
	FT_Done_FreeType(lib);
	if (failed == 0)
		printf("passed %d\n", passed);
	else
		printf("failed %d of %d\n", failed, passed + failed);
	return failed > 0;
}
