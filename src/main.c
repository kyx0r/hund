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
	TASK_RENAME,
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
	static const char* help = \
	"Usage: hund [OPTION] [left panel] [right panel]\n"
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
	
	struct file_view fvs[2];
	memset(fvs, 0, sizeof(struct file_view)*2);
	struct file_view* pv = &fvs[0];
	struct file_view* sv = &fvs[1];
	struct ui i = ui_init(pv, sv);

	for (int v = 0; v < 2; ++v) {
		get_cwd(fvs[v].wd);
		if (init_wd[v]) {
			enter_dir(fvs[v].wd, init_wd[v]);
		}
		scan_dir(fvs[v].wd, &fvs[v].file_list, &fvs[v].num_files);
		first_entry(&fvs[v]);
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
				sv = i.fvs[i.active_view];
				i.active_view += 1;
				i.active_view %= 2;
				pv = i.fvs[i.active_view];
				break;
			case CMD_ENTRY_DOWN:
				next_entry(pv);
				break;
			case CMD_ENTRY_UP:
				prev_entry(pv);
				break;
			case CMD_ENTER_DIR:
				if (!pv->show_hidden && !ifaiv(pv, pv->selection)) break;
				else if (pv->file_list[pv->selection]->t == DIRECTORY) {
					enter_dir(pv->wd, pv->file_list[pv->selection]->file_name);
				}
				else if (pv->file_list[pv->selection]->t == LINK) {
					enter_dir(pv->wd, pv->file_list[pv->selection]->link_path);
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				first_entry(pv);
				break;
			case CMD_UP_DIR:
				{
				char* prevdir = malloc(NAME_MAX);
				current_dir(pv->wd, prevdir);
				int r = up_dir(pv->wd);
				if (!r) {
					scan_dir(pv->wd, &pv->file_list, &pv->num_files);
					file_index(pv->file_list, pv->num_files,
							prevdir, &pv->selection);
					if (!ifaiv(pv, pv->selection)) {
						first_entry(pv);
					}
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
				strcpy(t.src, pv->wd);
				enter_dir(t.src, pv->file_list[pv->selection]->file_name);
				t.t = TASK_RM;
				t.s = TASK_STATE_DATA_GATHERED;
				break;
			case CMD_CREATE_DIR:
				t.src = calloc(PATH_MAX, sizeof(char));
				strcpy(t.src, pv->wd);
				strcat(t.src, "/");
				i.prompt_textbox_top = i.prompt_textbox = t.src+strlen(t.src);
				i.prompt_textbox_size = NAME_MAX;
				t.t = TASK_MKDIR;
				t.s = TASK_STATE_GATHERING_DATA;
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
				first_entry(pv);
				break;
			case CMD_ENTRY_LAST:
				last_entry(pv);
				break;
			case CMD_CHMOD:
				i.chmod_path = malloc(PATH_MAX);
				strcpy(i.chmod_path, pv->wd);
				enter_dir(i.chmod_path,
						pv->file_list[pv->selection]->file_name);
				chmod_open(&i, i.chmod_path,
						pv->file_list[pv->selection]->s.st_mode);
				break;
			case CMD_RENAME:
				{
				size_t plen = strlen(pv->wd);
				size_t fnlen = strlen(pv->file_list[pv->selection]->file_name);
				t.src = calloc(PATH_MAX, sizeof(char));
				strcpy(t.src, pv->wd);
				enter_dir(t.src, pv->file_list[pv->selection]->file_name);
				t.dst = calloc(PATH_MAX, sizeof(char));
				strcpy(t.dst, t.src);
				i.prompt_textbox = t.dst + plen + 1;
				i.prompt_textbox_top = i.prompt_textbox + fnlen;
				i.prompt_textbox_size = NAME_MAX;
				t.t = TASK_RENAME;
				t.s = TASK_STATE_GATHERING_DATA;
				i.m = MODE_PROMPT;
				}
				break;
			case CMD_TOGGLE_HIDDEN:
				pv->show_hidden = !pv->show_hidden;
				if (!ifaiv(pv, pv->selection)) {
					first_entry(pv);
				}
				break;
			case CMD_REFRESH:
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				break;
			default:
				break;
			}
		} // MODE_MANGER

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
			case CMD_TOGGLE_UIOX: TOGGLE_BIT(i.chmod_mode, S_ISUID); break;
			case CMD_TOGGLE_GIOX: TOGGLE_BIT(i.chmod_mode, S_ISGID); break;
			case CMD_TOGGLE_SB: TOGGLE_BIT(i.chmod_mode, S_ISVTX); break;
			case CMD_TOGGLE_UR: TOGGLE_BIT(i.chmod_mode, S_IRUSR); break;
			case CMD_TOGGLE_UW: TOGGLE_BIT(i.chmod_mode, S_IWUSR); break;
			case CMD_TOGGLE_UX: TOGGLE_BIT(i.chmod_mode, S_IXUSR); break;
			case CMD_TOGGLE_GR: TOGGLE_BIT(i.chmod_mode, S_IRGRP); break;
			case CMD_TOGGLE_GW: TOGGLE_BIT(i.chmod_mode, S_IWGRP); break;
			case CMD_TOGGLE_GX: TOGGLE_BIT(i.chmod_mode, S_IXGRP); break;
			case CMD_TOGGLE_OR: TOGGLE_BIT(i.chmod_mode, S_IROTH); break;
			case CMD_TOGGLE_OW: TOGGLE_BIT(i.chmod_mode, S_IWOTH); break;
			case CMD_TOGGLE_OX: TOGGLE_BIT(i.chmod_mode, S_IXOTH); break;
			default: break;
			}
		} // MODE_CHMOD

		else if (i.m == MODE_FIND) {
			int r = fill_textbox(i.find, &i.find_top, i.find_size, panel_window(i.hint));
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
			/* Truth table - "Perform searching?"
			 *    .hidden.file | visible.file
			 * show_hidden 0 |0|1|
			 * show_hidden 1 |1|1|
			 * Also, don't perform search at all on empty input
			 */
			else if ((i.find_top - i.find) &&
					!(!pv->show_hidden && i.find[0] == '.')) {
				file_find(pv->file_list, pv->num_files,
					i.find, &pv->selection);
			}
		} // MODE_FIND

		else if (i.m ==	MODE_PROMPT) {
			int r = fill_textbox(i.prompt_textbox, &i.prompt_textbox_top,
					i.prompt_textbox_size, panel_window(i.hint));
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
		} // MODE_PROMPT

		if (t.s == TASK_STATE_DATA_GATHERED) {
			t.s = TASK_STATE_EXECUTING;
			int err;
			switch (t.t) {
			case TASK_MKDIR:
				syslog(LOG_DEBUG, "task_mkdir %s (%s)",
						t.src, i.prompt_textbox);
				err = dir_make(t.src);
				if (err) {
					syslog(LOG_ERR, "dir_make(\"%s\") failed: %s",
							t.src, strerror(err));
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				file_index(pv->file_list,
						pv->num_files, i.prompt_textbox, &pv->selection);
				t.s = TASK_STATE_FINISHED;
				break;
			case TASK_RM:
				syslog(LOG_DEBUG, "task_rmdir %s", t.src);
				err = file_remove(t.src);
				if (err) {
					syslog(LOG_ERR, "file_remove(\"%s\") failed: %s",
							t.src, strerror(err));
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				prev_entry(pv);
				t.s = TASK_STATE_FINISHED;
				break;
			case TASK_COPY:
				syslog(LOG_DEBUG, "task_copy %s -> %s", t.src, t.dst);
				err = file_copy(t.src, t.dst);
				if (err) {
					syslog(LOG_ERR, "file_copy(\"%s\", \"%s\") failed: %s",
							t.src, t.dst, strerror(err));
				}
				scan_dir(sv->wd, &sv->file_list, &sv->num_files);
				t.s = TASK_STATE_FINISHED;
				break;
			case TASK_RENAME:
				{
				syslog(LOG_DEBUG, "task_rename %s -> %s", t.src, t.dst);
				char named[NAME_MAX];
				current_dir(t.dst, named);
				err = rename(t.src, t.dst);
				if (err) {
					syslog(LOG_ERR, "rename(\"%s\", \"%s\") failed: %s",
							t.src, t.dst, strerror(err));
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				file_index(pv->file_list, pv->num_files,
						named, &pv->selection);
				t.s = TASK_STATE_FINISHED;
				}
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
				prev_entry(pv);
				t.s = TASK_STATE_FINISHED;
				break;
			case TASK_NONE:
				break;
			}
		}
		else if (t.s == TASK_STATE_EXECUTING) {
			/* TODO
			 * This state is for later.
			 * Copying, moving, listing etc. is going to be iterative.
			 * So that tasks can be paused, stopped or changed anytime.
			 */
		}
		else if (t.s == TASK_STATE_FINISHED) {
			if (t.src) free(t.src);
			if (t.dst) free(t.dst);
			t.src = t.dst = NULL;
			t.s = TASK_STATE_CLEAN;
			t.t = TASK_NONE;
		} // task state
	} // while (run)

	ui_end(&i);
	syslog(LOG_NOTICE, "hund finished");
	closelog();
	exit(EXIT_SUCCESS);
}
