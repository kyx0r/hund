#include "test.h"

int main() {
	SETUP_TESTS;
	
	SECTION("test test");
	// There will be tests
	TEST(1, "pass");
	TEST(!0, "pass");

	END_SECTION("test test");
	
	END_TESTS;
}
