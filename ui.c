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

#include "ui.h"

/* This file contains UI-related functions
 * These functions are supposed to draw elements of UI.
 * They are supposed to read panel contents, but never modify it.
 */

static enum theme_element mode2theme(const mode_t m) {
	if (EXECUTABLE(m)) return THEME_ENTRY_REG_EXE_UNS;
	static const enum theme_element tm[] = {
		[S_IFBLK>>S_IFMT_TZERO] = THEME_ENTRY_BLK_UNS,
		[S_IFCHR>>S_IFMT_TZERO] = THEME_ENTRY_CHR_UNS,
		[S_IFIFO>>S_IFMT_TZERO] = THEME_ENTRY_FIFO_UNS,
		[S_IFREG>>S_IFMT_TZERO] = THEME_ENTRY_REG_UNS,
		[S_IFDIR>>S_IFMT_TZERO] = THEME_ENTRY_DIR_UNS,
		[S_IFSOCK>>S_IFMT_TZERO] = THEME_ENTRY_SOCK_UNS,
		[S_IFLNK>>S_IFMT_TZERO] = THEME_ENTRY_LNK_UNS,
	};
	return tm[(m & S_IFMT) >> S_IFMT_TZERO];
}

struct ui* global_i;

static int setup_signals(void);

static void sighandler(int sig) {
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		global_i->run = false; // TODO
		break;
	case SIGTSTP:
		stop_raw_mode(&global_i->T);
		signal(SIGTSTP, SIG_DFL);
		kill(getpid(), SIGTSTP);
		break;
	case SIGCONT:
		start_raw_mode(&global_i->T);
		global_i->dirty |= DIRTY_ALL;
		ui_draw(global_i);
		setup_signals();
		break;
	case SIGWINCH:
		for (int b = 0; b < BUF_NUM; ++b) {
			free(global_i->B[b].buf);
			global_i->B[b].top = global_i->B[b].capacity = 0;
			global_i->B[b].buf = NULL;
		}
		global_i->dirty |= DIRTY_ALL;
		ui_draw(global_i);
		break;
	default:
		break;
	}
}

static int setup_signals(void) {
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = sighandler;
	if (sigaction(SIGTERM, &sa, NULL)
	|| sigaction(SIGINT, &sa, NULL)
	|| sigaction(SIGTSTP, &sa, NULL)
	|| sigaction(SIGCONT, &sa, NULL)
	|| sigaction(SIGWINCH, &sa, NULL)) {
		return errno;
	}
	return 0;
}

void ui_init(struct ui* const i, struct panel* const pv,
		struct panel* const sv) {
	setlocale(LC_ALL, "");
	i->scrh = i->scrw = i->pw[0] = i->pw[1] = 0;
	i->ph = i->pxoff[0] = i->pxoff[1] = 0;
	i->run = true;

	i->m = MODE_MANAGER;
	i->mt = MSG_NONE;

	i->prompt = NULL;
	i->prompt_cursor_pos = i->timeout = -1;

	memset(i->B, 0, BUF_NUM*sizeof(struct append_buffer));
	i->dirty = DIRTY_ALL;

	i->fvs[0] = i->pv = pv;
	i->fvs[1] = i->sv = sv;

	i->kml = default_mapping_length;
	i->kmap = malloc(i->kml*sizeof(struct input2cmd));
	for (size_t k = 0; k < i->kml; ++k) {
		memcpy(&i->kmap[k], &default_mapping[k], sizeof(struct input2cmd));
	}
	memset(i->K, 0, sizeof(struct input)*INPUT_LIST_LENGTH);

	i->perm[0] = i->perm[1] = 0;
	i->o[0] = i->o[1] = 0;
	i->g[0] = i->g[1] = 0;

	global_i = i;
	int err;
		if ((err = start_raw_mode(&i->T)) || (err = setup_signals())) {
		fprintf(stderr, "failed to initalize screen: (%d) %s\n",
				err, strerror(err));
		exit(EXIT_FAILURE);
	}
	write(STDOUT_FILENO, CSI_CURSOR_HIDE);
}

