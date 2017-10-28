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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <syslog.h>

#include "include/ui.h"

enum task_type {
	TASK_NONE = 0,
	TASK_MKDIR,
	TASK_RM,
	TASK_COPY,
	TASK_MOVE,
};

enum task_state {
	TASK_STATE_CLEAN = 0,
	TASK_STATE_GATHERING_DATA,
	TASK_STATE_DATA_GATHERED, // AKA ready to execute
	TASK_STATE_EXECUTING,
	TASK_STATE_FINISHED // AKA all done, cleanme
};

struct task {
	enum task_state s;
	enum task_type t;
	char *src, *dst;
};

int main(int argc, char* argv[])  {
	static const char* help = "Usage: hund [OPTION] [left panel] [right panel]\n"
	"Options:\n"
	"  -c, --chdir=PATH\t\tchange initial directory\n"
	"  -v, --verbose\t\tbe verbose\n"
	"  -h, --help\t\tdisplay this help message\n";

	static const char sopt[] = "vhc:";
	static const struct option lopt[] = {
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

	char* init_wd[2] = { NULL, NULL };
	int init_wd_top = 0;
	while (optind < argc) {
		if (init_wd_top < 2) {
			init_wd[init_wd_top] = argv[optind];
			init_wd_top += 1;
		}
		else {
			//
		}
		optind += 1;
	}

	openlog(argv[0], LOG_PID, LOG_USER);
	syslog(LOG_NOTICE, "%s started", argv[0]+2);
	
	struct ui i;
	ui_init(&i);

	struct file_view* pv = &i.fvs[0];
	struct file_view* sv = &i.fvs[1];
	for (int v = 0; v < 2; ++v) {
		if (init_wd[v]) {
			strcpy(i.fvs[v].wd, init_wd[v]);
		}
		else {
			get_cwd(i.fvs[v].wd);
		}
		scan_dir(i.fvs[v].wd, &i.fvs[v].file_list, &i.fvs[v].num_files);
	}

	struct task t = (struct task) {
		.t = TASK_NONE,
		.s = TASK_STATE_CLEAN,
		.src = NULL,
		.dst = NULL,
	};

	bool run = true;
	while (run) {
		ui_update_geometry(&i);
		ui_draw(&i);
		if (i.m == MODE_MANAGER) {
			switch (get_cmd(&i)) {
			case CMD_QUIT:
				run = false;
				break;
			case CMD_SWITCH_PANEL:
				sv = &i.fvs[i.active_view];
				i.active_view += 1;
				i.active_view %= 2;
				pv = &i.fvs[i.active_view];
				break;
			case CMD_ENTRY_DOWN:
				if (pv->selection < pv->num_files-1) {
					pv->selection += 1;
				}
				break;
			case CMD_ENTRY_UP:
				if (pv->selection > 0) {
					pv->selection -= 1;
				}
				break;
			case CMD_ENTER_DIR:
				if (pv->file_list[pv->selection]->t == DIRECTORY) {
					enter_dir(pv->wd, pv->file_list[pv->selection]->file_name);
					pv->selection = 0;
					scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				}
				else if (pv->file_list[pv->selection]->t == LINK) {
					enter_dir(pv->wd, pv->file_list[pv->selection]->link_path);
					pv->selection = 0;
					scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				}
				break;
			case CMD_UP_DIR:
				{
				char* prevdir = malloc(NAME_MAX);
				current_dir(pv->wd, prevdir);
				int r = up_dir(pv->wd);
				if (!r) {
					scan_dir(pv->wd, &pv->file_list, &pv->num_files);
					file_index(pv->file_list, pv->num_files, prevdir, &pv->selection);
				}
				free(prevdir);
				}
				break;
			case CMD_COPY:
				t.src = calloc(PATH_MAX, sizeof(char));
				strcpy(t.src, pv->wd);
				enter_dir(t.src, pv->file_list[pv->selection]->file_name);
				t.dst = calloc(PATH_MAX, sizeof(char));
				strcpy(t.dst, sv->wd);
				strcat(t.dst, "/");
				t.t = TASK_COPY;
				t.s = TASK_STATE_GATHERING_DATA;
				i.m = MODE_PROMPT;
				i.prompt_textbox_top = i.prompt_textbox = t.dst+strlen(t.dst);
				i.prompt_textbox_size = NAME_MAX;
				break;
			case CMD_MOVE:
				t.src = calloc(PATH_MAX, sizeof(char));
				strcpy(t.src, pv->wd);
				enter_dir(t.src, pv->file_list[pv->selection]->file_name);
				t.dst = calloc(PATH_MAX, sizeof(char));
				strcpy(t.dst, sv->wd);
				/* same name; TODO detect conflicting file names */
				enter_dir(t.dst, pv->file_list[pv->selection]->file_name);
				t.t = TASK_MOVE;
				t.s = TASK_STATE_DATA_GATHERED;
				break;
			case CMD_REMOVE:
				t.src = calloc(PATH_MAX, sizeof(char));
				t.t = TASK_RM;
				t.s = TASK_STATE_DATA_GATHERED;
				strcpy(t.src, pv->wd);
				enter_dir(t.src, pv->file_list[pv->selection]->file_name);
				break;
			case CMD_CREATE_DIR:
				t.src = calloc(PATH_MAX, sizeof(char));
				t.t = TASK_MKDIR;
				t.s = TASK_STATE_GATHERING_DATA;
				strcpy(t.src, pv->wd);
				strcat(t.src, "/");
				i.prompt_textbox_top = i.prompt_textbox = t.src+strlen(t.src);
				i.prompt_textbox_size = NAME_MAX;
				i.m = MODE_PROMPT;
				break;
			case CMD_FIND:
				i.find = calloc(NAME_MAX, 1);
				i.find_top = i.find;
				i.find_size = NAME_MAX;
				i.find_init = pv->selection;
				i.m = MODE_FIND;
				break;
			case CMD_ENTRY_FIRST:
				pv->selection = 0;
				break;
			case CMD_ENTRY_LAST:
				pv->selection = pv->num_files-1;
				break;
			case CMD_CHMOD:
				i.chmod_path = malloc(PATH_MAX);
				strcpy(i.chmod_path, pv->wd);
				enter_dir(i.chmod_path,
						pv->file_list[pv->selection]->file_name);
				chmod_open(&i, i.chmod_path,
						pv->file_list[pv->selection]->s.st_mode);
				break;
			case CMD_REFRESH:
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				break;
			default:
				break;
			}
		}
		else if (i.m == MODE_CHMOD) {
			switch (get_cmd(&i)) {
			case CMD_RETURN:
				chmod_close(&i, MODE_MANAGER);
				break;
			case CMD_CHANGE:
				chmod(i.chmod_path, i.chmod_mode);
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				chmod_close(&i, MODE_MANAGER);
				break;
			case CMD_TOGGLE_UIOX: TOGGLE_MODE_BIT(i.chmod_mode, S_ISUID); break;
			case CMD_TOGGLE_GIOX: TOGGLE_MODE_BIT(i.chmod_mode, S_ISGID); break;
			case CMD_TOGGLE_SB: TOGGLE_MODE_BIT(i.chmod_mode, S_ISVTX); break;
			case CMD_TOGGLE_UR: TOGGLE_MODE_BIT(i.chmod_mode, S_IRUSR); break;
			case CMD_TOGGLE_UW: TOGGLE_MODE_BIT(i.chmod_mode, S_IWUSR); break;
			case CMD_TOGGLE_UX: TOGGLE_MODE_BIT(i.chmod_mode, S_IXUSR); break;
			case CMD_TOGGLE_GR: TOGGLE_MODE_BIT(i.chmod_mode, S_IRGRP); break;
			case CMD_TOGGLE_GW: TOGGLE_MODE_BIT(i.chmod_mode, S_IWGRP); break;
			case CMD_TOGGLE_GX: TOGGLE_MODE_BIT(i.chmod_mode, S_IXGRP); break;
			case CMD_TOGGLE_OR: TOGGLE_MODE_BIT(i.chmod_mode, S_IROTH); break;
			case CMD_TOGGLE_OW: TOGGLE_MODE_BIT(i.chmod_mode, S_IWOTH); break;
			case CMD_TOGGLE_OX: TOGGLE_MODE_BIT(i.chmod_mode, S_IXOTH); break;
			default: break;
			}
		}
		else if (i.m == MODE_FIND) {
			curs_set(2);
			WINDOW* hw = panel_window(i.hint);
			wmove(hw, 1, i.prompt_textbox_top-i.prompt_textbox+1);
			wrefresh(hw);
			int c = wgetch(hw);
			int r = fill_textbox(i.find, &i.find_top, i.find_size, c);
			if (r == -1) {
				pv->selection = i.find_init;
				i.m = MODE_MANAGER;
				free(i.find);
				i.find = NULL;
			}
			else if (r == 0) {
				i.m = MODE_MANAGER;
				free(i.find);
				i.find = NULL;
			}
			else {
				file_find(pv->file_list, pv->num_files, i.find, &pv->selection);
			}
			curs_set(0);
		}
		else if (i.m ==	MODE_PROMPT) {
			curs_set(2);
			WINDOW* hw = panel_window(i.hint);
			wmove(hw, 1, i.prompt_textbox_top-i.prompt_textbox+1);
			wrefresh(hw);
			int c = wgetch(hw);
			int r = fill_textbox(i.prompt_textbox, &i.prompt_textbox_top, i.prompt_textbox_size, c);
			if (!r) {
				if (t.t != TASK_NONE && t.s == TASK_STATE_GATHERING_DATA) {
					t.s = TASK_STATE_DATA_GATHERED;
				}
				i.m = MODE_MANAGER;
			}
			else if (r == -1) {
				if (t.src) free(t.src);
				if (t.dst) free(t.dst);
				t.t = TASK_NONE;
				t.s = TASK_STATE_CLEAN;
				i.m = MODE_MANAGER;
			}
			curs_set(0);
		}

		if (t.s == TASK_STATE_DATA_GATHERED) {
			t.s = TASK_STATE_EXECUTING;
			int err;
			switch (t.t) {
			case TASK_MKDIR:
				syslog(LOG_DEBUG, "task_mkdir %s (%s)", t.src, i.prompt_textbox);
				err = dir_make(t.src);
				if (err) {
					syslog(LOG_ERR, "dir_make(\"%s\") failed: %s", t.src, strerror(err));
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				file_index(pv->file_list, pv->num_files, i.prompt_textbox, &pv->selection);
				t.s = TASK_STATE_FINISHED;
				break;
			case TASK_RM:
				syslog(LOG_DEBUG, "task_rmdir %s", t.src);
				err = file_remove(t.src);
				if (err) {
					syslog(LOG_ERR, "file_remove(\"%s\") failed: %s", t.src, strerror(err));
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				if (pv->selection >= pv->num_files) {
					pv->selection -= 1;
				}
				t.s = TASK_STATE_FINISHED;
				break;
			case TASK_COPY:
				syslog(LOG_DEBUG, "task_copy %s -> %s", t.src, t.dst);
				err = file_copy(t.src, t.dst);
				if (err) {
					syslog(LOG_ERR, "file_copy(\"%s\", \"%s\") failed: %s",
							t.src, t.dst, strerror(err));
				}
				//scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				scan_dir(sv->wd, &sv->file_list, &sv->num_files);
				t.s = TASK_STATE_FINISHED;
				break;
			case TASK_MOVE:
				syslog(LOG_DEBUG, "task_move %s -> %s", t.src, t.dst);
				err = file_move(t.src, t.dst);
				if (err) {
					syslog(LOG_ERR, "file_move(\"%s\", \"%s\") failed: %s",
							t.src, t.dst, strerror(err));
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				scan_dir(sv->wd, &sv->file_list, &sv->num_files);
				if (pv->selection >= pv->num_files) {
					pv->selection -= 1;
				}
				t.s = TASK_STATE_FINISHED;
				break;
			case TASK_NONE:
				break;
			}
		}

		if (t.s == TASK_STATE_EXECUTING) {
			/* TODO
			 * This state is for later.
			 * Copying, moving, listing etc. is going to be iterative.
			 * So that tasks can be paused, stopped or changed anytime.
			 */
		}

		if (t.s == TASK_STATE_FINISHED) {
			if (t.src) free(t.src);
			if (t.dst) free(t.dst);
			t.src = t.dst = NULL;
			t.s = TASK_STATE_CLEAN;
			t.t = TASK_NONE;
		}
	} // while (run)

	ui_end(&i);
	syslog(LOG_NOTICE, "hund finished");
	closelog();
	exit(EXIT_SUCCESS);
}
