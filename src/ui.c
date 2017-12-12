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

/* get cmd2help coresponding to given command */
// TODO FIXME
static const struct cmd2help* get_help_data(const enum command c) {
	for (size_t i = 0; i < CMD_NUM && i < cmd_help_length; ++i) {
		if (cmd_help[i].c == c) return &cmd_help[i];
	}
	return NULL;
}

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
	i.error[0] = i.info[0] = 0;
	i.helpy = 0;
	i.help = NULL;
	i.run = true;
	i.ui_needs_refresh = true;
	i.kml = default_mapping_length;
	i.mks = calloc(default_mapping_length, sizeof(int));
	i.kmap = malloc(default_mapping_length*sizeof(struct input2cmd));
	// TODO
	for (size_t k = 0; k < default_mapping_length; ++k) {
		i.kmap[k].m = default_mapping[k].m;
		i.kmap[k].c = default_mapping[k].c;
		for (int il = 0; il < INPUT_LIST_LENGTH; ++il) {
			i.kmap[k].i[il].t = default_mapping[k].i[il].t;
			switch (i.kmap[k].i[il].t) {
			case END: break;
			case UTF8:
				strncpy(i.kmap[k].i[il].utf, default_mapping[k].i[il].utf, 5);
				break;
			case SPECIAL:
				i.kmap[k].i[il].c = default_mapping[k].i[il].c;
				break;
			case CTRL:
				i.kmap[k].i[il].ctrl = default_mapping[k].i[il].ctrl;
				break;
			}
		}
	}
	WINDOW* sw = newwin(1, 1, 0, 0);
	keypad(sw, TRUE);
	//wtimeout(hw, DEFAULT_GETCH_TIMEOUT);
	i.status = new_panel(sw);
	return i;
}

/* Delete panel first, THEN it's window.
 * If you resize the terminal window at least once and then quit (q)
 * then the program will hang in infinite loop
 * (I attached to it via gdb and it was stuck either on del_panel or poll.)
 * or segfault/double free/corrupted unsorted chunks.
 */
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
}
static void _printw_pathbar(const char* const path,
		WINDOW* const w, const fnum_t width) {
	// TODO
	const struct passwd* const pwd = get_pwd();
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
	static const char* corrupted = "(error)"; // TODO display errno or something
	const enum theme_element te = mode2theme(cfr->s.st_mode,
			(cfr->l ? cfr->l->st_mode : 0)) + (highlight ? 1 : 0);
	const size_t bufl = width*4;
	char* const buf = malloc(bufl);
	memset(buf, ' ', bufl);
	buf[bufl-1] = 0;
	buf[strnlen(buf, bufl)] = ' ';

	wattron(w, COLOR_PAIR(te));
	fnum_t space = width;
	fnum_t begin = 0;

	char invname[NAME_MAX+1];
	const bool valid = utf8_validate(cfr->file_name);
	const utf8* const fn = (valid ? cfr->file_name : invname);
	const utf8* const lp = (cfr->l ? cfr->link_path : corrupted);
	if (!valid) cut_non_ascii(cfr->file_name, invname, NAME_MAX);

	const size_t fnw = utf8_width(fn);
	const size_t printfn = (space > fnw ? fnw : space);
	mvwprintw(w, dr, begin, "%c%.*s",
			theme_scheme[te][0], utf8_slice_length(fn, printfn), fn);
	space -= printfn+1;
	begin += printfn+1;

	if (!highlight) {
		if (cfr->l) wattron(w, COLOR_PAIR(THEME_ENTRY_LNK_PATH));
		else wattron(w, COLOR_PAIR(THEME_ENTRY_LNK_PATH_INV));
	}
	if ((cfr->s.st_mode & S_IFMT) == S_IFLNK) {
		const size_t lpw = utf8_width(lp);
		const size_t printlp = (space > 4+lpw ? 4+lpw : space);
		mvwprintw(w, dr, begin, " -> %.*s",
				utf8_slice_length(lp, printlp), lp);
		space -= printlp;
		begin += printlp;
	}
	if (!highlight) {
		if (cfr->l) wattroff(w, COLOR_PAIR(THEME_ENTRY_LNK_PATH));
		else wattron(w, COLOR_PAIR(THEME_ENTRY_LNK_PATH_INV));
	}
	mvwprintw(w, dr, begin, "%*c", space, ' ');
	wattroff(w, COLOR_PAIR(te));
	free(buf);
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
	const size_t status_size = pw*4; // TODO
	char* const status = malloc(status_size);
	static const char* const timefmt = "%Y-%m-%d %H:%M";
	const size_t time_size = 4+1+2+1+2+1+2+1+2+1;
	char time[time_size]; // TODO
	memset(time, 0, time_size);

	off_t fsize = 0;
	if (hfr && hfr->l) {
		fsize = (s->selection < s->num_files ? hfr->l->st_size : 0);
		const time_t lt = hfr->l->st_mtim.tv_sec;
		const struct tm* const tt = localtime(&lt);
		strftime(time, time_size, timefmt, tt);
	}
	char sbuf[SIZE_BUF_SIZE];
	pretty_size(fsize, sbuf);
	// TODO FIXME does not always fit 80x24
	snprintf(status, status_size,
			"%uf %u%c %03o %s",
			s->num_files-(sh ? 0 : nhf),
			nhf, (sh ? 'H' : 'h'),
			(hfr && hfr->l ? (hfr->l->st_mode & 0xfff) : 0),
			sbuf);
	/* BTW chmod man page says chmod
	 * cannot change symlink permissions.
	 * ...but that is not and issue since
	 * symlinks permissions are never used.
	 */

	// TODO glitches when padding is <0 (screen too small)
	mvwprintw(w, dr, 0, " %s%*c%s ", status,
			pw-utf8_width(status)-strnlen(time, time_size)-2, ' ', time);
	wattroff(w, COLOR_PAIR(THEME_STATUSBAR));
	free(status);
	wrefresh(w);
}