void ui_end(struct ui* const i) {
	write(STDOUT_FILENO, CSI_CLEAR_ALL);
	write(STDOUT_FILENO, CSI_CURSOR_SHOW);
	for (int b = 0; b < BUF_NUM; ++b) {
		if (i->B[b].buf) free(i->B[b].buf);
	}
	int err;
	if ((err = stop_raw_mode(&i->T))) {
		fprintf(stderr, "failed to deinitalize screen: (%d) %s\n",
				err, strerror(err));
		exit(EXIT_FAILURE);
	}
	free(i->kmap);
	memset(i, 0, sizeof(struct ui));
}

void ui_pathbar(struct ui* const i, struct append_buffer* const ab) {
	append(ab, CSI_CLEAR_LINE);
	append_theme(ab, THEME_PATHBAR);
	for (size_t j = 0; j < 2; ++j) {
		const int p = prettify_path_i(i->fvs[j]->wd);
		const int t = p ? 1 : 0;
		const char* wd = i->fvs[j]->wd + p;
		const size_t wdw = utf8_width(wd) + t;
		const size_t rem = i->pw[j]-2;
		size_t padding, wdl;
		fill(ab, ' ', 1);
		if (wdw > rem) {
			padding = 0;
			wd += utf8_w2nb(wd, wdw - rem) - t;
			wdl = utf8_w2nb(wd, rem);
		}
		else {
			if (t) fill(ab, '~', 1);
			padding = rem - wdw;
			wdl = i->fvs[j]->wdlen - p + t;
		}
		append(ab, wd, wdl);
		fill(ab, ' ', 1+padding);
	}
	append_attr(ab, ATTR_NORMAL, NULL);
	append(ab, "\r\n", 2);
}

static size_t stringify_p(const mode_t m, char* const perms) {
	perms[0] = mode_type_symbols[(m & S_IFMT) >> S_IFMT_TZERO];
	memcpy(perms+1, perm2rwx[(m>>6) & 07], 3);
	memcpy(perms+1+3, perm2rwx[(m>>3) & 07], 3);
	memcpy(perms+1+3+3, perm2rwx[(m>>0) & 07], 3);
	if (m & S_ISUID) {
		perms[3] = 's';
		if (!(m & S_IXUSR)) perms[3] ^= 0x20;
	}
	if (m & S_ISGID) {
		perms[6] = 's';
		if (!(m & S_IXGRP)) perms[6] ^= 0x20;
	}
	if (m & S_ISVTX) {
		perms[9] = 't';
		if (!(m & S_IXOTH)) perms[9] ^= 0x20;
	}
	return 10;
}

static size_t stringify_u(const uid_t u, char* const user) {
	const struct passwd* const pwd = getpwuid(u);
	if (pwd) return snprintf(user, LOGIN_BUF_SIZE, "%s", pwd->pw_name);
	else return snprintf(user, LOGIN_BUF_SIZE, "%u", u);
}

static size_t stringify_g(const gid_t g, char* const group) {
	const struct group* const grp = getgrgid(g);
	if (grp) return snprintf(group, LOGIN_BUF_SIZE, "%s", grp->gr_name);
	else return snprintf(group, LOGIN_BUF_SIZE, "%u", g);
}

