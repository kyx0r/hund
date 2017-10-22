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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <syslog.h>

#include "include/ui.h"

static void create_directory(struct file_view* v) {
	char* path = malloc(PATH_MAX);
	char* name = malloc(NAME_MAX);
	name[0] = 0;
	strcpy(path, v->wd);
	//prompt("directory name", NAME_MAX, name);
	enter_dir(path, name);
	dir_make(path);
	scan_dir(v->wd, &v->file_list, &v->num_files);
	v->selection = file_index(v->file_list, v->num_files, name);
	free(path);
	free(name);
}

static void remove_file(struct file_view* v) {
	char* path = malloc(PATH_MAX);
	strcpy(path, v->wd);
	enter_dir(path, v->file_list[v->selection]->file_name);
	file_remove(path);
	free(path);
	scan_dir(v->wd, &v->file_list, &v->num_files);
	v->selection -= 1;
}

static void move_file(struct file_view* pv, struct file_view* sv) {
	char* src_path = malloc(PATH_MAX);
	char* dest_path = malloc(PATH_MAX);
	char* name = malloc(NAME_MAX);
	name[0] = 0;
	strcpy(src_path, pv->wd);
	strcpy(dest_path, sv->wd);
	enter_dir(src_path, pv->file_list[pv->selection]->file_name);
	//prompt("new name", NAME_MAX, name);
	enter_dir(dest_path, name);
	int fmr = file_move(src_path, dest_path);
	syslog(LOG_DEBUG, "file_move() returned %d", fmr);
	free(src_path);
	free(dest_path);
	free(name);
	scan_dir(pv->wd, &pv->file_list, &pv->num_files);
	scan_dir(sv->wd, &sv->file_list, &sv->num_files);
	pv->selection = 0;
}

static void copy_file(struct file_view* pv, struct file_view* sv) {
	char* src_path = malloc(PATH_MAX);
	char* dest_path = malloc(PATH_MAX);
	char* name = malloc(NAME_MAX);
	name[0] = 0;
	strcpy(src_path, pv->wd);
	strcpy(dest_path, sv->wd);
	enter_dir(src_path, pv->file_list[pv->selection]->file_name);
	//prompt("copy name", NAME_MAX, name);
	enter_dir(dest_path, name);
	int fcr = file_copy(src_path, dest_path);
	syslog(LOG_DEBUG, "file_copy() returned %d", fcr);
	free(src_path);
	free(dest_path);
	free(name);
	scan_dir(sv->wd, &sv->file_list, &sv->num_files);
}

static void go_up_dir(struct file_view* v) {
	char* prevdir = malloc(NAME_MAX);
	current_dir(v->wd, prevdir);
	int r = up_dir(v->wd);
	if (!r) {
		scan_dir(v->wd, &v->file_list, &v->num_files);
		v->selection = file_index(v->file_list, v->num_files, prevdir);
	}
	free(prevdir);
}

static void go_enter_dir(struct file_view* v) {
	if (v->file_list[v->selection]->t == DIRECTORY) {
		enter_dir(v->wd, v->file_list[v->selection]->file_name);
		v->selection = 0;
		scan_dir(v->wd, &v->file_list, &v->num_files);
	}
	else if (v->file_list[v->selection]->t == LINK) {
		enter_dir(v->wd, v->file_list[v->selection]->link_path);
		v->selection = 0;
		scan_dir(v->wd, &v->file_list, &v->num_files);
	}
}