static void ui_draw_help(struct ui* const i) {
	// TODO
	WINDOW* const hw = panel_window(i->help);
	const int hheight = i->scrh-1;
	const int lines = 4*2 + cmd_help_length - 1 + 5; // TODO
	int dr = -i->helpy;
	if (dr + lines < hheight) {
		dr = hheight - lines;
		i->helpy = -dr;
	}
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
		wattron(hw, A_BOLD);
		switch (m) {
		case MODE_HELP:
			mvwprintw(hw, dr, 0, "HELP%*c", i->scrw-4, ' ');
			break;
		case MODE_CHMOD:
			mvwprintw(hw, dr, 0, "CHMOD%*c", i->scrw-5, ' ');
			break;
		case MODE_MANAGER:
			mvwprintw(hw, dr, 0, "FILE VIEW%*c", i->scrw-9, ' ');
			break;
		case MODE_WAIT:
			mvwprintw(hw, dr, 0, "WAIT%*c", i->scrw-4, ' ');
			break;
		default: continue;
		}
		wattroff(hw, A_BOLD);
		dr += 1;
		for (size_t c = 1; c < CMD_NUM; ++c) { // TODO redo
			size_t w = 0;
			int last = -1;
			for (size_t k = 0; k < i->kml; ++k) {
				if (i->kmap[k].c != c || i->kmap[k].m != m) continue;
				int ks = 0;
				last = k;
				wattron(hw, COLOR_PAIR(THEME_HINT_KEY));
				wattron(hw, A_BOLD);
				int ww = 0;
				while (i->kmap[k].i[ks].t != END) {
					switch (i->kmap[k].i[ks].t) {
					case UTF8:
						mvwprintw(hw, dr, w, "%s", i->kmap[k].i[ks].utf);
						w += utf8_width(i->kmap[k].i[ks].utf);
						ww += utf8_width(i->kmap[k].i[ks].utf);
						break;
					case CTRL:
						mvwprintw(hw, dr, w, "^%c", i->kmap[k].i[ks].ctrl);
						w += 2;
						ww += 2;
						break;
					case SPECIAL:
						mvwprintw(hw, dr, w, "%s",
								keyname(i->kmap[k].i[ks].c)+4);
						w += strlen(keyname(i->kmap[k].i[ks].c)+4);
						ww += strlen(keyname(i->kmap[k].i[ks].c)+4);
						break;
					default: break;
					}
					ks += 1;
				}
				wattroff(hw, COLOR_PAIR(THEME_HINT_KEY));
				wattroff(hw, A_BOLD);
				const int tab = INPUT_LIST_LENGTH+2;
				mvwprintw(hw, dr, w, "%*c", tab-ww, ' ');
				w += tab-ww;
			}
			if (last != -1) { // -1 suggests that there was no matching command
				const int tab = (INPUT_LIST_LENGTH+2)*(INPUT_LIST_LENGTH-1);
				utf8* help = get_help_data(i->kmap[last].c)->help;
				mvwprintw(hw, dr, w, "%*c%s%*c",
						tab-w+1, ' ', help,
						i->scrw-w-strlen(help), ' ');
				dr += 1;
			}
		}
		mvwprintw(hw, dr, 0, "%*c", i->scrw, ' ');
		dr += 1;
	}
	wrefresh(hw);
}

