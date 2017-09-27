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

void scan_wd(char* wd, struct dirent*** namelist, int* num_files) {
	DIR* dir = opendir(wd);
	int n = scandir(wd, namelist, NULL, alphasort);	
	closedir(dir);
	*num_files = n;
}

void file_view_update_geometry(struct file_view* fv) {
	WINDOW* oldwin = panel_window(fv->pan);
	WINDOW* tmpwin = newwin(fv->height, fv->width, fv->position_y, fv->position_x);
	wtimeout(tmpwin, 100);
	wborder(tmpwin, '|', '|', '-', '-', '+', '+', '+', '+');
	if (fv->pan == NULL) {
		fv->pan = new_panel(tmpwin);
		fv->win = tmpwin;
	}
	else {
		replace_panel(fv->pan, tmpwin);
		fv->win = tmpwin;
		delwin(oldwin);
	}
	move_panel(fv->pan, fv->position_y, fv->position_x);
}

void file_view_refresh(struct file_view* fv) {
	mvwprintw(fv->win, 1, 1, "%s", fv->wd);
	for (int i = 0; i < fv->num_files; i++) {
		mvwprintw(fv->win, i+2, 1, "%d %02hhx %s", i, fv->namelist[i]->d_type, fv->namelist[i]->d_name); 
	}
}

void file_view_setup_pair(struct file_view fvs[2], int scrh, int scrw) {
	fvs[0] = (struct file_view) {
		.win = NULL,
		.pan = NULL,
		.position_x = 0,
		.position_y = 0,
		.width = scrw/2,
		.height = scrh,
	};

	fvs[1] = (struct file_view) {
		.win = NULL,
		.pan = NULL,
		.position_x = scrw/2,
		.position_y = 0,
		.width = scrw/2,
		.height = scrh, 
	};

	for (int i = 0; i < 2; i++) {
		file_view_update_geometry(&fvs[i]);
	}
}

void file_view_delete_pair(struct file_view fvs[2]) {
	for (int i = 0; i < 2; i++) {
		delwin(fvs[i].win);
		del_panel(fvs[i].pan);
	}
}
