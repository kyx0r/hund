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

#ifndef _DEFAULT_SOURCE
	#define _DEFAULT_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>

#include "ui.h"
#include "task.h"

/*
 * GENERAL TODO
 * - Messages may be blocked by other messages
 * - IDEA: Detecting file formats -> display name of a program that
 *     would open highlighted file
 * - Dir scanning via task?
 * - When "dereferencing", calculate additional file volume
 * - After renaming, highlight one of the renamed files (?)
 *   - Or add command jumping to next/prev selected file
 * - Swap panels
 * - Copy/move: merge+overwrite fails with "file exists" error lol
 * - Rename/copy/move: display conflicts in list and allow user to browse the list
 * - Creating links: offer relative or absolute link path
 * - Optimize string operations. (include info about it's length)
 * - Keybindings must make more sense
 */

struct mark_path {
	size_t len[2];
	char data[];
};

struct marks {
	struct mark_path* az['z'-'a'];
	struct mark_path* AZ['Z'-'A'];
};

void marks_free(struct marks* const);
struct mark_path* marks_get(const struct marks* const, const char);
struct mark_path* marks_set(struct marks* const, const char,
		const char* const, size_t, const char* const, size_t);
void marks_input(struct ui* const, struct marks* const);
void marks_jump(struct ui* const, struct marks* const);

void marks_free(struct marks* const M) {
	for (size_t i = 0; i < 'z'-'a'; ++i) {
		if (M->az[i]) free(M->az[i]);
	}
	for (size_t i = 0; i < 'Z'-'A'; ++i) {
		if (M->AZ[i]) free(M->AZ[i]);
	}
	memset(M, 0, sizeof(struct marks));
}

struct mark_path* marks_get(const struct marks* const M, const char C) {
	if ('a' <= C && C <= 'z') {
		return M->az[C-'a'];
	}
	else if ('A' <= C && C <= 'Z') {
		return M->AZ[C-'A'];
	}
	return NULL;
}

struct mark_path* marks_set(struct marks* const M, const char C,
		const char* const wd, size_t wdlen,
		const char* const n, size_t nlen) {
	struct mark_path** mp;
	if ('a' <= C && C <= 'z') {
		mp = &M->az[C-'a'];
	}
	else if ('A' <= C && C <= 'Z') {
		mp = &M->AZ[C-'A'];
	}
	else {
		return NULL;
	}
	if (*mp) free(*mp);
	*mp = malloc(sizeof(struct mark_path)+wdlen+1+nlen+1);
	memcpy((*mp)->data, wd, wdlen+1);
	memcpy((*mp)->data+wdlen+1, n, nlen+1);
	(*mp)->len[0] = wdlen;
	(*mp)->len[1] = nlen;
	return *mp;
}

void marks_input(struct ui* const i, struct marks* const m) {
	const struct input in = get_input(-1);
	if (in.t != I_UTF8) return;
	const struct file* f = hfr(i->pv);
	if (!f) return;
	marks_set(m, in.utf[0], i->pv->wd,
		strnlen(i->pv->wd, PATH_MAX_LEN), f->name, f->nl);
}

void marks_jump(struct ui* const i, struct marks* const m) {
	const struct input in = get_input(-1);
	if (in.t != I_UTF8) return;
	struct mark_path* mp = marks_get(m, in.utf[0]);
	if (!mp) {
		failed(i, "jump to mark", "Unknown mark");
		return;
	}
	mp->data[mp->len[0]] = '/';
	if (access(mp->data, F_OK)) {
		int e = errno;
		failed(i, "jump to mark", strerror(e));
		mp->data[mp->len[0]] = 0;
		return;
	}
	mp->data[mp->len[0]] = 0;
	memcpy(i->pv->wd, mp->data, mp->len[0]+1);
	if (ui_rescan(i, i->pv, NULL)) {
		file_highlight(i->pv, mp->data+mp->len[0]+1);
	}
}

static char* get_editor(void) {
	char* ed = getenv("VISUAL");
	if (!ed) ed = getenv("EDITOR");
	if (!ed) ed = "vi";
	return ed;
}

static int open_file_with(char* const p, char* const f) {
	char exeimg[PATH_BUF_SIZE];
	memcpy(exeimg, "/usr/bin", 9);
	append_dir(exeimg, p);
	char* const arg[] = { exeimg, f, NULL };
	return spawn(arg);
}

static int edit_list(struct string_list* const in,
		struct string_list* const out) {
	int err = 0, tmpfd;
	char tmpn[] = "/tmp/hund.XXXXXXXX";
	if ((tmpfd = mkstemp(tmpn)) == -1) return errno;
	if ((err = list_to_file(in, tmpfd))
	|| (err = open_file_with(get_editor(), tmpn))
	|| (err = file_to_list(tmpfd, out))) {
		// Failed; One operation failed, the rest was skipped.
	}
	if (close(tmpfd) && !err) err = errno;
	if (unlink(tmpn) && !err) err = errno;
	return err;
}

