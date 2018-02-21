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

#include "include/ui.h"

/* This file contains UI-related functions
 * These functions are supposed to draw elements of UI.
 * They are supposed to read file_view contents, but never modify it.
 */

static enum theme_element mode2theme(const mode_t m) {
	// TODO find a better way
	switch (m & S_IFMT) {
	case S_IFBLK: return THEME_ENTRY_BLK_UNS;
	case S_IFCHR: return THEME_ENTRY_CHR_UNS;
	case S_IFIFO: return THEME_ENTRY_FIFO_UNS;
	case S_IFREG:
		if (EXECUTABLE(m)) return THEME_ENTRY_REG_EXE_UNS;
		return THEME_ENTRY_REG_UNS;
	case S_IFDIR: return THEME_ENTRY_DIR_UNS;
	case S_IFSOCK: return THEME_ENTRY_SOCK_UNS;
	case S_IFLNK: return THEME_ENTRY_LNK_UNS;
	default: return THEME_ENTRY_REG_UNS;
	}
}

struct ui* global_i;
static void handle_winch(int sig) {
	if (sig != SIGWINCH) return;
	//free(global_i->B.buf);
	//global_i->B.top = global_i->B.capacity = 0;
	//global_i->B.buf = NULL;
	write(STDOUT_FILENO, CSI_CLEAR_ALL);
	ui_update_geometry(global_i);
	ui_draw(global_i);
}

void ui_init(struct ui* const i, struct file_view* const pv,
		struct file_view* const sv) {
	setlocale(LC_ALL, "");
	i->fvs[0] = i->pv = pv;
	i->fvs[1] = i->sv = sv;
	i->scrw = i->scrh = 0;
	i->m = MODE_MANAGER;
	memset(i->prch, 0, sizeof(i->prch));
	i->prompt = NULL;
	i->prompt_cursor_pos = -1;
	i->timeout = -1;
	i->mt = MSG_NONE;
	i->helpy = 0;
	i->run = i->ui_needs_refresh = true;
	i->ili = 0;
	memset(&i->B, 0, sizeof(struct append_buffer));
	memset(i->il, 0, INPUT_LIST_LENGTH*sizeof(struct input));
	i->kml = default_mapping_length;
	i->mks = calloc(default_mapping_length, sizeof(unsigned short));
	i->kmap = default_mapping; // TODO ???
	//i->kmap = malloc(default_mapping_length*sizeof(struct input2cmd));
	/*for (size_t k = 0; k < default_mapping_length; ++k) {
		memcpy(&i->kmap[k], &default_mapping[k], sizeof(struct input2cmd));
	}*/
	memset(i->perm, 0, sizeof(i->perm));
	memset(i->o, 0, sizeof(i->o));
	memset(i->g, 0, sizeof(i->g));

	global_i = i;
	int err;
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = handle_winch;
	if ((err = start_raw_mode(&i->T))
	|| (sigaction(SIGWINCH, &sa, NULL) ? (err = errno) : 0)) {
		fprintf(stderr, "failed to initalize screen: (%d) %s\n",
				err, strerror(err));
		exit(EXIT_FAILURE);
	}
	write(STDOUT_FILENO, CSI_CURSOR_HIDE);
}

void ui_end(struct ui* const i) {
	free(i->mks);
	write(STDOUT_FILENO, CSI_CLEAR_ALL);
	write(STDOUT_FILENO, CSI_CURSOR_SHOW);
	free(i->B.buf);
	int err;
	if ((err = stop_raw_mode(&i->T))) {
		fprintf(stderr, "failed to deinitalize screen: (%d) %s\n",
				err, strerror(err));
		exit(EXIT_FAILURE);
	}
	memset(i, 0, sizeof(struct ui));
}

