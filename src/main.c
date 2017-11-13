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
	TASK_CD,
	TASK_RENAME,
	TASK_CHOWN,
	TASK_CHGRP,
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
	utf8 *src, *dst;
};

static utf8* failed(const utf8* f, int e) {
	utf8* m = malloc(MSG_BUFFER_SIZE);
	snprintf(m, MSG_BUFFER_SIZE, "%s failed: %s", f, strerror(e));
	return m;
}

static int open_file(const utf8* const path) {
	if (is_dir(path)) return EISDIR;
	static const utf8* prog = "less";
	utf8* cmd = malloc(strlen(path)+strlen(prog)+2+1+1);
	snprintf(cmd, PATH_MAX, "%s '%s'", prog, path); // TODO recognize format
	ui_system(cmd);
	free(cmd);
	return 0;
}

int main(int argc, char* argv[])  {
	static const char* help = \
	"Usage: hund [OPTION] [left panel] [right panel]\n"
	"Options:\n"
	"  -c, --chdir=PATH\t\tchange initial directory\n"
	"  -v, --verbose\t\tbe verbose\n"
	"  -h, --help\t\tdisplay this help message\n"
	"Type `?` while in hund for keybindings\n";

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
			fprintf(stderr, "invalid argument: '%s'\n", argv[optind]);
			exit(EXIT_FAILURE);
		}
		optind += 1;
	}

	openlog(argv[0]+2, LOG_PID, LOG_USER);
	syslog(LOG_NOTICE, "%s started", argv[0]+2);

	struct file_view fvs[2];
	memset(fvs, 0, sizeof(struct file_view)*2);
	struct file_view* pv = &fvs[0];
	struct file_view* sv = &fvs[1];
	struct ui i = ui_init(pv, sv);

	for (int v = 0; v < 2; ++v) {
		get_cwd(fvs[v].wd);
		if (init_wd[v]) {
			utf8* e = strdup(init_wd[v]);
			int r = enter_dir(fvs[v].wd, e);
			if (r) {
				ui_end(&i);
				fprintf(stderr, "path too long: it's %lu, limit is %d\n",
						strlen(fvs[v].wd), PATH_MAX);
				exit(EXIT_FAILURE);
			}
			free(e);
		}
		int r = scan_dir(fvs[v].wd, &fvs[v].file_list, &fvs[v].num_files);
		if (r) {
			ui_end(&i);
			fprintf(stderr, "cannot scan directory '%s': %s (%d)\n",
					fvs[v].wd, strerror(r), r);
			exit(EXIT_FAILURE);
		}
		first_entry(&fvs[v]);
	}

	struct task t = (struct task) {
		.t = TASK_NONE,
		.s = TASK_STATE_CLEAN,
		.src = NULL,
		.dst = NULL,
	};

	int err;
	bool run = true;
	while (run) {
		err = 0;
		ui_update_geometry(&i);
		ui_draw(&i);

		if (i.m == MODE_MANAGER) {
			if (i.error) {
				free(i.error);
				i.error = NULL;
			}
			else if (i.info) {
				free(i.info);
				i.info = NULL;
			}
			switch (get_cmd(&i)) {
			case CMD_QUIT:
				run = false;
				break;
			case CMD_HELP:
				help_open(&i);
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
			case CMD_OPEN_FILE:
				{
				utf8* file = malloc(PATH_MAX);
				strcpy(file, pv->wd);
				enter_dir(file, pv->file_list[pv->selection]->file_name);
				open_file(file);
				free(file);
				}
				break;
			case CMD_CD:
				t.dst = calloc(PATH_MAX, sizeof(char));
				t.t = TASK_CD;
				t.s = TASK_STATE_GATHERING_DATA;
				prompt_open(&i, t.dst, t.dst, PATH_MAX);
				break;
			case CMD_ENTER_DIR:
				if (!pv->show_hidden && !ifaiv(pv, pv->selection)) break;
				else if (S_ISDIR(pv->file_list[pv->selection]->s.st_mode)) {
					err = enter_dir(pv->wd,
							pv->file_list[pv->selection]->file_name);
					if (err) {
						i.error = failed("enter dir", ENAMETOOLONG);
						break;
					}
				}
				else if (S_ISLNK(pv->file_list[pv->selection]->s.st_mode)) {
					utf8* p = malloc(PATH_MAX);
					strcpy(p, pv->wd);
					err = enter_dir(p,
							pv->file_list[pv->selection]->link_path);
					if (err) {
						i.error = failed("enter dir", ENAMETOOLONG);
						free(p);
						break;
					}
					if (is_dir(p)) {
						enter_dir(pv->wd,
								pv->file_list[pv->selection]->link_path);
					}
					free(p);
				}
				else break;
				{
				int r = scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				if (r) {
					i.error = failed("directory scan", r);
					up_dir(pv->wd);
					r = scan_dir(pv->wd, &pv->file_list, &pv->num_files);
					if (r) abort(); // TODO
					break;
				}
				first_entry(pv);
				}
				break;
			case CMD_UP_DIR:
				{
				utf8* prevdir = malloc(NAME_MAX);
				current_dir(pv->wd, prevdir);
				up_dir(pv->wd);
				err = scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				if (err) {
					i.error = failed("directory scan", err);
					err = enter_dir(pv->wd, prevdir);
					if (err) abort(); // TODO
					free(prevdir);
					break;
				}
				file_index(pv, prevdir);
				free(prevdir);
				}
				break;
			case CMD_COPY:
				t.src = malloc(PATH_MAX);
				strcpy(t.src, pv->wd);
				err = enter_dir(t.src, pv->file_list[pv->selection]->file_name);
				if (err) {
					i.error = failed("copy", ENAMETOOLONG);
					t.s = TASK_STATE_FINISHED;
					break;
				}
				t.dst = calloc(PATH_MAX, sizeof(char));
				strcpy(t.dst, sv->wd);
				strcat(t.dst, "/");
				t.t = TASK_COPY;
				t.s = TASK_STATE_GATHERING_DATA;
				prompt_open(&i, t.dst+strlen(t.dst),
						t.dst+strlen(t.dst), NAME_MAX);
				break;
			case CMD_MOVE:
				t.src = calloc(PATH_MAX, sizeof(char));
				strcpy(t.src, pv->wd);
				err = enter_dir(t.src, pv->file_list[pv->selection]->file_name);
				if (err) {
					i.error = failed("move", ENAMETOOLONG);
					t.s = TASK_STATE_FINISHED;
					break;
				}
				t.dst = calloc(PATH_MAX, sizeof(char));
				strcpy(t.dst, sv->wd);
				err = enter_dir(t.dst, pv->file_list[pv->selection]->file_name);
				if (err) {
					i.error = failed("move", ENAMETOOLONG);
					t.s = TASK_STATE_FINISHED;
					break;
				}
				t.t = TASK_MOVE;
				t.s = TASK_STATE_DATA_GATHERED;
				break;
			case CMD_REMOVE:
				if (!pv->num_files) break;
				t.src = calloc(PATH_MAX, sizeof(char));
				strcpy(t.src, pv->wd);
				err = enter_dir(t.src, pv->file_list[pv->selection]->file_name);
				if (err) {
					i.error = failed("remove", ENAMETOOLONG);
					t.s = TASK_STATE_FINISHED;
					break;
				}
				t.t = TASK_RM;
				t.s = TASK_STATE_DATA_GATHERED;
				break;
			case CMD_CREATE_DIR:
				t.src = calloc(PATH_MAX, sizeof(char));
				strcpy(t.src, pv->wd);
				strcat(t.src, "/");
				t.t = TASK_MKDIR;
				t.s = TASK_STATE_GATHERING_DATA;
				prompt_open(&i, t.src+strlen(t.src),
						t.src+strlen(t.src), NAME_MAX);
				break;
			case CMD_FIND:
				{
				utf8* t = calloc(NAME_MAX, sizeof(char));
				find_open(&i, t, t, NAME_MAX);
				}
				break;
			case CMD_ENTRY_FIRST:
				first_entry(pv);
				break;
			case CMD_ENTRY_LAST:
				last_entry(pv);
				break;
			case CMD_CHMOD:
				{
				utf8* p = malloc(PATH_MAX);
				strcpy(p, pv->wd);
				err = enter_dir(p, pv->file_list[pv->selection]->file_name);
				if (err) {
					i.error = failed("chmod", ENAMETOOLONG);
					free(p);
					break;
				}
				chmod_open(&i, p, pv->file_list[pv->selection]->s.st_mode);
				}
				break;
			case CMD_RENAME:
				{
				size_t plen = strlen(pv->wd);
				size_t fnlen = strlen(pv->file_list[pv->selection]->file_name);
				t.src = calloc(PATH_MAX, sizeof(char));
				strcpy(t.src, pv->wd);
				err = enter_dir(t.src, pv->file_list[pv->selection]->file_name);
				if (err) {
					i.error = failed("rename", ENAMETOOLONG);
					t.s = TASK_STATE_FINISHED;
					break;
				}
				if (!utf8_validate(t.src)) {
					i.error = failed("rename", EILSEQ);
					// TODO offer clearing entire name and giving it a new one
					t.s = TASK_STATE_FINISHED;
					break;
				}
				t.dst = calloc(PATH_MAX, sizeof(char));
				strcpy(t.dst, t.src);
				t.t = TASK_RENAME;
				t.s = TASK_STATE_GATHERING_DATA;
				prompt_open(&i, t.dst+plen+1, t.dst+plen+1+fnlen, NAME_MAX);
				}
				break;
			case CMD_TOGGLE_HIDDEN:
				pv->show_hidden = !pv->show_hidden;
				if (!pv->show_hidden && !ifaiv(pv, pv->selection)) {
					first_entry(pv);
				}
				break;
			case CMD_REFRESH:
				err = scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				if (err) i.error = failed("refresh", err);
				break;
			default:
				break;
			}
		} // MODE_MANGER

		else if (i.m == MODE_HELP) {
			switch (get_cmd(&i)) {
			case CMD_HELP_QUIT:
				help_close(&i);
				break;
			case CMD_HELP_DOWN:
				i.helpy += 1;
				break;
			case CMD_HELP_UP:
				if (i.helpy > 0) {
					i.helpy -= 1;
				}
				break;
			default: break;
			}
		} // MODE_HELP

		else if (i.m == MODE_CHMOD) {
			switch (get_cmd(&i)) {
			case CMD_RETURN:
				chmod_close(&i);
				break;
			case CMD_CHANGE:
				syslog(LOG_DEBUG, "chmod %s: %o, %d:%d",
						i.chmod->path, i.chmod->m, i.chmod->o, i.chmod->g);
				if (chmod(i.chmod->path, i.chmod->m)) {
					i.error = failed("chmod", errno);
				}
				if (lchown(i.chmod->path, i.chmod->o, i.chmod->g)) {
					i.error = failed("chmod", errno);
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				chmod_close(&i);
				break;
			case CMD_CHOWN:
				i.chmod->tmp = calloc(LOGIN_NAME_MAX, sizeof(char));
				t.t = TASK_CHOWN;
				t.s = TASK_STATE_GATHERING_DATA;
				prompt_open(&i, i.chmod->tmp, i.chmod->tmp, LOGIN_NAME_MAX);
				break;
			case CMD_CHGRP:
				i.chmod->tmp = calloc(LOGIN_NAME_MAX, sizeof(char));
				t.t = TASK_CHGRP;
				t.s = TASK_STATE_GATHERING_DATA;
				prompt_open(&i, i.chmod->tmp, i.chmod->tmp, LOGIN_NAME_MAX);
				break;
			case CMD_TOGGLE_UIOX: i.chmod->m ^= S_ISUID; break;
			case CMD_TOGGLE_GIOX: i.chmod->m ^= S_ISGID; break;
			case CMD_TOGGLE_SB: i.chmod->m ^= S_ISVTX; break;
			case CMD_TOGGLE_UR: i.chmod->m ^= S_IRUSR; break;
			case CMD_TOGGLE_UW: i.chmod->m ^= S_IWUSR; break;
			case CMD_TOGGLE_UX: i.chmod->m ^= S_IXUSR; break;
			case CMD_TOGGLE_GR: i.chmod->m ^= S_IRGRP; break;
			case CMD_TOGGLE_GW: i.chmod->m ^= S_IWGRP; break;
			case CMD_TOGGLE_GX: i.chmod->m ^= S_IXGRP; break;
			case CMD_TOGGLE_OR: i.chmod->m ^= S_IROTH; break;
			case CMD_TOGGLE_OW: i.chmod->m ^= S_IWOTH; break;
			case CMD_TOGGLE_OX: i.chmod->m ^= S_IXOTH; break;
			default: break;
			}
		} // MODE_CHMOD

		else if (i.m == MODE_FIND) {
			int r = fill_textbox(i.find->t, &i.find->t_top,
					i.find->t_size, 1, panel_window(i.status));
			if (r == -1) {
				find_close(&i, false);
			}
			else if (r == 0) {
				find_close(&i, true);
			}
			else if (r == 2 || r == -2 || i.find->t_top != i.find->t) {
				fnum_t s = 0;
				fnum_t e = pv->num_files-1;
				bool dofind = true;
				if (r == 2) {
					if (pv->selection < pv->num_files-1) {
						s = pv->selection+1;
						e = pv->num_files-1;
					}
					else dofind = false;
				}
				else if (r == -2) {
					if (pv->selection > 0) {
						s = pv->selection-1;
						e = 0;
					}
					else dofind = false;
				}
				bool found = false;
				if (dofind) found = file_find(pv, i.find->t, s, e);
				if (!found) {
					//i.error = failed("find", ENOENT);
				}
			}
			else {
				pv->selection = i.find->sbfc;
			}
		} // MODE_FIND

		else if (i.m ==	MODE_PROMPT) {
			int r = fill_textbox(i.prompt->tb, &i.prompt->tb_top,
					i.prompt->tb_size, 0, panel_window(i.status));
			if (!r) {
				if (t.t != TASK_NONE && t.s == TASK_STATE_GATHERING_DATA) {
					t.s = TASK_STATE_DATA_GATHERED;
				}
				prompt_close(&i);
			}
			else if (r == -1) {
				prompt_close(&i);
			}
		} // MODE_PROMPT

		if (t.s == TASK_STATE_DATA_GATHERED) {
			t.s = TASK_STATE_EXECUTING;
			switch (t.t) {
			case TASK_CHOWN:
				{
				syslog(LOG_DEBUG, "chown %s", i.chmod->tmp);
				/* Some username was entered
				 * check if such user exists */
				struct passwd* pwd = getpwnam(i.chmod->tmp);
				if (errno) {
					i.error = failed("chown", errno);
				}
				else {
					i.chmod->o = pwd->pw_uid;
					strcpy(i.chmod->owner, pwd->pw_name);
				}
				t.s = TASK_STATE_FINISHED;
				}
				break;
			case TASK_CHGRP:
				syslog(LOG_DEBUG, "chgrp %s", i.chmod->tmp);
				/* Some group was entered
				 * check if such group exists */
				struct group* grp = getgrnam(i.chmod->tmp);
				if (errno) {
					i.error = failed("chgrp", errno);
				}
				else {
					i.chmod->g = grp->gr_gid;
					strcpy(i.chmod->group, grp->gr_name);
				}
				t.s = TASK_STATE_FINISHED;
				break;
			case TASK_MKDIR:
				{
				syslog(LOG_DEBUG, "task_mkdir %s", t.src);
				err = dir_make(t.src);
				if (err) {
					i.error = failed("mkdir", err);
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				int fno = current_dir_i(t.src);
				file_index(pv, t.src+fno);
				t.s = TASK_STATE_FINISHED;
				}
				break;
			case TASK_CD:
				{
				char* path = malloc(PATH_MAX);
				strcpy(path, pv->wd);
				err = enter_dir(path, t.dst);
				if (err) {
					i.error = failed("cd", ENAMETOOLONG);
				}
				syslog(LOG_DEBUG, "task_cd %s", path);
				if (!file_exists(path)) {
					i.error = failed("cd", ENOENT);
					t.s = TASK_STATE_FINISHED;
					free(path);
					break;
				}
				if (!is_dir(path)) {
					i.error = failed("cd", EISDIR);
					t.s = TASK_STATE_FINISHED;
					free(path);
					break;
				}
				err = enter_dir(pv->wd, path);
				if (err) {
					i.error = failed("cd", ENAMETOOLONG);
					t.s = TASK_STATE_FINISHED;
					free(path);
					break;
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				first_entry(pv);
				free(path);
				t.s = TASK_STATE_FINISHED;
				}
				break;
			case TASK_RM:
				syslog(LOG_DEBUG, "task_rmdir %s", t.src);
				err = file_remove(t.src);
				if (err) {
					i.error = failed("rmdir", err);
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				if (pv->num_files) {
					if (pv->selection >= pv->num_files-1) {
						last_entry(pv);
					}
					if (!pv->show_hidden && !ifaiv(pv, pv->selection)){
						next_entry(pv);
					}
				}
				else {
					pv->selection = 0;
				}
				i.info = malloc(MSG_BUFFER_SIZE);
				snprintf(i.info, MSG_BUFFER_SIZE,
						"removed %s", t.src+current_dir_i(t.src));
				t.s = TASK_STATE_FINISHED;
				break;
			case TASK_COPY:
				syslog(LOG_DEBUG, "task_copy %s -> %s", t.src, t.dst);
				if (file_exists(t.dst)) {
					i.error = failed("copy", EEXIST);
					t.s = TASK_STATE_FINISHED;
					break;
				}
				err = file_copy(t.src, t.dst);
				if (err) {
					i.error = failed("copy", err);
				}
				i.info = malloc(MSG_BUFFER_SIZE);
				snprintf(i.info, MSG_BUFFER_SIZE,
						"copied to %s", t.dst+current_dir_i(t.dst));
				scan_dir(sv->wd, &sv->file_list, &sv->num_files);
				t.s = TASK_STATE_FINISHED;
				break;
			case TASK_RENAME:
				{
				syslog(LOG_DEBUG, "task_rename %s -> %s", t.src, t.dst);
				if (file_exists(t.dst)) {
					i.error = failed("rename", EEXIST);
					t.s = TASK_STATE_FINISHED;
					break;
				}
				char* named = malloc(NAME_MAX);
				current_dir(t.dst, named);
				err = rename(t.src, t.dst);
				if (err) {
					i.error = failed("rename", err);
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				file_index(pv, named);
				t.s = TASK_STATE_FINISHED;
				free(named);
				}
				break;
			case TASK_MOVE:
				syslog(LOG_DEBUG, "task_move %s -> %s", t.src, t.dst);
				if (file_exists(t.dst)) {
					i.error = failed("move", EEXIST);
					t.s = TASK_STATE_FINISHED;
					break;
				}
				err = file_move(t.src, t.dst);
				if (err) {
					i.error = failed("move", err);
					t.s = TASK_STATE_FINISHED;
					break;
				}
				scan_dir(pv->wd, &pv->file_list, &pv->num_files);
				scan_dir(sv->wd, &sv->file_list, &sv->num_files);
				prev_entry(pv);
				i.info = malloc(MSG_BUFFER_SIZE);
				snprintf(i.info, MSG_BUFFER_SIZE,
						"moved to %s", t.dst+current_dir_i(t.dst));
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
			syslog(LOG_DEBUG, "task cleanup");
			if (t.src) free(t.src);
			if (t.dst) free(t.dst);
			t.src = t.dst = NULL;
			t.s = TASK_STATE_CLEAN;
			t.t = TASK_NONE;
		}
	} // while (run)

	for (int v = 0; v < 2; ++v) {
		delete_file_list(&fvs[v]);
	}
	ui_end(&i);
	syslog(LOG_NOTICE, "hund finished");
	closelog();
	exit(EXIT_SUCCESS);
}
