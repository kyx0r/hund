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

#include "include/file_view.h"

/* file_view_pair_setup(), file_view_pair_delete(), file_view_pair_update_geometry()
 * all take array of two file_view.
 * Only file_view_redraw() takes only one file_view.
 */
void file_view_pair_setup(struct file_view fvp[2], int scrh, int scrw) {
	fvp[0] = (struct file_view) {
		.file_list = NULL,
		.num_files = 0,
		.selection = 0,
		.view_offset = 0,
		.focused = true,
		.position_x = 0,
		.position_y = 0,
		.width = scrw/2,
		.height = scrh,
	};

	fvp[1] = (struct file_view) {
		.file_list = NULL,
		.num_files = 0,
		.selection = 0,
		.view_offset = 0,
		.focused = false,
		.position_x = scrw/2,
		.position_y = 0,
		.width = scrw/2,
		.height = scrh,
	};

	for (int i = 0; i < 2; i++) {
		WINDOW* tmpwin = newwin(fvp[i].height, fvp[i].width, fvp[i].position_y, fvp[i].position_x);
		wtimeout(tmpwin, 100);
		wborder(tmpwin, '|', '|', '-', '-', '+', '+', '+', '+');
		fvp[i].pan = new_panel(tmpwin);
	}
}

void file_view_pair_delete(struct file_view fvp[2]) {
	/* Delete panel first, THEN it's window.
	 * If you resize the terminal window at least once and then quit (q)
	 * then the program will hang in infinite loop
	 * (I attached to it via gdb and it was stuck either on del_panel or poll.)
	 * or segfault/double free/corrupted unsorted chunks.
	 *
	 * my_remove_panel() in demo_panels.c from ncurses-examples
	 * did this is this order
	 * http://invisible-island.net/ncurses/ncurses-examples.html
	 */

	for (int i = 0; i < 2; i++) {
		PANEL* p = fvp[i].pan;
		WINDOW* w = panel_window(p);
		del_panel(p);
		delwin(w);
	}
}

void file_view_pair_update_geometry(struct file_view fvp[2]) {
	/* Sometimes it's better to create a new window
	 * instead of resizing existing one.
	 * If you just resize window (to bigger size),
	 * borders stay in buffer and the only thing
	 * you can do about them is explicitly overwrite them by
	 * mvwprintw or something.
	 * I do overwrite all buffer with spaces, so I dont have to.
	 * ...but if you do, delete old window AFTER you assign new one to panel.
	 * All ncurses examples do so, so it's probably THE way to do it.
	 */
	for (int i = 0; i < 2; i++) {
		WINDOW* ow = panel_window(fvp[i].pan);
		wresize(ow, fvp[i].height, fvp[i].width);
		wborder(ow, '|', '|', '-', '-', '+', '+', '+', '+');
		move_panel(fvp[i].pan, fvp[i].position_y, fvp[i].position_x);
	}
}

void file_view_redraw(struct file_view* fv) {
	WINDOW* w = panel_window(fv->pan);
	wborder(w, '|', '|', '-', '-', '+', '+', '+', '+');
	mvwprintw(w, 0, 2, "%s", fv->wd);
	// First, adjust view_offset to selection
	if (fv->num_files <= fv->height - 1) {
		fv->view_offset = 0;
	}
	else if (fv->selection < fv->height/2) {
		fv->view_offset = 0;
	}
	else if (fv->selection > fv->num_files - fv->height/2) {
		fv->view_offset = fv->num_files - fv->height + 2;
	}
	else {
		fv->view_offset = fv->selection - fv->height/2 + 1;
	}
	int view_row = 1; // Skipping border
	int ei = fv->view_offset; // Entry Index
	while (ei < fv->num_files && view_row < fv->height-1) {
		// Current File Record
		const struct file_record* cfr = fv->file_list[ei];
		char entry[500];
		char type_symbol = '?';
		int color_pair_enabled = 0;
		switch (cfr->t) {
		case BLOCK:
			type_symbol = '+';
			color_pair_enabled = 7;
			break;
		case CHARACTER:
			type_symbol = '-';
			color_pair_enabled = 7;
			break;
		case DIRECTORY:
			type_symbol = '/';
			color_pair_enabled = 3;
			break;
		case FIFO:
			type_symbol = '|';
			color_pair_enabled = 1;
			break;
		case LINK:
			type_symbol = '~';
			color_pair_enabled = 5;
			break;
		case REGULAR:
			type_symbol = ' ';
			color_pair_enabled = 1;
			break;
		case SOCKET:
			type_symbol = '=';
			color_pair_enabled = 5;
			break;
		case UNKNOWN:
		default:
			type_symbol = '?';
			color_pair_enabled = 1;
			break;
		}
		snprintf(entry, sizeof(entry), "%c%s", type_symbol, cfr->file_name);
		int visible_len = strlen(entry);
		int padding_len = (fv->width - 2) - visible_len;
		if (ei == fv->selection && fv->focused) {
			color_pair_enabled += 1;
		}
		wattron(w, COLOR_PAIR(color_pair_enabled));
		mvwprintw(w, view_row, 1, "%s%*c", entry, padding_len, ' ');
		wattroff(w, COLOR_PAIR(color_pair_enabled));
		view_row += 1;
		ei += 1;
	}
	while (view_row < fv->height-1) {
		mvwprintw(w, view_row, 1, "%*c", fv->width-2, ' ');
		view_row += 1;
	}
	mvwprintw(w, view_row, 2, "files: %d", fv->num_files);
	wrefresh(w);
}