static void _pathbars(struct ui* const i) {
	// TODO
	//const struct passwd* const pwd = getpwuid(geteuid());
	const char* wd[2] = {
		i->fvs[0]->wd,
		i->fvs[1]->wd
	};
	const int wdw[2] = {
		utf8_width(wd[0]),
		utf8_width(wd[1])
	};
	append(&i->B, CSI_CLEAR_LINE);
	append_theme(&i->B, THEME_PATHBAR);
	for (size_t p = 0; p < 2; ++p) {
		size_t padding, wdl;
		if (wdw[p] > i->pw[p]-2) {
			padding = 0;
			wd[p] += utf8_w2nb(wd[p], wdw[p] - (i->pw[p]-2));
			wdl = utf8_w2nb(wd[p], i->pw[p]-2);
		}
		else {
			padding = (i->pw[p]-2) - wdw[p];
			wdl = strnlen(wd[p], PATH_MAX_LEN);
		}
		fill(&i->B, ' ', 1);
		append(&i->B, wd[p], wdl);
		fill(&i->B, ' ', 1+padding);
	}
	append_attr(&i->B, ATTR_NORMAL, NULL);
	append(&i->B, "\r\n", 2);
}

static void _entry(struct ui* const i, const struct file_view* const fv,
		const size_t width, const fnum_t e) {
	// TODO scroll filenames that are too long to fit in the panel width
	// TODO signal invalid filenames
	const struct file_record* const cfr = fv->file_list[e];
	const bool hl = e == fv->selection && fv == i->pv;

	// File SYMbol
	const enum theme_element fsym = mode2theme(cfr->s.st_mode);

	char name[NAME_BUF_SIZE];
	cut_unwanted(cfr->file_name, name, '.', NAME_BUF_SIZE);

	char sbuf[SIZE_BUF_SIZE];
	pretty_size(cfr->s.st_size, sbuf);
	const size_t slen = strnlen(sbuf, SIZE_BUF_SIZE-1);

	if (1+SIZE_BUF_SIZE+1 > width) return;
	const size_t name_allowed = width - (1+SIZE_BUF_SIZE+1);
	const size_t name_width = utf8_width(name);
	const size_t name_draw = (name_width < name_allowed
			? name_width : name_allowed);
	const size_t space = name_allowed - name_draw;

	char open = file_symbols[fsym];
	char close = ' ';
	if (cfr->selected || (hl && !fv->num_selected)) {
		open = '[';
		close = ']';
	}

	append_theme(&i->B, fsym + (hl ? 1 : 0));
	fill(&i->B, open, 1);
	append(&i->B, name, utf8_w2nb(name, name_draw));
	fill(&i->B, ' ', space);
	fill(&i->B, ' ', SIZE_BUF_SIZE-slen);
	append(&i->B, sbuf, slen);
	fill(&i->B, close, 1);
	append_attr(&i->B, ATTR_NORMAL, NULL);
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
static fnum_t _start_search_index(const struct file_view* const s,
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

/* PUG = Permissions User Group */
static void stringify_pug(const mode_t m, const uid_t u, const gid_t g,
		char perms[10],
		char user[LOGIN_BUF_SIZE],
		char group[LOGIN_BUF_SIZE]) {
	memset(perms, 0, 10);
	memset(user, 0, LOGIN_BUF_SIZE);
	memset(group, 0, LOGIN_BUF_SIZE);
	const struct passwd* const pwd = getpwuid(u);
	const struct group* const grp = getgrgid(g);

	if (pwd) strncpy(user, pwd->pw_name, LOGIN_BUF_SIZE);
	else snprintf(user, LOGIN_BUF_SIZE, "uid:%u", u);

	if (grp) strncpy(group, grp->gr_name, LOGIN_BUF_SIZE);
	else snprintf(group, LOGIN_BUF_SIZE, "gid:%u", g);

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
}

static void _statusbar(struct ui* const i) {
	const struct file_record* const _hfr = hfr(i->pv);
	struct tm T;
	if (!_hfr || !localtime_r(&(_hfr->s.st_mtim.tv_sec), &T)) {
		memset(&T, 0, sizeof(struct tm));
	}
	strftime(i->time, TIME_SIZE, timefmt, &T);

	char status[10+2+10+2+10+1+1];
	const fnum_t nhf = (i->pv->show_hidden ? 0 : i->pv->num_hidden);
	int n = snprintf(status, sizeof(status),
			"%uf %u%c %us, %c%.*s",
			i->pv->num_files-nhf,
			i->pv->num_hidden,
			(i->pv->show_hidden ? 'H' : 'h'),
			i->pv->num_selected,
			(i->pv->scending > 0 ? '+' : '-'),
			(int)FV_ORDER_SIZE, i->pv->order);

	const size_t cw = utf8_width(status);
	const size_t uw = utf8_width(i->user);
	const size_t gw = utf8_width(i->group);
	const size_t sw = uw+1+gw+1+10+1+TIME_SIZE+1;
	if ((size_t)i->scrw < cw+sw) {
		append_theme(&i->B, THEME_STATUSBAR);
		fill(&i->B, ' ', i->scrw);
		append_attr(&i->B, ATTR_NORMAL, NULL);
		append(&i->B, "\r\n", 2);
		return;
	}
	append_theme(&i->B, THEME_STATUSBAR);
	fill(&i->B, ' ', 1);
	append(&i->B, status, n);
	fill(&i->B, ' ', i->scrw-cw-sw); // Padding
	append(&i->B, i->user, strnlen(i->user, LOGIN_MAX_LEN));
	fill(&i->B, ' ', 1);
	append(&i->B, i->group, strnlen(i->group, LOGIN_MAX_LEN));
	fill(&i->B, ' ', 1);
	append(&i->B, &i->perms[0], 1);
	for (size_t p = 1; p < 10; ++p) {
		const mode_t m[2] = {
			(i->perm[0] & 0777) & (0400 >> (p-1)),
			(i->perm[1] & 0777) & (0400 >> (p-1))
		};
		if (m[0] != m[1]) append_attr(&i->B, ATTR_UNDERLINE, NULL);
		append(&i->B, &i->perms[p], 1);
		if (m[0] != m[1]) append_attr(&i->B, ATTR_NOT_UNDERLINE, NULL);
	}
	fill(&i->B, ' ', 1);
	append(&i->B, i->time, strnlen(i->time, TIME_SIZE));
	fill(&i->B, ' ', 1);
	append_attr(&i->B, ATTR_NORMAL, NULL);
	append(&i->B, "\r\n", 2);
}

void _cmd_and_keyseqs(struct ui* const i, const enum command c,
		const struct input2cmd* const k[], const size_t ki) {
	// TODO some valid inputs such as SPACE are invisible
	const int maxsequences = INPUT_LIST_LENGTH-1;
	const size_t seqwid = 8; // TODO may not be enough
	size_t W = 0;
	append(&i->B, CSI_CLEAR_LINE);
	for (size_t s = 0; s < ki; ++s) {
		const struct input2cmd* const seq = k[s];
		size_t j = 0;
		size_t w = 0;
		while (seq->i[j].t != I_NONE) {
			const struct input* const v = &seq->i[j];
			append_attr(&i->B, ATTR_BOLD, NULL);
			switch (v->t) {
			case I_UTF8:
				//append_attr(B, ATTR_UNDERLINE, NULL);
				append(&i->B, v->utf, strlen(v->utf));
				//append_attr(B, ATTR_NOT_UNDERLINE, NULL);
				w += utf8_width(v->utf);
				break;
			case I_CTRL:
				fill(&i->B, '^', 1);
				fill(&i->B, v->utf[0], 1);
				w += 2;
				break;
			default:
				append(&i->B, keynames[v->t],
						strlen(keynames[v->t]));
				w += utf8_width(keynames[v->t]);
				break;
			}
			append_attr(&i->B, ATTR_NOT_BOLD_OR_FAINT, NULL);
			j += 1;
		}
		W += 1;
		fill(&i->B, ' ', seqwid - w);
	}
	fill(&i->B, ' ', (maxsequences-W) * seqwid);
	append(&i->B, cmd_help[c], strlen(cmd_help[c]));
}

void _find_all_keyseqs4cmd(const struct ui* const i, const enum command c,
		const enum mode m, const struct input2cmd* ic[],
		size_t* const ki) {
	*ki = 0;
	for (size_t k = 0; k < i->kml; ++k) {
		if (i->kmap[k].c != c || i->kmap[k].m != m) continue;
		ic[*ki] = &i->kmap[k];
		*ki += 1;
	}
}

static void _help(struct ui* const i) {
	// TODO detect when to stop
	int dr = -i->helpy;
	for (size_t m = 0; m < MODE_NUM; ++m) {
		/* MODE TITLE */
		if (dr < i->scrh) {
			append_attr(&i->B, ATTR_BOLD, NULL);
			const size_t ml = strlen(mode_strings[m]);
			append(&i->B, mode_strings[m], ml);
			append_attr(&i->B, ATTR_NOT_BOLD_OR_FAINT, NULL);
			fill(&i->B, ' ', i->scrw-ml);
			append(&i->B, "\r\n", 2);
		}
		dr += 1;

		/* LIST OF AVAILABLE KEYS */
		const struct input2cmd* k[4];
		size_t ki = 0;
		for (size_t c = CMD_NONE+1; c < CMD_NUM; ++c) {
			_find_all_keyseqs4cmd(i, c, m, k, &ki);
			if (ki) { // ^^^ may output empty array
				if (dr < i->scrh) {
					_cmd_and_keyseqs(i, c, k, ki);
					append(&i->B, "\r\n", 2);
				}
				dr += 1;
			}
		}
		/* EMPTY LINE PADDING */
		if (dr < i->scrh) {
			fill(&i->B, ' ', i->scrw);
			append(&i->B, "\r\n", 2);
		}
		dr += 1;
	}
	i->B.top -= 2; // last line

	/* COPYRIGHT NOTICE */
	append_attr(&i->B, ATTR_BOLD, NULL);
	int cnl = 0; // Copytight Notice Line
	while (copyright_notice[cnl]) {
		const char* const cr = copyright_notice[cnl];
		if (dr < i->scrh) {
			append(&i->B, cr, strlen(cr));
			fill(&i->B, ' ', i->scrw-utf8_width(cr));
		}
		dr += 1;
		cnl += 1;
	}
	append_attr(&i->B, ATTR_NOT_BOLD_OR_FAINT, NULL);
}

static void _panels(struct ui* const i) {
	fnum_t e[2] = {
		_start_search_index(i->fvs[0], i->fvs[0]->num_hidden, i->ph-1),
		_start_search_index(i->fvs[1], i->fvs[1]->num_hidden, i->ph-1),
	};
	for (int L = 0; L < i->ph; ++L) {
		append(&i->B, CSI_CLEAR_LINE);
		for (size_t p = 0; p < 2; ++p) {
			const fnum_t nf = i->fvs[p]->num_files;
			while (e[p] < nf && !visible(i->fvs[p], e[p])) {
				e[p] += 1;
			}
			if (e[p] >= nf) {
				append_theme(&i->B, THEME_OTHER);
				fill(&i->B, ' ', i->pw[p]);
				append_attr(&i->B, ATTR_NORMAL, NULL);
			}
			else {
				_entry(i, i->fvs[p], i->pw[p], e[p]);
			}
			e[p] += 1;
		}
		append(&i->B, "\r\n", 2);
	}
}

static void _bottombar(struct ui* const i) {
	append(&i->B, CSI_CLEAR_LINE);
	if (i->mt) {
		int cp = 0;
		switch (i->mt) {
		case MSG_INFO: cp = THEME_INFO; break;
		case MSG_ERROR: cp = THEME_ERROR; break;
		default: break;
		}
		append_theme(&i->B, cp);
		append(&i->B, i->msg, strlen(i->msg));
		append_attr(&i->B, ATTR_NORMAL, NULL);
		fill(&i->B, ' ', i->scrw-utf8_width(i->msg));
		i->mt = MSG_NONE;
	}
	else if (i->prompt) {
		const size_t aw = utf8_width(i->prch);
		const size_t pw = utf8_width(i->prompt);
		size_t padding, prompt_slice;
		if ((size_t)i->scrw > pw+aw) {
			padding = i->scrw-(pw+aw);
			prompt_slice = pw;
		}
		else {
			padding = 0;
			prompt_slice = i->scrw-aw;
		}
		append(&i->B, i->prch, aw);
		append(&i->B, i->prompt, utf8_w2nb(i->prompt, prompt_slice));
		if (padding) {
			fill(&i->B, ' ', padding);
		}
	}
	else if (i->m == MODE_CHMOD) {
		append(&i->B, "-- CHMOD --", 11);
		char p[4+1+1+4+1+1+4+1+1+4+1];
		fill(&i->B, ' ', i->scrw-(sizeof(p)-1)-11);
		snprintf(p, sizeof(p), "%04o +%04o -%04o =%04o",
				i->perm[0] & 07777, i->plus,
				i->minus, i->perm[1] & 07777);
		append(&i->B, p, sizeof(p));
	}
}

void ui_draw(struct ui* const i) {
	i->B.top = 0;
	memset(i->B.buf, 0, i->B.capacity);
	append(&i->B, CSI_CURSOR_HIDE);
	append(&i->B, CSI_CURSOR_TOP_LEFT);
	if (i->m == MODE_HELP) {
		_help(i);
	}
	else if (i->m == MODE_MANAGER || i->m == MODE_WAIT) { // TODO mode wait
		const struct file_record* const H = hfr(i->pv);
		if (H) {
			stringify_pug(H->s.st_mode, H->s.st_uid,
					H->s.st_gid, i->perms,
					i->user, i->group);
		}
		else {
			memset(i->perms, '-', 10);
			memcpy(i->user, "-", 2);
			memcpy(i->group, "-", 2);
		}
		_pathbars(i);
		_panels(i);
		_statusbar(i);
		_bottombar(i);
	}
	else if (i->m == MODE_CHMOD) {
		i->perm[1] = i->perm[0];
		i->perm[1] |= i->plus;
		i->perm[1] &= ~i->minus;
		stringify_pug(i->perm[1], i->o[1], i->g[1],
				i->perms, i->user, i->group);
		_pathbars(i);
		_panels(i);
		_statusbar(i);
		_bottombar(i);
	}
	const bool pa = i->prompt
		&& i->prompt_cursor_pos >= 0
		&& i->prompt_cursor_pos < i->scrw;
	if (!pa) append(&i->B, CSI_CURSOR_HIDE);
	write(STDOUT_FILENO, i->B.buf, i->B.top);
	if (pa) {
		write(STDOUT_FILENO, CSI_CURSOR_SHOW);
		move_cursor(i->scrh, i->prompt_cursor_pos+1);
	}
}

void ui_update_geometry(struct ui* const i) {
	window_size(&i->scrh, &i->scrw);
	i->pw[0] = i->scrw/2;
	i->pw[1] = i->scrw - i->pw[0];
	i->ph = i->scrh - 3;
	i->pxoff[0] = 0;
	i->pxoff[1] = i->scrw/2;
	i->ui_needs_refresh = false;
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
	i->perm[0] = i->perm[1] = s.st_mode;
	i->path = path;
	i->m = MODE_CHMOD;
	strncpy(i->user, pwd->pw_name, LOGIN_BUF_SIZE);
	strncpy(i->group, grp->gr_name, LOGIN_BUF_SIZE);
	i->ui_needs_refresh = true;
	return 0;
}

void chmod_close(struct ui* const i) {
	i->m = MODE_MANAGER;
	free(i->path);
	i->path = NULL;
	i->plus = i->minus = 0;
	memset(i->perm, 0, sizeof(i->perm));
	memset(i->o, 0, sizeof(i->o));
	memset(i->g, 0, sizeof(i->g));
}

int ui_select(struct ui* const i, const char* const q,
		const struct select_option* o, const size_t oc) {
	// TODO snprintf
	const int oldtimeout = i->timeout;
	i->timeout = -1;
	int T = 0;
	char P[500]; // TODO
	i->prch[0] = 0;
	i->prompt = P;
	T += snprintf(P+T, sizeof(P)-T, "%s ", q);
	for (size_t j = 0; j < oc; ++j) {
		if (j) T += snprintf(P+T, sizeof(P)-T, ", ");
		switch (o[j].i.t) {
		case I_UTF8:
			T += snprintf(P+T, sizeof(P)-T, "%s", o[j].i.utf);
			break;
		case I_CTRL:
			T += snprintf(P+T, sizeof(P)-T, "^%c", o[j].i.utf[0]);
			break;
		default:
			T += snprintf(P+T, sizeof(P)-T,
					"%s", keynames[o[j].i.t]);
			break;
		}
		T += snprintf(P+T, sizeof(P)-T, "=%s", o[j].h);
	}
	ui_draw(i);
	struct input in;
	for (;;) {
		in = get_input(i->timeout);
		for (size_t j = 0; j < oc; ++j) {
			if (!memcmp(&in, &o[j].i, sizeof(struct input))) {
				i->prompt = NULL;
				i->timeout = oldtimeout;
				return j;
			}
		}
	}
	//return 0; // unreachable
}

/*
 * Find matching mappings
 * If there are a few, do nothing, wait longer.
 * If there is only one, send it.
 */
enum command get_cmd(struct ui* const i) {
	if (i->ili >= INPUT_LIST_LENGTH) {
		memset(i->il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
		i->ili = 0;
	}
	i->il[i->ili] = get_input(i->timeout);
	i->ili += 1;
	if (i->il[i->ili-1].t == I_NONE) {
		return CMD_NONE;
	}
	if (i->il[i->ili-1].t == I_ESCAPE || IS_CTRL(i->il[i->ili-1], '[')) {
		memset(i->il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
		i->ili = 0;
		return CMD_NONE;
	}

	for (size_t m = 0; m < i->kml; ++m) {
		i->mks[m] = 0;
		const struct input2cmd* const i2c = &i->kmap[m];
		if (i2c->m != i->m) continue; // mode mismatch
		const struct input* const in = i2c->i;
		for (int s = 0; s < i->ili; ++s) {
			if (in[s].t != i->il[s].t) break;
			if (!memcmp(&in[s], &i->il[s], sizeof(struct input))) {
				i->mks[m] += 1;
			}
			else {
				i->mks[m] = 0;
				break;
			}
		}
	}

	int matches = 0; // number of matches
	size_t mi = 0; // (last) Match Index
	for (size_t m = 0; m < i->kml; ++m) {
		if (i->mks[m]) {
			matches += 1;
			mi = m;
		}
	}

	if (!matches) {
		memset(i->il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
		i->ili = 0;
		return CMD_NONE;
	}
	if (matches == 1 && !memcmp(i->il, i->kmap[mi].i,
				INPUT_LIST_LENGTH*sizeof(struct input))) {
		memset(i->il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
		i->ili = 0;
		return i->kmap[mi].c;
	}
	return CMD_NONE;
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
		if (*buftop != buf) return 0;
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
	i->prompt_cursor_pos = 0;
	ui_draw(i);
	int r;
	do {
		r = fill_textbox(i, t, &t_top, t_size, NULL);
		ui_draw(i); // TODO only redraw bottombar
	} while (r && r != -1);
	i->prompt = NULL;
	i->prompt_cursor_pos = -1;
	return r;
}

inline void failed(struct ui* const i, const char* const what,
		const char* const why) {
	i->mt = MSG_ERROR;
	snprintf(i->msg, MSG_BUFFER_SIZE, "%s failed: %s", what, why);
}

int spawn(char* const arg[]) {
	// TODO errors
	// TODO that global...
	int ret = 0, status = 0, nullfd;
	write(STDOUT_FILENO, CSI_CLEAR_ALL);
	if ((ret = stop_raw_mode(&global_i->T))) return ret;
	global_i->mt = MSG_INFO;
	strcpy(global_i->msg, "external program is running...");
	pid_t pid = fork();
	if (pid == 0) {
		nullfd = open("/dev/null", O_WRONLY, 0100);
		if (dup2(nullfd, STDERR_FILENO) == -1) ret = errno;
		/*
		 * TODO redirect to tmp file
		 * if not empty, offer to show it's contents in pager
		 */

		//if (dup2(nullfd, STDOUT_FILENO) == -1) ret = errno;
		// TODO ???
		close(nullfd);
		if (execve(arg[0], arg, environ)) ret = errno;
	}
	else {
		while (waitpid(pid, &status, 0) == -1);
	}
	global_i->msg[0] = 0;
	ret = start_raw_mode(&global_i->T);
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