static void ui_draw_hintbar(struct ui* const i, WINDOW* const sw) {
	mvwprintw(sw, 0, 0, "%*c", i->scrw-1, ' ');
	wmove(sw, 0, 0);
	for (size_t x = 0; x < i->kml; ++x) { // TODO redo
		if (!i->mks[x]) continue;
		wprintw(sw, " ");
		const struct input* const ins = i->kmap[x].i;
		int c = 0;
		wattron(sw, COLOR_PAIR(THEME_HINT_KEY));
		while (ins[c].t != END && c < INPUT_LIST_LENGTH) {
			switch (ins[c].t) {
			case END: break;
			case UTF8:
				wprintw(sw, "%s", ins[c].utf);
				break;
			case SPECIAL:
				wprintw(sw, "%s", keyname(ins[c].c)+4);
				break;
			case CTRL:
				wprintw(sw, "^%c", ins[c].ctrl);
				break;
			default:
				break;
			}
			c += 1;
		}
		wattroff(sw, COLOR_PAIR(THEME_HINT_KEY));
		wattron(sw, COLOR_PAIR(THEME_HINT_DESC));
		const struct cmd2help* ch = get_help_data(i->kmap[x].c);
		wprintw(sw, "%s", (ch ? ch->hint : "(no hint)"));
		wattroff(sw, COLOR_PAIR(THEME_HINT_DESC));
	}
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
	(void)(ph);

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
		mvwprintw(cw, dr++, 2, "[%c] %s", s, mode_bit_meaning[b]);
		m >>= 1;
	}
	wrefresh(cw);
}

