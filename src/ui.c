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

#include "include/ui.h"

/* This file contains UI-related functions
 * These functions are supposed to draw elements of UI.
 * They are supposed to read file_view contents, but never modify it.
 *
 * TODO KEY_RESIZE - better handling, less mess
 */

static enum theme_element mode2theme(const mode_t m, const mode_t n) {
	switch (m & S_IFMT) {
	case S_IFBLK: return THEME_ENTRY_BLK_UNS;
	case S_IFCHR: return THEME_ENTRY_CHR_UNS;
	case S_IFIFO: return THEME_ENTRY_FIFO_UNS;
	case S_IFREG:
		if (executable(m, 0)) return THEME_ENTRY_REG_EXE_UNS;
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

struct ui ui_init(struct file_view* const pv,
		struct file_view* const sv) {
	struct ui i;
	setlocale(LC_ALL, "");
	initscr();
	if (has_colors() == FALSE) {
		endwin();
		printf("no colors :(\n"); // TODO
		exit(EXIT_FAILURE);
	}
	start_color();
	noecho();
	//cbreak();
	//nonl();
	//raw();
	//intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	//timeout(DEFAULT_GETCH_TIMEOUT);
	curs_set(0);

	// TODO
	//init_color(COLOR_WHITE, 1000, 1000, 1000);
	//init_color(COLOR_MAGENTA, 1000, 0, 1000);
	//init_color(COLOR_YELLOW, 1000, 1000, 0);
	//init_color(COLOR_CYAN, 0, 1000, 1000);

	for (int i = THEME_OTHER+1; i < THEME_ELEM_NUM; ++i) {
		init_pair(i, theme_scheme[i][1], theme_scheme[i][2]);
	}

	i.fvs[0] = i.pv = pv;
	i.fvs[1] = i.sv = sv;
	for (int x = 0; x < 2; ++x) {
		WINDOW* tmpwin = newwin(1, 1, 0, 0);
		i.fvp[x] = new_panel(tmpwin);
	}
	i.scrw = i.scrh = 0;
	i.m = MODE_MANAGER;
	i.prch = ' ';
	i.prompt = NULL;
	i.mt = MSG_NONE;
	i.helpy = 0;
	i.run = true;
	i.ui_needs_refresh = true;
	i.ili = 0;
	memset(i.il, 0, INPUT_LIST_LENGTH*sizeof(struct input));
	i.kml = default_mapping_length;
	i.mks = calloc(default_mapping_length, sizeof(int));
	i.kmap = default_mapping; // TODO ???
	//i.kmap = malloc(default_mapping_length*sizeof(struct input2cmd));
	/*for (size_t k = 0; k < default_mapping_length; ++k) {
		memcpy(&i.kmap[k], &default_mapping[k], sizeof(struct input2cmd));
	}*/
	WINDOW* sw = newwin(1, 1, 0, 0);
	keypad(sw, TRUE);
	//wtimeout(hw, DEFAULT_GETCH_TIMEOUT);
	i.status = new_panel(sw);
	return i;
}

void ui_end(struct ui* const i) {
	for (int x = 0; x < 2; x++) {
		PANEL* p = i->fvp[x];
		WINDOW* w = panel_window(p);
		del_panel(p);
		delwin(w);
	}
	WINDOW* sw = panel_window(i->status);
	del_panel(i->status);
	delwin(sw);
	free(i->mks);
	endwin();
	memset(i, 0, sizeof(struct ui));
}
static void _printw_pathbar(const char* const path,
		WINDOW* const w, const fnum_t width) {
	// TODO
	const struct passwd* const pwd = getpwuid(geteuid());
	const int pi = prettify_path_i(path, pwd->pw_dir);
	const size_t path_width = utf8_width(path+pi) + (pi ? 1 : 0);
	wattron(w, COLOR_PAIR(THEME_PATHBAR));
	if (path_width <= width-2) {
		mvwprintw(w, 0, 0, "%s%s%*c ", (pi ? " ~" : " "),
				path+pi, width-(path_width+2), ' ');
	}
	else {
		const size_t sg = path_width - ((width-2) + (pi?1:0));
		const char* const p = path+pi+utf8_slice_length(path+pi, sg);
		mvwprintw(w, 0, 0, " %s ", p);
	}
	wattroff(w, COLOR_PAIR(THEME_PATHBAR));
}

static void _printw_entry(WINDOW* const w, const fnum_t dr,
		const struct file_record* const cfr,
		const fnum_t width, const bool highlight) {
	// TODO scroll filenames that are too long to fit in the panel width
	const enum theme_element te = mode2theme(cfr->s.st_mode,
			(cfr->l ? cfr->l->st_mode : 0)) + (highlight ? 1 : 0);
	char* invname = NULL;
	const bool valid = utf8_validate(cfr->file_name);
	const utf8* const fn = (valid ? cfr->file_name : invname);
	if (!valid) {
		invname = malloc(NAME_MAX+1);
		cut_non_ascii(cfr->file_name, invname, NAME_MAX);
	}

	char sbuf[SIZE_BUF_SIZE];
	size_t slen = 0;
	sbuf[0] = 0;
	if (S_ISREG(cfr->l->st_mode)) {
		pretty_size(cfr->l->st_size, sbuf);
		slen = strnlen(sbuf, SIZE_BUF_SIZE);
	}

	const size_t fnw = utf8_width(fn);
	size_t fn_slice = fnw;
	size_t space = 0;
	if (width < 1+fnw+1+slen+1) {
		fn_slice = width - (1+1+slen+1);
		space = 1;
	}
	else {
		space = width - (1+fnw+1+slen);
	}
	wattron(w, COLOR_PAIR(te));
	mvwprintw(w, dr, 0, "%c%.*s%*c%s ",
			theme_scheme[te][0],
			utf8_slice_length(fn, fn_slice), fn,
			space, ' ', sbuf);
	wattroff(w, COLOR_PAIR(te));
	if (!valid) free(invname);
}

/* - Max Entries = how many entries I need to fill
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

static void ui_draw_panel(struct ui* const i, const int v) {
	// TODO make readable
	const struct file_view* const s = i->fvs[v];
	const bool sh = s->show_hidden;
	const fnum_t nhf = i->fvs[v]->num_hidden;
	WINDOW* const w = panel_window(i->fvp[v]);

	int _ph = 0, _pw = 0;
	getmaxyx(w, _ph, _pw);
	if (_ph < 0 || _ph < 0) return; // these may be -1
	const fnum_t ph = _ph, pw = _pw;

	/* Top pathbar */
	_printw_pathbar(s->wd, w, pw);

	/* Entry list */
	const struct file_record* const hfr =
		(s->selection < s->num_files ?
		 s->file_list[s->selection] : NULL);

	fnum_t dr = 1; // Drawing Row
	fnum_t e = _start_search_index(s, nhf, ph - 3);
	while (s->num_files-nhf && e < s->num_files && dr < ph-1) {
		if (!visible(s, e)) {
			e += 1;
			continue;
		}
		const bool hl = (e == s->selection && i->fvs[v] == i->pv);
		_printw_entry(w, dr, s->file_list[e], pw, hl);
		dr += 1;
		e += 1;
	}
	for (; dr < ph-1; ++dr) {
		mvwprintw(w, dr, 0, "%*c", pw, ' ');
	}

	/* Statusbar */
	/* Padded on both sides with space */
	wattron(w, COLOR_PAIR(THEME_STATUSBAR));
	static const char* const timefmt = "%Y-%m-%d %H:%M:%S";
	const size_t time_size = 4+1+2+1+2+1+2+1+2+1+2+1;
	char time[time_size];
	time[0] = 0;

	const size_t status_size = 10+1+1+10+1+1+9+1;
	char* const status = malloc(status_size);

	if (hfr && hfr->l) {
		const time_t lt = hfr->l->st_mtim.tv_sec;
		const struct tm* const tt = localtime(&lt);
		strftime(time, time_size, timefmt, tt);
	}
	mode_t m = (hfr ? hfr->l->st_mode : 0);
	snprintf(status, status_size,
			"%uf %u%c %s%s%s",
			s->num_files-(sh ? 0 : nhf),
			nhf, (sh ? 'H' : 'h'),
			perm2rwx[(m>>6) & 07],
			perm2rwx[(m>>3) & 07],
			perm2rwx[(m>>0) & 07]);

	/* BTW chmod man page says chmod
	 * cannot change symlink permissions.
	 * ...but that is not and issue since
	 * symlinks permissions are never used.
	 */
	mvwprintw(w, dr, 0, " %s%*c%s ", status,
			pw-utf8_width(status)-strnlen(time, time_size)-2, ' ', time);
	wattroff(w, COLOR_PAIR(THEME_STATUSBAR));
	free(status);
	wrefresh(w);
}

void _printw_cmd_and_keyseqs(WINDOW* const w,
		const int dr, const enum command c,
		const struct input2cmd* const k[], const size_t ki) {
	// TODO
	size_t x = 0;
	size_t align = 0;
	const int maxsequences = INPUT_LIST_LENGTH-1;
	const size_t seqlen = 6; // TODO FIXME may not always be enough (for more complicated inputs)
	const size_t seqwid = 8;
	for (size_t s = 0; s < ki; ++s) {
		const struct input2cmd* const seq = k[s];
		size_t i = 0;
		align = 0;
		while (seq->i[i].t != END) {
			const struct input* const v = &seq->i[i];
			size_t wi;
			attron(A_BOLD);
			switch (v->t) {
			case UTF8:
				mvwprintw(w, dr, x, "%s", v->d.utf);
				wi = utf8_width(v->d.utf);
				x += wi;
				align += wi;
				break;
			case CTRL:
				mvwprintw(w, dr, x, "^%c", v->d.c);
				x += 2;
				align += 2;
				break;
			case SPECIAL:
				/* +4 skips "KEY_" in returned string */
				mvwprintw(w, dr, x, "%.*s",
						seqlen, keyname(v->d.c)+4);
				wi = strnlen(keyname(v->d.c)+4, seqlen);
				x += wi;
				align += wi;
				break;
			default: break;
			}
			attroff(A_BOLD);
			i += 1;
		}
		x += seqwid - align;
	}
	x += ((maxsequences-ki)*seqwid);
	mvwprintw(w, dr, x, "%s", cmd_help[c]);
}

void _find_all_keyseqs4cmd(const struct ui* const i, const enum command c,
		const enum mode m, const struct input2cmd* ic[], size_t* const ki) {
	*ki = 0;
	for (size_t k = 0; k < i->kml; ++k) {
		if (i->kmap[k].c != c || i->kmap[k].m != m) continue;
		ic[*ki] = &i->kmap[k];
		*ki += 1;
	}
}

static void ui_draw_help(struct ui* const i) {
	WINDOW* const hw = stdscr;
	wclear(hw);
	int dr = -i->helpy; // TODO
	wattron(hw, A_BOLD);
	int crl = 0; // Copytight Notice Line
	while (copyright_notice[crl]) {
		const char* const cr = copyright_notice[crl];
		mvwprintw(hw, dr, 0, "%s%*c", cr, i->scrw-utf8_width(cr), ' ');
		dr += 1;
		crl += 1;
	}
	mvwprintw(hw, dr, 0, "%*c", i->scrw, ' ');
    dr += 1;
	wattroff(hw, A_BOLD);
	for (size_t m = 0; m < MODE_NUM; ++m) {
		/* MODE TITLE */
		wattron(hw, A_BOLD);
		mvwprintw(hw, dr, 0, "%s%*c", mode_strings[m],
				i->scrw-strlen(mode_strings[m]), ' ');
		wattroff(hw, A_BOLD);
		dr += 1;

		/* LIST OF AVAILABLE KEYS */
		const struct input2cmd* k[4];
		size_t ki = 0;
		for (size_t c = CMD_NONE+1; c < CMD_NUM; ++c) {
			_find_all_keyseqs4cmd(i, c, m, k, &ki);
			if (ki) { // ^^^ may output empty array
				_printw_cmd_and_keyseqs(hw, dr, c, k, ki);
				dr += 1;
			}
		}
		/* EMPTY LINE PADDING */
		mvwprintw(hw, dr, 0, "%*c", i->scrw, ' ');
		dr += 1;
	}
	wrefresh(hw);
}

/*
 * TODO TODO TODO FIXME
 * TODO either remove hintbar
 * (rather not - still needed for errors and infos)
 * or use for info such as recent keystroke... dunno
 */
static void ui_draw_hintbar(struct ui* const i, WINDOW* const sw) {
	char last[32]; last[0] = 0;
	char tmp[16];
	for (int x = 0; x < i->ili && i->il[x].t != END; ++x) {
		switch (i->il[x].t) {
		case UTF8:
			snprintf(tmp, sizeof(tmp), "%s", i->il[x].d.utf);
			break;
		case SPECIAL:
			snprintf(tmp, sizeof(tmp), "%s", keyname(i->il[x].d.c)+4);
			break;
		case CTRL:
			snprintf(tmp, sizeof(tmp), "^%c", i->il[x].d.c);
			break;
		case END:
		default:
			break;
		}
		strncat(last, tmp, sizeof(tmp));
	}
	mvwprintw(sw, 0, 0, "%*c%s", i->scrw-strlen(last), ' ', last);
}

static void ui_draw_chmod(struct ui* const i) {
	WINDOW* cw;
	for (int p = 0; p < 2; ++p) {
		if (i->fvs[p] == i->sv) {
			cw = panel_window(i->fvp[p]);
		}
	}
	wclear(cw);
	mode_t m = i->perm;
	int dr = 1;

	int ph, pw;
	getmaxyx(cw, ph, pw); // TODO
	(void)(ph); // FIXME

	_printw_pathbar(i->path, cw, pw);
	static const char* txt[] = { "permissions: ", "owner: ", "group: " };
	mvwprintw(cw, dr++, 2, "%s%06o", txt[0], m,
			pw-(strlen(txt[0])+6), ' ');
	mvwprintw(cw, dr++, 2, "%s%s%*c", txt[1], i->owner,
			pw-(strlen(txt[1])+strnlen(i->owner, LOGIN_NAME_MAX)), ' ');
	mvwprintw(cw, dr++, 2, "%s%s%*c", txt[2], i->group,
			pw-(strlen(txt[2])+strnlen(i->group, LOGIN_NAME_MAX)), ' ');

	for (int b = 0; b < 12; ++b) {
		const char s = (m & (mode_t)1 ? 'x' : ' ');
		mvwprintw(cw, dr+11-b, 2, "[%c] %s", s, mode_bit_meaning[b]);
		m >>= 1;
	}
	wrefresh(cw);
}

void ui_draw(struct ui* const i) {
	if (i->m == MODE_HELP) {
		ui_draw_help(i);
	}
	else if (i->m == MODE_CHMOD) {
		ui_draw_chmod(i);
	}
	else if (i->m == MODE_MANAGER) {
		for (int v = 0; v < 2; ++v) {
			ui_draw_panel(i, v);
		}
	}
	WINDOW* sw = panel_window(i->status);
	// TODO
	if (i->mt) {
		int cp = 0;
		switch (i->mt) {
		case MSG_INFO: cp = THEME_INFO; break;
		case MSG_ERROR: cp = THEME_ERROR; break;
		default: break;
		}
		wattron(sw, COLOR_PAIR(cp));
		mvwprintw(sw, 0, 0, "%s", i->msg);
		wattroff(sw, COLOR_PAIR(cp));

		mvwprintw(sw, 0, strlen(i->msg), "%*c",
				i->scrw-utf8_width(i->msg), ' ');
		i->mt = MSG_NONE;
	}
	else if (i->prompt) { // TODO
		mvwprintw(sw, 0, 0, "%c%s%*c", i->prch, i->prompt,
				i->scrw-(utf8_width(i->prompt)+2), ' ');
	}
	else {
		ui_draw_hintbar(i, sw);
	}
	wrefresh(sw);

	update_panels();
	doupdate();
	refresh();
}

void ui_update_geometry(struct ui* const i) {
	getmaxyx(stdscr, i->scrh, i->scrw);
	const int w[2] = { i->scrw/2, i->scrw - w[0] };
	const int px[2] = { 0, w[0] };
	for (int x = 0; x < 2; ++x) {
		PANEL* p = i->fvp[x];
		WINDOW* ow = panel_window(p);
		wresize(ow, i->scrh-1, w[x]);
		move_panel(p, 0, px[x]);
	}

	WINDOW* sw = panel_window(i->status);
	wresize(sw, 1, i->scrw);
	move_panel(i->status, i->scrh-1, 0);

	i->ui_needs_refresh = false;
}

int chmod_open(struct ui* const i, utf8* const path) {
	struct stat s;
	if (stat(path, &s)) return errno;
	errno = 0;
	struct passwd* pwd = getpwuid(s.st_uid);
	if (!pwd) return errno;
	struct group* grp = getgrgid(s.st_gid);
	if (!grp) return errno;

	i->o = s.st_uid;
	i->g = s.st_gid;
	i->m = MODE_CHMOD;
	i->path = path;
	i->perm = s.st_mode;
	strncpy(i->owner, pwd->pw_name, LOGIN_NAME_MAX);
	strncpy(i->group, grp->gr_name, LOGIN_NAME_MAX);
	i->ui_needs_refresh = true;
	return 0;
}

void chmod_close(struct ui* const i) {
	i->m = MODE_MANAGER;
	free(i->path);
	i->path = NULL;
}

/*
 * Unused bytes of union input_data are zeroed,
 * so comparing two inputs via memcmp is possible.
 */
struct input get_input(WINDOW* const w) {
	struct input r;
	memset(&r, 0, sizeof(struct input));
	size_t utflen = 0;
	int init = wgetch(w);
	const utf8 u = (utf8)init;
	const char* kn = keyname(init);
	if (init == -1) {
		r.t = END;
	}
	else if (strlen(kn) == 2 && kn[0] == '^') {
		r.t = CTRL;
		r.d.c = kn[1];
	}
	else if (has_key(init)) {
		r.t = SPECIAL;
		r.d.c = init;
	}
	else if ((utflen = utf8_g2nb(&u))) {
		r.t = UTF8;
		r.d.utf[0] = u;
		for (size_t i = 1; i < utflen; ++i) {
			r.d.utf[i] = (utf8)wgetch(w);
		}
	}
	else {
		if (init == KEY_RESIZE) {
			r.t = SPECIAL;
			r.d.c = init;
		}
		else r.t = END;
	}
	return r;
}

/* Find matching mappings
 *
 * If input is ESC or no matches,
 * clear ili, and hint/matching table.
 * If there are a few, do nothing, wait longer.
 * If there is only one, send it.
 *
 * HANDLES KEY_RESIZE // FIXME somewhere else?
 */
enum command get_cmd(struct ui* const i) {
	memset(i->mks, 0, i->kml*sizeof(int));
	struct input newinput = get_input(stdscr);
	if (newinput.t == SPECIAL && newinput.d.c == KEY_RESIZE) {
		ui_update_geometry(i); // TODO TODO FIXME maybe not here
		ui_draw(i);
		memset(i->il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
		i->ili = 0;
		return CMD_NONE;
	}

	if (newinput.t == END) return CMD_NONE;
	else if (newinput.t == CTRL && newinput.d.c == '[') { // ESC
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
		memset(i->mks, 0, i->kml*sizeof(int));
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
			return i->kmap[mi].c;
		}
	}
	return CMD_NONE;
}

/* Gets input to buffer
 * Responsible for cursor movement (buftop) and guarding buffer bounds (bsize)
 * If text is ready (enter pressed) returns 0,
 * If aborted, returns -1.
 * If keeps gathering, returns 1.
 * Additionally:
 * returns 2 on ^N and -2 on ^P
 * returns -3 if KEY_RESIZE was 'pressed'
 */
int fill_textbox(utf8* const buf, utf8** const buftop,
		const size_t bsize, WINDOW* const w) {
	curs_set(2);
	wmove(w, 0, utf8_width(buf)-utf8_width(*buftop)+1);
	wrefresh(w);
	struct input i = get_input(w);
	curs_set(0);
	if (i.t == SPECIAL && i.d.c == KEY_RESIZE) return -3;
	if (i.t == END) return 1;
	if (i.t == CTRL && i.d.c == '[') return -1;
	else if (i.t == CTRL && i.d.c == 'N') return 2;
	else if (i.t == CTRL && i.d.c == 'P') return -2;
	else if ((i.t == CTRL && i.d.c == 'J') ||
	         (i.t == UTF8 &&
	         (i.d.utf[0] == '\n' || i.d.utf[0] == '\r'))) {
		if (*buftop != buf) return 0;
	}
	else if (i.t == UTF8 && (size_t)(*buftop - buf) < bsize) {
		utf8_insert(buf, i.d.utf, utf8_ng_till(buf, *buftop));
		*buftop += utf8_g2nb(i.d.utf);
	}
	else if ((i.t == CTRL && i.d.c == 'H') ||
	         (i.t == SPECIAL && i.d.c == KEY_BACKSPACE)) {
		if (*buftop != buf) {
			const size_t before = strnlen(buf, bsize);
			utf8_remove(buf, utf8_ng_till(buf, *buftop)-1);
			*buftop -= before - strnlen(buf, bsize);
		}
		else if (strnlen(buf, bsize));
 		// Exit only when buf is empty
		else return -1;
	}
	else if ((i.t == CTRL && i.d.c == 'D') ||
	         (i.t == SPECIAL && i.d.c == KEY_DC)) {
		utf8_remove(buf, utf8_ng_till(buf, *buftop));
	}
	else if ((i.t == CTRL && i.d.c == 'A') ||
	         (i.t == SPECIAL && i.d.c == KEY_HOME)) {
		*buftop = buf;
	}
	else if ((i.t == CTRL && i.d.c == 'E') ||
	         (i.t == SPECIAL && i.d.c == KEY_END)) {
		*buftop = buf+strnlen(buf, bsize);
	}
	else if (i.t == CTRL && i.d.c == 'U') {
		*buftop = buf;
		memset(buf, 0, bsize);
	}
	else if (i.t == CTRL && i.d.c == 'K') {
		const size_t clen = strnlen(buf, bsize);
		memset(*buftop, 0, strnlen(*buftop, bsize-clen));
	}
	else if ((i.t == CTRL && i.d.c == 'F') ||
	         (i.t == SPECIAL && i.d.c == KEY_RIGHT)) {
		if ((size_t)(*buftop - buf) < bsize) {
			*buftop += utf8_g2nb(*buftop);
		}
	}
	else if ((i.t == CTRL && i.d.c == 'B') ||
	         (i.t == SPECIAL && i.d.c == KEY_LEFT)) {
		if (*buftop - buf) {
			const size_t gt = utf8_ng_till(buf, *buftop);
			*buftop = buf;
			for (size_t i = 0; i < gt-1; ++i) {
				*buftop += utf8_g2nb(*buftop);
			}
		}
	}
	return 1;
}