static void _column(const enum column C, const struct file* const cfr,
		char* const buf, const size_t bufsize, size_t* const buflen) {
	// TODO adjust width of columns
	// TODO inode, longsize, shortsize: length may be very different
	struct tm T;
	time_t t;
	const char* tfmt;
	time_t tspec;
	switch (C) {
		case COL_LONGATIME:
		case COL_SHORTATIME:
			tspec = cfr->s.st_atim.tv_sec;
			break;
		case COL_LONGCTIME:
		case COL_SHORTCTIME:
			tspec = cfr->s.st_ctim.tv_sec;
			break;
		case COL_LONGMTIME:
		case COL_SHORTMTIME:
			tspec = cfr->s.st_mtim.tv_sec;
			break;
		default:
			tspec = 0;
			break;
	}
	switch (C) {
	case COL_INODE:
		*buflen = snprintf(buf, bufsize, "%6lu", cfr->s.st_ino); // TODO fmt type
		break;
	case COL_LONGSIZE:
		*buflen = snprintf(buf, bufsize, "%10ld", cfr->s.st_size); // TODO fmt type
		break;
	case COL_SHORTSIZE:
		pretty_size(cfr->s.st_size, buf);
		*buflen = strlen(buf);
		memmove(buf+(SIZE_BUF_SIZE-1-*buflen), buf, *buflen);
		memset(buf, ' ', (SIZE_BUF_SIZE-1-*buflen));
		*buflen = SIZE_BUF_SIZE-1;
		break;
	case COL_LONGPERM:
		*buflen = stringify_p(cfr->s.st_mode, buf);
		buf[*buflen] = 0;
		break;
	case COL_SHORTPERM:
		*buflen = snprintf(buf, bufsize,
			"%04o", cfr->s.st_mode & 07777);
		break;
	case COL_UID:
		*buflen = snprintf(buf, bufsize, "%u", cfr->s.st_uid);
		break;
	case COL_USER:
		*buflen = stringify_u(cfr->s.st_uid, buf);
		break;
	case COL_GID:
		*buflen = snprintf(buf, bufsize, "%u", cfr->s.st_gid);
		break;
	case COL_GROUP:
		*buflen = stringify_g(cfr->s.st_gid, buf);
		break;
	case COL_LONGATIME:
	case COL_LONGCTIME:
	case COL_LONGMTIME:
		if (!localtime_r(&tspec, &T)) {
			memset(&T, 0, sizeof(struct tm));
		}
		strftime(buf, TIME_SIZE, timefmt, &T);
		*buflen = strlen(buf);
		break;
	case COL_SHORTATIME:
	case COL_SHORTCTIME:
	case COL_SHORTMTIME:
		t = time(NULL);
		if (t - tspec > 60*60*24*365) {
			tfmt = "%y'%b";
		}
		else if (t - tspec > 60*60*24) {
			tfmt = "%b %d";
		}
		else {
			tfmt = " %H:%M";
		}
		if (!localtime_r(&tspec, &T)) {
			memset(&T, 0, sizeof(struct tm));
		}
		strftime(buf, TIME_SIZE, tfmt, &T);
		*buflen = strlen(buf);
		break;
	default:
		buf[0] = 0;
		*buflen = 0;
		break;
	}
}

static void _entry(struct ui* const i, const struct panel* const fv,
		const size_t width, const fnum_t e) {
	struct append_buffer* const ab = &i->B[BUF_PANELS];
	// TODO scroll filenames that are too long to fit in the panel width
	const struct file* const cfr = fv->file_list[e];

	// File SYMbol
	enum theme_element fsym = mode2theme(cfr->s.st_mode);

	char name[NAME_BUF_SIZE];
	const unsigned u = cut_unwanted(cfr->name, name, '.', NAME_BUF_SIZE);

	char column[48];
	size_t cl;
	_column(fv->column, cfr, column, sizeof(column), &cl);

	if (1+(fv->column != COL_NONE)+cl+1 > width) return;
	const size_t name_allowed = width - (1+(fv->column != COL_NONE)+cl+1);
	const size_t name_width = utf8_width(name);
	const size_t name_draw = (name_width < name_allowed
			? name_width : name_allowed);

	char open = file_symbols[fsym];
	char close = ' ';

	if (e == fv->selection) {
		if (!fv->num_selected) {
			open = '[';
			close = ']';
		}
		if (fv == i->pv) {
			fsym += 1;
		}
		else {
			append_attr(ab, ATTR_UNDERLINE, NULL);
		}
	}
	if (cfr->selected) {
		open = '[';
		close = ']';
		append_attr(ab, ATTR_BOLD, NULL);
	}
	append_theme(ab, fsym);
	fill(ab, open, 1);
	append(ab, name, utf8_w2nb(name, name_draw));
	fill(ab, ' ', width - (1+name_draw+cl+1));
	append(ab, column, cl);
	if (u) {
		close = '*';
		append_attr(ab, ATTR_YELLOW|ATTR_BOLD|ATTR_FOREGROUND, NULL);
	}
	fill(ab, close, 1);
	append_attr(ab, ATTR_NORMAL, NULL);
}

/*
 * - Max Entries = how many entries I need to fill
 * all available space in file view
 * (selection excluded - it's always drawn)
 *
 * me = panel height - 1 for path bar, 1 for selection, 1 for info bar
 *
 * - Entries Over = how many entries are over selection
 * - Entries Under = how many entries are under selection
 *
 * - Begin Index = from which index should I start looking for
 * visible entries to catch all I need
 *
 * - Over Index = iterator; an index offset relative from selection,
 *   selection+oi = effective index
 * - Under Index = iterator; an index offset relative from selection,
 *   selection-ui = effective index
 */

/*
 * At which index should I start looking
 * for visible entries to catch all that can be displayed
 */
