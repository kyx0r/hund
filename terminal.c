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

#include "terminal.h"

ssize_t xread(int fd, void* buf, ssize_t count, int timeout_us) {
	struct timespec T = { 0, (suseconds_t)timeout_us*1000 };
	fd_set rfds;
	int retval;
	if (timeout_us > 0) {
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
	}
	ssize_t rd;
	do {
		if (timeout_us > 0) {
			retval = pselect(fd+1, &rfds, NULL, NULL, &T, NULL);
			if (retval == -1 || !retval) {
				FD_CLR(fd, &rfds);
				return 0;
			}
		}
		rd = read(fd, buf, count);
	} while (rd < 0 && errno == EINTR && errno == EAGAIN);
	FD_CLR(fd, &rfds);
	return rd;
}

static enum input_type which_key(char* const seq) {
	int i = 0;
	while (SKM[i].seq != NULL && SKM[i].t != I_NONE) {
		if (!strcmp(SKM[i].seq, seq)) return SKM[i].t;
		i += 1;
	}
	return I_NONE;
}

struct input get_input(int timeout_us) {
	const int fd = STDIN_FILENO;
	struct input i;
	memset(&i, 0, sizeof(struct input));
	int utflen;
	char seq[7];
	memset(seq, 0, sizeof(seq));
	if (xread(fd, seq, 1, timeout_us) == 1 && seq[0] == '\x1b') {
		if (xread(fd, seq+1, 1, ESC_TIMEOUT_MS*1000) == 1
		&& (seq[1] == '[' || seq[1] == 'O')) {
			if (xread(fd, seq+2, 1, 0) == 1
			&& '0' <= seq[2] && seq[2] <= '9') {
				xread(fd, seq+3, 1, 0);
			}
		}
		i.t = which_key(seq);
	}
	else if (seq[0] == 0x7f) {
#if defined(__linux__) || defined(__linux) || defined(linux)
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
		i.utf[0] = seq[0];
		int b;
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
	return i;
}

int start_raw_mode(struct termios* const before) {
	const int fd = STDIN_FILENO;
	if (write(STDOUT_FILENO, CSI_SCREEN_ALTERNATIVE) == -1
	|| tcgetattr(fd, before)) {
		return errno;
	}
	struct termios raw = *before;
	cfmakeraw(&raw);
	//raw.c_cc[VINTR] = 0x03;
	//raw.c_cc[VSUSP] = 0x1a;
	raw.c_iflag &= ~(BRKINT);
	raw.c_lflag |= ISIG;
	write(STDOUT_FILENO, CSI_CURSOR_HIDE); // TODO
	return (tcsetattr(fd, TCSAFLUSH, &raw) ? errno : 0);
}

int stop_raw_mode(struct termios* const before) {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, before)) {
		return errno;
	}
	write(STDOUT_FILENO, CSI_SCREEN_NORMAL); // TODO
	write(STDOUT_FILENO, CSI_CURSOR_SHOW);
	write(STDOUT_FILENO, "\r\n", 2);
	return 0;
}

int char_attr(char* const B, const size_t S,
		const int F, const unsigned char* const v) {
	if (F < 32) {
		return snprintf(B, S, "\x1b[%dm", F);
	}
	if (!(F & (ATTR_FOREGROUND | ATTR_BACKGROUND))) return 0;
	const char fb = ((F & ATTR_FOREGROUND) ? '3' : '4');
	const char C = F & 0x0000007f;
	if (C) {
		return snprintf(B, S, "\x1b[%c%cm", fb, C);
	}
	if (F & ATTR_COLOR_256) {
		return snprintf(B, S, "\x1b[%c8;5;%hhum", fb, v[0]);
	}
	if (F & ATTR_COLOR_TRUE) {
		return snprintf(B, S, "\x1b[%c8;2;%hhu;%hhu;%hhum",
				fb, v[0], v[1], v[2]);
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

size_t append(struct append_buffer* const ab,
		const char* const b, const size_t s) {
	if (ab->capacity - ab->top < s) {
		do {
			ab->capacity += APPEND_BUFFER_INC;
		} while (ab->capacity - ab->top < s);
		void* tmp = realloc(ab->buf, ab->capacity);
		if (!tmp) return 0;
		ab->buf = tmp;
	}
	memcpy(ab->buf+ab->top, b, s);
	ab->top += s;
	return s;
}

size_t append_attr(struct append_buffer* const ab,
		const int F, const unsigned char* const v) {
	char attr[1+1+1+1+1+1+1+3+1+3+1+3+1+1];
	int n = char_attr(attr, sizeof(attr), F, v);
	return append(ab, attr, n);
}

size_t fill(struct append_buffer* const ab, const char C, const size_t s) {
	if (ab->capacity - ab->top < s) {
		do {
			ab->capacity += APPEND_BUFFER_INC;
		} while (ab->capacity - ab->top < s);
		void* tmp = realloc(ab->buf, ab->capacity);
		if (!tmp) return 0;
		ab->buf = tmp;
	}
	memset(ab->buf+ab->top, C, s);
	ab->top += s;
	return s;
}
