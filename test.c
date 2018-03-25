#ifndef _DEFAULT_SOURCE
	#define _DEFAULT_SOURCE
#endif

#include "test.h"
#include "fs.h"
#include "panel.h"
#include "task.h"
#include "utf8.h"
#include "terminal.h"
#include "ui.h"

int main() {
	SETUP_TESTS;

	SECTION("fs");

	int r;

	char home[PATH_BUF_SIZE];
	strcpy(home, getenv("HOME"));

	char path[PATH_BUF_SIZE] = "/";
	size_t pl = 1;
	r = cd(path, &pl, "~", 1);
	TESTVAL(r, 0, "");
	TESTSTR(path, home, "");

	r = cd(path, &pl, "~/", 2);
	TESTVAL(r, 0, "");
	TESTSTR(path, home, "");

	strcat(home, "/file");
	r = cd(path, &pl, "~/file", 6);
	TESTVAL(r, 0, "");
	TESTSTR(path, home, "");

	strcpy(home, "/~file");
	memset(path, 0, sizeof(path));
	pl = 0;

	r = cd(path, &pl, "~file", 5);
	TESTVAL(r, 0, "");
	TESTSTR(path, home, "");

	memset(path, 0, sizeof(path));
	pl = 0;

	r = cd(path, &pl, "", 0);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/", "");

	r = cd(path, &pl, "", 0);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/", "");

	path[0] = 0;
	pl = 0;
	r = cd(path, &pl, "", 0);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/", "");

	r = cd(path, &pl, "/a/b/", 5);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/a/b", "");

	r = cd(path, &pl, "c/", 2);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/a/b/c", "");

	r = cd(path, &pl, "d/////////////////////////////////e", 35);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/a/b/c/d/e", "");

	r = cd(path, &pl, "/d/////e/////////////////////////////////////////////////////f", 62);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/d/e/f", "");

	r = cd(path, &pl, "//d//e//f", 9);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/d/e/f", "");

	r = cd(path, &pl, "/////////", 9);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/", "");

	r = cd(path, &pl, "home", 4);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/home", "");

	r = cd(path, &pl, "user", 4);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/home/user", "");

	memset(path, '/', 5);
	path[6] = 0;
	pl = 5;
	r = cd(path, &pl, "/////////", 9);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/", "");

	memset(path, 0, sizeof(path));
	pl = 0;
	r = cd(path, &pl, "dir", 3);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/dir", "");

	memset(path, 0, sizeof(path));
	path[0] = '/';
	pl = 1;
	r = cd(path, &pl, "usr", 3);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/usr", "entered directory in root (respect /)");

	r = cd(path, &pl, "bin", 3);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/usr/bin", "correct path");

	r = cd(path, &pl, ".", 1);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/usr/bin", "path unchanged");

	r = cd(path, &pl, "..", 2);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/usr", "");

	r = cd(path, &pl, "..", 2);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/", "");

	r = cd(path, &pl, "..", 2);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/", "");

	r = cd(path, &pl, "lol/../wat", 10);
	TESTVAL(r, 0, "");
	TESTSTR(path, "/wat", "");

	memset(path, 0, sizeof(path));
	path[0] = '/';
	memset(path+1, 'a', PATH_BUF_SIZE-2); // 1 for /, 1 for b
	pl = strlen(path);
	r = cd(path, &pl, "b", 1);
	TESTVAL(r, ENAMETOOLONG, "");
	TESTVAL(strlen(path), PATH_MAX_LEN, "path filled");

	r = cd(path, &pl, "end", 3);
	TESTVAL(r, ENAMETOOLONG, "");
	TESTVAL(strlen(path), PATH_MAX_LEN, "respect PATH_MAX; leave path unchanged");

	r = pushd(path, &pl, "end", 3);
	TESTVAL(r, ENAMETOOLONG, "");
	TESTVAL(strlen(path), PATH_MAX_LEN, "respect PATH_MAX; leave path unchanged");

	TEST(!PATH_IS_RELATIVE("/"), "absolute");
	TEST(!PATH_IS_RELATIVE("/etc/netctl"), "absolute");
	TEST(!PATH_IS_RELATIVE("~/.config"), "absolute");
	TEST(PATH_IS_RELATIVE("./netctl"), "relative");
	TEST(PATH_IS_RELATIVE("etc/netctl"), "relative");
	TEST(PATH_IS_RELATIVE("../netctl"), "relative");
	TEST(PATH_IS_RELATIVE("../"), "relative");
	TEST(PATH_IS_RELATIVE(".."), "relative");
	TEST(PATH_IS_RELATIVE("."), "relative");
	TEST(PATH_IS_RELATIVE("./"), "relative");

	strcpy(path, "/dope/");
	pl = 6;
	char dir5[] = "./wat/lol/../wut";
	cd(path, &pl, dir5, strlen(dir5));
	TESTSTR(path, "/dope/wat/wut", "");

	cd(path, &pl, "/home/user", 10);
	TESTSTR(path, "/home/user", "");

	char path7[PATH_BUF_SIZE] = "/home/user/images/memes";
	size_t p7l = strlen(path7);
	int e = pushd(path7, &p7l, "lol", 3);
	TESTVAL(e, 0, "");
	TESTSTR(path7, "/home/user/images/memes/lol", "");

	memset(path, 0, sizeof(path));
	size_t path_len = 0;
	r = pushd(path, &path_len, "a", 1);
	TESTVAL(r, 0, "no error");
	TESTSTR(path, "/a", "");
	r = pushd(path, &path_len, "b", 1);
	TESTVAL(r, 0, "no error");
	TESTSTR(path, "/a/b", "");
	r = pushd(path, &path_len, "c", 1);
	TESTVAL(r, 0, "no error");
	TESTSTR(path, "/a/b/c", "");
	popd(path, &path_len);
	TESTSTR(path, "/a/b", "");
	popd(path, &path_len);
	TESTSTR(path, "/a", "");
	popd(path, &path_len);
	TESTSTR(path, "/", "");
	popd(path, &path_len);
	TESTSTR(path, "/", "");
	for (size_t i = 0; i < PATH_MAX_LEN/2; ++i) {
		r = pushd(path, &path_len, "a", 1);
	}
	TESTVAL(r, 0, "no error");
	r = pushd(path, &path_len, "a", 1);
	TESTVAL(r, ENAMETOOLONG, "ENAMETOOLONG");
	TESTVAL(path_len, PATH_MAX_LEN-1, "ENAMETOOLONG");


	TESTVAL(imb("1234567", "123"), 3, "");
	TESTVAL(imb("1234567", "023"), 0, "");
	TESTVAL(imb("1234567", "1234567"), 7, "");
	TESTVAL(imb("1034567", "1214567"), 1, "");

	TEST(contains("lol", "lol"), "");
	TEST(contains("lolz", "lol"), "");
	TEST(contains("qqlolz", "lol"), "");
	TEST(contains("qłąkąz", "łąką"), "");
	TEST(!contains("qqloz", "lol"), "");
	TEST(!contains("qqlooolz", "looool"), "");
	TEST(contains("/home/user/lols", "/"), "");
	TEST(contains("/home/user/loł", "ł"), "");
	TEST(contains("anything", ""), "");
	TEST(contains("", ""), "");
	TEST(contains("", "\0"), "");
	TEST(!contains("", "fug"), "");
	TEST(!contains("", "?"), "");

	char buf[SIZE_BUF_SIZE];
	pretty_size(100, buf);
	TESTSTR(buf, "100B", "");
	pretty_size(999, buf);
	TESTSTR(buf, "999B", "");
	pretty_size(1000, buf);
	TESTSTR(buf, "0.97K", "");
	pretty_size(1023, buf);
	TESTSTR(buf, "0.99K", "");
	pretty_size(1024, buf);
	TESTSTR(buf, "1K", "");
	pretty_size(1035, buf);
	TESTSTR(buf, "1.01K", "");
	pretty_size(1024+512, buf);
	TESTSTR(buf, "1.5K", "");
	pretty_size(1024+1023, buf);
	TESTSTR(buf, "1.99K", "");
	pretty_size(2048, buf);
	TESTSTR(buf, "2K", "");

	pretty_size(1024*1000, buf);
	TESTSTR(buf, "0.97M", "");

	pretty_size((off_t) 1, buf);
	TESTSTR(buf, "1B", "");

	pretty_size((off_t) 1<<10, buf);
	TESTSTR(buf, "1K", "");

	pretty_size((off_t) 1<<20, buf);
	TESTSTR(buf, "1M", "");

	pretty_size((off_t) 1<<30, buf);
	TESTSTR(buf, "1G", "");

	pretty_size((off_t) 1<<40, buf);
	TESTSTR(buf, "1T", "");

	pretty_size((off_t) 1<<50, buf);
	TESTSTR(buf, "1P", "");

	pretty_size((off_t) 1<<60, buf);
	TESTSTR(buf, "1E", "");

	off_t s = (off_t) 1 << 60;
	s += (off_t) 1 << 59;
	pretty_size(s, buf);
	TESTSTR(buf, "1.5E", "");
	s += (off_t) 1 << 58;
	pretty_size(s, buf);
	TESTSTR(buf, "1.75E", "");
	s += (off_t) 1 << 57;
	pretty_size(s, buf);
	TESTSTR(buf, "1.87E", "");
	s = 0x7fffffffffffffff;
	pretty_size(s, buf);
	TESTSTR(buf, "7.99E", "");

	END_SECTION("fs");


	SECTION("utf8");

	struct test_utf8_pair {
		char* b;
		codepoint_t cp;
	} tup[] = {
		{ .b = " ", .cp = 0x20 },
		{ .b = "@", .cp = 0x40 },
		{ .b = "ł", .cp = 0x0142 },
		// TODO fill with some sample glyphs
	};

	for (unsigned i = 0; i < sizeof(tup)/sizeof(struct test_utf8_pair); ++i) {
		TESTVAL(tup[i].cp, utf8_b2cp(tup[i].b), "sample pairs of glyphs and codepoints match");
	}

	bool symmetric = true;
	char b[4];
	for (codepoint_t cp = 0; cp < 0x20000; ++cp) {
		if (!utf8_cp2nb(cp)) continue;
		utf8_cp2b(b, cp);
		codepoint_t cp2 = utf8_b2cp(b);
		symmetric = symmetric && (cp2 == cp) &&
			(utf8_cp2nb(cp) == utf8_cp2nb(cp2));
	}
	TEST(symmetric, "cp2b and b2cp are symmetric");

	struct string_and_width {
		char* s;
		size_t w;
	} svs[] = {
		{ "\xe2\x80\x8b", 0 },
		// TODO more
		{ "", 0 },
		{ "", 0 },
		{ "\0", 0 },
		{ "\x1b", 0 },
		{ " ", 1 },
		{ "  ", 2 },
		{ "wat", 3 },
		{ "łąć", 3 },
		{ "~!@#$%^&*()_+", 13 },
		{ "`1234567890-=", 13 },
		{ "[]\\;',./", 8 },
		{ "{}|:\"<>?", 8 },
		{ "qwertyuiop", 10 },
		{ "πœę©ß←↓→óþ", 10 },
		{ "asdfghjkl", 9 },
		{ "ąśðæŋ’ə…ł", 9 },
		{ "zxcvbnm", 7 },
		{ "żźć„”ńµ", 7 },
		{ "¬≠²³¢€½§·«»–≤≥", 14 },
		{ "∨¡¿£¼‰∧≈¾±°—", 12 },
		{ "ΩŒĘ®™¥↑↔ÓÞĄŚÐÆŊ•ƏŻŹĆ‘“Ń∞", 24 },
		{ "÷", 1 },
		{ "ascii is cool", 13 },
		{ "Pchnąć w tę łódź jeża lub ośm skrzyń fig.", 41 },
		{ "Zwölf große Boxkämpfer jagen Viktor quer über den Sylter Deich.", 63 },
		{ "Любя, съешь щипцы, — вздохнёт мэр, — кайф жгуч.", 47 },
		{ "Nechť již hříšné saxofony ďáblů rozzvučí síň úděsnými tóny waltzu, tanga a quickstepu.", 86 },
		{ "Eble ĉiu kvazaŭdeca fuŝĥoraĵo ĝojigas homtipon.", 47 },
		{ "Γαζίες καὶ μυρτιὲς δὲν θὰ βρῶ πιὰ στὸ χρυσαφὶ ξέφωτο.", 53 },
	};

	for (size_t i = 0; i < sizeof(svs)/sizeof(struct string_and_width); ++i) {
		TEST(utf8_width(svs[i].s) <= strlen(svs[i].s), "apparent width <= length; loose, but always true");
		TESTVAL(utf8_width(svs[i].s), svs[i].w, "");
		TESTVAL(utf8_width(svs[i].s), utf8_wtill(svs[i].s, svs[i].s+strlen(svs[i].s)), "");
		TEST(utf8_validate(svs[i].s), "all valid strings are valid");
	}

	char* sis[] = {
		"\xf0", // byte says 4 bytes, but there is only one
		"\xe0",
		"\xb0",
		// ff, c0, c1, fe, ff are never used in utf-8
		"\xff",
		"\xc0",
		"\xc1",
		"\xfe",
		"\xff",
		"\x20\xac", // Euro sign in utf-16, big endian
		"\xac\x20", // Euro sign in utf-16, little endian
	};
	for (size_t i = 0; i < sizeof(sis)/sizeof(char*); ++i) {
		TEST(!utf8_validate(sis[i]), "all invalid strings are invalid");
	}

	TESTVAL(utf8_w2nb("łaka łaką", 2), 3, "");
	TESTVAL(utf8_w2nb("qq", 2), 2, "");
	TESTVAL(utf8_w2nb("", 2), 0, "");
	TESTVAL(utf8_w2nb("a", 0), 0, "");
	TESTVAL(utf8_w2nb("a", 666), 1, "");
	TESTVAL(utf8_w2nb("łłłłł", 666), 10, "");

	char inserted[20] = "łąkała";
	utf8_insert(inserted, "ń", 2);
	TESTSTR(inserted, "łąńkała", "");
	utf8_insert(inserted, "ł", 5);
	TESTSTR(inserted, "łąńkałła", "");
	utf8_remove(inserted, 0);
	TESTSTR(inserted, "ąńkałła", "");
	utf8_remove(inserted, 4);
	TESTSTR(inserted, "ąńkała", "");
	utf8_remove(inserted, 4);
	TESTSTR(inserted, "ąńkaa", "");
	TESTVAL(utf8_wtill(inserted, inserted+5), 3, "");

	char inv[NAME_MAX];
	cut_unwanted("works", inv, 'x', NAME_MAX);
	TESTSTR(inv, "works", "");
	cut_unwanted("\xffwor\x1b\b\a\nks", inv, '.', NAME_MAX);
	TESTSTR(inv, ".wor....ks", "");

	END_SECTION("utf8");

	SECTION("task");


	struct task t;
	memset(&t, 0, sizeof(struct task));
	char result[PATH_BUF_SIZE];
	static char* bnp[][6] = {
		{ "/home/user/doc/dir/file.txt",
			"/home/user/doc", "/home/user/.trash",
			"dir", "repl",
			"/home/user/.trash/repl/file.txt"
		},
		{ "/home/user/doc/dir/file.txt",
			"/home/user/doc", "/b",
			"dir", "repl",
			"/b/repl/file.txt"
		},
		{ "/aaa/file.txt",
			"/", "/home/user",
			"aaa", "b",
			"/home/user/b/file.txt"
		},
		{ "/a/b/c/d/e/f/file.txt",
			"/a/b/c", "/home",
			"d", "xxx",
			"/home/xxx/e/f/file.txt"
		},
		{ "/a/b/c/d/e/f/file.txt",
			"/a/b/c", "/home",
			"d", NULL,
			"/home/d/e/f/file.txt"
		},
		{ "/home/user/doc/dir/file.txt",
			"/home/user/doc", "/home/user/.trash",
			NULL, NULL,
			"/home/user/.trash/dir/file.txt"
		},
		{ "/home/user/doc/dir",
			"/home/user", "/home/root/.trash",
			NULL, NULL,
			"/home/root/.trash/doc/dir"
		},
		{ "/home/root",
			"/home", "/",
			NULL, NULL,
			"/root"
		},
		{ "/home/boot/deep/file.txt",
			"/home", "/",
			"boot", "shoe",
			"/shoe/deep/file.txt"
		},
		{ "/root",
			"/", "/home",
			NULL, NULL,
			"/home/root"
		},
		{ "/root/doot.txt",
			"/", "/",
			"root", "boot",
			"/boot/doot.txt"
		},
		{ "/root/file.txt",
			"/root", "/wazzup",
			NULL, NULL,
			"/wazzup/file.txt"
		},
		{ "/roooooot/file.txt",
			"/roooooot", "/q",
			NULL, NULL,
			"/q/file.txt"
		},
		{ "/file.txt",
			"/", "/",
			NULL, NULL,
			"/file.txt"
		},
		{ "/dir/file.txt",
			"/dir", "/dir",
			NULL, NULL,
			"/dir/file.txt"
		},
		{ "/gear/bear",
			"/", "/boot",
			"gear", "beer",
			"/boot/beer/bear"
		},
	};
	for (size_t i = 0; i < sizeof(bnp)/sizeof(bnp[0]); ++i) {
		t.tw.path = bnp[i][0];
		t.tw.pathlen = strlen(bnp[i][0]);
		t.src = bnp[i][1];
		t.dst = bnp[i][2];
		list_push(&t.sources, bnp[i][3], -1);
		list_push(&t.renamed, bnp[i][4], -1);
		r = task_build_path(&t, result);
		list_free(&t.sources);
		list_free(&t.renamed);
		TESTSTR(result, bnp[i][5], "");
		TESTVAL(r, 0, "");
	}
	t.tw.path = calloc(PATH_BUF_SIZE, 1);
	memcpy(t.tw.path, "/", 1);
	memset(t.tw.path+1, 'a', PATH_MAX_LEN-1);
	t.tw.pathlen = strlen(t.tw.path);
	t.src = "/";
	t.dst = "/r";
	list_push(&t.sources, NULL, -1);
	list_push(&t.renamed, NULL, -1);
	r = task_build_path(&t, result);
	TESTSTR(result, "", "");
	TESTVAL(r, ENAMETOOLONG, "");
	list_free(&t.sources);
	list_free(&t.renamed);
	free(t.tw.path);

	END_SECTION("task");


	SECTION("terminal");

	struct append_buffer ab;
	memset(&ab, 0, sizeof(struct append_buffer));
	ab.buf = calloc(100, sizeof(char));
	ab.capacity = 100;

	append(&ab, "lol", 3);
	append(&ab, "pls", 3);
	append(&ab, "pls", 0);
	TEST(!memcmp(ab.buf, "lolpls", 6), "");
	TESTVAL(ab.top, 6, "");
	TESTVAL(ab.capacity, 100, "");

	fill(&ab, ';', 3);
	TEST(!memcmp(ab.buf, "lolpls;;;", 9), "");
	TESTVAL(ab.top, 9, "");
	TESTVAL(ab.capacity, 100, "");

	fill(&ab, '.', 91);
	TESTVAL(ab.capacity, 100, "");

	fill(&ab, ' ', 1);
	TESTVAL(ab.capacity, 100+APPEND_BUFFER_INC, "");

	free(ab.buf);
	memset(&ab, 0, sizeof(struct append_buffer));
	append(&ab, "?", 1);
	TESTVAL(ab.capacity, APPEND_BUFFER_INC, "");
	append(&ab, path, APPEND_BUFFER_INC-1);
	TESTVAL(ab.capacity, APPEND_BUFFER_INC, "");
	append(&ab, "?", 1);
	TESTVAL(ab.capacity, APPEND_BUFFER_INC*2, "");

	END_SECTION("terminal");
	END_TESTS;
}