static fnum_t _start_search_index(const struct panel* const s,
		const fnum_t nhf, const fnum_t me) {
	fnum_t eo = 0; // Entries Over
	fnum_t oi = 1; // Over Index
	fnum_t bi = 0; // Begin Index
	fnum_t eu = 0; // Entries Under
	fnum_t ui = 1; // Under Index
	/* How many entries are under selection? */
	while (s->num_files-nhf && s->selection+ui < s->num_files && eu < me/2) {
		if (visible(s, s->selection+ui)) eu += 1;
		ui += 1;
	}
	/* How many entries are over selection?
	 * (If there are few entries under, then use up all remaining space)
	 */
	while (s->num_files-nhf && s->selection >= oi && eo + 1 + eu <= me) {
		if (visible(s, s->selection-oi)) eo += 1;
		bi = s->selection-oi;
		oi += 1;
	}
	return bi;
}

void ui_statusbar(struct ui* const i, struct append_buffer* const ab) {
	// TODO now that there are columns...
	const struct file* const _hfr = hfr(i->pv);
	struct tm T;
	if (!_hfr || !localtime_r(&(_hfr->s.st_mtim.tv_sec), &T)) {
		memset(&T, 0, sizeof(struct tm));
	}
	strftime(i->time, TIME_SIZE, timefmt, &T);

	char S[10 +1+10 +2 +1+10+2 +1+FV_ORDER_SIZE +1];
	const fnum_t nhf = (i->pv->show_hidden ? 0 : i->pv->num_hidden);
	int sl = 0;
	sl += snprintf(S, sizeof(S), "%u", i->pv->num_files-nhf);
	if (!i->pv->show_hidden) {
		sl += snprintf(S+sl, sizeof(S)-sl, "+%u", i->pv->num_hidden);
	}
	sl += snprintf(S+sl, sizeof(S)-sl, "f ");
	if (i->pv->num_selected) {
		sl += snprintf(S+sl, sizeof(S)-sl,
				"[%u] ", i->pv->num_selected);
	}
	sl += snprintf(S+sl, sizeof(S)-sl, "%c%.*s",
		(i->pv->scending > 0 ? '+' : '-'),
		(int)FV_ORDER_SIZE, i->pv->order);

	const size_t cw = utf8_width(S);
	const size_t uw = utf8_width(i->user);
	const size_t gw = utf8_width(i->group);
	const size_t sw = uw+1+gw+1+10+1+TIME_SIZE+1;
	append_theme(ab, THEME_STATUSBAR);
	fill(ab, ' ', 1);
	if ((size_t)i->scrw < cw+sw) {
		fill(ab, ' ', i->scrw-1);
		append_attr(ab, ATTR_NORMAL, NULL);
		append(ab, "\r\n", 2);
		return;
	}
	append(ab, S, sl);
	fill(ab, ' ', i->scrw-cw-sw); // Padding
	append(ab, i->user, strnlen(i->user, LOGIN_MAX_LEN));
	fill(ab, ' ', 1);
	append(ab, i->group, strnlen(i->group, LOGIN_MAX_LEN));
	fill(ab, ' ', 1);
	append(ab, &i->perms[0], 1);
	for (size_t p = 1; p < 10; ++p) {
		const mode_t m[2] = {
			(i->perm[0] & 0777) & (0400 >> (p-1)),
			(i->perm[1] & 0777) & (0400 >> (p-1))
		};
		if (m[0] != m[1]) append_attr(ab, ATTR_UNDERLINE, NULL);
		append(ab, &i->perms[p], 1);
		if (m[0] != m[1]) append_attr(ab, ATTR_NOT_UNDERLINE, NULL);
	}
	fill(ab, ' ', 1);
	append(ab, i->time, strnlen(i->time, TIME_SIZE));
	fill(ab, ' ', 1);
	append_attr(ab, ATTR_NORMAL, NULL);
	append(ab, "\r\n", 2);
}

