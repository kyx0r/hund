/*
 *  Copyright (C) 2017-2018 by Michał Czarnecki <czarnecky@va.pl>
 *
 *  This file is part of the Hund.
 *
 *  The Hund is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  The Hund is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "utf8.h"

/* CodePoint To Bytes */
void utf8_cp2b(char* const b, codepoint_t cp) {
#if 1
	const size_t nb = utf8_cp2nb(cp);
	static const unsigned char p[] = { 0x00, 0x7f, 0x1f, 0x0f, 0x07 };
	static const unsigned char o[] = { 0x00, 0x00, 0xc0, 0xe0, 0xf0 };
	for (size_t i = nb-1; i; --i, cp >>= 6) {
		b[i] = 0x80 | (cp & 0x3f);
	}
	b[0] = (codepoint_t)o[nb] | (cp & (codepoint_t)p[nb]);
#else
	if (cp < (codepoint_t) 0x80) {
		b[0] = 0x7f & cp;
	}
	else if (cp < (codepoint_t) 0x0800) {
		b[0] = 0xc0 | ((cp >> 6) & 0x1f);
		b[1] = 0x80 | (cp & 0x3f);
	}
	else if (cp < (codepoint_t) 0x010000) {
		b[0] = 0xe0 | ((cp >> 12) & 0x000f);
		b[1] = 0x80 | ((cp >> 6) & 0x003f);
		b[2] = 0x80 | (cp & 0x003f);
	}
	else if (cp < (codepoint_t) 0x0200000) {
		b[0] = 0xf0 | ((cp >> 18) & 0x000007);
		b[1] = 0x80 | ((cp >> 12) & 0x00003f);
		b[2] = 0x80 | ((cp >> 6) & 0x00003f);
		b[3] = 0x80 | (cp & 0x00003f);
	}
#endif
}

/* Bytes To CodePoint */
codepoint_t utf8_b2cp(const char* const b) {
	register codepoint_t cp = 0;
#if 1
	const size_t nb = utf8_g2nb(b);
	static const unsigned char p[5] = { 0x00, 0x7f, 0x1f, 0x0f, 0x07 };
	cp = b[0] & p[nb];
	for (size_t i = 1; i < nb; ++i) {
		cp <<= 6;
		cp |= b[i] & 0x3f;
	}
#else
	if ((b[0] & 0x80) == 0) {
		cp = b[0];
	}
	else if ((b[0] & 0xe0) == 0xc0) {
		cp |= (b[0] & 0x1f);
		cp <<= 6;
		cp |= (b[1] & 0x3f);
	}
	else if ((b[0] & 0xf0) == 0xe0) {
		cp |= (b[0] & 0x0f);
		cp <<= 6;
		cp |= (b[1] & 0x3f);
		cp <<= 6;
		cp |= (b[2] & 0x3f);
	}
	else if ((b[0] & 0xf8) == 0xf0) {
		cp |= (b[0] & 0x07);
		cp <<= 6;
		cp |= (b[1] & 0x3f);
		cp <<= 6;
		cp |= (b[2] & 0x3f);
		cp <<= 6;
		cp |= (b[3] & 0x3f);
	}
#endif
	return cp;
}

/* Glyph To Number of Bytes
 * how long the current glyph is (in bytes)
 * returns 0 if initial byte is invalid
 */

/* It's simple, really
 * I'm looking at top 5 bits (32 possibilities) of initial byte
 * 00001xxx -> 1 byte
 * 00010xxx -> 1 byte
 * 00011xxx -> 1 byte
 * 16 x 1 byte
 * 01111xxx -> 1 byte
 * 10000xxx -> invalid
 * 8 x invalid
 * 10111xxx -> invalid
 * 11000xxx -> 2 bytes
 * 4 x 2 bytes
 * 11011xxx -> 2 bytes
 * 11100xxx -> 3 bytes
 * 2 x 3 bytes
 * 11101xxx -> 3 bytes
 * 11110xxx -> 4 bytes
 * 1 x 4 bytes
 * 11111xxx -> invalid (see impementation)
 */
