#include <string>
#include "pch.h"

import FileSession;

TEST(FileSystemFileDoesNotExist, FileSystemTests) {
	const std::string sstr("abc");
	IO::FileSession f(sstr);
}