static void _keyname(const struct input* const in, char* const buf) {
	// TODO
	// TODO strncpy
	static const char* const N[] = {
		[I_ARROW_UP] = "up",
		[I_ARROW_DOWN] = "down",
		[I_ARROW_RIGHT] = "right",
		[I_ARROW_LEFT] = "left",
		[I_HOME] = "home",
		[I_END] = "end",
		[I_PAGE_UP] = "pgup",
		[I_PAGE_DOWN] = "pgdn",
		[I_INSERT] = "ins",
		[I_BACKSPACE] = "bsp",
		[I_DELETE] = "del",
		[I_ESCAPE] = "esc"
	};
	switch (in->t) {
	case I_NONE:
		strcpy(buf, "??");
		break;
	case I_UTF8:
		if (in->utf[0] == ' ') {
			strcpy(buf, "spc");
		}
		else {
			strcpy(buf, in->utf);
		}
		break;
	case I_CTRL:
		switch (in->utf[0]) {
		case 'I':
			strcpy(buf, "tab");
			break;
		case 'M':
			strcpy(buf, "enter");
			break;
		default:
			buf[0] = '^';
			strcpy(buf+1, in->utf);
			break;
		}
		break;
	default:
		strcpy(buf, N[in->t]);
		break;
	}
}

inline static void _find_all_keyseqs4cmd(const struct ui* const i,
		const enum command c, const enum mode m,
		const struct input2cmd* ic[], size_t* const ki) {
	*ki = 0;
	for (size_t k = 0; k < i->kml; ++k) {
		if (i->kmap[k].c != c || i->kmap[k].m != m) continue;
		ic[*ki] = &i->kmap[k];
		*ki += 1;
	}
}

/*
 * TODO errors
 * TODO tab (8 spaces) may not be enough
 * TODO ambiguity: there is no difference between tab (3 ascii keys)
 *      and tab (1 special key) + others
 */
int help_to_fd(struct ui* const i, const int tmpfd) {
	int err = 0;
	for (size_t m = 0; m < MODE_NUM; ++m) {
		/* MODE TITLE */
		write(tmpfd, mode_strings[m], strlen(mode_strings[m]));
		write(tmpfd, "\n", 1);

		/* LIST OF AVAILABLE KEYS */
		const struct input2cmd* k[4];
		size_t ki = 0;
		for (size_t c = CMD_NONE+1; c < CMD_NUM; ++c) {
			_find_all_keyseqs4cmd(i, c, m, k, &ki);
			if (!ki) continue; // ^^^ may output empty array
			size_t maxsequences = 4;
			char key[KEYNAME_BUF_SIZE];
			for (size_t s = 0; s < ki; ++s) {
				unsigned j = 0;
				while (k[s]->i[j].t != I_NONE) {
					_keyname(&k[s]->i[j], key);
					write(tmpfd, key, strlen(key));
					j += 1;
				}
				maxsequences -= 1;
				write(tmpfd, "\t", 1);
			}
			for (unsigned s = 0; s < maxsequences; ++s) {
				write(tmpfd, "\t", 1);
			}
			write(tmpfd, cmd_help[c], strlen(cmd_help[c]));
			write(tmpfd, "\n", 1);
		}
		write(tmpfd, "\n", 1);
	}

	int L = 0;
	while (more_help[L]) {
		write(tmpfd, more_help[L], strlen(more_help[L]));
		write(tmpfd, "\n", 1);
		L += 1;
	}
	write(tmpfd, "\n", 1);
	L = 0;
	while (copyright_notice[L]) {
		write(tmpfd, copyright_notice[L], strlen(copyright_notice[L]));
		write(tmpfd, "\n", 1);
		L += 1;
	}
	return err;
}

void ui_panels(struct ui* const i, struct append_buffer* const ab) {
	fnum_t e[2] = {
		_start_search_index(i->fvs[0], i->fvs[0]->num_hidden, i->ph-1),
		_start_search_index(i->fvs[1], i->fvs[1]->num_hidden, i->ph-1),
	};
	for (int L = 0; L < i->ph; ++L) {
		append(ab, CSI_CLEAR_LINE);
		for (size_t p = 0; p < 2; ++p) {
			const fnum_t nf = i->fvs[p]->num_files;
			while (e[p] < nf && !visible(i->fvs[p], e[p])) {
				e[p] += 1;
			}
			if (e[p] >= nf) {
				append_theme(ab, THEME_OTHER);
				fill(ab, ' ', i->pw[p]);
				append_attr(ab, ATTR_NORMAL, NULL);
			}
			else {
				_entry(i, i->fvs[p], i->pw[p], e[p]);
			}
			e[p] += 1;
		}
		append(ab, "\r\n", 2);
	}
}

