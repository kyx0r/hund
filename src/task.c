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

#include "include/task.h"

void task_new(struct task* t, enum task_type tp,
		utf8* src, utf8* dst, utf8* newname) {
	*t = (struct task) {
		.s = TASK_STATE_GATHERING_DATA,
		.t = tp,
		.running = true,
		.src = src, // Full path of source file/dir
		.dst = dst, // Contains working directory of the other panel
		.newname = newname, // Used for conflicts
		.checklist = NULL,
		.in = -1,
		.out = -1,
		.size_total = 0,
		.size_done = 0,
		.files_total = 0,
		.files_done = 0,
		.dirs_total = 0,
		.dirs_done = 0
	};
}

static void _push_file_todo(struct task* t,
		struct stat s, utf8* p, enum todo td) {
	struct file_todo* ft = malloc(sizeof(struct file_todo));
	*ft = (struct file_todo) {
		.next = t->checklist,
		.td = td,
		.path = p,
		.s = s,
		.progress = 0
	};
	t->checklist = ft;
	// TODO symlinks
	if (S_ISDIR(s.st_mode)) {
		if ((t->t == TASK_RM && td == TODO_REMOVE) ||
				(td == TODO_COPY && (t->t == TASK_COPY || t->t == TASK_MOVE))) {
			t->dirs_total += 1;
		}
	}
	else {
		if ((t->t == TASK_RM && td == TODO_REMOVE) ||
				(td == TODO_COPY && (t->t == TASK_COPY || t->t == TASK_MOVE))) {
			t->files_total += 1;
			t->size_total += s.st_size;
		}
	}
}

/* removing:
 * 	a list of files to remove,
 * 	with direcories after their contents
 * 	(cannot rmdir a non-empty directory)
 *
 * copying:
 * 	a list of files to copy,
 * 	with direcories before their contents
 * 	(cannot create file in non-existent directory)
 *
 * moving:
 * 	1. create (copy) new directory
 * 	2. copy all it's contents to new directory
 * 	3. remove old contents
 * 	4. remove old directory
 *
 * TODO share paths to minimize memory usage when moving files
 * TODO symlinks
 */
static int _build_file_list(struct task* t, utf8* path, enum task_type tt) {
	int r = 0;
	struct stat s;
	if (lstat(path, &s)) return errno;
	if (tt == TASK_RM || tt == TASK_MOVE) _push_file_todo(t, s, path, TODO_REMOVE);
	if (S_ISDIR(s.st_mode)) {
		DIR* dir = opendir(path);
		struct dirent* de;
		while ((de = readdir(dir)) != NULL) {
			if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
				continue;
			}
			utf8* fpath = malloc(strlen(path)+1+strlen(de->d_name)+1);
			strcpy(fpath, path);
			strcat(fpath, "/");
			strcat(fpath, de->d_name);
			struct stat ss;
			if (lstat(fpath, &ss)) {
				free(fpath);
				return errno;
			}
			if (S_ISDIR(ss.st_mode)) {
				r |= _build_file_list(t, fpath, tt);
			}
			else {
				if (tt == TASK_RM) {
					_push_file_todo(t, ss, fpath, TODO_REMOVE);
				}
				else if (tt == TASK_COPY) {
					_push_file_todo(t, ss, fpath, TODO_COPY);
				}
				else if (tt == TASK_MOVE) {
					_push_file_todo(t, ss, fpath, TODO_REMOVE);
					_push_file_todo(t, ss, strdup(fpath), TODO_COPY);
				}
			}
		}
		closedir(dir);
	}
	if (tt == TASK_COPY || tt == TASK_MOVE) {
		if (tt == TASK_MOVE) path = strdup(path);
		_push_file_todo(t, s, path, TODO_COPY);
	}
	return r;
}

int task_build_file_list(struct task* t) {
	utf8* path = malloc(strlen(t->src)+1);
	strcpy(path, t->src);
	int r = _build_file_list(t, path, t->t);
	return r;
}