static void open_selected_with(struct ui* const i, char* const w) {
	char* path;
	if ((path = file_view_path_to_selected(i->pv))) {
		const mode_t m = hfr(i->pv)->s.st_mode;
		if (S_ISREG(m) || S_ISLNK(m)) { // TODO
			int err;
			if ((err = open_file_with(w, path))) {
				failed(i, "open", strerror(err));
			}
		}
		else {
			failed(i, "edit", "not a regular file");
		}
		free(path);
	}
}

static void cmd_find(struct ui* const i) {
	if (!i->pv->num_files) return;
	char t[NAME_BUF_SIZE];
	char* t_top = t;
	memset(t, 0, sizeof(t));
	memcpy(i->prch, "/", 2);
	i->prompt = t;
	int r;
	const fnum_t S = i->pv->selection;
	const fnum_t N = i->pv->num_files;
	struct input o;
	fnum_t s = 0; // Start
	fnum_t e = N-1; // End
	for (;;) {
		ui_draw(i);
		r = fill_textbox(i, t, &t_top, NAME_MAX_LEN, &o);
		if (!r) {
			break;
		}
		else if (r == -1) {
			i->pv->selection = S;
			break;
		}
		else if (r == 2 || t_top != t) {
			if (IS_CTRL(o, 'V')) {
				file_view_select_file(i->pv);
				continue;
			}
			else if (IS_CTRL(o, 'N') && i->pv->selection < N-1) {
				s = i->pv->selection+1;
				e = N-1;
			}
			else if (IS_CTRL(o, 'P') && i->pv->selection > 0) {
				s = i->pv->selection-1;
				e = 0;
			}
		}
		file_find(i->pv, t, s, e);
	}
	i->prompt = NULL;
}

#if 0
inline static void estimate_volume_for_selected(struct file_view* const fv) {
	// TODO REDO
	int sink_fc = 0, sink_dc = 0;
	fnum_t f = 0, s = 0;
	char* const opath = strncpy(malloc(PATH_MAX), fv->wd, PATH_MAX);
	const bool sc = !fv->num_selected; // Single Selection
	if (sc) {
		fv->num_selected = 1;
		fv->file_list[fv->selection]->selected = true;
	}
	for (; f < fv->num_files && s < fv->num_selected; ++f) {
		const struct stat* const S = fv->file_list[f]->l;
		if (!fv->file_list[f]->selected
		    || (S && !S_ISDIR(S->st_mode))) continue;
		append_dir(opath, fv->file_list[f]->file_name);
		fv->file_list[f]->dir_volume = 0;
		/*estimate_volume(opath, &fv->file_list[f]->dir_volume,
				&sink_fc, &sink_dc); // TODO*/
		up_dir(opath);
		s += 1;
	}
	free(opath);
	if (sc) {
		fv->num_selected = 0;
		fv->file_list[fv->selection]->selected = false;
		next_entry(fv);
	}
}
#endif

/*
 * Returns:
 * true - success and there are files to work with (skipping may empty list)
 * false - failure or aborted
 */
inline static bool _solve_name_conflicts_if_any(struct ui* const i,
		struct string_list* const s, struct string_list* const r) {
	static const char* const question = "Conflicting names.";
	static const struct select_option o[] = {
		{ KUTF8("s"), "skip" },
		{ KUTF8("r"), "rename" },
		{ KUTF8("m"), "merge" },
		{ KUTF8("a"), "abort" }
	};
	int err;
	bool was_conflict = false;
	list_copy(r, s);
	while (conflicts_with_existing(i->sv, r)) {
		was_conflict = true;
		switch (ui_select(i, question, o, 4)) {
		case 0:
			remove_conflicting(i->sv, s);
			list_copy(r, s);
			return s->len != 0;
		case 1:
			if ((err = edit_list(r, r))) {
				failed(i, "editor", strerror(err));
				return false;
			}
			break;
		case 2:
			// merge policy is chosen after estimating
			// (if there are any conflicts in the tree)
			free_list(r);
			return true;
		case 3:
		default:
			return false;
		}
	}
	if (!was_conflict) {
		free_list(r);
	}
	return true;
}

