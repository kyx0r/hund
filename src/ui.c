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

/*
 * UI TODO NOTES
 * 1. TODO selection is not very visible
 * 2. TODO scroll too long filenames
 * 3. Change panel split with < >
 */

static enum theme_element mode2theme(const mode_t m, const mode_t n) {
	switch (m & S_IFMT) {
	case S_IFBLK: return THEME_ENTRY_BLK_UNS;
	case S_IFCHR: return THEME_ENTRY_CHR_UNS;
	case S_IFIFO: return THEME_ENTRY_FIFO_UNS;
	case S_IFREG:
		if (EXECUTABLE(m, 0)) return THEME_ENTRY_REG_EXE_UNS;
		return THEME_ENTRY_REG_UNS;
	case S_IFDIR: return THEME_ENTRY_DIR_UNS;
	case S_IFSOCK: return THEME_ENTRY_SOCK_UNS;
	case S_IFLNK:
		switch (n & S_IFMT) {
		case S_IFBLK:
		case S_IFCHR:
		case S_IFIFO:
		case S_IFREG:
		case S_IFSOCK:
		default: return THEME_ENTRY_LNK_OTH_UNS;
		case S_IFDIR: return THEME_ENTRY_LNK_DIR_UNS;
		}
	default: return THEME_ENTRY_REG_UNS;
	}
}

struct ui* I;
static void handle_winch(int sig) {
	if (sig != SIGWINCH) return;
	write(STDOUT_FILENO, CSI_CLEAR_ALL);
	ui_update_geometry(I);
	ui_draw(I);
}

