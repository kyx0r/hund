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
 * TODO maybe move the select() code to get_input()???
 */
ssize_t xread(int fd, void* buf, ssize_t count, int timeout_ms) {
	struct timeval T = { 0, (suseconds_t)timeout_ms };
	fd_set rfds;
	int retval;
	if (timeout_ms > 0) {
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
	}
	ssize_t rd;
	do {
		if (timeout_ms > 0) {
			retval = select(fd+1, &rfds, NULL, NULL, &T);
			if (retval == -1 || !retval) {
				FD_CLR(fd, &rfds);
				return 0;
			}
			FD_CLR(fd, &rfds);
		}
		rd = read(fd, buf, count);
	} while (rd < 0 && errno == EINTR && errno == EAGAIN);
	//fprintf(stderr, "buf: %x, len: %llu\n", *(char*)buf, rd);
	FD_CLR(fd, &rfds);
	return rd;
}

static enum input_type which_key(char* const seq) {
	int i = 0;
	while (SKM[i].seq != NULL && SKM[i].t != I_NONE) {
		if (!strcmp(SKM[i].seq, seq)) return SKM[i].t; // TODO FIXME
		i += 1;
	}
	return I_NONE;
}

struct input get_input(int timeout_ms) {
	const int fd = STDIN_FILENO;
	struct input i;
	memset(i.utf, 0, 5);
	i.t = I_NONE;
	int utflen;
	char seq[7];
	memset(seq, 0, sizeof(seq));
	if (xread(fd, seq, 1, timeout_ms) == 1 && seq[0] == '\x1b') {
		if (xread(fd, seq+1, 1, 0) == 1
				&& (seq[1] == '[' || seq[1] == 'O')) {
			if (xread(fd, seq+2, 1, 0) == 1 && isdigit(seq[2])) {
				xread(fd, seq+3, 1, 0);
			}
		}
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
			if (xread(fd, i.utf+b, 1, 0) != 1) {
				i.t = I_NONE;
				memset(i.utf, 0, 5);
				return i;
			}
		}
		for (; b < 5; ++b) {
			i.utf[b] = 0;
		}
	}
	/*switch (i.t) {
	case I_UTF8: fprintf(stderr, "%s\n", i.utf); break;
	case I_CTRL: fprintf(stderr, "^%c\n", i.utf[0]); break;
	default: fprintf(stderr, "%d %s\n", i.t, keynames[i.t]); break;
	}*/
	return i;
}

int start_raw_mode(struct termios* const before) {
	const int fd = STDIN_FILENO;
	memset(before, 0, sizeof(struct termios));
	if (tcgetattr(fd, before) == -1) return errno;
	struct termios raw;
	memcpy(&raw, before, sizeof(struct termios));
	cfmakeraw(&raw);
	return (tcsetattr(fd, TCSAFLUSH, &raw) == -1 ? errno : 0);
}

int stop_raw_mode(struct termios* const before) {
	const int fd = STDIN_FILENO;
	return (tcsetattr(fd, TCSAFLUSH, before) == -1 ? errno : 0);
}

int char_attr(char* const buf, const size_t bufs,
		const int F, const unsigned char* const v) {
	if (F < 8) {
		const char C = '0' + (F & 0x0000000f);
		return snprintf(buf, bufs, "\x1b[%cm", C);
	}
	char fgbg = '3'; // TODO vvvv
	if (F & ATTR_FOREGROUND) fgbg = '3';
	else if (F & ATTR_BACKGROUND) fgbg = '4';
	if (F & 0x7f) {
		const char C = F & 0x0000007f;
		return snprintf(buf, bufs, "\x1b[%c%cm", fgbg, C);
	}
	if (F & ATTR_COLOR_256) {
		return snprintf(buf, bufs, "\x1b[%c8;5;%um", fgbg, v[0]);
	}
	if (F & ATTR_COLOR_TRUE) {
		return snprintf(buf, bufs, "\x1b[%c8;2;%u;%u;%um",
				fgbg, v[0], v[1], v[2]);
	}
	return 0;
}

int move_cursor(const unsigned int R, const unsigned int C) {
	char buf[1+1+4+1+4+1+1];
	size_t n = snprintf(buf, sizeof(buf), "\x1b[%u;%uH", R, C);
	return (write(STDOUT_FILENO, buf, n) == -1 ? errno : 0);
}

int window_size(int* const R, int* const C) {
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
		*R = *C = 0;
		return errno;
	}
	*R = ws.ws_row;
	*C = ws.ws_col;
	return 0;
}