void task_file_done(struct task* t) {
	if (!t->checklist) return;
	struct file_todo* head = t->checklist;
	t->checklist = head->next;
	if ((head->td == TODO_REMOVE && t->t == TASK_RM) ||
		(head->td == TODO_COPY &&
			 (t->t == TASK_MOVE || t->t == TASK_COPY))) {
		if (S_ISDIR(head->s.st_mode)) {
			t->dirs_done += 1;
		}
		else {
			t->files_done += 1;
		}
	}
	free(head->path);
	free(head);
}

void task_clean(struct task* t) {
	if (t->src) free(t->src);
	if (t->dst) free(t->dst);
	if (t->newname) free(t->newname);
	t->src = NULL;
	t->dst = NULL;
	t->newname = NULL;
	struct file_todo* n = t->checklist;
	struct file_todo* p;
	while (n) {
		p = n;
		n = n->next;
		free(p->path);
		free(p);
	}
	t->s = TASK_STATE_CLEAN;
	t->t = TASK_NONE;
	t->checklist = NULL;
	if (t->in != -1) close(t->in);
	t->in = -1;
	if (t->out != -1) close(t->out);
	t->out = -1;
}

static inline int _close_inout(struct task* t) {
	int r = 0; // TODO
	r |= close(t->in);
	r |= close(t->out);
	t->in = -1;
	t->out = -1;
	return r;
}

static inline int _copy_some(struct task* t, utf8* npath, void* buf, int* c) {
	if (t->out == -1 || t->in == -1) {
		t->out = open(t->checklist->path, O_RDONLY);
		if (t->out == -1) return errno;
		t->in = open(npath, O_CREAT | O_WRONLY | t->checklist->s.st_mode);
		if (t->in == -1) return errno;

		struct stat outs;
		fstat(t->out, &outs);
		int e = posix_fallocate(t->in, 0, outs.st_size);
		// TODO detect earlier if fallocate is supported
		if (e != EOPNOTSUPP && e != ENOSYS) return errno;
	}
	ssize_t wb = -1, rb = -1;
	while (*c > 0) {
		rb = read(t->out, buf, BUFSIZ);
		if (!rb) {
			task_file_done(t);
			return _close_inout(t);
		}
		if (rb == -1) {
			return errno | _close_inout(t);
		}
		wb = write(t->in, buf, rb);
		if (wb == -1) {
			return errno | _close_inout(t);
		}
		t->size_done += wb;
		t->checklist->progress += wb;
		*c -= 1;
	}
	return 0;
}

/* Finds SUBString at the begining of STRing and changes it ti REPLacement */
bool substitute(char* str, char* subs, char* repl) {
	const size_t subs_l = strlen(subs);
	const size_t str_l = strlen(str);
	if (subs_l > str_l) return false;
	if (memcmp(str, subs, subs_l)) return false;
	const size_t repl_l = strlen(repl);
	memmove(str+repl_l, str+subs_l, str_l-subs_l+1);
	memcpy(str, repl, repl_l);
	return true;
}

