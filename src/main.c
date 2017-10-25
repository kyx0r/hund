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

enum task_type {
	TASK_NONE = 0,
	TASK_MKDIR,
	TASK_RM,
	TASK_COPY,
	TASK_MOVE
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
	static const char* help = "Usage: hund [OPTION]...\n"
	"Options:\n"
	"  -c, --chdir\t\tchange initial directory\n"
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

	struct task t = (struct task) {
		.t = TASK_NONE,
		.s = TASK_STATE_CLEAN,
		.src = NULL,
		.dst = NULL,
	};

	bool run = true;
	while (run) {
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
				go_enter_dir(pv);
				break;
			case CMD_UP_DIR:
				go_up_dir(pv);
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
				prompt_open(&i, "copy name", t.dst+strlen(t.dst), NAME_MAX);
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
				prompt_open(&i, "new directory name", t.src+strlen(t.src), NAME_MAX);
				break;
			case CMD_REFRESH:
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				break;
			case CMD_NONE:
				break;
			}
		}
		else if (i.m ==	MODE_PROMPT) {
			curs_set(2);
			WINDOW* pw = panel_window(i.prompt);
			wmove(pw, 1, i.prompt_textbox_top+1);
			int c = wgetch(pw);
			if (c == -1);
			else if (c == '\n') {
				syslog(LOG_DEBUG, "exit prompt");
				prompt_close(&i, MODE_MANAGER);
				if (t.t != TASK_NONE && t.s == TASK_STATE_GATHERING_DATA) {
					t.s = TASK_STATE_DATA_GATHERED;
				}
			}
			else if (c == KEY_BACKSPACE) {
				if (i.prompt_textbox_top > 0) {
					i.prompt_textbox[i.prompt_textbox_top-1] = 0;
					i.prompt_textbox_top -= 1;
				}
			}
			else if (i.prompt_textbox_top < i.prompt_textbox_size) {
				//syslog(LOG_DEBUG, "input: %d", c);
				i.prompt_textbox[i.prompt_textbox_top] = c;
				i.prompt_textbox_top += 1;
			}
			curs_set(0);
		}

		if (t.s == TASK_STATE_DATA_GATHERED) {
			t.s = TASK_STATE_EXECUTING;
			switch (t.t) {
			case TASK_MKDIR:
				syslog(LOG_DEBUG, "task_mkdir %s (%s)", t.src, i.prompt_textbox);
				dir_make(t.src);
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				pv->selection = file_index(pv->file_list, pv->num_files, i.prompt_textbox);
				free(t.src);
				t.src = NULL;
				break;
			case TASK_RM:
				syslog(LOG_DEBUG, "task_rmdir %s (%s)", t.src, i.prompt_textbox);
				file_remove(t.src);
				free(t.src);
				t.src = NULL;
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				if (pv->selection >= pv->num_files) {
					pv->selection -= 1;
				}
				break;
			case TASK_COPY:
				syslog(LOG_DEBUG, "task_copy %s -> %s", t.src, t.dst);
				if (file_copy(t.src, t.dst)) {
					syslog(LOG_ERR, "file copy failed");
				}
				free(t.src);
				free(t.dst);
				t.src = t.dst = NULL;
				//scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				scan_dir(sv->wd, &sv->file_list, &sv->num_files);
				break;
			case TASK_MOVE:
				syslog(LOG_DEBUG, "task_move %s -> %s", t.src, t.dst);
				file_move(t.src, t.dst);
				free(t.src);
				free(t.dst);
				t.src = t.dst = NULL;
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				scan_dir(sv->wd, &sv->file_list, &sv->num_files);
				if (pv->selection >= pv->num_files) {
					pv->selection -= 1;
				}
				break;
			case TASK_NONE:
				break;
			}
			t.s = TASK_STATE_FINISHED;
		}

		ui_update_geometry(&i);
		ui_draw(&i);
	}
	ui_end(&i);
	syslog(LOG_DEBUG, "exit");
	closelog();
	exit(EXIT_SUCCESS);
}
