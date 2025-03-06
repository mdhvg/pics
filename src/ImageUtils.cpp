#include "ImageUtils.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

void listImagesHelper(fs::path& dir, std::vector<fs::path>& imagePaths, std::vector<fs::path>& pending) {
	for (const fs::path& entry : fs::directory_iterator{ dir }) {
		if (fs::is_directory(entry)) {
			pending.push_back(entry);
		}
		if (entry.extension() == ".jpg" || entry.extension() == ".png") {
			imagePaths.push_back(fs::absolute(entry));
		}
		//std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void listImages(fs::path& root, std::vector<fs::path>& imagePaths) {

	// TODO: Document about max directory traversal levels = 15
	const int maxLevel = 15;
	int currentLevel = 0;
	std::vector<fs::path> pending = { root };
	while (pending.size() && currentLevel <= maxLevel) {
		const int numDirs = pending.size();
		for (int i = 0; i < numDirs; i++) {
			fs::path current = pending[0];
			pending.erase(pending.begin());
			listImagesHelper(current, imagePaths, pending);
		}
		currentLevel++;
	}
	if (pending.size()) {
		SPDLOG_WARN("Max directory depth ({}) exceeded", maxLevel);
	}
}