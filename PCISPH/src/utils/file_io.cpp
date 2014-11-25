#include "file_io.h"

#include <fstream>

namespace utils {
	std::string read_file(const std::string& path) {
		std::ifstream file(path);
		return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}
}