/* How does hund determine paths of files when performing operations?
 * (see build_file_list() too)
 *
 * removing:
 * 		Just rmdir/unlink given path
 * 		src:       "/home/user/unwanted-file.txt"
 * 		checklist: "/home/user/unwanted-file.txt"
 * 		unlink("/home/user/unwanted-file.txt");
 *
 * 		src:       "/home/user/unwanted-dir"
 * 		checklist: "/home/user/unwanted-dir/a/deep/tree"
 * 		(if dir is empty or emptied)
 * 		rmdir("/home/user/unwanted-dir/a/deep/tree");
 *
 * moving/copying
 * (no name conflicts, newname == NULL):
 *		- Prepare replacement path;
 *		  src with cut out target file/dir together with /
 *		src  "/home/user/doc/file.pdf"
 *		repl "/home/user/doc"
 *
 *		src  "/home/user/dir"
 *		repl "/home/user"
 *		- Checklist will contain full and absolute path to file.
 *		  Now replace one and first occurence of `repl` in checklist path
 *		  with dst. (dst is always a directory)
 *		checklist: "/home/user/doc/file.pdf"
 *		src:       "/home/user/doc/file.pdf"
 *		repl:      "/home/user/doc"
 *		dst:       "/home/guest/gifts"
 *		new_path:  "/home/guest/gifts/file.pdf"
 *
 *		checklist: "/home/user/dir/a/very/deep/tree"
 *		src:       "/home/user/dir"
 *		repl:      "/home/user"
 *		dst:       "/home/guest/gifts"
 *		new_path:  "/home/guest/gifts/dir/a/very/deep/tree"
 *
 * moving/copying
 * (conflicting names, user is prompted for new name, newname != NULL):
 *		- Now repl == src
 *		- Append new name to dst
 *		  (dst may now point a file, but otherwise it's always a dir)
 *		- Do as previously
 *		checklist: "/home/user/doc/file.pdf"
 *		src:       "/home/user/doc/file.pdf"
 *		repl:      "/home/user/doc/file.pdf"
 *		new_name:  "doc.pdf"
 *		dst:       "/home/guest/gifts/doc.pdf"
 *		new_path:  "/home/guest/gifts/doc.pdf"
 *
 *		checklist: "/home/user/dir/a/very/deep/tree"
 *		src:       "/home/user/dir"
 *		repl:      "/home/user/dir"
 *		new_name:  "folder"
 *		dst:       "/home/guest/gifts/folder"
 *		new_path:  "/home/guest/gifts/folder/a/very/deep/tree"
 *
 * When new_path is determined:
 *      copying = copy checklist -> new_path
 *		moving = copy checklist -> new_path + remove checklist
 *
 * TODO block doing anything to block devices, sockets and other non-regular files
 */
utf8* build_new_path(struct task* t, utf8* cp) {
	utf8* new_path = malloc(PATH_MAX);
	strcpy(new_path, cp);
	if (t->newname) { // name colission; using new name
		utf8* _dst = malloc(PATH_MAX); // TODO static
		strcpy(_dst, t->dst);
		strcat(_dst, "/");
		strcat(_dst, t->newname);
		substitute(new_path, t->src, _dst);
		free(_dst);
	}
	else { // name stays the same
		const size_t repll = current_dir_i(t->src)-1;
		utf8* repl = malloc(repll);
		strncpy(repl, t->src, repll);
		repl[repll] = 0;
		substitute(new_path, repl, t->dst);
		free(repl);
	}
	return new_path;
}

int do_task(struct task* t, int c) {
	while (t->checklist && c > 0) {
		mode_t ftype = (t->checklist->s.st_mode & S_IFMT);
		if (ftype == S_IFBLK || ftype == S_IFCHR ||
				ftype == S_IFIFO || ftype == S_IFSOCK) {
			task_file_done(t);
			c -= 1;
			continue;
		}
		bool isdir = S_ISDIR(t->checklist->s.st_mode);
		bool islnk = (t->checklist->s.st_mode & S_IFMT) == S_IFLNK;
		if (islnk) {
			if (t->checklist->td == TODO_COPY) {
				utf8* npath = build_new_path(t, t->checklist->path);
				link_copy(t->src, t->checklist->path, npath);
				task_file_done(t);
				free(npath);
				c -= 1;
			}
			else {
				if (unlink(t->checklist->path)) return errno;
				task_file_done(t);
				c -= 1;
			}
		}
		else if (isdir) {
			if (t->checklist->td == TODO_COPY) {
				utf8* npath = build_new_path(t, t->checklist->path);
				if (mkdir(npath, t->checklist->s.st_mode)) return errno;
				task_file_done(t);
				free(npath);
				c -= 1;
			}
			else {
				if (rmdir(t->checklist->path)) return errno;
				task_file_done(t);
				c -= 1;
			}
		}
		else { // regular file
			if (t->checklist->td == TODO_COPY) {
				utf8* npath = build_new_path(t, t->checklist->path);
				char buf[BUFSIZ];
				int e = _copy_some(t, npath, buf, &c);
				free(npath);
				if (e) return e;
			}
			else {
				if (unlink(t->checklist->path)) return errno;
				t->size_done += t->checklist->s.st_size;
				task_file_done(t);
				c -= 1;
			}
		}
	}
	if (!t->checklist) t->s = TASK_STATE_FINISHED;
	return 0;
}