static void prepare_task(struct ui* const i, struct task* const t,
		const enum task_type tt) {
	static const struct select_option o[] = {
		{ KUTF8("n"), "no" },
		{ KUTF8("y"), "yes" },
	};
	struct string_list S = { NULL, 0 }; // Selected
	struct string_list R = { NULL, 0 }; // Renamed
	file_view_selected_to_list(i->pv, &S);
	if (!S.len) return;
	if (tt & (TASK_MOVE | TASK_COPY)) {
		if (!_solve_name_conflicts_if_any(i, &S, &R)) {
			free_list(&S);
			free_list(&R);
			return;
		}
	}
	enum task_flags tf = TF_NONE;
	if (tt == TASK_CHMOD && S_ISDIR(i->perm[0])
	&& ui_select(i, "Apply recursively?", o, 2)) {
		tf |= TF_RECURSIVE_CHMOD;
	}
	task_new(t, tt, tf, i->pv->wd, i->sv->wd, &S, &R);
	if (tt == TASK_CHMOD) {
		t->chp = i->plus;
		t->chm = i->minus;
		t->cho = ((i->o[0] == i->o[1]) ? (uid_t)-1 : i->o[1]);
		t->chg = ((i->g[0] == i->g[1]) ? (gid_t)-1 : i->g[1]);
		chmod_close(i);
	}
}

/*
 * op = Old Path
 * np = New Path
 * on = Old Name
 * nn = New Name
 */
static int _rename(char* const op, char* const np,
		const char* const on, const char* const nn) {
	int err = 0;
	if ((err = append_dir(op, on))) {
		return err;
	}
	if ((err = append_dir(np, nn))) {
		up_dir(op);
		return err;
	}
	if (rename(op, np)) {
		err = errno;
	}
	up_dir(op);
	up_dir(np);
	return err;
}

/*
 */
static int rename_trivial(const char* const wd,
		struct string_list* const S, struct string_list* const R) {
	int err = 0;
	const size_t wdl = strnlen(wd, PATH_MAX_LEN);
	char* op = malloc(wdl+1+NAME_BUF_SIZE);
	if (!op) {
		return ENOMEM;
	}
	char* np = malloc(wdl+1+NAME_BUF_SIZE);
	if (!np) {
		free(op);
		return ENOMEM;
	}
	memcpy(op, wd, wdl+1);
	memcpy(np, wd, wdl+1);
	for (fnum_t f = 0; f < S->len; ++f) {
		if (!S->arr[f]->len) {
			continue;
		}
		if ((err = _rename(op, np, S->arr[f]->str, R->arr[f]->str))) {
			break;
		}
	}
	free(op);
	free(np);
	return err;
}

static int rename_interdependent(const char* const wd,
		struct string_list* const N,
		struct assign* const A, const fnum_t Al) {
	int err = 0;
	const size_t wdl = strnlen(wd, PATH_MAX_LEN);
	char* op = malloc(wdl+1+NAME_BUF_SIZE);
	memcpy(op, wd, wdl+1);
	char* np = malloc(wdl+1+NAME_BUF_SIZE);
	memcpy(np, wd, wdl+1);
	static const char* const tmpdirn = ".hund.rename.tmpdir.";
	char tn[sizeof(tmpdirn)+8];
	snprintf(tn, sizeof(tn), "%s%08x", tmpdirn, getpid());
	fnum_t tc; // Temponary file Content
	for (;;) {
		fnum_t i = 0;
		while (i < Al && A[i].to == (fnum_t)-1) {
			i += 1;
		}
		if (i == Al) break;

		tc = A[i].to;
		const fnum_t tv = A[i].to;
		if ((err = _rename(op, np, N->arr[A[i].from]->str, tn))) {
			break;
		}
		fnum_t from = A[i].from;
		do {
			fnum_t j = (fnum_t)-1;
			for (fnum_t f = 0; f < Al; ++f) {
				if (A[f].to == from) {
					j = f;
					break;
				}
			}
			if (j == (fnum_t)-1) break;
			if ((err = _rename(op, np,
				N->arr[A[j].from]->str, N->arr[A[j].to]->str))) {
				free(op);
				free(np);
				return err;
			}
			from = A[j].from;
			A[j].from = A[j].to = (fnum_t)-1;
		} while (from != tv);
		if ((err = _rename(op, np, tn, N->arr[tc]->str))) {
			break;
		}
		A[i].from = A[i].to = (fnum_t)-1;
	}
	free(op);
	free(np);
	return err;
}

static void cmd_rename(struct ui* const i) {
	int err;
	struct string_list S = { NULL, 0 }; // Selected files
	struct string_list R = { NULL, 0 }; // Renamed files
	struct string_list N = { NULL, 0 };
	struct assign* a = NULL;
	fnum_t al = 0;
	static const struct select_option o[] = {
		{ KUTF8("n"), "no" },
		{ KUTF8("y"), "yes" },
		{ KUTF8("a"), "abort" }
	};
	bool ok = true;
	file_view_selected_to_list(i->pv, &S);
	do {
		if ((err = edit_list(&S, &R))) {
			failed(i, "edit", strerror(err));
			free_list(&S);
			free_list(&R);
			return;
		}
		const char* msg = "There are conflicts. Retry?";
		if (blank_lines(&R)) {
			msg = "File contains blank lines. Retry?";
			ok = false;
		}
		else if (S.len > R.len) {
			msg = "File does not contain enough lines. Retry?";
			ok = false;

		}
		else if (S.len < R.len) {
			msg = "File contains too much lines. Retry?";
			ok = false;
		}
		if ((!ok || !(ok = rename_prepare(i->pv, &S, &R, &N, &a, &al)))
		&& !ui_select(i, msg, o, 2)) {
			free_list(&S);
			free_list(&R);
			return;
		}
	} while (!ok);
	if ((err = rename_trivial(i->pv->wd, &S, &R))
	&& (err = rename_interdependent(i->pv->wd, &N, a, al))) {
		failed(i, "rename", strerror(err));
	}
	free_list(&S);
	free_list(&N);
	free(a);

	if (ui_rescan(i, i->pv, NULL)) {
		select_from_list(i->pv, &R);
	}
	free_list(&R);
}

