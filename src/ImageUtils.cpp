#include "ImageUtils.h"
#include "Debug.h"

#include <spdlog/spdlog.h>
#include <glad/glad.h>

#include <thread>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

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


void loadImage(fs::path imagePath, std::unordered_map<fs::path, ImageTexture>& imageTextures) {
	auto path = imagePath.u8string();
	auto s = std::string(reinterpret_cast<const char*>(path.data()), path.size());
	int width, height, channels;
	unsigned char* pixelData = stbi_load(s.c_str(), &width, &height, &channels, 4);
	ASSERT(pixelData != nullptr, fmt::format("Couldn't load image: {}", s).c_str());

	const float aspect = (float)width / (float)height;

	// TODO: Make the MAX_SIZE depend on atlas dimensions and pictures per atlas
	const int MAX_SIDE = 256;
	int newHeight, newWidth;

	if (height > width) {
		newHeight = MAX_SIDE / aspect;
		newWidth = MAX_SIDE;
	}
	else {
		newHeight = MAX_SIDE;
		newWidth = MAX_SIDE * aspect;
	}

	// TODO: Make number of channels also dependent on kind of image
	unsigned char* resizedPixelData = new unsigned char[newHeight * newWidth * 4];
	stbir_resize_uint8_srgb(pixelData, width, height, 0, resizedPixelData, newWidth, newHeight, 0, STBIR_RGBA);
	stbi_image_free(pixelData);

	unsigned int texture;
	GL_CALL(glGenTextures(1, &texture));
	GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));

	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

	GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
	GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, newWidth, newHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, resizedPixelData));
	stbi_image_free(resizedPixelData);

	float uvX = (newWidth - 256.0f) / (newWidth * 2.0f);
	float uvY = (newHeight - 256.0f) / (newHeight * 2.0f);

	ImageTexture t{
		.textureID = texture,
		.width = newWidth,
		.height = newHeight,
		.uv0 = {uvX, uvY},
		.uv1 = {1 - uvX, 1 - uvY}
	};

	imageTextures[imagePath] = t;
}