size_t utf8_g2nb(const char* const g) {
	// Top 5 bits To Length
	static const char t2l[32] = {
		1, 1, 1, 1, 1, 1, 1, 1, //00000xxx - 00111xxx
		1, 1, 1, 1, 1, 1, 1, 1, //01000xxx - 01111xxx
		0, 0, 0, 0, 0, 0, 0, 0, //10000xxx - 10111xxx
		2, 2, 2, 2, 3, 3, 4, 0  //11000xxx - 11111xxx
	};
	/* This cast is very important */
	return t2l[(unsigned char)(*g) >> 3];
}

/* CodePoint To Number of Bytes */
size_t utf8_cp2nb(const codepoint_t cp) {
	if (cp < (codepoint_t) 0x80) return 1;
	if (cp < (codepoint_t) 0x0800) return 2;
	if (cp < (codepoint_t) 0x010000) return 3;
	if (cp < (codepoint_t) 0x200000) return 4;
	return 0;
}

/* Glyph to width */
size_t utf8_g2w(const codepoint_t cp) {
	if (cp < 0x20) return 0;
	if (cp < 0x7f) return 1;
	if (cp_in(zero_width, zero_width_len-1, cp)) return 0;
	if (cp_in(double_width, double_width_len-1, cp)) return 2;
	return 1;
}

/* Apparent width */
size_t utf8_width(const char* b) {
	size_t g = 0;
	size_t s;
	while (*b && (s = utf8_g2nb(b)) != 0) {
		g += utf8_g2w(utf8_b2cp(b));
		b += s;
	}
	return g;
}

/*
 * Width To Number of Bytes
 * Calculates how much bytes will fill given width
 */
size_t utf8_w2nb(const char* const b, size_t w) {
	size_t r = 0;
	while (*(b+r) && w > 0) {
		r += utf8_g2nb(b+r);
		w -= utf8_g2w(utf8_b2cp(b+r));
	}
	return r;
}

/*
 * Width till some address in that string
 */
size_t utf8_wtill(const char* a, const char* const b) {
	size_t w = 0;
	while (b - a > 0) {
		w += utf8_g2w(utf8_b2cp(a));
		a += utf8_g2nb(a);
	}
	return w;
}

bool utf8_validate(const char* const b) {
	const size_t bl = strlen(b);
	size_t i = 0;
	while (i < bl) {
		const size_t s = utf8_g2nb(b+i);
		if (!s) break;
		/* Now check if bytes following the
		 * inital byte are like 10xxxxxx
		 */
		if (s > 1) {
			const char* v = b+i+1;
			while (v < b+i+s) {
				if ((*v & 0xc0) != 0x80) return false;
				v += 1;
			}
		}
		i += s;
	}
	return i == bl;
}

void utf8_insert(char* a, const char* const b, const size_t pos) {
	const size_t bl = strlen(b);
	for (size_t i = 0; i < pos; ++i) {
		a += utf8_g2nb(a);
	}
	memmove(a+bl, a, strlen(a));
	memcpy(a, b, bl);
}

/*
 * Remove glyph at index
 * returns length in bytes of removed glyph
 */
size_t utf8_remove(char* const a, const size_t j) {
	char* t = a;
	for (size_t i = 0; i < j; ++i) {
		t += utf8_g2nb(t);
	}
	const size_t rl = utf8_g2nb(t); // Removed glyph Length
	memmove(t, t+rl, strlen(t));
	return rl;
}

/*
 * Copies only valid utf8 characters and non-control ascii to buf
 */
unsigned cut_unwanted(const char* str, char* buf, const char c, size_t n) {
	unsigned u = 0;
	while (*str && n) {
		const size_t nb = utf8_g2nb(str);
		if (!nb || (nb == 1 && *str < ' ')) {
			*buf = c;
			buf += 1;
			str += 1;
			u += 1;
		}
		else {
			memcpy(buf, str, nb);
			buf += nb;
			str += nb;
		}
		n -= 1;
	}
	*buf = 0; // null-terminator
	return u;
}

/*
 * Binary search. Determines if the codepoint is in given list.
 */
bool cp_in(const codepoint_t r[][2], size_t Z, const codepoint_t cp) {
	if (cp < r[0][0] || r[Z][1] < cp) return false;
	size_t A = 0;
	size_t I;
	while (A <= Z) {
		I = (A+Z)/2;
		if (cp < r[I][0]) {
			Z = I-1;
		}
		else if (cp > r[I][1]) {
			A = I+1;
		}
		else {
			return true;
		}
	}
	return false;
}
