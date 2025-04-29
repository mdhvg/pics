#include "ImageUtils.h"
#include "Debug.h"
#include "Application.h"
#include "ImageTexture.h"
#include "GLUtils.h"
#include "spdlog/fmt/bundled/format.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <glad/glad.h>

#include <thread>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define IMAGE_BATCH_SIZE 100

void listImagesHelper(fs::path &dir, std::vector<fs::path> &imagePaths, std::vector<fs::path> &pending, int &atlasIndex) {
	for (const fs::path &entry : fs::directory_iterator{ dir }) {
		if (fs::is_directory(entry)) {
			pending.push_back(entry);
		}
		if (entry.extension() == ".jpg" || entry.extension() == ".png") {
			auto path = fs::absolute(entry);
			imagePaths.push_back(path);
			// loadImage(path);
		}
		if (imagePaths.size() >= IMAGE_BATCH_SIZE) {
			createThumbnailAtlas(imagePaths, atlasIndex);
			// app.executeSQL(
			// 	R"(

			//	)"
			// );
		}
		// std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}

void listImages(fs::path &root, std::vector<fs::path> &imagePaths) {
	SignalBus &bus = SignalBus::getInstance();
	int atlasIndex = 0;

	stbi_set_flip_vertically_on_load(1);

	unum::usearch::metric_punned_t metric(786, unum::usearch::metric_kind_t::cos_k, unum::usearch::scalar_kind_t::f32_k);
	auto index = unum::usearch::index_dense_t::make(metric);

	// TODO: Document about max directory traversal levels = 15
	const int maxLevel = 15;
	int currentLevel = 0;
	std::vector<fs::path> pending = { root };
	while (bus.appRunningM && pending.size() && currentLevel <= maxLevel) {
		const int numDirs = pending.size();
		for (int i = 0; i < numDirs; i++) {
			fs::path current = pending[0];
			pending.erase(pending.begin());
			listImagesHelper(current, imagePaths, pending, atlasIndex);
		}
		currentLevel++;
	}
	if (pending.size() && currentLevel >= maxLevel) {
		SPDLOG_WARN("Max directory depth ({}) exceeded", maxLevel);
	}
	createThumbnailAtlas(imagePaths, atlasIndex);
}

unsigned char *loadAndScaleThumbnail(fs::path &imagePath) {
#ifdef _WIN32
	auto path = imagePath.u8string();
	auto s = std::string(reinterpret_cast<const char *>(path.data()), path.size());
#else
	auto s = imagePath.string();
#endif
	int width, height, channels;
	unsigned char *pixelData = stbi_load(s.c_str(), &width, &height, &channels, 4);
	ASSERT(pixelData != nullptr && fmt::format("Couldn't load image: {}", s).c_str());
	channels = 4;

	int side = std::min(width, height);
	unsigned char *croppedImage = new unsigned char[side * side * channels];
	int xOffset = (width - side) / 2;
	int yOffset = (height - side) / 2;
	unsigned char *start = pixelData + (yOffset * width + xOffset) * channels;
	memcpy(croppedImage, start, side * side * channels);
	stbi_image_free(pixelData);

	// TODO: Make 224 variable
	unsigned char *resizedImage = new unsigned char[224 * 224 * channels];
	stbir_resize_uint8_srgb(croppedImage, side, side, 0, resizedImage, 224, 224, 0, STBIR_RGBA);
	delete[] croppedImage;

	return resizedImage;
}

void createThumbnailAtlas(std::vector<fs::path> &imagePaths, int &atlasIndex) {
	std::mutex finish;
	finish.lock();
	std::vector<unsigned char *> thumbnailData(imagePaths.size());
	for (int i = 0; i < imagePaths.size(); i++) {
		thumbnailData[i] = loadAndScaleThumbnail(imagePaths[i]);
	}

	Application &app = Application::getInstance();

	// Create New framebuffer and textureArray
	GLuint fbo, fbTex, texArray;
	std::shared_ptr<GLJob> frameBufferJob = std::make_shared<GLJob>([&fbo, &fbTex, &texArray]() {
		GLCall(glGenFramebuffers(1, &fbo));

		glGenTextures(1, &fbTex);
		glBindTexture(GL_TEXTURE_2D, fbTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2240, 2240, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GLCall(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbTex, 0);

		GLuint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		SPDLOG_INFO("Framebuffer status: 0x{:X}", status);
		ASSERT(status == GL_FRAMEBUFFER_COMPLETE);

		GLCall(glGenTextures(1, &texArray));
		GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, texArray));

		// TODO: Pass the image size as input to lambda
		GLCall(glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 224, 224, 100));

		// Set texture parameters for sampling
		GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
		GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
	});
	app.glJobQ.push(frameBufferJob);

	float vertices[] = {
		// clang-format off
		-1.0f, -1.0f, 0.0f, 0.0f,       // Bottom-left
		 1.0f, -1.0f, 1.0f, 0.0f,       // Bottom-right
		 1.0f,  1.0f, 1.0f, 1.0f,     // Top-right
		-1.0f,  1.0f, 0.0f, 1.0f    // Top-left
		// clang-format on
	};

	unsigned int indices[] = {
		// clang-format off
		0, 1, 2,
		2, 3, 0
		// clang-format on
	};

	GLuint vao;
	std::shared_ptr<GLJob> bufferBindJob = std::make_shared<GLJob>([&vertices, &indices, &vao]() {
		unsigned int vbo, ebo;
		GLCall(glGenVertexArrays(1, &vao));
		GLCall(glGenBuffers(1, &vbo));
		GLCall(glGenBuffers(1, &ebo));

		GLCall(glBindVertexArray(vao));

		GLCall(glBindBuffer(GL_ARRAY_BUFFER, vbo));
		GLCall(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW));

		GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));
		GLCall(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));

		GLCall(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0));
		GLCall(glEnableVertexAttribArray(0));

		GLCall(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float))));
		GLCall(glEnableVertexAttribArray(1));
	});
	app.glJobQ.push(bufferBindJob);

	// Bind each image as a separate job to the textureArray
	for (int i = 0; i < imagePaths.size(); i++) {
		std::shared_ptr<GLJob> imageNJob = std::make_shared<GLJob>([&texArray, &thumbnailData, i]() {
			ASSERT(thumbnailData[i] != nullptr);
			// TODO: Make the image size variable
			GLCall(glTextureSubImage3D(texArray, 0, 0, 0, i, 224, 224, 1, GL_RGBA, GL_UNSIGNED_BYTE, thumbnailData[i]));
			delete[] thumbnailData[i];
		});
		app.glJobQ.push(imageNJob);
	}

	// Compile shader
	// TODO: Change ROOT_DIR to installation/project root dir
	auto vertSrc = readSource(ROOT_DIR "/rsc/atlas.vert");
	auto fragSrc = readSource(ROOT_DIR "/rsc/atlas.frag");

	GLuint shaderProgram;
	std::shared_ptr<GLJob> shaderCompileJob = std::make_shared<GLJob>([&vertSrc, &fragSrc, &shaderProgram]() {
		int status;
		char infoLog[512];

		GLuint vertID = glCreateShader(GL_VERTEX_SHADER);
		const char *vertSrcPtr = vertSrc.c_str();
		GLCall(glShaderSource(vertID, 1, &vertSrcPtr, nullptr));
		GLCall(glCompileShader(vertID));
		glGetShaderiv(vertID, GL_COMPILE_STATUS, &status);
		if (!status) {
			glGetShaderInfoLog(vertID, 512, nullptr, infoLog);
			SPDLOG_ERROR("{} shader compilation failed: {}", "VERTEX", infoLog);
			ASSERT(false);
		}

		GLuint fragID = glCreateShader(GL_FRAGMENT_SHADER);
		const char *fragSrcPtr = fragSrc.c_str();
		GLCall(glShaderSource(fragID, 1, &fragSrcPtr, nullptr));
		GLCall(glCompileShader(fragID));
		glGetShaderiv(fragID, GL_COMPILE_STATUS, &status);
		if (!status) {
			glGetShaderInfoLog(fragID, 512, nullptr, infoLog);
			SPDLOG_ERROR("{} shader compilation failed: {}", "FRAGMENT", infoLog);
			ASSERT(false);
		}

		shaderProgram = glCreateProgram();
		GLCall(glAttachShader(shaderProgram, vertID));
		GLCall(glAttachShader(shaderProgram, fragID));
		GLCall(glLinkProgram(shaderProgram));
		glGetProgramiv(shaderProgram, GL_LINK_STATUS, &status);
		if (!status) {
			glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
			SPDLOG_ERROR("Shader program linking failed: {}", infoLog);
			ASSERT(false);
		}
	});
	app.glJobQ.push(shaderCompileJob);

	// Bind and draw
	std::shared_ptr<GLJob> renderJob = std::make_shared<GLJob>([&fbo, &shaderProgram, &vao, &texArray, &imagePaths]() {
		if (rdoc_api)
			rdoc_api->StartFrameCapture(NULL, NULL);
		GLCall(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
		GLCall(glUseProgram(shaderProgram));
		GLCall(glBindVertexArray(vao));
		GLCall(glActiveTexture(GL_TEXTURE0));
		GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, texArray));
		GLCall(glUniform1i(glGetUniformLocation(shaderProgram, "textureCount"), imagePaths.size()));
		// TODO: Make 224 variable
		GLCall(glViewport(0, 0, 2240, 2240));
		GLCall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr));
		if (rdoc_api)
			rdoc_api->EndFrameCapture(NULL, NULL);
	});
	app.glJobQ.push(renderJob);

	// Read framebuffer
	// TODO: Make 224 variable
	unsigned char *atlasData = new unsigned char[2240 * 2240 * 4];
	std::shared_ptr<GLJob> readJob = std::make_shared<GLJob>([&fbo, &atlasData, &finish]() {
		GLCall(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
		GLCall(glReadPixels(0, 0, 2240, 2240, GL_RGBA, GL_UNSIGNED_BYTE, atlasData));
		finish.unlock();
	},
															 "AtlasCopy",
															 true);
	app.glJobQ.push(readJob);
	finish.lock();
	const fs::path atlasDir = ROOT_DIR "/.atlas";
	if (!fs::exists(atlasDir)) {
		fs::create_directories(atlasDir);
	}
	stbi_flip_vertically_on_write(1);
	stbi_write_png(fmt::format("{}/atlas_{}.png", atlasDir.string(), atlasIndex).c_str(), 2240, 2240, 4, atlasData, 0);
}

void loadImage(fs::path imagePath) {
#ifdef _WIN32
	auto path = imagePath.u8string();
	auto s = std::string(reinterpret_cast<const char *>(path.data()), path.size());
#else
	auto s = imagePath.string();
#endif
	int width, height, channels;
	float *pixelData = stbi_loadf(s.c_str(), &width, &height, &channels, 4);
	ASSERT(pixelData != nullptr && fmt::format("Couldn't load image: {}", s).c_str());

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
	float *resizedPixelData = new float[newHeight * newWidth * 4];
	stbir_resize_float_linear(pixelData, width, height, 0, resizedPixelData, newWidth, newHeight, 0, STBIR_RGBA);
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, t.width, t.height, 0, GL_RGBA, GL_FLOAT, t.data);
	stbi_image_free(t.data);
	t.initialized = true;
}