inline static void cmd_cd(struct ui* const i) {
	// TODO
	// TODO buffers
	int err = 0;
	struct stat s;
	char* path = strncpy(malloc(PATH_BUF_SIZE), i->pv->wd, PATH_BUF_SIZE);
	char* cdp = calloc(PATH_BUF_SIZE, sizeof(char));
	if (prompt(i, cdp, cdp, PATH_MAX_LEN)
	|| (err = cd(path, cdp))
	|| (err = ENOENT, access(path, F_OK))
	|| (stat(path, &s) ? (err = errno) : 0)
	|| (err = ENOTDIR, !S_ISDIR(s.st_mode))) {
		if (err) failed(i, "cd", strerror(err));
	}
	else {
		strncpy(i->pv->wd, path, PATH_BUF_SIZE);
		ui_rescan(i, i->pv, NULL);
	}
	free(path);
	free(cdp);
}

inline static void cmd_mkdir(struct ui* const i) {
	// TODO errors
	int err;
	struct string_list F = { NULL, 0 }; // Files
	const size_t wdl = strnlen(i->pv->wd, PATH_MAX_LEN);
	char* const path = malloc(wdl+1+NAME_BUF_SIZE);
	memcpy(path, i->pv->wd, wdl+1);
	edit_list(&F, &F);
	for (fnum_t f = 0; f < F.len; ++f) {
		if (!F.arr[f]->len) continue;
		if (memchr(F.arr[f]->str, '/', F.arr[f]->len+1)) {
			failed(i, "mkdir", "name contains '/'");
		}
		else if ((err = append_dir(path, F.arr[f]->str))
		|| (mkdir(path, MKDIR_DEFAULT_PERM) ? (err = errno) : 0)) {
			failed(i, "mkdir", strerror(err));
		}
		up_dir(path);
	}
	free(path);
	free_list(&F);
	ui_rescan(i, i->pv, NULL);
}

inline static void cmd_change_sorting(struct ui* const i) {
	// TODO visibility
	// -- SORT -- (message?)
	// ???
	// TODO cursor
	char old[FV_ORDER_SIZE];
	memcpy(old, i->pv->order, FV_ORDER_SIZE);
	char* buf = i->pv->order;
	char* top = i->pv->order + strlen(buf);
	int r;
	ui_draw(i);
	for (;;) {
		r = fill_textbox(i, buf, &top, FV_ORDER_SIZE, NULL);
		if (!r) break;
		else if (r == -1) {
			memcpy(i->pv->order, old, FV_ORDER_SIZE);
			return;
		}
		ui_draw(i);
	}
	for (size_t j = 0; j < FV_ORDER_SIZE; ++j) {
		bool v = false;
		for (size_t k = 0; k < strlen(compare_values); ++k) {
			v = v || (compare_values[k] == i->pv->order[j]);
		}
		if (!v) {
			i->pv->order[j] = 0;
		}
	}
	for (size_t j = 0; j < FV_ORDER_SIZE; ++j) {
		if (i->pv->order[0]) continue;
		memmove(&i->pv->order[0], &i->pv->order[1], FV_ORDER_SIZE-1);
		i->pv->order[FV_ORDER_SIZE-1] = 0;
	}
	file_view_sorting_changed(i->pv);
}

static void cmd_mklnk(struct ui* const i) {
	// TODO conflicts
	//char tmpn[] = "/tmp/hund.XXXXXXXX";
	//int tmpfd = mkstemp(tmpn);
	struct string_list sf = { NULL, 0 };
	file_view_selected_to_list(i->pv, &sf);
	const size_t target_l = strnlen(i->pv->wd, PATH_MAX_LEN);
	const size_t slpath_l = strnlen(i->sv->wd, PATH_MAX_LEN);
	char* target = malloc(target_l+1+NAME_BUF_SIZE);
	char* slpath = malloc(slpath_l+1+NAME_BUF_SIZE);
	memcpy(target, i->pv->wd, target_l+1);
	memcpy(slpath, i->sv->wd, slpath_l+1);
	for (fnum_t f = 0; f < sf.len; ++f) {
		append_dir(target, sf.arr[f]->str);
		append_dir(slpath, sf.arr[f]->str);
		symlink(target, slpath); // TODO err
		up_dir(slpath);
		up_dir(target);
	}
	free(target);
	free(slpath);
	//close(tmpfd);
	//unlink(tmpn);
	ui_rescan(i, i->sv, NULL);
	select_from_list(i->sv, &sf);
}