void ui_bottombar(struct ui* const i, struct append_buffer* const ab) {
	append(ab, CSI_CLEAR_LINE);
	if (i->prompt) {
		const size_t aw = utf8_width(i->prch);
		const size_t pw = utf8_width(i->prompt);
		size_t padding;
		if ((size_t)i->scrw > pw+aw) {
			padding = i->scrw-(pw+aw);
		}
		else {
			padding = 0;
		}
		append(ab, i->prch, aw);
		append(ab, i->prompt, strlen(i->prompt));
		if (padding) fill(ab, ' ', padding);
	}
	else if (i->mt) {
		int cp = 0;
		switch (i->mt) {
		case MSG_INFO: cp = THEME_INFO; break;
		case MSG_ERROR: cp = THEME_ERROR; break;
		default: break;
		}
		append_theme(ab, cp);
		append(ab, i->msg, strlen(i->msg));
		append_attr(ab, ATTR_NORMAL, NULL);
		fill(ab, ' ', i->scrw-utf8_width(i->msg));
		i->mt = MSG_NONE;
	}
	else if (i->m == MODE_CHMOD) {
		append(ab, "-- CHMOD --", 11);
		char p[4+1+1+4+1+1+4+1+1+4+1];
		fill(ab, ' ', i->scrw-(sizeof(p)-1)-11);
		snprintf(p, sizeof(p), "%04o +%04o -%04o =%04o",
				i->perm[0] & 07777, i->plus,
				i->minus, i->perm[1] & 07777);
		append(ab, p, sizeof(p));
	}
	else {
		// TODO input buffer
	}
}

void ui_draw(struct ui* const i) {
	ui_update_geometry(i);
	if (i->m == MODE_MANAGER || i->m == MODE_WAIT) {
		const struct file* const H = hfr(i->pv);
		if (H) {
			stringify_p(H->s.st_mode, i->perms);
			stringify_u(H->s.st_uid, i->user);
			stringify_g(H->s.st_gid, i->group);
		}
		else {
			memset(i->perms, '-', 10);
			i->user[0] = i->group[0] = 0;
		}
	}
	else if (i->m == MODE_CHMOD) {
		i->perm[1] = (i->perm[0] | i->plus) & ~i->minus;
		stringify_p(i->perm[1], i->perms);
		stringify_u(i->o[1], i->user);
		stringify_g(i->g[1], i->group);
	}
	write(STDOUT_FILENO, CSI_CURSOR_HIDE_TOP_LEFT);
	for (int b = 0; b < BUF_NUM; ++b) {
		if (i->dirty & (1 << b)) {
			i->B[b].top = 0;
			do_draw[b](i, &i->B[b]);
		}
		write(STDOUT_FILENO, i->B[b].buf, i->B[b].top);
	}
	i->dirty = 0;
	if (i->prompt && i->prompt_cursor_pos >= 0) {
		write(STDOUT_FILENO, CSI_CURSOR_SHOW);
		move_cursor(i->scrh, i->prompt_cursor_pos%i->scrw+1);
	}
}

void ui_update_geometry(struct ui* const i) {
	window_size(&i->scrh, &i->scrw);
	i->pw[0] = i->scrw/2;
	i->pw[1] = i->scrw - i->pw[0];
	i->ph = i->scrh - 3;
	i->pxoff[0] = 0;
	i->pxoff[1] = i->scrw/2;
}

int chmod_open(struct ui* const i, char* const path) {
	struct stat s;
	if (stat(path, &s)) return errno;
	errno = 0;
	struct passwd* pwd = getpwuid(s.st_uid);
	if (!pwd) return errno;
	errno = 0;
	struct group* grp = getgrgid(s.st_gid);
	if (!grp) return errno;

	i->o[0] = i->o[1] = s.st_uid;
	i->g[0] = i->g[1] = s.st_gid;
	i->plus = i->minus = 0;
	i->perm[0] = i->perm[1] = s.st_mode;
	i->path = path;
	i->m = MODE_CHMOD;
	xstrlcpy(i->user, pwd->pw_name, LOGIN_BUF_SIZE);
	xstrlcpy(i->group, grp->gr_name, LOGIN_BUF_SIZE);
	return 0;
}

void chmod_close(struct ui* const i) {
	i->m = MODE_MANAGER;
	free(i->path);
	i->path = NULL;
	i->perm[0] = i->perm[1] = 0;
}

