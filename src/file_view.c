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

/* This file contains file_view-related helper functions.
 *
 * They do things that would otherwise clutter main loop.
 * The main problem that inspired separating those tasks from main loop
 * was performing visibility tests.
 *
 * They are supposed to care about:
 * - selection position and it's validity
 * - whether file is visible or not
 * - whether file_list is empty or not
 * - error codes/failure of functions that they call
 * - rescanning file_list (if needed)
 */

// Is File At Index Visible
// TODO include test for show_hidden and cut it out of functions
bool ifaiv(const struct file_view* const fv, const fnum_t i) {
	return fv->file_list[i]->file_name[0] != '.';
}

void next_entry(struct file_view* fv) {
	if (!fv->num_files || fv->selection > fv->num_files-1) {
		fv->selection = 0;
		return;
	}
	if (fv->show_hidden) {
		if (fv->selection < fv->num_files-1) {
			fv->selection += 1;
		}
	}
	else {
		fnum_t i = fv->selection;
		/* Truth table - "Keep looking?" or "What to put in while"
		 * 1 = Keep looking
		 * 0 = Give up
		 *        visible | hidden
		 * out of array |0|0|
		 * in array     |0|1|
		 */
		do {
			i += 1;
		} while (i <= fv->num_files-1 && fv->file_list[i]->file_name[0] == '.');
		if (i > fv->num_files-1) return;
		fv->selection = i;
	}
}

void prev_entry(struct file_view* fv) {
	if (!fv->num_files || fv->selection > fv->num_files-1) {
		fv->selection = 0;
		return;
	}
	if (fv->show_hidden) {
		if (fv->selection > 0) {
			fv->selection -= 1;
		}
	}
	/* Truth table - "Perform backward search?"
	 *    .hidden | visible
	 * sel == 0 |1*|0| * = first_entry()
	 * sel != 0 |1 |1|
	 */
	else if (fv->selection == 0 && !ifaiv(fv, fv->selection)) {
		first_entry(fv);
	}
   	else if (fv->selection != 0 && ifaiv(fv, fv->selection)) {
	   	/* Truth table - "Keep looking?" or "What to put in while"
	   	 * 1 = Keep looking
	   	 * 0 = Stop looking
	   	 *  visible | hidden
	   	 * i == 0 |0|0|
	   	 * i != 0 |0|1|
	   	 */
	   	fnum_t i = fv->selection;
	   	do {
	   		i -= 1;
	   	} while (i != 0 && fv->file_list[i]->file_name[0] == '.');
	   	if (i == 0 && !ifaiv(fv, i)) {
			first_entry(fv);
		}
		else {
	   		fv->selection = i;
		}
	}
}

void first_entry(struct file_view* fv) {
	if (!fv->num_files || fv->show_hidden ||
			(!fv->show_hidden && !fv->num_files)) {
		fv->selection = 0;
	}
	else {
		fnum_t i = 0;
		while (i < fv->num_files-1 && !ifaiv(fv, i)) {
			i += 1;
		}
		if (i == fv->num_files-1 && !ifaiv(fv, i)) {
			fv->selection = 0;
		}
		else {
			fv->selection = i;
		}
	}
}

void last_entry(struct file_view* fv) {
	if (!fv->num_files) return;
	if (fv->show_hidden) {
		fv->selection = fv->num_files-1;
	}
	else {
		fnum_t i = fv->num_files-1;
		while (i != 0 && !ifaiv(fv, i)) {
			i -= 1;
		}
		if (i == 0 && !ifaiv(fv, i)) {
			fv->selection = 0;
		}
		else {
			fv->selection = i;
		}
	}
}

void delete_file_list(struct file_view* fv) {
	if (fv->num_files) {
		for (fnum_t i = 0; i < fv->num_files; i++) {
			free(fv->file_list[i]->file_name);
			free(fv->file_list[i]->link_path);
			free(fv->file_list[i]);
		}
		free(fv->file_list);
		fv->file_list = NULL;
		fv->num_files = 0;
	}
}

/* Finds and highlighs file with given name */
void file_index(struct file_view* fv, const utf8* const name) {
	fnum_t i = 0;
	while (i < fv->num_files &&	strcmp(fv->file_list[i]->file_name, name)) {
		i += 1;
	}
	if (i != fv->num_files) {
		if (fv->show_hidden || ifaiv(fv, i)) {
			fv->selection = i;
		}
		else {
			first_entry(fv);
		}
	}
}

/* Initial Matching Bytes */
size_t imb(const char* const a, const char* const b) {
	size_t m = 0;
	const char* aa = a;
	const char* bb = b;
	while (*aa && *bb && *aa == *bb) {
		aa += 1;
		bb += 1;
		m += 1;
	}
	return m;
}

/* Checks if STRing contains SUBString */
bool contains(const char* const str, const char* const subs) {
	for (size_t j = 0; strlen(str+j) >= strlen(subs); ++j) {
		if (strlen(subs) == imb(str+j, subs)) return true;
	}
	return false;
}

bool file_find(struct file_view* fv, const utf8* const name,
		fnum_t start, fnum_t end) {
	syslog(LOG_DEBUG, "%u %u", start, end);
	if (start <= end) {
		for (fnum_t i = start; i <= end; ++i) {
			if ((fv->show_hidden || ifaiv(fv, i)) &&
					contains(fv->file_list[i]->file_name, name)) {
				fv->selection = i;
				return true;
			}
			//if (i == end) break;
		}
	}
	else if (end < start) {
		for (fnum_t i = start; i >= end; --i) {
			if ((fv->show_hidden || ifaiv(fv, i)) &&
					contains(fv->file_list[i]->file_name, name)) {
				fv->selection = i;
				return true;
			}
			if (i == end) break;
			// ^ prevents unsigned integer underflow
			// TODO do it better
		}
	}
	return false;
}

int file_view_enter_selected_dir(struct file_view* fv) {
	if (!fv->show_hidden && !ifaiv(fv, fv->selection)) return 0;
	int err;
	if (S_ISDIR(fv->file_list[fv->selection]->s.st_mode)) {
		err = enter_dir(fv->wd, fv->file_list[fv->selection]->file_name);
		if (err) return ENAMETOOLONG;
	}
	else if (S_ISLNK(fv->file_list[fv->selection]->s.st_mode)) {
		utf8* p = malloc(PATH_MAX);
		strcpy(p, fv->wd);
		err = enter_dir(p, fv->file_list[fv->selection]->link_path);
		if (err) {
			free(p);
			return ENAMETOOLONG;
		}
		if (is_dir(p)) {
			enter_dir(fv->wd, fv->file_list[fv->selection]->link_path);
		}
		free(p);
	}
	else return ENOTDIR;
	int r = scan_dir(fv->wd, &fv->file_list, &fv->num_files);
	if (r) {
		up_dir(fv->wd);
		r = scan_dir(fv->wd, &fv->file_list, &fv->num_files);
		if (r) abort(); // TODO
		return r;
	}
	first_entry(fv);
	return 0;
}

void file_view_afterdel(struct file_view* fv) {
	scan_dir(fv->wd, &fv->file_list, &fv->num_files);
	if (fv->num_files) {
		if (fv->selection >= fv->num_files-1) {
			last_entry(fv);
		}
		if (!fv->show_hidden && !ifaiv(fv, fv->selection)){
			next_entry(fv);
		}
	}
	else {
		fv->selection = 0;
	}
}

void file_view_toggle_hidden(struct file_view* fv) {
	fv->show_hidden = !fv->show_hidden;
	if (!fv->show_hidden && !ifaiv(fv, fv->selection)) {
		first_entry(fv);
	}
}
