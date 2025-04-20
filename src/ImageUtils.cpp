#include "ImageUtils.h"
#include "Debug.h"
#include "Application.h"
#include "ImageTexture.h"

#include <spdlog/spdlog.h>
#include <glad/glad.h>

#include <thread>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

void listImagesHelper(fs::path &dir, std::vector<fs::path> &imagePaths, std::vector<fs::path> &pending) {
	for (const fs::path &entry : fs::directory_iterator{ dir }) {
		if (fs::is_directory(entry)) {
			pending.push_back(entry);
		}
		if (entry.extension() == ".jpg" || entry.extension() == ".png") {
			auto path = fs::absolute(entry);
			imagePaths.push_back(path);
			loadImage(path);
		}
		// std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}

void listImages(fs::path &root, std::vector<fs::path> &imagePaths) {
	SignalBus &bus = SignalBus::getInstance();

	// TODO: Document about max directory traversal levels = 15
	const int maxLevel = 15;
	int currentLevel = 0;
	std::vector<fs::path> pending = { root };
	while (bus.appRunningM && pending.size() && currentLevel <= maxLevel) {
		const int numDirs = pending.size();
		for (int i = 0; i < numDirs; i++) {
			fs::path current = pending[0];
			pending.erase(pending.begin());
			listImagesHelper(current, imagePaths, pending);
		}
		currentLevel++;
	}
	if (pending.size() && currentLevel >= maxLevel) {
		SPDLOG_WARN("Max directory depth ({}) exceeded", maxLevel);
	}
}

void loadImage(fs::path imagePath) {
#ifdef _WIN32
	auto path = imagePath.u8string();
	auto s = std::string(reinterpret_cast<const char *>(path.data()), path.size());
#else
	auto s = imagePath.string();
#endif
	int width, height, channels;
	unsigned char *pixelData = stbi_load(s.c_str(), &width, &height, &channels, 4);
	ASSERT(pixelData != nullptr, fmt::format("Couldn't load image: {}", s).c_str());

	const float aspect = (float)width / (float)height;

	Application &app = Application::getInstance();

	// TODO: Make the MAX_SIZE depend on atlas dimensions and pictures per atlas
	const int MAX_SIDE = 224;
	int newHeight, newWidth;

	if (height > width) {
		newHeight = MAX_SIDE / aspect;
		newWidth = MAX_SIDE;
	} else {
		newHeight = MAX_SIDE;
		newWidth = MAX_SIDE * aspect;
	}

	// TODO: Make number of channels also dependent on kind of image
	unsigned char *resizedPixelData = new unsigned char[newHeight * newWidth * 4];
	stbir_resize_uint8_srgb(pixelData, width, height, 0, resizedPixelData, newWidth, newHeight, 0, STBIR_RGBA);
	stbi_image_free(pixelData);

	float uvX = (newWidth - 224.0f) / (newWidth * 2.0f);
	float uvY = (newHeight - 224.0f) / (newHeight * 2.0f);

	ImageTexture t{
		.width = newWidth,
		.height = newHeight,
		.uv0 = { uvX, uvY },
		.uv1 = { 1 - uvX, 1 - uvY },
		.initialized = false,
		.data = resizedPixelData
	};

	app.imageTextures[imagePath] = t;

	// unsigned int texture;
}

void initializeTexture(fs::path imagePath) {
	Application &app = Application::getInstance();

	ImageTexture &t = app.imageTextures[imagePath];
	glGenTextures(1, &(t.textureID));
	glBindTexture(GL_TEXTURE_2D, (t.textureID));

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.width, t.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, t.data);
	stbi_image_free(t.data);
	t.initialized = true;
}