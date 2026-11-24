/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * WOFF2's tests for gate W0. Its source carries none that run without a
 * download, so these are Minkar's, through its public API (woff2/encode.h
 * and woff2/decode.h), on the fonts the system ships under the OFL (Inter
 * and Agave). For each font given:
 *
 *   - it compresses to WOFF2, smaller than the font, with its signature;
 *   - the WOFF2 decompresses to a font of the size the library computes;
 *   - that font has the same tables as the original (each table's tag,
 *     and, but for the ones WOFF2 transforms or adjusts, its bytes);
 *   - compressing the decompressed font again gives the same WOFF2 size
 *     within a few bytes (the round trip is stable);
 *   - a WOFF2 cut short, or with its signature broken, is refused.
 *
 * One line a check, "PASS name" or "FAIL name: why"; the totals last.
 */

#include <woff2/decode.h>
#include <woff2/encode.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

static int passed, failed;

static void
check(bool ok, const std::string &font, const char *what, const std::string &why)
{
	if (ok) {
		passed++;
		std::printf("PASS %s: %s\n", font.c_str(), what);
	} else {
		failed++;
		std::printf("FAIL %s: %s: %s\n", font.c_str(), what, why.c_str());
	}
}

static uint32_t
be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

/* An sfnt's tables, by tag: their bytes. Empty when it is no sfnt. */
static std::map<std::string, std::string>
tables(const std::string &font)
{
	std::map<std::string, std::string> out;
	const uint8_t *d = (const uint8_t *)font.data();

	if (font.size() < 12)
		return out;
	unsigned n = (unsigned)d[4] << 8 | d[5];
	if (12 + 16 * (size_t)n > font.size())
		return out;
	for (unsigned i = 0; i < n; i++) {
		const uint8_t *r = d + 12 + 16 * i;
		uint32_t off = be32(r + 8), len = be32(r + 12);
		if ((size_t)off + len > font.size())
			return std::map<std::string, std::string>();
		out[std::string((const char *)r, 4)] = font.substr(off, len);
	}
	return out;
}

static std::string
compress(const std::string &font)
{
	size_t room = woff2::MaxWOFF2CompressedSize((const uint8_t *)font.data(), font.size());
	std::string out(room, '\0');
	if (!woff2::ConvertTTFToWOFF2((const uint8_t *)font.data(), font.size(),
	    (uint8_t *)&out[0], &room))
		return std::string();
	out.resize(room);
	return out;
}

static bool
decompress(const std::string &woff, std::string *font)
{
	size_t size = woff2::ComputeWOFF2FinalSize((const uint8_t *)woff.data(), woff.size());
	if (size == 0)
		return false;
	font->assign(size, '\0');
	woff2::WOFF2MemoryOut out((uint8_t *)&(*font)[0], size);
	if (!woff2::ConvertWOFF2ToTTF((const uint8_t *)woff.data(), woff.size(), &out))
		return false;
	font->resize(out.Size());
	return true;
}

static void
test_font(const char *path)
{
	std::string name = path;
	size_t slash = name.rfind('/');
	if (slash != std::string::npos)
		name = name.substr(slash + 1);
	std::ifstream in(path, std::ios::binary);
	std::string font((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (font.empty()) {
		check(false, name, "reads", "empty or missing");
		return;
	}

	std::string woff = compress(font);
	check(!woff.empty() && woff.size() < font.size() && woff.compare(0, 4, "wOF2") == 0, name,
	    "compresses to a smaller WOFF2", std::to_string(woff.size()) + " of " + std::to_string(font.size()));
	if (woff.empty())
		return;

	std::string back;
	bool ok = decompress(woff, &back);
	check(ok && !back.empty(), name, "decompresses", "ConvertWOFF2ToTTF failed");
	if (!ok)
		return;

	/* The tables: the same set; bytes equal but for those WOFF2
	   transforms (glyf, loca, hmtx) or adjusts (head: its checksum
	   adjustment and flags; DSIG is dropped when transformed). */
	std::map<std::string, std::string> a = tables(font), b = tables(back);
	std::string differ;
	size_t same = 0;
	for (auto &t : a) {
		if (t.first == "DSIG")
			continue;
		auto it = b.find(t.first);
		if (it == b.end()) {
			differ += " missing " + t.first;
			continue;
		}
		if (t.first == "glyf" || t.first == "loca" || t.first == "hmtx" || t.first == "head")
			continue;
		if (it->second != t.second)
			differ += " " + t.first;
		else
			same++;
	}
	check(!a.empty() && differ.empty() && same > 4, name, "the same tables, the same bytes",
	    differ.empty() ? "no tables" : differ);

	/* glyf and loca come back as many glyphs: loca one entry longer. */
	std::string headA = a["head"], maxp = a["maxp"];
	if (headA.size() >= 54 && maxp.size() >= 6) {
		unsigned glyphs = (unsigned)(uint8_t)maxp[4] << 8 | (uint8_t)maxp[5];
		std::string headB = b["head"];
		unsigned format = headB.size() >= 54 ? (unsigned)(uint8_t)headB[51] : 9;
		size_t entry = format == 0 ? 2 : 4;
		check(b["loca"].size() == (glyphs + 1) * entry && !b["glyf"].empty(), name,
		    "every glyph comes back", "loca " + std::to_string(b["loca"].size()) + " for " +
		    std::to_string(glyphs) + " glyphs");
	}

	/* Stable: the decompressed font compresses as the original did. */
	std::string again = compress(back);
	long delta = (long)again.size() - (long)woff.size();
	check(!again.empty() && delta >= -64 && delta <= 64, name, "the round trip is stable",
	    "sizes " + std::to_string(woff.size()) + " and " + std::to_string(again.size()));

	/* Refused: cut short, or its signature broken. */
	std::string out;
	std::string cut = woff.substr(0, woff.size() / 2);
	std::string bad = woff;
	bad[0] = 'x';
	check(!decompress(cut, &out) && !decompress(bad, &out), name,
	    "a WOFF2 cut short or not signed is refused", "accepted");
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		std::fprintf(stderr, "usage: woff2-test FONT...\n");
		return 2;
	}
	for (int i = 1; i < argc; i++)
		test_font(argv[i]);
	if (failed == 0)
		std::printf("passed %d\n", passed);
	else
		std::printf("failed %d of %d\n", failed, passed + failed);
	return failed > 0;
}
