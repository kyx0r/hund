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

#ifndef PANEL_H
#define PANEL_H

#include "fs.h"
#include "utf8.h"

static const char compare_values[] = "nstdpx";
#define FV_ORDER_SIZE (sizeof(compare_values)-1)
enum compare {
	CMP_NAME = 'n',
	CMP_SIZE = 's',
	CMP_DATE = 't',
	CMP_ISDIR = 'd',
	CMP_PERM = 'p',
	CMP_ISEXE = 'x',
};
static const char default_order[FV_ORDER_SIZE] = {
	CMP_NAME,
	CMP_ISEXE,
	CMP_ISDIR
};

struct panel {
	char wd[PATH_BUF_SIZE];
	struct file** file_list;
	fnum_t num_files;
	fnum_t num_hidden;
	fnum_t selection;
	fnum_t num_selected;
	int scending; // 1 = ascending, -1 = descending
	char order[FV_ORDER_SIZE];
	bool show_hidden;
};

bool visible(const struct panel* const, const fnum_t);
struct file* hfr(const struct panel* const);

void first_entry(struct panel* const);
void last_entry(struct panel* const);

void jump_n_entries(struct panel* const, const int);

void delete_file_list(struct panel* const);
fnum_t file_on_list(const struct panel* const, const char* const);
void file_highlight(struct panel* const, const char* const);

bool file_find(struct panel* const, const char* const, fnum_t, fnum_t);

bool panel_select_file(struct panel* const);
int panel_enter_selected_dir(struct panel* const);
int panel_up_dir(struct panel* const);

void panel_toggle_hidden(struct panel* const);

int panel_scan_dir(struct panel* const);
void panel_sort(struct panel* const);

char* panel_path_to_selected(struct panel* const);

void panel_sorting_changed(struct panel* const);

void panel_selected_to_list(struct panel* const, struct string_list* const);

void select_from_list(struct panel* const, const struct string_list* const);

/*
 * TODO find a better name
 */
struct assign {
	fnum_t from, to;
};

bool rename_prepare(const struct panel* const, struct string_list* const,
		struct string_list* const, struct string_list* const,
		struct assign** const, fnum_t* const);

bool conflicts_with_existing(struct panel* const,
		const struct string_list* const);

void remove_conflicting(struct panel* const, struct string_list* const);
#endif