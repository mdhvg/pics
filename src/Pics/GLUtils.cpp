#include "GLUtils.h"
#include "Debug.h"

#include <fstream>

std::string readSource(const std::string &filename) {
	std::ifstream file(filename);
	SPDLOG_INFO("Reading file: {}", filename);
	ASSERT(file.is_open() && "Failed to open file");
	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();
	return content;
}