void ui_draw(struct ui* const i) {
	switch (i->m) {
	case MODE_HELP:
		ui_draw_help(i);
		break;
	case MODE_CHMOD:
		ui_draw_chmod(i);
		break;
	case MODE_MANAGER:
		for (int v = 0; v < 2; ++v) {
			ui_draw_panel(i, v);
		}
		break;
	default:
		break;
	}

	WINDOW* sw = panel_window(i->status);
	if (i->error[0]) { // TODO
		wattron(sw, COLOR_PAIR(THEME_ERROR));
		mvwprintw(sw, 0, 0, "%s", i->error);
		wattroff(sw, COLOR_PAIR(THEME_ERROR));
		mvwprintw(sw, 0, strlen(i->error), "%*c",
				i->scrw-utf8_width(i->error), ' ');
		i->error[0] = 0;
	}
	else if (i->info[0]) { // TODO
		wattron(sw, COLOR_PAIR(THEME_INFO));
		mvwprintw(sw, 0, 0, "%s", i->info);
		wattroff(sw, COLOR_PAIR(THEME_INFO));
		mvwprintw(sw, 0, strlen(i->info), "%*c",
				i->scrw-utf8_width(i->info), ' ');
		i->info[0] = 0;
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

/* Sometimes it's better to create a new window
 * instead of resizing existing one.
 * If you just resize window (to bigger size),
 * borders stay in buffer and the only thing
 * you can do about them is explicitly overwrite them by
 * mvwprintw or something.
 * I do overwrite all buffer with spaces, so I dont have to.
 * ...but if you do, delete old window AFTER you assign new one to panel.
 */
void ui_update_geometry(struct ui* const i) {
	int newscrh, newscrw;
	getmaxyx(stdscr, newscrh, newscrw);
	//syslog(LOG_DEBUG, "resized to %ux%u", newscrw, newscrh);
	i->scrh = newscrh;
	i->scrw = newscrw;
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

	if (i->help) {
		WINDOW* hw = panel_window(i->help);
		PANEL* hp = i->help;
		wresize(hw, i->scrh-1, i->scrw);
		move_panel(hp, 0, 0);
	}
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
	i->mb = i->m;
	i->m = MODE_CHMOD;
	i->path = path;
	i->perm = s.st_mode;
	strncpy(i->owner, pwd->pw_name, LOGIN_NAME_MAX);
	strncpy(i->group, grp->gr_name, LOGIN_NAME_MAX);
	i->ui_needs_refresh = true;
	return 0;
}

void chmod_close(struct ui* const i) {
	i->m = i->mb;
	free(i->path);
	i->path = NULL;
}

void help_open(struct ui* const i) {
	WINDOW* nw = newwin(1, 1, 0, 0);
	i->help = new_panel(nw);
	i->ui_needs_refresh = true;
	i->m = MODE_HELP;
}

void help_close(struct ui* const i) {
	WINDOW* hw = panel_window(i->help);
	del_panel(i->help);
	delwin(hw);
	i->help = NULL;
	i->m = MODE_MANAGER;
}

struct input get_input(WINDOW* const w) {
	struct input r;
	memset(&r, 0, sizeof(struct input));
	size_t utflen = 0;
	int init = wgetch(w);
	const utf8 u = (utf8)init;
	const char* kn = keyname(init);
	//syslog(LOG_DEBUG, "get_input: %s (%d), %d", kn, init, has_key(init));
	if (init == -1) {
		r.t = END;
	}
	else if (strlen(kn) == 2 && kn[0] == '^') {
		r.t = CTRL;
		r.ctrl = kn[1];
	}
	else if (has_key(init)) {
		r.t = SPECIAL;
		r.c = init;
	}
	else if ((utflen = utf8_g2nb(&u))) {
		r.t = UTF8;
		r.utf[0] = u;
		for (size_t i = 1; i < utflen; ++i) {
			r.utf[i] = (utf8)wgetch(w);
		}
	}
	else {
		if (init == KEY_RESIZE) {
			r.t = SPECIAL;
			r.c = init;
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
	// TODO simplify
	memset(i->mks, 0, i->kml*sizeof(int));
	static struct input il[INPUT_LIST_LENGTH];
	static int ili = 0;
	struct input newinput = get_input(stdscr);
	if (newinput.t == SPECIAL && newinput.c == KEY_RESIZE) {
		ui_update_geometry(i); // TODO TODO FIXME maybe not here
		ui_draw(i);
		memset(il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
		ili = 0;
		return CMD_NONE;
	}

	if (newinput.t == END) return CMD_NONE;
	else if (newinput.t == CTRL && newinput.ctrl == '[') {
		memset(il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
		ili = 0;
		return CMD_NONE;
	}
	il[ili] = newinput;
	ili += 1;

	for (size_t m = 0; m < i->kml; ++m) {
		const struct input2cmd* const i2c = &i->kmap[m];
		if (i2c->m != i->m) continue; // mode mismatch
		const struct input* const in = i2c->i;
		for (int s = 0; s < ili; ++s) {
			if (in[s].t != il[s].t) break;
			bool match = false;
			switch (in[s].t) {
			case END: break;
			case UTF8:
				match = !strcmp(in[s].utf, il[s].utf);
				break;
			case SPECIAL:
				match = in[s].c == il[s].c;
				break;
			case CTRL:
				match = in[s].ctrl == il[s].ctrl;
				break;
			}
			if (match) {
				i->mks[m] += 1;
			}
			else {
				i->mks[m] = 0;
				break;
			}
		}
	}

	int matches = 0;
	size_t mi = 0;
	for (size_t m = 0; m < i->kml; ++m) {
		if (i->mks[m]) {
			matches += 1;
			mi = m;
		}
	}

	if (!matches) {
		ili = 0;
		memset(il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
		memset(i->mks, 0, i->kml*sizeof(int));
		return CMD_NONE;
	}
	else if (matches == 1) {
		bool fullmatch = true;
		for (int s = 0; s < INPUT_LIST_LENGTH &&
				i->kmap[mi].i[s].t != END; ++s) {
			fullmatch = fullmatch && il[s].t == i->kmap[mi].i[s].t;
			switch (il[s].t) {
			case END: break;
			case UTF8:
				fullmatch = fullmatch &&
					!strcmp(il[s].utf, i->kmap[mi].i[s].utf);
				break;
			case SPECIAL:
				fullmatch = fullmatch && il[s].c == i->kmap[mi].i[s].c;
				break;
			case CTRL:
				fullmatch = fullmatch && il[s].ctrl == i->kmap[mi].i[s].ctrl;
				break;
			}
		}
		if (fullmatch) {
			memset(il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
			ili = 0;
			// Not clearing on full match to preserve mks information for hints
			// Instead, mks is cleared when entered get_cmd()
			return i->kmap[mi].c;
		}
	}
	else { // some matches

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
	if (i.t == SPECIAL && i.c == KEY_RESIZE) return -3;
	if (i.t == END) return 1;
	if (i.t == CTRL && i.ctrl == '[') return -1;
	else if (i.t == CTRL && i.ctrl == 'N') return 2;
	else if (i.t == CTRL && i.ctrl == 'P') return -2;
	else if ((i.t == CTRL && i.ctrl == 'J') ||
	         (i.t == UTF8 && (i.utf[0] == '\n' || i.utf[0] == '\r'))) {
		if (*buftop != buf) return 0;
	}
	else if (i.t == UTF8 && (size_t)(*buftop - buf) < bsize) {
		utf8_insert(buf, i.utf, utf8_ng_till(buf, *buftop));
		*buftop += utf8_g2nb(i.utf);
	}
	else if ((i.t == CTRL && i.ctrl == 'H') ||
	         (i.t == SPECIAL && i.c == KEY_BACKSPACE)) {
		if (*buftop != buf) {
			const size_t before = strnlen(buf, bsize);
			utf8_remove(buf, utf8_ng_till(buf, *buftop)-1);
			*buftop -= before - strnlen(buf, bsize);
		}
		else if (strnlen(buf, bsize));
 		// Exit only when buf is empty
		else return -1;
	}
	else if ((i.t == CTRL && i.ctrl == 'D') ||
	         (i.t == SPECIAL && i.c == KEY_DC)) {
		utf8_remove(buf, utf8_ng_till(buf, *buftop));
	}
	else if ((i.t == CTRL && i.ctrl == 'A') ||
	         (i.t == SPECIAL && i.c == KEY_HOME)) {
		*buftop = buf;
	}
	else if ((i.t == CTRL && i.ctrl == 'E') ||
	         (i.t == SPECIAL && i.c == KEY_END)) {
		*buftop = buf+strnlen(buf, bsize);
	}
	else if (i.t == CTRL && i.ctrl == 'U') {
		*buftop = buf;
		memset(buf, 0, bsize);
	}
	else if (i.t == CTRL && i.ctrl == 'K') {
		const size_t clen = strnlen(buf, bsize);
		memset(*buftop, 0, strnlen(*buftop, bsize-clen));
	}
	else if ((i.t == CTRL && i.ctrl == 'F') ||
	         (i.t == SPECIAL && i.c == KEY_RIGHT)) {
		if ((size_t)(*buftop - buf) < bsize) {
			*buftop += utf8_g2nb(*buftop);
		}
	}
	else if ((i.t == CTRL && i.ctrl == 'B') ||
	         (i.t == SPECIAL && i.c == KEY_LEFT)) {
		if ((size_t)(*buftop - buf)) {
			const size_t gt = utf8_ng_till(buf, *buftop);
			*buftop = buf;
			for (size_t i = 0; i < gt-1; ++i) {
				*buftop += utf8_g2nb(*buftop);
			}
		}
	}
	return 1;
}