int ui_ask(struct ui* const i, const char* const q,
		const struct select_option* o, const size_t oc) {
	const int oldtimeout = i->timeout;
	i->timeout = -1;
	int T = 0;
	char P[512]; // TODO
	i->prch[0] = 0;
	i->prompt = P;
	i->prompt_cursor_pos = -1;
	T += snprintf(P+T, sizeof(P)-T, "%s ", q);
	for (size_t j = 0; j < oc; ++j) {
		if (j) T += snprintf(P+T, sizeof(P)-T, ", ");
		char b[KEYNAME_BUF_SIZE];
		_keyname(&o[j].i, b);
		T += snprintf(P+T, sizeof(P)-T, "%s=%s", b, o[j].h);
	}
	i->dirty |= DIRTY_BOTTOMBAR;
	ui_draw(i);
	struct input in;
	for (;;) {
		in = get_input(i->timeout);
		for (size_t j = 0; j < oc; ++j) {
			if (!memcmp(&in, &o[j].i, sizeof(struct input))) {
				i->dirty |= DIRTY_BOTTOMBAR;
				i->prompt = NULL;
				i->timeout = oldtimeout;
				return j;
			}
		}
	}
}

/*
 * Find matching mappings
 * If there are a few, do nothing, wait longer.
 * If there is only one, send it.
 */
enum command get_cmd(struct ui* const i) {
#define ISIZE (sizeof(struct input)*INPUT_LIST_LENGTH)
	int Kn = 0;
	while (Kn < INPUT_LIST_LENGTH && i->K[Kn].t != I_NONE) {
		Kn += 1;
	}
	if (Kn == INPUT_LIST_LENGTH) {
		memset(i->K, 0, ISIZE);
		Kn = 0;
	}
	i->K[Kn] = get_input(i->timeout);
	if (i->K[Kn].t == I_NONE
	|| i->K[Kn].t == I_ESCAPE
	|| IS_CTRL(i->K[Kn], '[')) {
		memset(i->K, 0, ISIZE);
		return CMD_NONE;
	}
	int pm = 0; // partial match
	for (size_t m = 0; m < i->kml; ++m) {
		if (i->kmap[m].m != i->m) continue;
		if (!memcmp(i->K, i->kmap[m].i, ISIZE)) {
			memset(i->K, 0, ISIZE);
			return i->kmap[m].c;
		}
		bool M = true;
		for (int t = 0; t < Kn+1; ++t) {
			M = M && !memcmp(&i->kmap[m].i[t],
				&i->K[t], sizeof(struct input));
		}
		pm += M;
	}
	if (!pm) memset(i->K, 0, ISIZE);
	return CMD_NONE;
#undef ISIZE
}

/*
 * Gets input to buffer
 * Responsible for cursor movement (buftop) and guarding buffer bounds (bsize)
 * If text is ready (enter pressed) returns 0,
 * If aborted, returns -1.
 * If keeps gathering, returns 1.
 * Unhandled inputs are put into 'o' (if not NULL) and 2 is returned
 */
int fill_textbox(struct ui* const I, char* const buf, char** const buftop,
		const size_t bsize, struct input* const o) {
	const struct input i = get_input(I->timeout);
	if (i.t == I_NONE) return 1;
	if (IS_CTRL(i, '[') || i.t == I_ESCAPE) return -1;
	else if ((IS_CTRL(i, 'J') || IS_CTRL(i, 'M'))
	|| (i.t == I_UTF8 && (i.utf[0] == '\n' || i.utf[0] == '\r'))) {
		/* If empty, abort.
		 * Otherwise ready.
		 */
		return (*buftop == buf ? -1 : 0);
	}
	else if (i.t == I_UTF8 && strlen(buf)+utf8_g2nb(i.utf) <= bsize) {
		utf8_insert(buf, i.utf, utf8_wtill(buf, *buftop));
		*buftop += utf8_g2nb(i.utf);
	}
	else if (IS_CTRL(i, 'H') || i.t == I_BACKSPACE) {
		if (*buftop != buf) {
			const size_t wt = utf8_wtill(buf, *buftop);
			*buftop -= utf8_remove(buf, wt-1);
		}
		else if (!strnlen(buf, bsize)) {
			return -1;
		}
	}
	else if (IS_CTRL(i, 'D') || i.t == I_DELETE) {
		utf8_remove(buf, utf8_wtill(buf, *buftop));
	}
	else if (IS_CTRL(i, 'A') || i.t == I_HOME) {
		*buftop = buf;
	}
	else if (IS_CTRL(i, 'E') || i.t == I_END) {
		*buftop = buf+strnlen(buf, bsize);
	}
	else if (IS_CTRL(i, 'U')) {
		*buftop = buf;
		memset(buf, 0, bsize);
	}
	else if (IS_CTRL(i, 'K')) {
		const size_t clen = strnlen(buf, bsize);
		memset(*buftop, 0, strnlen(*buftop, bsize-clen));
	}
	else if (IS_CTRL(i, 'F') || i.t == I_ARROW_RIGHT) {
		if ((size_t)(*buftop - buf) < bsize) {
			*buftop += utf8_g2nb(*buftop);
		}
	}
	else if (IS_CTRL(i, 'B') || i.t == I_ARROW_LEFT) {
		if (*buftop - buf) {
			const size_t gt = utf8_wtill(buf, *buftop);
			*buftop = buf;
			for (size_t i = 0; i < gt-1; ++i) {
				*buftop += utf8_g2nb(*buftop);
			}
		}
	}
	I->prompt_cursor_pos = utf8_width(buf)-utf8_width(*buftop)+1;
	if (o) *o = i;
	return 2;
}

