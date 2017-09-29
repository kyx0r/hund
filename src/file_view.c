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

void get_cwd(char b[PATH_MAX]) {
	getcwd(b, PATH_MAX);
}

struct passwd* get_pwd(void) {
	return getpwuid(geteuid());
}

void scan_wd(char* wd, struct file_record*** file_list, int* num_files) {
	struct dirent** namelist;
	DIR* dir = opendir(wd);
	int n = scandir(wd, &namelist, NULL, alphasort);
	closedir(dir);
	*num_files = n;
	*file_list = malloc(sizeof(struct file_record*) * (*num_files));
	char path[PATH_MAX];
	for (int i = 0; i < (*num_files); i++) {
		(*file_list)[i] = malloc(sizeof(struct file_record));
		strcpy(path, wd);
		enter_dir(path, namelist[i]->d_name);
		const size_t name_len = strlen(namelist[i]->d_name);
		(*file_list)[i]->file_name = malloc(name_len+1); // +1 because cstring
		memcpy((*file_list)[i]->file_name, namelist[i]->d_name, name_len+1);
		lstat(path, &(*file_list)[i]->s);
		(*file_list)[i]->t = namelist[i]->d_type;
		free(namelist[i]);
	}
	free(namelist);
}

void delete_file_list(struct file_record*** file_list, int num_files) {
	for (int i = 0; i < num_files; i++) {
		free((*file_list)[i]->file_name);
		free((*file_list)[i]);
	}
	free(*file_list);
}

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

	for (int i = 0; i < 2; i++) {
		//file_view_update_geometry(&fvs[i]);
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
	int view_row = 1; // Skipping border
	int ei = fv->view_offset; // Entry Index
	while (ei < fv->num_files && view_row < fv->height-2) {
		// Current File Record
		const struct file_record* cfr = fv->file_list[ei];
		char entry[500];
		char type_symbol;
		int color_pair_enabled = 0;
		switch (cfr->t) {
		case DT_BLK: // Block device
			type_symbol = '+';
			color_pair_enabled = 7;
			break;
		case DT_CHR: // Charcter device
			type_symbol = '-';
			color_pair_enabled = 7;
			break;
		case DT_DIR: // Directory
			type_symbol = '/';
			color_pair_enabled = 3;
			break;
		case DT_FIFO: // Named pipe
			type_symbol = '|';
			color_pair_enabled = 1;
			break;
		case DT_LNK: // Symbolic link
			type_symbol = '~';
			color_pair_enabled = 5;
			break;
		case DT_REG: // Regular file
			type_symbol = ' ';
			color_pair_enabled = 1;
			break;
		case DT_SOCK: // UNIX domain socket
			type_symbol = '=';
			color_pair_enabled = 5;
			break;
		default:
		case DT_UNKNOWN: // unknown
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
	while (view_row < fv->height-2) {
		mvwprintw(w, view_row, 1, "%*c", fv->width-2, ' ');
		view_row += 1;
	}
	mvwprintw(w, view_row+1, 2, "files: %d", fv->num_files);
	wrefresh(w);
}