static void cmd_quick_chmod_plus_x(struct ui* const i) {
	char* path = NULL;
	struct stat s;
	if (!(path = file_view_path_to_selected(i->pv))) return;
	if (stat(path, &s) || !S_ISREG(s.st_mode)) return;
	const mode_t nm = s.st_mode | (S_IXUSR | S_IXGRP | S_IXOTH);
	chmod(path, nm);
	ui_rescan(i, i->pv, NULL);
}

static void process_input(struct ui* const i, struct task* const t,
		struct marks* const m) {
	char *path = NULL, *name = NULL;
	struct file_view* tmp = NULL;
	int err = 0;
	const enum command cmd = get_cmd(i);
	switch (cmd) {
	/* HELP */
	case CMD_HELP_QUIT:
		i->m = MODE_MANAGER;
		break;
	case CMD_HELP_DOWN:
		i->helpy += 1; // TODO
		break;
	case CMD_HELP_UP:
		if (i->helpy > 0) {
			i->helpy -= 1;
		}
		break;
	/* CHMOD */
	case CMD_RETURN:
		chmod_close(i);
		break;
	case CMD_CHANGE:
		prepare_task(i, t, TASK_CHMOD);
		break;
	case CMD_CHOWN:
		/* TODO in $VISUAL */
		name = calloc(LOGIN_BUF_SIZE, sizeof(char));
		if (!prompt(i, name, name, LOGIN_MAX_LEN)) {
			errno = 0;
			struct passwd* pwd = getpwnam(name);
			if (!pwd) {
				err = errno;
				failed(i, "chown", strerror(err));
			}
			else {
				i->o[1] = pwd->pw_uid;
				strncpy(i->user, pwd->pw_name, LOGIN_BUF_SIZE);
			}
		}
		free(name);
		break;
	case CMD_CHGRP:
		/* TODO in $VISUAL */
		name = calloc(LOGIN_BUF_SIZE, sizeof(char));
		if (!prompt(i, name, name, LOGIN_BUF_SIZE)) {
			errno = 0;
			struct group* grp = getgrnam(name);
			if (!grp) {
				err = errno;
				failed(i, "chgrp", strerror(err));
			}
			else {
				i->g[1] = grp->gr_gid;
				strncpy(i->group, grp->gr_name, LOGIN_BUF_SIZE);
			}
		}
		free(name);
		break;

	#define PLUS(m) i->plus |= (m); i->minus &= ~(m);
	#define MINUS(m) i->minus |= (m); i->plus &= ~(m);
	#define RESET(m) i->plus &= ~(m); i->minus &= ~(m);

	case CMD_A_PLUS_R: PLUS(S_IRUSR | S_IRGRP | S_IROTH); break;
	case CMD_A_MINUS_R: MINUS(S_IRUSR | S_IRGRP | S_IROTH); break;

	case CMD_A_PLUS_W: PLUS(S_IWUSR | S_IWGRP | S_IWOTH); break;
	case CMD_A_MINUS_W: MINUS(S_IWUSR | S_IWGRP | S_IWOTH); break;

	case CMD_A_PLUS_X: PLUS(S_IXUSR | S_IXGRP | S_IXOTH); break;
	case CMD_A_MINUS_X: MINUS(S_IXUSR | S_IXGRP | S_IXOTH); break;

	case CMD_U_PLUS_R: PLUS(S_IRUSR); break;
	case CMD_U_MINUS_R: MINUS(S_IRUSR); break;

	case CMD_U_PLUS_W: PLUS(S_IWUSR); break;
	case CMD_U_MINUS_W: MINUS(S_IWUSR) break;

	case CMD_U_PLUS_X: PLUS(S_IXUSR); break;
	case CMD_U_MINUS_X: MINUS(S_IXUSR); break;

	case CMD_U_PLUS_IOX: PLUS(S_ISUID); break;
	case CMD_U_MINUS_IOX: MINUS(S_ISUID); break;

	case CMD_G_PLUS_R: PLUS(S_IRGRP); break;
	case CMD_G_MINUS_R: MINUS(S_IRGRP); break;

	case CMD_G_PLUS_W: PLUS(S_IWGRP); break;
	case CMD_G_MINUS_W: MINUS(S_IWGRP); break;

	case CMD_G_PLUS_X: PLUS(S_IXGRP); break;
	case CMD_G_MINUS_X: MINUS(S_IXGRP); break;

	case CMD_G_PLUS_IOX: PLUS(S_ISGID); break;
	case CMD_G_MINUS_IOX: MINUS(S_ISGID); break;

	case CMD_O_PLUS_R: PLUS(S_IROTH); break;
	case CMD_O_MINUS_R: MINUS(S_IROTH); break;

	case CMD_O_PLUS_W: PLUS(S_IWOTH); break;
	case CMD_O_MINUS_W: MINUS(S_IWOTH); break;

	case CMD_O_PLUS_X: PLUS(S_IXOTH); break;
	case CMD_O_MINUS_X: MINUS(S_IXOTH); break;

	case CMD_O_PLUS_SB: PLUS(S_ISVTX); break;
	case CMD_O_MINUS_SB: MINUS(S_ISVTX); break;

	case CMD_U_ZERO: MINUS(S_IRUSR | S_IWUSR | S_IXUSR); break;
	case CMD_G_ZERO: MINUS(S_IRGRP | S_IWGRP | S_IXGRP); break;
	case CMD_O_ZERO: MINUS(S_IROTH | S_IWOTH | S_IXOTH); break;

	case CMD_U_RESET: RESET(S_IRUSR | S_IWUSR | S_IXUSR); break;
	case CMD_G_RESET: RESET(S_IRGRP | S_IWGRP | S_IXGRP); break;
	case CMD_O_RESET: RESET(S_IROTH | S_IWOTH | S_IXOTH); break;

	#undef PLUS
	#undef MINUS
	#undef RESET

	/* WAIT */
	case CMD_TASK_QUIT:
		//t->done = true; // TODO
		break;
	case CMD_TASK_PAUSE:
		t->ts = TS_PAUSED;
		break;
	case CMD_TASK_RESUME:
		t->ts = TS_RUNNING;
		break;
	/* MANAGER */
	case CMD_QUIT:
		i->run = false;
		break;
	case CMD_HELP:
		i->m = MODE_HELP;
		break;
	case CMD_SWITCH_PANEL:
		tmp = i->pv;
		i->pv = i->sv;
		i->sv = tmp;
		if (!visible(i->pv, i->pv->selection)) {
			first_entry(i->pv);
		}
		break;
	case CMD_DUP_PANEL:
		strncpy(i->sv->wd, i->pv->wd, PATH_BUF_SIZE);
		if (ui_rescan(i, i->sv, NULL)) {
			i->sv->selection = i->pv->selection;
			i->sv->show_hidden = i->pv->show_hidden;
		}
		break;
	case CMD_ENTRY_DOWN:
		jump_n_entries(i->pv, 1);
		break;
	case CMD_ENTRY_UP:
		jump_n_entries(i->pv, -1);
		break;
	case CMD_SCREEN_DOWN:
		jump_n_entries(i->pv, i->ph-1);
		break;
	case CMD_SCREEN_UP:
		jump_n_entries(i->pv, -(i->ph-1));
		break;
	case CMD_ENTER_DIR:
		err = file_view_enter_selected_dir(i->pv);
		if (err && err != ENOTDIR) {
			failed(i, "enter dir", strerror(err));
		}
		break;
	case CMD_UP_DIR:
		if ((err = file_view_up_dir(i->pv))) {
			failed(i, "up dir", strerror(err));
		}
		break;
	case CMD_COPY:
		prepare_task(i, t, TASK_COPY);
		break;
	case CMD_MOVE:
		prepare_task(i, t, TASK_MOVE);
		break;
	case CMD_REMOVE:
		prepare_task(i, t, TASK_REMOVE);
		break;
	case CMD_PAGER:
		name = getenv("PAGER");
		open_selected_with(i, (name ? name : "less"));
		break;
	case CMD_EDIT_FILE:
		open_selected_with(i, get_editor());
		break;
	case CMD_QUICK_PLUS_X:
		cmd_quick_chmod_plus_x(i);
		break;
	case CMD_OPEN_FILE:
		open_selected_with(i, "xdg-open"); // TODO
		break;
	case CMD_CD:
		cmd_cd(i);
		break;
	case CMD_CREATE_DIR:
		cmd_mkdir(i);
		break;
	case CMD_RENAME:
		cmd_rename(i);
		break;
	case CMD_LINK:
		cmd_mklnk(i);
		break;
	case CMD_DIR_VOLUME:
		//estimate_volume_for_selected(i->pv);
		break;
	case CMD_SELECT_FILE:
		if (file_view_select_file(i->pv)) {
			jump_n_entries(i->pv, 1);
		}
		break;
	case CMD_SELECT_ALL:
		i->pv->num_selected = 0;
		for (fnum_t f = 0; f < i->pv->num_files; ++f) {
			if (visible(i->pv, f)) {
				i->pv->file_list[f]->selected = true;
				i->pv->num_selected += 1;
			}
		}
		break;
	case CMD_SELECT_NONE:
		i->pv->num_selected = 0;
		for (fnum_t f = 0; f < i->pv->num_files; ++f) {
			i->pv->file_list[f]->selected = false;
		}
		break;
	case CMD_MARK_NEW:
		marks_input(i, m);
		break;
	case CMD_MARK_JUMP:
		marks_jump(i, m);
		break;
	case CMD_FIND:
		cmd_find(i);
		break;
	case CMD_ENTRY_FIRST:
		first_entry(i->pv);
		break;
	case CMD_ENTRY_LAST:
		last_entry(i->pv);
		break;
	case CMD_CHMOD:
		if ((path = file_view_path_to_selected(i->pv))) {
			chmod_open(i, path);
		}
		else {
			failed(i, "chmod", strerror(ENAMETOOLONG));
		}
		break;
	case CMD_TOGGLE_HIDDEN:
		file_view_toggle_hidden(i->pv);
		break;
	case CMD_REFRESH:
		ui_rescan(i, i->pv, NULL);
		break;
	case CMD_SORT_REVERSE:
		i->pv->scending = -i->pv->scending;
		file_view_sorting_changed(i->pv);
		break;
	case CMD_SORT_CHANGE:
		cmd_change_sorting(i);
		break;
	default:
		break;
	}
}

