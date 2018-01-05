/*
 *  Copyright (C) 2017 by Michał Czarnecki <czarnecky@va.pl>
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

#include "include/terminal.h"

/*
 * TODO test
 */
ssize_t xread(int fd, void* buf, ssize_t count, int timeout_ms) {
	struct timeval T = { 0, (suseconds_t)timeout_ms };
	fd_set rfds;
	int retval;
	if (timeout_ms > 0) {
		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO, &rfds);
	}

	ssize_t rd;
	do {
		if (timeout_ms > 0) {
			retval = select(1, &rfds, NULL, NULL, &T);
			if (retval == -1 || !retval) return 0;
		}
		rd = read(fd, buf, count);
	} while (rd < 0 && errno == EINTR && errno == EAGAIN);
	//fprintf(stderr, "buf: %x, len: %llu\n", *(char*)buf, rd);
	return rd;
}

static enum input_type which_key(char* const seq) {
	int i = 0;
	while (SKM[i].seq != NULL && SKM[i].t != I_NONE) {
		if (!memcmp(SKM[i].seq, seq, 7)) return SKM[i].t;
		i += 1;
	}
	return I_NONE;
}

struct input get_input(int timeout_ms) {
	struct input i;
	memset(i.utf, 0, 5);
	i.t = I_NONE;
	int utflen;
	char seq[7];
	memset(seq, 0, sizeof(seq));
	if (xread(STDIN_FILENO, seq, 1, timeout_ms) == 1 && seq[0] == '\x1b') {
		if (xread(STDIN_FILENO, seq+1, 1, 0) == 1
				&& (seq[1] == '[' || seq[1] == 'O')) {
			if (xread(STDIN_FILENO, seq+2, 1, 0) == 1
					&& isdigit(seq[2])) {
				xread(STDIN_FILENO, seq+3, 1, 0);
			}
		}
		//fprintf(stderr, "%x%x%x%x\n", seq[0], seq[1], seq[2], seq[3]);
		i.t = which_key(seq);
	}
	else if (seq[0] == 0x7f) {
#if defined(__linux__) || defined(__linux)
		i.t = I_BACKSPACE;
#else
		i.t = I_DELETE;
#endif
	}
	else if (!(seq[0] & 0x60)) {
		i.t = I_CTRL;
		i.utf[0] = seq[0] | 0x40;
	}
	else if ((utflen = utf8_g2nb(seq))) {
		i.t = I_UTF8;
		int b;
		i.utf[0] = seq[0];
		for (b = 1; b < utflen; ++b) {
			if (xread(STDIN_FILENO, i.utf+b, 1, 0) != 1) {
				i.t = I_NONE;
				memset(i.utf, 0, 5);
				return i;
			}
		}
		for (; b < 5; ++b) {
			i.utf[b] = 0;
		}
	}
	return i;
}

int start_raw_mode(struct termios* const before) {
	if (tcgetattr(STDIN_FILENO, before)) return errno;
	struct termios raw;
	memcpy(&raw, before, sizeof(struct termios));
	cfmakeraw(&raw);
	return (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) ? errno : 0);
}

int stop_raw_mode(struct termios* const before) {
	return (tcsetattr(STDIN_FILENO, TCSAFLUSH, before) ? errno : 0);
}
