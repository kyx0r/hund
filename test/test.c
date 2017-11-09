#include "test.h"
#include "../src/include/path.h"
#include "../src/include/utf8.h"

int main() {
	SETUP_TESTS;

	SECTION("path");

	char path0[PATH_MAX] = "/home/user/.config/fancyproggie";
	char home0[PATH_MAX] = "/home/user";
	int r = prettify_path(path0, home0);
	TEST(r == 0 && strcpy(path0, "~/.config/fancyproggie"), "path changed");

	char path1[PATH_MAX] = "/etc/X11/xinit/xinitrc.d";
	r = prettify_path(path1, home0);
	TEST(r == -1 && strcmp(path1, "/etc/X11/xinit/xinitrc.d") == 0, "path remained unchanged");

	char path2[PATH_MAX] = "/";
	r = enter_dir(path2, "usr");
	TEST(r == 0 && strcmp(path2, "/usr") == 0, "entered directory in root (respect /)");

	r = enter_dir(path2, "bin");
	TEST(r == 0 && strcmp(path2, "/usr/bin") == 0, "correct path");

	r = enter_dir(path2, ".");
	TEST(r == 0 && strcmp(path2, "/usr/bin") == 0, "path unchanged");

	r = enter_dir(path2, "..");
	TEST(r == 0 && strcmp(path2, "/usr") == 0, "up_dir used");

	char path3[PATH_MAX*2] = "/";
	memset(path3+1, 0, sizeof(path3) - 1);
	memset(path3+1, 'a', PATH_MAX-3);
	r = enter_dir(path3, "b");
	TEST(r == 0 && strlen(path3) == PATH_MAX, "path filled");

	r = enter_dir(path3, "end");
	TEST(r == -1 && strlen(path3) == PATH_MAX, "respect PATH_MAX; leave path unchanged");

	char path4[PATH_MAX] = "/bin";
	char cd[NAME_MAX];
	current_dir(path4, cd);
	TEST(strcmp(cd, "bin") == 0, "");

	enter_dir(path4, "wat");
	current_dir(path4, cd);
	TEST(strcmp(cd, "wat") == 0, "");

	strcpy(path4, "/");
	current_dir(path4, cd);
	TEST(strcmp(cd, "/") == 0, "");

	TEST(!path_is_relative("/etc/netctl"), "absolute");
	TEST(!path_is_relative("~/.config"), "absolute");
	TEST(path_is_relative("./netctl"), "relative");
	TEST(path_is_relative("etc/netctl"), "relative");

	char path5[PATH_MAX] = "/dope/";
	char dir5[] = "./wat/lol/../wut";
	enter_dir(path5, dir5);
	TEST(strcmp(path5, "/dope/wat/wut") == 0, "");

	enter_dir(path5, "/home/user");
	TEST(strcmp(path5, "/home/user") == 0, "");

	char path6[PATH_MAX];
	enter_dir(path6, "~");
	struct passwd* pwd = get_pwd();
	TEST(strcmp(path6, pwd->pw_dir) == 0, "");

	END_SECTION("path");


	SECTION("utf8");

	struct test_utf8_pair {
		utf8* b;
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

	// Some Valid Strings
	utf8* svs[] = {
		"!@#$%^&*()_+",
		"ascii is cool",
		"Pchnąć w tę łódź jeża lub ośm skrzyń fig.",
		"Zwölf große Boxkämpfer jagen Viktor quer über den Sylter Deich.",
		"Любя, съешь щипцы, — вздохнёт мэр, — кайф жгуч.",
		"Nechť již hříšné saxofony ďáblů rozzvučí síň úděsnými tóny waltzu, tanga a quickstepu.",
		"Eble ĉiu kvazaŭdeca fuŝĥoraĵo ĝojigas homtipon.",
		"Γαζίες καὶ μυρτιὲς δὲν θὰ βρῶ πιὰ στὸ χρυσαφὶ ξέφωτο.",
	};

	TEST(utf8_width("wat") == 3, "");
	TEST(utf8_width("łąć") == 3, "");
	TEST(utf8_width("łaka łaką") == 9, "");
	TEST(utf8_width("Γαζίες καὶ μυρτιὲς δὲν θὰ βρῶ πιὰ στὸ χρυσαφὶ ξέφωτο.") == 53, "");
	TEST(utf8_width("Eble ĉiu kvazaŭdeca fuŝĥoraĵo ĝojigas homtipon.") == 47, "");

	for (size_t i = 0; i < sizeof(svs)/sizeof(utf8*); ++i) {
		TEST(utf8_width(svs[i])<=strlen(svs[i]), "apparent width <= length; loose, but always true");
		TEST(utf8_width(svs[i]) == utf8_ng_till(svs[i], svs[i]+strlen(svs[i])), "");
		TEST(utf8_validate(svs[i]), "all valid strings are valid");
	}

	utf8* sis[] = {
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
	for (size_t i = 0; i < sizeof(sis)/sizeof(utf8*); ++i) {
		TEST(!utf8_validate(sis[i]), "all invalid strings are invalid");
	}

	TEST(utf8_slice_length("łaka łaką", 2) == 3, "");
	TEST(utf8_slice_length("qq", 2) == 2, "");
	TEST(utf8_slice_length("", 2) == 0, "");
	TEST(utf8_slice_length("a", 0) == 0, "");

	utf8 inserted[20] = "łąkała";
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
	TEST(utf8_ng_till(inserted, inserted+5) == 3, "");


	END_SECTION("utf8");

	END_TESTS;
}
