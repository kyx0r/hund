#ifndef _DEFAULT_SOURCE
	#define _DEFAULT_SOURCE
#endif

#include "test.h"
#include "../src/include/fs.h"
#include "../src/include/file_view.h"
#include "../src/include/task.h"
#include "../src/include/utf8.h"
#include "../src/include/terminal.h"
#include "../src/include/ui.h"

int main() {
	SETUP_TESTS;

	SECTION("fs");

	int r;

	/*char path0[PATH_MAX] = "/home/user/.config/fancyproggie";
	char home0[PATH_MAX] = "/home/user";
	r = prettify_path(path0, home0);
	TEST(r == 0 && strcpy(path0, "~/.config/fancyproggie"), "path changed");

	char path1[PATH_MAX] = "/etc/X11/xinit/xinitrc.d";
	r = prettify_path(path1, home0);
	TEST(r == -1 && strcmp(path1, "/etc/X11/xinit/xinitrc.d") == 0, "path remained unchanged");*/

	char path[PATH_MAX] = "/";
	r = enter_dir(path, "usr");
	TEST(r == 0 && strcmp(path, "/usr") == 0, "entered directory in root (respect /)");

	r = enter_dir(path, "bin");
	TEST(r == 0 && strcmp(path, "/usr/bin") == 0, "correct path");

	r = enter_dir(path, ".");
	TEST(r == 0 && strcmp(path, "/usr/bin") == 0, "path unchanged");

	r = enter_dir(path, "..");
	TEST(r == 0 && strcmp(path, "/usr") == 0, "up_dir used");

	memset(path, 0, sizeof(path));
	path[0] = '/';
	memset(path+1, 'a', PATH_MAX-3); // 1 to leave null-terminator, 1 for /, 1 for b
	r = enter_dir(path, "b");
	TEST(r == 0 && strlen(path) == PATH_MAX, "path filled");

	r = enter_dir(path, "end");
	TEST(r && strlen(path) == PATH_MAX, "respect PATH_MAX; leave path unchanged");
	r = append_dir(path, "end");
	TEST(r && strlen(path) == PATH_MAX, "respect PATH_MAX; leave path unchanged");

	/*char path4[PATH_MAX] = "/bin";
	char cd[NAME_MAX];
	current_dir(path4, cd);
	TEST(strcmp(cd, "bin") == 0, "");

	enter_dir(path4, "wat");
	current_dir(path4, cd);
	TEST(strcmp(cd, "wat") == 0, "");

	strcpy(path4, "/");
	current_dir(path4, cd);
	TEST(strcmp(cd, "/") == 0, "");*/

	TEST(!path_is_relative("/"), "absolute");
	TEST(!path_is_relative("/etc/netctl"), "absolute");
	TEST(!path_is_relative("~/.config"), "absolute");
	TEST(path_is_relative("./netctl"), "relative");
	TEST(path_is_relative("etc/netctl"), "relative");
	TEST(path_is_relative("../netctl"), "relative");
	TEST(path_is_relative("../"), "relative");
	TEST(path_is_relative(".."), "relative");
	TEST(path_is_relative("."), "relative");
	TEST(path_is_relative("./"), "relative");

	strcpy(path, "/dope/");
	char dir5[] = "./wat/lol/../wut";
	enter_dir(path, dir5);
	TEST(strcmp(path, "/dope/wat/wut") == 0, "");

	enter_dir(path, "/home/user");
	TEST(strcmp(path, "/home/user") == 0, "");

	/*char path6[PATH_MAX];
	enter_dir(path6, "~");
	struct passwd* pwd = get_pwd();
	TEST(strcmp(path6, pwd->pw_dir) == 0, "");*/

	char path7[PATH_MAX+1] = "/home/user/images/memes";
	int e = append_dir(path7, "lol");
	TEST(e == 0 && !strcmp(path7, "/home/user/images/memes/lol"), "");

	TEST(imb("1234567", "123") == 3, "");
	TEST(imb("1234567", "023") == 0, "");
	TEST(imb("1234567", "1234567") == 7, "");
	TEST(imb("1034567", "1214567") == 1, "");

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
	TEST(!strcmp(buf, "100B"), "");
	pretty_size(1023, buf);
	TEST(!strcmp(buf, "1023B"), "");
	pretty_size(1024, buf);
	TEST(!strcmp(buf, "1K"), "");
	pretty_size(1035, buf);
	TEST(!strcmp(buf, "1.01K"), "");
	pretty_size(1024+512, buf);
	TEST(!strcmp(buf, "1.5K"), "");
	pretty_size(1024+1023, buf);
	TEST(!strcmp(buf, "1.99K"), "");
	pretty_size(2048, buf);
	TEST(!strcmp(buf, "2K"), "");

	pretty_size((off_t) 1, buf);
	TEST(!strcmp(buf, "1B"), "");

	pretty_size((off_t) 1<<10, buf);
	TEST(!strcmp(buf, "1K"), "");

	pretty_size((off_t) 1<<20, buf);
	TEST(!strcmp(buf, "1M"), "");

	pretty_size((off_t) 1<<30, buf);
	TEST(!strcmp(buf, "1G"), "");

	pretty_size((off_t) 1<<40, buf);
	TEST(!strcmp(buf, "1T"), "");

	pretty_size((off_t) 1<<50, buf);
	TEST(!strcmp(buf, "1P"), "");

	pretty_size((off_t) 1<<60, buf);
	TEST(!strcmp(buf, "1E"), "");

	off_t s = (off_t) 1 << 60;
	s += (off_t) 1 << 59;
	pretty_size(s, buf);
	TEST(!strcmp(buf, "1.5E"), "");
	s += (off_t) 1 << 58;
	pretty_size(s, buf);
	TEST(!strcmp(buf, "1.75E"), "");
	s += (off_t) 1 << 57;
	pretty_size(s, buf);
	TEST(!strcmp(buf, "1.87E"), "");
	s = 0x7fffffffffffffff;
	pretty_size(s, buf);
	TEST(!strcmp(buf, "7.99E"), "");

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
		TEST(tup[i].cp == utf8_b2cp(tup[i].b), "sample pairs of glyphs and codepoints match");
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
		{ "\x1b", 1 },
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
		TEST(utf8_width(svs[i].s) == svs[i].w, "");
		TEST(utf8_width(svs[i].s) == utf8_wtill(svs[i].s, svs[i].s+strlen(svs[i].s)), "");
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

	TEST(utf8_w2nb("łaka łaką", 2) == 3, "");
	TEST(utf8_w2nb("qq", 2) == 2, "");
	TEST(utf8_w2nb("", 2) == 0, "");
	TEST(utf8_w2nb("a", 0) == 0, "");
	TEST(utf8_w2nb("a", 666) == 1, "");
	TEST(utf8_w2nb("łłłłł", 666) == 10, "");

	char inserted[20] = "łąkała";
	utf8_insert(inserted, "ń", 2);
	TEST(!strcmp(inserted, "łąńkała"), "");
	utf8_insert(inserted, "ł", 5);
	TEST(!strcmp(inserted, "łąńkałła"), "");
	utf8_remove(inserted, 0);
	TEST(!strcmp(inserted, "ąńkałła"), "");
	utf8_remove(inserted, 4);
	TEST(!strcmp(inserted, "ąńkała"), "");
	utf8_remove(inserted, 4);
	TEST(!strcmp(inserted, "ąńkaa"), "");
	TEST(utf8_wtill(inserted, inserted+5) == 3, "");

	char inv[NAME_MAX];
	cut_unwanted("works", inv, 'x', NAME_MAX);
	TEST(!strcmp(inv, "works"), "");
	cut_unwanted("\xffwor\e\b\a\nks", inv, '.', NAME_MAX);
	TEST(!strcmp(inv, ".wor....ks"), "");

	END_SECTION("utf8");

	SECTION("task");

	char* result = malloc(PATH_MAX);
	build_new_path("/home/user/doc/dir/file.txt",
			"/home/user/doc", "/home/user/.trash",
			"dir", "repl", result);
	TEST(!strcmp(result, "/home/user/.trash/repl/file.txt"), "");

	build_new_path("/home/user/doc/dir/file.txt",
			"/home/user/doc", "/home/user/.trash", "doc", NULL, result);
	TEST(!strcmp(result, "/home/user/.trash/dir/file.txt"), "");

	build_new_path("/home/user/doc/dir",
			"/home/user", "/home/root/.trash", "user", NULL, result);
	TEST(!strcmp(result, "/home/root/.trash/doc/dir"), "");

	build_new_path("/root",
			"/", "/home", "root", NULL, result);
	TEST(!strcmp(result, "/home/root"), "");

	free(result);

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
	TEST(ab.top == 6, "");
	TEST(ab.capacity == 100, "");

	fill(&ab, ';', 3);
	TEST(!memcmp(ab.buf, "lolpls;;;", 9), "");
	TEST(ab.top == 9, "");
	TEST(ab.capacity == 100, "");

	fill(&ab, '.', 91);
	TEST(ab.capacity == 100, "");

	fill(&ab, ' ', 1);
	TEST(ab.capacity == 101, "");

	free(ab.buf);

	END_SECTION("terminal");

	END_TESTS;
}
