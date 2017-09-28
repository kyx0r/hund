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
	//const uid_t uid = geteuid();
	//const struct passwd *pwd = getpwuid(uid);
	getcwd(b, PATH_MAX);
	//int cwd_len = strlen(b);
	//cwd[cwd_len] = '/';
	//cwd[cwd_len+1] = 0;
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
		strcat(path, "/");
		strcat(path, namelist[i]->d_name);
		syslog(LOG_DEBUG, "%s\n", path);
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

void file_view_update_geometry(struct file_view* fv) {
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
	//WINDOW* nw = newwin(fv->height, fv->width, fv->position_y, fv->position_x);
	WINDOW* ow = panel_window(fv->pan);
	wresize(ow, fv->height, fv->width);
	wborder(ow, '|', '|', '-', '-', '+', '+', '+', '+');
	//wtimeout(ow, 100);
	//replace_panel(fv->pan, nw);
	//delwin(ow);
	move_panel(fv->pan, fv->position_y, fv->position_x);
}

void file_view_refresh(struct file_view* fv) {
	WINDOW* w = panel_window(fv->pan);
	mvwprintw(w, 0, 1, "%s", fv->wd);
	int i;
	for (i = 0; (i < fv->num_files && i < fv->height-2); i++) {
		char entry[500];
		snprintf(entry, sizeof(entry), "%d t: %02hhx, m: %d, uid: %d, gid: %d, s: %d, n: %s",
				i+fv->view_offset,
				fv->file_list[i+fv->view_offset]->t,
				fv->file_list[i+fv->view_offset]->s.st_mode,
				fv->file_list[i+fv->view_offset]->s.st_uid,
				fv->file_list[i+fv->view_offset]->s.st_gid,
				fv->file_list[i+fv->view_offset]->s.st_size,
				fv->file_list[i+fv->view_offset]->file_name);
		int visible_len = strlen(entry);
		int padding_len = (fv->width - 2) - visible_len;
		if (i+fv->view_offset == fv->selection && fv->focused) {
			wattron(w, COLOR_PAIR(1));
		}
		mvwprintw(w, i+1, 1, "%s%*c", entry, padding_len, ' ');
		if (i+fv->view_offset == fv->selection && fv->focused) {
			wattroff(w, COLOR_PAIR(1));
		}
	}
	for (; i < fv->height-3; i++) {
		mvwprintw(w, i+1, 1, "%*c", fv->width-2, ' ');
	}
	wrefresh(w);
}

void file_view_setup_pair(struct file_view fvs[2], int scrh, int scrw) {
	fvs[0] = (struct file_view) {
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

	fvs[1] = (struct file_view) {
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
		WINDOW* tmpwin = newwin(fvs[i].height, fvs[i].width, fvs[i].position_y, fvs[i].position_x);
		wtimeout(tmpwin, 100);
		wborder(tmpwin, '|', '|', '-', '-', '+', '+', '+', '+');
		fvs[i].pan = new_panel(tmpwin);
	}

	for (int i = 0; i < 2; i++) {
		//file_view_update_geometry(&fvs[i]);
	}
}

void file_view_delete_pair(struct file_view fvs[2]) {
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
		PANEL* p = fvs[i].pan;
		WINDOW* w = panel_window(p);
		del_panel(p);
		delwin(w);
	}
}