int prompt(struct ui* const i, char* const t,
		char* t_top, const size_t t_size) {
	strcpy(i->prch, ">");
	i->prompt = t;
	i->prompt_cursor_pos = 1;
	i->dirty |= DIRTY_BOTTOMBAR;
	ui_draw(i);
	int r;
	do {
		r = fill_textbox(i, t, &t_top, t_size, NULL);
		i->dirty |= DIRTY_BOTTOMBAR;
		ui_draw(i);
	} while (r && r != -1);
	i->dirty |= DIRTY_BOTTOMBAR;
	i->prompt = NULL;
	i->prompt_cursor_pos = -1;
	return r;
}

/*
 * success = true
 * failure = false
 */
bool ui_rescan(struct ui* const i, struct panel* const a,
		struct panel* const b) {
	int err;
	i->dirty = DIRTY_ALL;
	if (a && (err = panel_scan_dir(a))) {
		failed(i, "directory scan", strerror(err));
		return false;
	}
	if (b && (err = panel_scan_dir(b))) {
		failed(i, "directory scan", strerror(err));
		return false;
	}
	return true;
}

void failed(struct ui* const i, const char* const what,
		const char* const why) {
	i->mt = MSG_ERROR;
	i->dirty |= DIRTY_BOTTOMBAR;
	snprintf(i->msg, MSG_BUFFER_SIZE, "%s failed: %s", what, why);
}

int spawn(char* const arg[], const enum spawn_flags f) {
	int ret = 0, status = 0, nullfd;
	pid_t pid;
	if (write(STDOUT_FILENO, CSI_CLEAR_ALL) == -1) return errno;
	if ((ret = stop_raw_mode(&global_i->T))) return ret;
	if (write(STDOUT_FILENO, "\r\n", 2) == -1) return errno;
	if ((pid = fork()) == -1) {
		ret = errno;
	}
	else if (pid == 0) {
		if (f & SF_SLIENT) {
			nullfd = open("/dev/null", O_WRONLY, 0100);
			if (dup2(nullfd, STDERR_FILENO) == -1) ret = errno;
			if (dup2(nullfd, STDOUT_FILENO) == -1) ret = errno;
			close(nullfd);
		}
		execvp(arg[0], arg);
		exit(EXIT_FAILURE);
	}
	else {
		global_i->dirty |= DIRTY_BOTTOMBAR;
		global_i->mt = MSG_INFO;
		xstrlcpy(global_i->msg,
			"external program is running...", MSG_BUFFER_SIZE);
		waitpid(pid, &status, 0);
	}
	global_i->msg[0] = 0;
	ret = start_raw_mode(&global_i->T);
	global_i->dirty = DIRTY_ALL;
	ui_draw(global_i);
	return ret;
}

size_t append_theme(struct append_buffer* const ab,
		const enum theme_element te) {
	size_t n = 0;
	n += append_attr(ab, theme_scheme[te].fg | ATTR_FOREGROUND,
			theme_scheme[te].fg_color);
	n += append_attr(ab, theme_scheme[te].bg | ATTR_BACKGROUND,
			theme_scheme[te].bg_color);
	return n;
}
