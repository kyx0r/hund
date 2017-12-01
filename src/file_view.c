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

/* checks if file is hidden just by looking at the name */
bool hidden(const struct file_view* const fv, const fnum_t i) {
	return fv->file_list[i]->file_name[0] == '.';
}

/* checks if file is visible at the moment */
/* Truth table - is file visible at the moment?
 *          file.ext | .hidden.ext
 * show_hidden = 1 |1|1|
 * show_hidden = 0 |1|0|
 */
bool visible(const struct file_view* const fv, const fnum_t i) {
	return fv->show_hidden || !hidden(fv, i);
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
		 *        visible | not visible
		 * out of array |0|0|
		 * in array     |0|1|
		 */
		do {
			i += 1;
		} while (i <= fv->num_files-1 && hidden(fv, i));
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
	 * .notvisible | visible
	 * sel == 0 |1*|0| * = first_entry()
	 * sel != 0 |1 |1|
	 */
	else if (fv->selection == 0 && hidden(fv, fv->selection)) {
		first_entry(fv);
	}
	else if (fv->selection != 0 && !hidden(fv, fv->selection)) {
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
		} while (i != 0 && hidden(fv, i));
		if (i == 0 && hidden(fv, i)) {
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
		while (i < fv->num_files-1 && hidden(fv, i)) {
			i += 1;
		}
		if (i == fv->num_files-1 && hidden(fv, i)) {
			fv->selection = 0;
		}
		else {
			fv->selection = i;
		}
	}
}

void last_entry(struct file_view* fv) {
	if (!fv->num_files) {
		fv->selection = 0;
	}
	else if (fv->show_hidden) {
		fv->selection = fv->num_files-1;
	}
	else {
		fnum_t i = fv->num_files-1;
		while (i != 0 && hidden(fv, i)) {
			i -= 1;
		}
		if (i == 0 && hidden(fv, i)) {
			fv->selection = 0;
		}
		else {
			fv->selection = i;
		}
	}
}

void delete_file_list(struct file_view* fv) {
	file_list_clean(&fv->file_list, &fv->num_files);
	fv->selection = 0;
}

bool file_on_list(struct file_view* fv, const utf8* const name) {
	fnum_t i = 0;
	while (i < fv->num_files &&	strcmp(fv->file_list[i]->file_name, name)) {
		i += 1;
	}
	return i != fv->num_files;
}

/* Finds and highlighs file with given name */
void file_highlight(struct file_view* fv, const utf8* const name) {
	fnum_t i = 0;
	while (i < fv->num_files &&	strcmp(fv->file_list[i]->file_name, name)) {
		i += 1;
	}
	if (i != fv->num_files) {
		if (visible(fv, i)) {
			fv->selection = i;
		}
		else {
			first_entry(fv);
		}
	}
}

bool file_find(struct file_view* fv, const utf8* const name,
		fnum_t start, fnum_t end) {
	if (start <= end) {
		for (fnum_t i = start; i <= end; ++i) {
			if (visible(fv, i) &&
					contains(fv->file_list[i]->file_name, name)) {
				fv->selection = i;
				return true;
			}
			//if (i == end) break;
		}
	}
	else if (end < start) {
		for (fnum_t i = start; i >= end; --i) {
			if (visible(fv, i) &&
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
	if (!fv->num_files || !visible(fv, fv->selection)) return 0;
	utf8* fn = fv->file_list[fv->selection]->file_name;
	const struct stat* rst = &fv->file_list[fv->selection]->s;
	const struct stat* lst = fv->file_list[fv->selection]->l;
	if (S_ISDIR(rst->st_mode) || S_ISDIR(lst->st_mode)) {
		if (enter_dir(fv->wd, fn)) return ENAMETOOLONG;
	}
	else return ENOTDIR;
	int err = scan_dir(fv->wd, &fv->file_list, &fv->num_files);
	if (err) {
		delete_file_list(fv);
		return err;
	}
	sort_file_list(fv->file_list, fv->num_files);
	first_entry(fv);
	return 0;
}

int file_view_up_dir(struct file_view* fv) {
	utf8* prevdir = malloc(NAME_MAX+1);
	current_dir(fv->wd, prevdir);
	up_dir(fv->wd);
	int err = scan_dir(fv->wd, &fv->file_list, &fv->num_files);
	if (err) {
		delete_file_list(fv);
		free(prevdir);
		return err;
	}
	sort_file_list(fv->file_list, fv->num_files);
	file_highlight(fv, prevdir);
	free(prevdir);
	return 0;
}

void file_view_afterdel(struct file_view* fv) {
	if (fv->num_files) {
		if (fv->selection >= fv->num_files-1) {
			last_entry(fv);
		}
		if (!visible(fv, fv->selection)){
			next_entry(fv);
		}
	}
	else {
		fv->selection = 0;
	}
}

void file_view_toggle_hidden(struct file_view* fv) {
	fv->show_hidden = !fv->show_hidden;
	if (!fv->num_files || !visible(fv, fv->selection)) {
		first_entry(fv);
	}
}

utf8* file_view_path_to_selected(struct file_view* fv) {
	if (!fv->num_files) return NULL;
	utf8* p = malloc(PATH_MAX+1);
	strncpy(p, fv->wd, PATH_MAX);
	if (enter_dir(p, fv->file_list[fv->selection]->file_name)) {
		free(p);
		return NULL;
	}
	return p;
}