static void task_progress(struct ui* const i,
		struct task* const t,
		const char* const S) {
	i->mt = MSG_INFO;
	int n = snprintf(i->msg, MSG_BUFFER_SIZE, // TODO
			"%s %d/%df, %d/%dd", S,
			t->files_done, t->files_total,
			t->dirs_done, t->dirs_total);
	if (t->t & (TASK_REMOVE | TASK_MOVE | TASK_COPY)) {
		char sdone[SIZE_BUF_SIZE];
		char stota[SIZE_BUF_SIZE];
		pretty_size(t->size_done, sdone);
		pretty_size(t->size_total, stota);
		snprintf(i->msg+n, MSG_BUFFER_SIZE-n,
			", %s/%s", sdone, stota);
	}
}

/* Return value: false = abort, true = go on */
inline static bool _conflict_policy(struct ui* const i, struct task* const t) {
	char question[64];
	snprintf(question, sizeof(question),
			"There are %d conflicts",
			t->conflicts);
	static const struct select_option o[] = {
		{ KUTF8("s"), "skip" },
		{ KUTF8("o"), "overwrite" },
		{ KUTF8("a"), "abort" }
	};
	switch (ui_select(i, question, o, 3)) {
	case 0:
		t->tf |= TF_SKIP_CONFLICTS;
		break;
	case 1:
		t->tf |= TF_OVERWRITE_CONFLICTS;
		break;
	case 2:
		return false;
	default:
		break;
	}
	return true;
}