int get_cmd(const int kml, int* mks) {
	static int keyseq[MAX_KEYSEQ_LENGTH] = { 0 };
	static int ksi = 0;
	int c = getch();
	//syslog(LOG_DEBUG, "%d, (%d) %d %d %d %d", c, ksi, keyseq[0], keyseq[1], keyseq[2], keyseq[3]);

	if (c == -1 && !ksi) return NONE;
	if (c == 27) {
		memset(mks, 0, kml*sizeof(int));
		memset(keyseq, 0, sizeof(keyseq));
		ksi = 0;
		return NONE;
	}
	if (c != -1 || !ksi) {
		keyseq[ksi] = c;
		ksi += 1;
	}

	memset(mks, 0, kml*sizeof(int));
	for (int c = 0; c < kml; c++) {
		int s = 0;
		// Skip zeroes; these does not matter
		while (keyseq[s]) {
			/* mks[c] will contain length of matching sequence
			 * mks[c] will be zeroed if sequence broken at any point
			 */
			if (key_mapping[c].ks[s] == keyseq[s]) {
				mks[c] += 1;
			}
			else {
				mks[c] = 0;
				break;
			}
			s += 1;
		}
	}

	bool match = false; // At least one match
	for (int c = 0; c < kml; c++) {
		match = match || mks[c];
	}

	enum command cmd = NONE;
	if (match) {
		for (int c = 0; c < kml; c++) {
			int ksl = 0;
			for (int s = 0; key_mapping[c].ks[s]; s++) {
				ksl += 1;
			}
			if (mks[c] == ksl) {
				cmd = key_mapping[c].c;
			}
		}
	}
	if (!match || ksi == MAX_KEYSEQ_LENGTH || cmd != NONE) {
		memset(keyseq, 0, sizeof(keyseq));
		ksi = 0;
	}
	return cmd;
}

int main(int argc, char* argv[])  {
	static char* help = "Usage: hund [OPTION]...\n"
	"Options:\n"
	"  -c, --chdir\t\tchange initial directory\n"
	"  -v, --verbose\t\tbe verbose\n"
	"  -h, --help\t\tdisplay this help message\n";

	static char sopt[] = "vhc:";
	static struct option lopt[] = {
		{"chdir", required_argument, 0, 'c'},
		{"verbose", no_argument, 0, 'v'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
    };
	int o, opti = 0;
	while ((o = getopt_long(argc, argv, sopt, lopt, &opti)) != -1) {
		if (o == -1) break;
		switch (o) {
		case 'c':
			chdir(optarg);
			break;
		case 'v':
			puts("woof!");
			exit(EXIT_SUCCESS);
		case 'h':
			printf(help);
			exit(EXIT_SUCCESS);
		default:
			puts("unknown argument");
			exit(1);
		}
	}

	while (optind < argc) {
		printf("non-option argument at index %d: %s\n", optind, argv[optind]);
		optind += 1;
	}

	openlog(argv[0], LOG_PID, LOG_USER);
	syslog(LOG_NOTICE, "%s started", argv[0]+2);
	
	struct ui i;
	ui_init(&i);
	struct file_view* pv = &i.fvs[0];
	struct file_view* sv = &i.fvs[1];

	get_cwd(pv->wd);
	get_cwd(sv->wd);
	scan_dir(pv->wd, &pv->file_list, &pv->num_files);
	scan_dir(sv->wd, &sv->file_list, &sv->num_files);

	bool run = true;
	while (run) {
		switch (get_cmd(i.kml, i.mks)) {
		case QUIT:
			run = false;
			break;
		case SWITCH_PANEL:
			sv = &i.fvs[i.active_view];
			i.active_view += 1;
			i.active_view %= 2;
			pv = &i.fvs[i.active_view];
			break;
		case ENTRY_DOWN:
			if (pv->selection < pv->num_files-1) {
				pv->selection += 1;
			}
			break;
		case ENTRY_UP:
			if (pv->selection > 0) {
				pv->selection -= 1;
			}
			break;
		case ENTER_DIR:
			go_enter_dir(pv);
			break;
		case UP_DIR:
			go_up_dir(pv);
			break;
		case COPY:
			copy_file(pv, sv);
			break;
		case MOVE:
			move_file(pv, sv);
			break;
		case REMOVE:
			remove_file(pv);
			break;
		case CREATE_DIR:
			create_directory(pv);
			break;
		case REFRESH:
			scan_dir(pv->wd, &pv->file_list, &pv->num_files);
			break;
		case NONE:
			break;
		}
		ui_update_geometry(&i);
		ui_draw(&i);
	}
	ui_end(&i);
	syslog(LOG_DEBUG, "exit");
	closelog();
	exit(EXIT_SUCCESS);
}