void ui_init(struct ui* const i, struct file_view* const pv,
		struct file_view* const sv) {
	setlocale(LC_ALL, "");
	i->fvs[0] = i->pv = pv;
	i->fvs[1] = i->sv = sv;
	i->scrw = i->scrh = 0;
	i->m = MODE_MANAGER;
	i->prch = ' ';
	i->prompt = NULL;
	i->prompt_cursor_pos = -1;
	i->timeout = -1;
	i->mt = MSG_NONE;
	i->helpy = 0;
	i->run = i->ui_needs_refresh = true;
	i->ili = 0;
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

	I = i;
	int err;
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = handle_winch;
	if ((err = start_raw_mode(&i->T))
	   || (err = sigaction(SIGWINCH, &sa, NULL))) {
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
	const char* const wd[2] = {
		i->fvs[0]->wd,
		i->fvs[1]->wd
	};
	const int wdw[2] = {
		utf8_width(wd[0]),
		utf8_width(wd[1]),
	};
	const int wdl[2] = {
		strnlen(wd[0], PATH_MAX),
		strnlen(wd[1], PATH_MAX),
	};
	append(&i->B, CSI_CLEAR_LINE);
	append_theme(&i->B, THEME_PATHBAR);
	for (size_t p = 0; p < 2; ++p) {
		append(&i->B, " ", 1);
		if (wdw[p] <= i->pw[p]-2) {
			append(&i->B, wd[p], wdl[p]);
			fill(&i->B, ' ', i->pw[p]-wdw[p]-1);
		}
		else {
			int sg = wdw[p] - i->pw[p]-2;
			const size_t P = utf8_w2nb(wd[p], sg);
			append(&i->B, wd[p], P);
			append(&i->B, " ", 1);
		}
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
	const enum theme_element fsym = mode2theme(cfr->s.st_mode,
			(cfr->l ? cfr->l->st_mode : 0));
	// THeme ELement
	const enum theme_element thel = fsym + (hl ? 1 : 0);

	char name[NAME_MAX+1];
	cut_unwanted(cfr->file_name, name, '.', NAME_MAX+1);

	char sbuf[SIZE_BUF_SIZE];
	size_t slen = 0;
	sbuf[0] = 0;

	if (cfr->l) { // TODO signal broken symlink
		if (S_ISREG(cfr->l->st_mode)) {
			pretty_size(cfr->l->st_size, sbuf);
			slen = strnlen(sbuf, SIZE_BUF_SIZE);
		}
		else if (S_ISDIR(cfr->l->st_mode) && cfr->dir_volume != -1) {
			pretty_size(cfr->dir_volume, sbuf);
			slen = strnlen(sbuf, SIZE_BUF_SIZE);
		}
	}

	const size_t name_allowed = width - (1+SIZE_BUF_SIZE+1);
	const size_t name_width = utf8_width(name);
	const size_t name_draw = (name_width < name_allowed ? name_width : name_allowed);
	const size_t space = name_allowed - name_draw;

	char open = file_symbols[fsym];
	char close = ' ';
	if (cfr->selected || (hl && !fv->num_selected)) {
		open = '[';
		close = ']';
	}

	append_theme(&i->B, thel);
	append(&i->B, &open, 1);
	append(&i->B, name, utf8_w2nb(name, name_draw));
	fill(&i->B, ' ', space);
	fill(&i->B, ' ', SIZE_BUF_SIZE-slen);
	append(&i->B, sbuf, slen);
	append(&i->B, &close, 1);
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
static void stringify_pug(mode_t m, uid_t u, gid_t g,
		char perms[10],
		char user[LOGIN_NAME_MAX+1],
		char group[LOGIN_NAME_MAX+1]) {
	perms[0] = 0;
	user[0] = 0;
	group[0] = 0;
	const struct passwd* const pwd = getpwuid(u);
	const struct group* const grp = getgrgid(g);

	if (pwd) strncpy(user, pwd->pw_name, LOGIN_NAME_MAX+1);
	else snprintf(user, LOGIN_NAME_MAX+1, "uid:%u", u);

	if (grp) strncpy(group, grp->gr_name, LOGIN_NAME_MAX+1);
	else snprintf(group, LOGIN_NAME_MAX+1, "gid:%u", g);

	switch (m & S_IFMT) {
	case S_IFBLK: perms[0] = 'b'; break;
	case S_IFCHR: perms[0] = 'c'; break;
	case S_IFDIR: perms[0] = 'd'; break;
	case S_IFIFO: perms[0] = 'p'; break;
	case S_IFLNK: perms[0] = 'l'; break;
	case S_IFREG: perms[0] = '-'; break;
	case S_IFSOCK: perms[0] = 's'; break;
	default: perms[0] = '-'; break;
	}
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
	append_theme(&i->B, THEME_STATUSBAR);
	fill(&i->B, ' ', 1);
	const struct file_record* const _hfr = hfr(i->pv);
	if (_hfr) {
		const time_t lt = _hfr->s.st_mtim.tv_sec;
		const struct tm* const tt = localtime(&lt);
		strftime(i->time, TIME_SIZE, timefmt, tt);
	}
	else {
		strcpy(i->time, "0000-00-00 00:00:00");
	}
	char status[10+2+10+2+10+1+1];
	const fnum_t nhf = (i->pv->show_hidden ? 0 : i->pv->num_hidden);
	int n = snprintf(status, sizeof(status), "%uf %u%c %us",
			i->pv->num_files-nhf, i->pv->num_hidden,
			(i->pv->show_hidden ? 'H' : 'h'), i->pv->num_selected);

	const size_t cw = utf8_width(status);
	const size_t uw = utf8_width(i->user);
	const size_t gw = utf8_width(i->group);
	const size_t sw = uw+1+gw+1+10+1+TIME_SIZE+1;
	const size_t padding = i->scrw-cw-sw;

	append(&i->B, status, n);
	fill(&i->B, ' ', padding);
	append(&i->B, i->user, strnlen(i->user, LOGIN_NAME_MAX));
	fill(&i->B, ' ', 1);
	append(&i->B, i->group, strnlen(i->group, LOGIN_NAME_MAX));
	fill(&i->B, ' ', 1);
	append(&i->B, &i->perms[0], 1);
	for (size_t p = 1; p < 10; ++p) {
		const mode_t m[2] = {
			(i->perm[0] & 0777) & (0400 >> (p-1)),
			(i->perm[1] & 0777) & (0400 >> (p-1))
		};
		const bool diff = m[0] != m[1];
		if (diff) append_attr(&i->B, ATTR_UNDERLINE, NULL);
		append(&i->B, &i->perms[p], 1);
		if (diff) append_attr(&i->B, ATTR_NOT_UNDERLINE, NULL);
	}
	fill(&i->B, ' ', 1);
	append(&i->B, i->time, strlen(i->time));
	fill(&i->B, ' ', 1);
	append_attr(&i->B, ATTR_NORMAL, NULL);
	append(&i->B, "\r\n", 3);
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
		for (int p = 0; p < 2; ++p) {
			while (e[p] < i->fvs[p]->num_files
					&& !visible(i->fvs[p], e[p])) {
				e[p] += 1;
			}
			if (e[p] >= i->fvs[p]->num_files) {
				fill(&i->B, ' ', i->pw[p]);
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
		fill(&i->B, i->prch, 1);
		append(&i->B, i->prompt, strlen(i->prompt));
		const int padding = i->scrw-utf8_width(i->prompt)-1;
		if (padding > 0) {
			fill(&i->B, ' ', padding);
		}
	}
	else if (i->m == MODE_CHMOD) {
		append(&i->B, "-- CHMOD --", 11);
		fill(&i->B, ' ', i->scrw-11);
	}
}

void ui_draw(struct ui* const i) {
	i->B.top = 0;
	append(&i->B, CSI_CLEAR_ALL);
	append(&i->B, CSI_CURSOR_HIDE);
	append(&i->B, CSI_CURSOR_TOP_LEFT);
	if (i->m == MODE_HELP) {
		_help(i);
	}
	else if (i->m == MODE_MANAGER || i->m == MODE_WAIT) { // TODO mode wait
		const struct file_record* const _hfr = hfr(i->pv);
		if (_hfr) {
			stringify_pug(_hfr->s.st_mode, _hfr->s.st_uid,
					_hfr->s.st_gid, i->perms, i->user, i->group);
		}
		else {
			memset(i->perms, '-', 10);
			strcpy(i->user, "-");
			strcpy(i->group, "-");
			// TODO maybe better just display nothing?
			// but then _statusbar() must be modified
		}
		_pathbars(i);
		_panels(i);
		_statusbar(i);
		_bottombar(i);
	}
	else if (i->m == MODE_CHMOD) {
		stringify_pug(i->perm[1], i->o[1], i->g[1],
				i->perms, i->user, i->group);
		_pathbars(i);
		_panels(i);
		_statusbar(i);
		_bottombar(i);
	}
	write(STDOUT_FILENO, i->B.buf, i->B.top);
	if (i->prompt && i->prompt_cursor_pos >= 0) {
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
	strncpy(i->user, pwd->pw_name, LOGIN_NAME_MAX+1);
	strncpy(i->group, grp->gr_name, LOGIN_NAME_MAX+1);
	i->ui_needs_refresh = true;
	return 0;
}

void chmod_close(struct ui* const i) {
	i->m = MODE_MANAGER;
	free(i->path);
	i->path = NULL;
	memset(i->perm, 0, sizeof(i->perm));
	memset(i->o, 0, sizeof(i->o));
	memset(i->g, 0, sizeof(i->g));
}

int ui_select(struct ui* const i, const char* const q,
		const struct select_option* o, const size_t oc) {
	int T = 0;
	char P[500]; // TODO
	i->prch = ' ';
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
				return j;
			}
		}
	}
	//return 0; // unreachable
}

/*
 * Find matching mappings
 *
 * If input is ESC or no matches,
 * clear ili, and hint/matching table.
 * If there are a few, do nothing, wait longer.
 * If there is only one, send it.
 */
enum command get_cmd(struct ui* const i) {
	memset(i->mks, 0, i->kml*sizeof(unsigned short));
	struct input newinput = get_input(i->timeout);
	if (newinput.t == I_NONE) return CMD_NONE;
	else if (newinput.t == I_ESCAPE || IS_CTRL(newinput, '[')) {
		memset(i->il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
		i->ili = 0;
		return CMD_NONE;
	}
	i->il[i->ili] = newinput;
	i->ili += 1;

	for (size_t m = 0; m < i->kml; ++m) {
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
		i->ili = 0;
		memset(i->il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
		memset(i->mks, 0, i->kml*sizeof(unsigned short));
		return CMD_NONE;
	}
	else if (matches == 1) {
		const bool fullmatch = !memcmp(i->il,
				i->kmap[mi].i, INPUT_LIST_LENGTH*sizeof(struct input));
		if (fullmatch) {
			memset(i->il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
			i->ili = 0;
			// Not clearing mks on full match to preserve this information for hints
			// Instead, mks is cleared when entered get_cmd()
			// TODO
			return i->kmap[mi].c;
		}
	}
	else if (i->ili >= INPUT_LIST_LENGTH) {
		// TODO test it. ili would sometimes exceed INPUT_LIST_LENGTH
		i->ili = 0;
		memset(i->il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
		memset(i->mks, 0, i->kml*sizeof(unsigned short));
	}
	return CMD_NONE;
}

/*
 * Gets input to buffer
 * Responsible for cursor movement (buftop) and guarding buffer bounds (bsize)
 * If text is ready (enter pressed) returns 0,
 * If aborted, returns -1.
 * If keeps gathering, returns 1.
 * Additionally:
 * returns 2 on ^N and -2 on ^P
 */
int fill_textbox(struct ui* const I,
		char* const buf, char** const buftop, const size_t bsize) {
	struct input i = get_input(I->timeout);
	if (i.t == I_NONE) return 1;
	if (IS_CTRL(i, '[')) return -1;
	else if (IS_CTRL(i, 'N')) return 2;
	else if (IS_CTRL(i, 'P')) return -2;
	else if ((IS_CTRL(i, 'J') || IS_CTRL(i, 'M')) ||
	         (i.t == I_UTF8 &&
	         (i.utf[0] == '\n' || i.utf[0] == '\r'))) {
		if (*buftop != buf) return 0;
	}
	else if (i.t == I_UTF8 && (size_t)(*buftop - buf) < bsize) {
		utf8_insert(buf, i.utf, utf8_wtill(buf, *buftop));
		*buftop += utf8_g2nb(i.utf);
	}
	else if (IS_CTRL(i, 'H') || i.t == I_BACKSPACE) {
		if (*buftop != buf) {
			const size_t before = strnlen(buf, bsize);
			utf8_remove(buf, utf8_wtill(buf, *buftop)-1);
			*buftop -= before - strnlen(buf, bsize);
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
	return 1;
}

int prompt(struct ui* const i, char* const t,
		char* t_top, const size_t t_size) {
	i->prch = '>';
	i->prompt = t;
	i->prompt_cursor_pos = 0;
	ui_draw(i);
	int r;
	do {
		r = fill_textbox(i, t, &t_top, t_size);
		ui_draw(i); // TODO only redraw bottombar
	} while (r && r != -1);
	i->prompt = NULL;
	i->prompt_cursor_pos = -1;
	return r;
}

void failed(struct ui* const i, const char* const f,
		const int reason, const char* const custom) {
	i->mt = MSG_ERROR;
	if (custom) {
		snprintf(i->msg, MSG_BUFFER_SIZE, "%s failed: %s", f, custom);
	}
	else {
		snprintf(i->msg, MSG_BUFFER_SIZE, "%s failed: %s (%d)",
				f, strerror(reason), reason);
	}
}

int spawn(char* const arg[]) {
	// TODO errors
	// TODO that global...
	int ret = 0, status = 0, nullfd;
	write(STDOUT_FILENO, CSI_CLEAR_ALL);
	if ((ret = stop_raw_mode(&I->T))) return ret;
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
	ret = start_raw_mode(&I->T);
	ui_draw(I);
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