inline static bool _symlink_policy(struct ui* const i, struct task* const t) {
	char question[64];
	snprintf(question, sizeof(question),
			"There are %d symlinks", t->symlinks);
	static const struct select_option o[] = {
		{ KUTF8("r"), "raw" },
		{ KUTF8("c"), "recalculate" },
		{ KUTF8("d"), "dereference" },
		{ KUTF8("s"), "skip" },
		{ KUTF8("a"), "abort" }
	};
	switch (ui_select(i, question, o, 5)) {
	case 0:
		t->tf |= TF_RAW_LINKS;
		break;
	case 2:
		t->tf |= TF_DEREF_LINKS;
		break;
	case 3:
		t->tf |= TF_SKIP_LINKS;
		break;
	case 4:
		return false;
	case 1:
	default:
		break;
	}
	return true;
}

inline static bool _confirm_removal(struct ui* const i, struct task* const t) {
	static const struct select_option o[] = {
		{ KUTF8("n"), "no" },
		{ KUTF8("y"), "yes" }
	};
	char question[80];
	snprintf(question, sizeof(question),
			"Remove %u files?", t->files_total);
	switch (ui_select(i, question, o, 2)) {
	case 1: return true;
	default:
	case 0: return false;
	}
}

static void task_execute(struct ui* const i, struct task* const t) {
	// TODO error handling is chaotic
	task_action ta = NULL;
	char msg[512]; // TODO
	static const struct select_option o[] = {
		{ KUTF8("n"), "no" },
		{ KUTF8("y"), "yes" },
	};
	switch (t->ts) {
	case TS_CLEAN:
		break;
	case TS_ESTIMATE:
		i->timeout = 500;
		i->m = MODE_WAIT;
		task_do(t, 1024*10, task_action_estimate, TS_CONFIRM);
		if (t->err) t->ts = TS_FAILED;
		break;
	case TS_CONFIRM:
		t->ts = TS_RUNNING;
		if (t->t == TASK_REMOVE && !_confirm_removal(i, t)) {
			t->ts = TS_FINISHED;
		}
		else if (t->t & (TASK_COPY | TASK_MOVE)) {
			if ((t->conflicts && !_conflict_policy(i, t))
			|| (t->symlinks && !_symlink_policy(i, t))) {
				t->ts = TS_FINISHED;
			}
		}
		break;
	case TS_RUNNING:
		i->timeout = 500;
		if (t->t & (TASK_REMOVE | TASK_COPY | TASK_MOVE)) {
			ta = task_action_copyremove;
		}
		else if (t->t == TASK_CHMOD) {
			ta = task_action_chmod;
		}
		task_do(t, 1024*10, ta, TS_FINISHED);
		task_progress(i, t, ">>");
		break;
	case TS_PAUSED:
		i->timeout = -1;
		task_progress(i, t, "||");
		break;
	case TS_FAILED: // TODO
		snprintf(msg, sizeof(msg), "@ '%s'\r\n(%d) %s. Continue?",
				t->tw.cpath, t->err, strerror(t->err));
		t->err = 0;
		t->ts = (ui_select(i, msg, o, 2) ? TS_RUNNING : TS_FINISHED);
		break;
	case TS_FINISHED:
		i->timeout = -1;
		if (ui_rescan(i, i->pv, i->sv)) {
			if (t->t == TASK_MOVE) {
				jump_n_entries(i->pv, -1);
			}
			i->mt = MSG_INFO;
			snprintf(i->msg, MSG_BUFFER_SIZE,
					"processed %u files & dirs",
					t->files_done+t->dirs_done);
		}
		task_clean(t);
		i->m = MODE_MANAGER;
		break;
	default:
		break;
	}
}

inline static int _init_wd(struct file_view fvs[2], char* const init_wd[2]) {
	int e;
	for (int v = 0; v < 2; ++v) {
		const char* const d = (init_wd[v] ? init_wd[v] : "");
		if (!getcwd(fvs[v].wd, PATH_BUF_SIZE)) {
			memcpy(fvs[v].wd, "/", 2);
			// TODO display error?
		}
		else if ((e = cd(fvs[v].wd, d))
		|| (e = file_view_scan_dir(&fvs[v]))) {
			return e;
		}
		first_entry(&fvs[v]);
	}
	return 0;
}

extern struct ui* I;

int main(int argc, char* argv[]) {
	static const char* const help = \
	"Usage: hund [OPTION...] [left panel] [right panel]\n"
	"Options:\n"
	"  -c, --chdir=PATH      change initial directory\n"
	"  -h, --help            display this help message\n"
	"Type `?` while in hund for more help\n";

	static const char sopt[] = "hc:";
	static const struct option lopt[] = {
		{"chdir", required_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	int o, opti = 0;
	int err;
	while ((o = getopt_long(argc, argv, sopt, lopt, &opti)) != -1) {
		switch (o) {
		case 'c':
			if (chdir(optarg)) {
				err = errno;
				fprintf(stderr, "chdir failed:"
						" %s (%d)\n",
						strerror(err), err);
				exit(EXIT_FAILURE);
			}
			break;
		case 'h':
			printf("%s", help);
			exit(EXIT_SUCCESS);
		default:
			exit(EXIT_FAILURE);
		}
	}

	/* If user passed initial directories in cmdline,
	 * save these paths for later
	 */
	char* init_wd[2] = { NULL, NULL };
	int init_wd_top = 0;
	while (optind < argc) {
		if (init_wd_top < 2) {
			init_wd[init_wd_top] = argv[optind];
			init_wd_top += 1;
		}
		else {
			// Only two panels, only two paths allowed
			fprintf(stderr, "invalid argument:"
					" '%s'\n", argv[optind]);
			exit(EXIT_FAILURE);
		}
		optind += 1;
	}

	struct file_view fvs[2];
	memset(fvs, 0, sizeof(fvs));
	fvs[0].scending = 1;
	memcpy(fvs[0].order, default_order, FV_ORDER_SIZE);
	fvs[1].scending = 1;
	memcpy(fvs[1].order, default_order, FV_ORDER_SIZE);

	struct ui i;
	ui_init(&i, &fvs[0], &fvs[1]);
	if ((err = _init_wd(fvs, init_wd))) {
		fprintf(stderr, "failed to initalize: (%d) %s\n",
				err, strerror(err));
		ui_end(&i);
		exit(EXIT_FAILURE);
	}

	struct task t;
	memset(&t, 0, sizeof(struct task));
	t.in = t.out = -1;

	struct marks m;
	memset(&m, 0, sizeof(struct marks));

	i.mt = MSG_INFO;
	strncpy(i.msg, "Type ? for help and license notice.", MSG_BUFFER_SIZE);
	while (i.run) { // TODO but task may be still running
		ui_draw(&i);
		process_input(&i, &t, &m);
		task_execute(&i, &t);
	}
	for (int v = 0; v < 2; ++v) {
		delete_file_list(&fvs[v]);
	}
	marks_free(&m);
	task_clean(&t);
	ui_end(&i);
	memset(fvs, 0, sizeof(fvs));
	memset(&t, 0, sizeof(struct task));
	exit(EXIT_SUCCESS);
}
