#include "ImageUtils.h"
#include "Application.h"
#include "Debug.h"
#include "SQLiteHelper.h"
#include "fmt/core.h"
#include "glad/glad.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <queue>

#include <chrono>
#include <string>
#include <thread>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define IMAGE_BATCH_SIZE 100

// void listImages(fs::path &root, std::vector<fs::path> &imagePaths) {
// 	SignalBus &bus = SignalBus::getInstance();
// 	int atlasIndex = 0;

// 	unum::usearch::metric_punned_t metric(786,
// unum::usearch::metric_kind_t::cos_k, unum::usearch::scalar_kind_t::f32_k);
// 	auto index = unum::usearch::index_dense_t::make(metric);

// 	// TODO: Document about max directory traversal levels = 15
// 	const int maxLevel = 15;
// 	int currentLevel = 0;
// 	std::vector<fs::path> pending = { root };
// 	while (bus.appRunningM && pending.size() && currentLevel <= maxLevel) {
// 		const int numDirs = pending.size();
// 		for (int i = 0; i < numDirs; i++) {
// 			fs::path current = pending[0];
// 			pending.erase(pending.begin());
// 			listImagesHelper(current, imagePaths, pending,
// atlasIndex);
// 		}
// 		currentLevel++;
// 	}
// 	if (pending.size() && currentLevel >= maxLevel) {
// 		SPDLOG_WARN("Max directory depth ({}) exceeded", maxLevel);
// 	}
// 	createThumbnailAtlas(imagePaths, atlasIndex);
// }

unsigned char *loadAndScaleThumbnail(const fs::path &imagePath) {
#ifdef _WIN32
	auto path = imagePath.u8string();
	auto s =
		std::string(reinterpret_cast<const char *>(path.data()), path.size());
#else
	auto s = imagePath.string();
#endif
	int			   width, height, channels;
	unsigned char *pixelData =
		stbi_load(s.c_str(), &width, &height, &channels, 4);
	ASSERT(pixelData != nullptr && fmt::format("Couldn't load image: {}", s).c_str());
	channels = 4;

	int			   side = std::min(width, height);
	unsigned char *croppedImage = new unsigned char[side * side * channels];
	int			   xOffset = (width - side) / 2;
	int			   yOffset = (height - side) / 2;
	unsigned char *start = pixelData + (yOffset * width + xOffset) * channels;
	memcpy(croppedImage, start, side * side * channels);
	stbi_image_free(pixelData);

	// TODO: Make 224 variable
	unsigned char *resizedImage = new unsigned char[224 * 224 * channels];
	stbir_resize_uint8_srgb(
		croppedImage,
		side,
		side,
		0,
		resizedImage,
		224,
		224,
		0,
		STBIR_RGBA);
	delete[] croppedImage;

	return resizedImage;
}

void createAtlas(std::queue<fs::path> &newImagePaths,
	LastAtlasInfo					  &info,
	DBWrapper						  &db) {
	std::mutex finish;
	finish.lock();

	int newImages = IMAGE_BATCH_SIZE;
	if (!info.complete) {
		newImages -= info.imageCount;
	}
	int _sz = newImagePaths.size();
	newImages = std::min(newImages, _sz);
	std::vector<unsigned char *> thumbnailData(newImages);

	const fs::path atlasDir = ROOT_DIR "/.atlas";
	const fs::path atlasPath =
		fmt::format("{}/atlas_{}.png", atlasDir.string(), info.index);

	for (int i = 0; i < newImages; i++) {
		auto &current = newImagePaths.front();
		thumbnailData[i] = loadAndScaleThumbnail(current);
		// TODO: Create into batch query
		db.execute_command(
			fmt::format("INSERT INTO Images(path, atlas_path, atlas_index) "
						"VALUES('{}', '{}', {});",
				current.string(),
				atlasPath.string(),
				info.imageCount + i));
		newImagePaths.pop();
	}

	Application &app = Application::get_instance();

	// If texture is to be re-used, load previous atlas and bind it to fbTex
	int			   atlasWidth, atlasHeight, atlasChannels;
	unsigned char *prevAtlasData = nullptr;
	if (!info.complete) {
		prevAtlasData = stbi_load(
			atlasPath.c_str(),
			&atlasWidth,
			&atlasHeight,
			&atlasChannels,
			4);
	}
	GLuint				   fbTex;
	std::shared_ptr<GLJob> previousAtlasJob =
		std::make_shared<GLJob>([&fbTex, &info, prevAtlasData]() {
			GLCall(glGenTextures(1, &fbTex));
			GLCall(glBindTexture(GL_TEXTURE_2D, fbTex));
			GLCall(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));

			if (!info.complete) {
				GLCall(glTexImage2D(GL_TEXTURE_2D,
					0,
					GL_RGBA,
					2240,
					2240,
					0,
					GL_RGBA,
					GL_UNSIGNED_BYTE,
					prevAtlasData));
			} else {
				GLCall(glTexImage2D(GL_TEXTURE_2D,
					0,
					GL_RGBA,
					2240,
					2240,
					0,
					GL_RGBA,
					GL_UNSIGNED_BYTE,
					nullptr));
			}
			GLCall(glTexParameteri(
				GL_TEXTURE_2D,
				GL_TEXTURE_MIN_FILTER,
				GL_LINEAR));
			GLCall(glTexParameteri(
				GL_TEXTURE_2D,
				GL_TEXTURE_MAG_FILTER,
				GL_LINEAR));
			GLCall(glTexParameteri(
				GL_TEXTURE_2D,
				GL_TEXTURE_WRAP_S,
				GL_CLAMP_TO_BORDER));
			GLCall(glTexParameteri(
				GL_TEXTURE_2D,
				GL_TEXTURE_WRAP_T,
				GL_CLAMP_TO_BORDER));

			if (!info.complete) {
				stbi_image_free(prevAtlasData);
			}
		});
	app.glJobQ.push(previousAtlasJob);

	// Create New framebuffer and textureArray
	GLuint				   fbo, texArray;
	std::shared_ptr<GLJob> frameBufferJob =
		std::make_shared<GLJob>([&fbo, &fbTex, &texArray]() {
			GLCall(glGenFramebuffers(1, &fbo));
			GLCall(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
			GLCall(glFramebufferTexture2D(
				GL_FRAMEBUFFER,
				GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D,
				fbTex,
				0));

			GLuint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			SPDLOG_INFO("Framebuffer status: 0x{:X} ({})",
				status,
				status == GL_FRAMEBUFFER_COMPLETE ? "Complete"
												  : "Incomplete");
			ASSERT(status == GL_FRAMEBUFFER_COMPLETE);

			GLCall(glGenTextures(1, &texArray));
			GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, texArray));

			// TODO: Pass the image size as input to lambda
			// Below line not working on NVIDIA
			GLCall(glTexImage3D(GL_TEXTURE_2D_ARRAY,
				0,
				GL_RGBA,
				224,
				224,
				100,
				0,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				nullptr));

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
		 1.0f,  1.0f, 1.0f, 1.0f,		// Top-right
		-1.0f,  1.0f, 0.0f, 1.0f		// Top-left
		// clang-format on
	};

	unsigned int indices[] = {
		// clang-format off
		0, 1, 2,
		2, 3, 0
		// clang-format on
	};

	GLuint				   vao;
	std::shared_ptr<GLJob> bufferBindJob = std::make_shared<GLJob>([&vertices,
																	   &indices,
																	   &vao]() {
		unsigned int vbo, ebo;
		GLCall(glGenVertexArrays(1, &vao));
		GLCall(glGenBuffers(1, &vbo));
		GLCall(glGenBuffers(1, &ebo));

		GLCall(glBindVertexArray(vao));

		GLCall(glBindBuffer(GL_ARRAY_BUFFER, vbo));
		GLCall(glBufferData(
			GL_ARRAY_BUFFER,
			sizeof(vertices),
			vertices,
			GL_STATIC_DRAW));

		GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));
		GLCall(glBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			sizeof(indices),
			indices,
			GL_STATIC_DRAW));

		GLCall(glVertexAttribPointer(
			0,
			2,
			GL_FLOAT,
			GL_FALSE,
			4 * sizeof(float),
			( void * )0));
		GLCall(glEnableVertexAttribArray(0));

		GLCall(glVertexAttribPointer(1,
			2,
			GL_FLOAT,
			GL_FALSE,
			4 * sizeof(float),
			( void * )(2 * sizeof(float))));
		GLCall(glEnableVertexAttribArray(1));
	});
	app.glJobQ.push(bufferBindJob);

	// Bind each image as a separate job to the textureArray
	for (int i = 0; i < newImages; i++) {
		std::shared_ptr<GLJob> imageNJob =
			std::make_shared<GLJob>([&texArray, &thumbnailData, i]() {
				ASSERT(thumbnailData[i] != nullptr);
				GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, texArray));
				// TODO: Make the image size variable
				GLCall(glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
					0,
					0,
					0,
					i,
					224,
					224,
					1,
					GL_RGBA,
					GL_UNSIGNED_BYTE,
					thumbnailData[i]));
				delete[] thumbnailData[i];
			});
		app.glJobQ.push(imageNJob);
	}

	// Compile shader
	// GLuint shaderProgram;
	// std::shared_ptr<GLJob> shaderCompileJob =
	// 	std::make_shared<GLJob>([&vertSrc, &fragSrc, &shaderProgram]() {
	// 		int status;
	// 		char infoLog[512];

	// 		GLuint vertID = glCreateShader(GL_VERTEX_SHADER);
	// 		const char *vertSrcPtr = vertSrc.c_str();
	// 		GLCall(glShaderSource(vertID, 1, &vertSrcPtr, nullptr));
	// 		GLCall(glCompileShader(vertID));
	// 		glGetShaderiv(vertID, GL_COMPILE_STATUS, &status);
	// 		if (!status) {
	// 			glGetShaderInfoLog(vertID, 512, nullptr, infoLog);
	// 			SPDLOG_ERROR(
	// 				"{} shader compilation failed: {}",
	// 				"VERTEX",
	// 				infoLog);
	// 			ASSERT(false);
	// 		}

	// 		GLuint fragID = glCreateShader(GL_FRAGMENT_SHADER);
	// 		const char *fragSrcPtr = fragSrc.c_str();
	// 		GLCall(glShaderSource(fragID, 1, &fragSrcPtr, nullptr));
	// 		GLCall(glCompileShader(fragID));
	// 		glGetShaderiv(fragID, GL_COMPILE_STATUS, &status);
	// 		if (!status) {
	// 			glGetShaderInfoLog(fragID, 512, nullptr, infoLog);
	// 			SPDLOG_ERROR(
	// 				"{} shader compilation failed: {}",
	// 				"FRAGMENT",
	// 				infoLog);
	// 			ASSERT(false);
	// 		}

	// 		shaderProgram = glCreateProgram();
	// 		GLCall(glAttachShader(shaderProgram, vertID));
	// 		GLCall(glAttachShader(shaderProgram, fragID));
	// 		GLCall(glLinkProgram(shaderProgram));
	// 		glGetProgramiv(shaderProgram, GL_LINK_STATUS, &status);
	// 		if (!status) {
	// 			glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
	// 			SPDLOG_ERROR("Shader program linking failed: {}", infoLog);
	// 			ASSERT(false);
	// 		}
	// 	});
	// app.glJobQ.push(shaderCompileJob);

	// Bind and draw
	// std::shared_ptr<GLJob> renderJob = std::make_shared<GLJob>(
	// 	[&fbo, &shaderProgram, &vao, &texArray, &fbTex, newImages, &info]() {
	// 		if (rdoc_api)
	// 			rdoc_api->StartFrameCapture(NULL, NULL);
	// 		GLCall(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
	// 		GLCall(glUseProgram(shaderProgram));
	// 		GLCall(glBindVertexArray(vao));

	// 		GLCall(glActiveTexture(GL_TEXTURE0));
	// 		GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, texArray));
	// 		GLCall(glUniform1i(glGetUniformLocation(shaderProgram, "texArray"),
	// 			0));

	// 		GLCall(glActiveTexture(GL_TEXTURE1));
	// 		GLCall(glBindTexture(GL_TEXTURE_2D, fbTex));
	// 		GLCall(
	// 			glUniform1i(glGetUniformLocation(shaderProgram, "fbTex"), 1));

	// 		GLCall(
	// 			glUniform1i(glGetUniformLocation(shaderProgram, "textureCount"),
	// 				newImages + info.imageCount));
	// 		GLCall(
	// 			glUniform1i(glGetUniformLocation(shaderProgram, "textureStart"),
	// 				info.imageCount));

	// 		// TODO: Make 224 variable
	// 		GLCall(glViewport(0, 0, 2240, 2240));
	// 		GLCall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr));
	// 		if (rdoc_api)
	// 			rdoc_api->EndFrameCapture(NULL, NULL);
	// 	});
	// app.glJobQ.push(renderJob);

	// Read framebuffer
	// TODO: Make 224 variable
	// unsigned char *atlasData = new unsigned char[2240 * 2240 * 4];
	// std::shared_ptr<GLJob> readJob = std::make_shared<GLJob>(
	// 	[&app, &atlasPath, &fbo, &atlasData, &fbTex]() {
	// 		GLCall(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
	// 		GLCall(glReadPixels(
	// 			0,
	// 			0,
	// 			2240,
	// 			2240,
	// 			GL_RGBA,
	// 			GL_UNSIGNED_BYTE,
	// 			atlasData));
	// 		// Delete framebuffer after use
	// 		GLCall(glDeleteFramebuffers(1, &fbo));
	// 		// If atlas already exists on GPU, delete it and update it with new
	// 		// one
	// 		if (app.atlasTextures.find(atlasPath) != app.atlasTextures.end() && app.atlasTextures[atlasPath]) { // exists and not 0
	// 			GLCall(glDeleteTextures(1, &(app.atlasTextures[atlasPath])));
	// 		}
	// 		app.atlasTextures[atlasPath] = fbTex;
	// 	},
	// 	"AtlasCopy",
	// 	&finish);
	// app.glJobQ.push(readJob);
	// finish.lock();
	// if (!fs::exists(atlasDir)) {
	// 	fs::create_directories(atlasDir);
	// }
	// stbi_flip_vertically_on_write(1);
	// stbi_write_png(atlasPath.c_str(), 2240, 2240, 4, atlasData, 0);
	// delete[] atlasData;
	db.execute_command(
		fmt::format(R"(INSERT INTO Atlas (atlas_path, idx, image_count) 
		VALUES ('{}', {}, {})
		ON CONFLICT(atlas_path) 
		DO UPDATE SET 
    	idx = excluded.idx, 
    	image_count = excluded.image_count;)",
			atlasPath.string(),
			info.index,
			newImages + info.imageCount));
	loadAtlas();
}

// void loadImage(fs::path imagePath) {
// #ifdef _WIN32
// 	auto path = imagePath.u8string();
// 	auto s = std::string(reinterpret_cast<const char *>(path.data()),
// path.size()); #else 	auto s = imagePath.string(); #endif 	int width,
// height, channels; 	float *pixelData = stbi_loadf(s.c_str(), &width,
// &height, &channels, 4); 	ASSERT(pixelData != nullptr &&
// fmt::format("Couldn't load image: {}", s).c_str());

// 	const float aspect = (float)width / (float)height;

// 	Application &app = Application::getInstance();

// 	// TODO: Make the MAX_SIZE depend on atlas dimensions and pictures per
// atlas 	const int MAX_SIDE = 224; 	int newHeight, newWidth;

// 	if (height > width) {
// 		newHeight = MAX_SIDE / aspect;
// 		newWidth = MAX_SIDE;
// 	} else {
// 		newHeight = MAX_SIDE;
// 		newWidth = MAX_SIDE * aspect;
// 	}

// 	// TODO: Make number of channels also dependent on kind of image
// 	float *resizedPixelData = new float[newHeight * newWidth * 4];
// 	stbir_resize_float_linear(pixelData, width, height, 0, resizedPixelData,
// newWidth, newHeight, 0, STBIR_RGBA); 	stbi_image_free(pixelData);

// 	float uvX = (newWidth - 224.0f) / (newWidth * 2.0f);
// 	float uvY = (newHeight - 224.0f) / (newHeight * 2.0f);

// 	ImageTexture t{
// 		.width = newWidth,
// 		.height = newHeight,
// 		.uv0 = { uvX, uvY },
// 		.uv1 = { 1 - uvX, 1 - uvY },
// 		.initialized = false,
// 		.data = resizedPixelData
// 	};

// 	app.imageTextures[imagePath] = t;
// }

// void initializeTexture(fs::path imagePath) {
// 	Application &app = Application::getInstance();

// 	ImageTexture &t = app.imageTextures[imagePath];
// 	glGenTextures(1, &(t.textureID));
// 	glBindTexture(GL_TEXTURE_2D, (t.textureID));

// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

// 	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
// 	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, t.width, t.height, 0,
// GL_RGBA, GL_FLOAT, t.data); 	stbi_image_free(t.data); 	t.initialized =
// true;
// }

// void listImagesHelper(fs::path &dir, std::vector<fs::path> &imagePaths,
// std::vector<fs::path> &pending, int &atlasIndex) { 	for (const fs::path
// &entry : fs::directory_iterator{ dir }) { 		if
// (fs::is_directory(entry)) { 			pending.push_back(entry);
// 		}
// 		if (entry.extension() == ".jpg" || entry.extension() == ".png")
// { 			auto path = fs::absolute(entry);
// imagePaths.push_back(path);
// 			// loadImage(path);
// 		}
// 		if (imagePaths.size() >= IMAGE_BATCH_SIZE) {
// 			createThumbnailAtlas(imagePaths, atlasIndex);
// 			// app.executeSQL(
// 			// 	R"(

// 			//	)"
// 			// );
// 		}
// 		// std::this_thread::sleep_for(std::chrono::milliseconds(1000));
// 	}
// }

LastAtlasInfo getLastAtlasInfo(DBWrapper &db) {
	const std::string query1 = fmt::format(
		"SELECT idx, image_count FROM Atlas WHERE image_count < {};",
		IMAGE_BATCH_SIZE);
	const std::string query2 =
		"SELECT idx FROM Atlas ORDER BY idx DESC LIMIT 1;";

	LastAtlasInfo info;

	// Check if any of previous atlases are incomplete
	db.execute_command(
		query1,
		[](void *data, int, char **argv, char **) -> int {
			LastAtlasInfo *info = ( LastAtlasInfo * )data;
			info->complete = false;
			info->index = std::atoi(argv[0]);
			info->imageCount = std::atoi(argv[1]);
			return 0;
		},
		&info);

	// When previous atlases are complete, create new one
	if (info.complete) {
		db.execute_command(
			query2,
			[](void *data, int, char **argv, char **) -> int {
				LastAtlasInfo *info = ( LastAtlasInfo * )data;
				info->imageCount = 0;
				info->index = std::atoi(argv[0]) + 1;
				return 0;
			},
			&info);
	}

	return info;
}

void discoverImages() {
	// stbi_set_flip_vertically_on_load(1);

	// Find images
	std::queue<fs::path> newImagePaths;
	std::mutex			 imagePathMutex;

	// SignalBus &bus = SignalBus::getInstance();

	Application &app = Application::get_instance();
	json		 config = app.getConfig();

	std::queue<std::string> pending;
	for (const auto &path :
		config["images"]["paths"].get<std::vector<std::string>>()) {
		pending.push(path);
	}

	int currentDepth = 0;
	// TODO: Document about max directory traversal levels = 15
	const int maxDepth = 15;

	while (pending.size() && currentDepth <= maxDepth) {
		int numDirs = pending.size();
		for (int i = 0; i < numDirs; i++) {
			fs::path current = pending.front();
			pending.pop();
			for (const fs::path &entry : fs::directory_iterator(current)) {
				if (fs::is_directory(entry)) {
					pending.push(entry);
					continue;
				}
				if (entry.extension() == ".jpg" || entry.extension() == ".png") {
					// if (imageExists(entry, app.db)) {
					// 	continue;
					// }
					imagePathMutex.lock();
					newImagePaths.push(fs::absolute(entry));
					imagePathMutex.unlock();
				}
				// Create their atlas when image count >= 100
				if (newImagePaths.size() >= IMAGE_BATCH_SIZE) {
					// Check the Atlas table to find last atlas index and if
					// there are any atlases that less than IMAGE_BATCH_SIZE
					// images if such exists (should be exactly 1 entry)
					LastAtlasInfo info = getLastAtlasInfo(app.db);
					// load it as texture then create atlas on it (making sure
					// not to overwrite previous tiles) otherwise just create a
					// new atlas at new atlas index Save their info to database
					createAtlas(newImagePaths, info, app.db);
					// Call loadAtlas(); if a new atlas is created
					app.loadImages();
				}
			}
			currentDepth++;
		}
		if (pending.size() && currentDepth >= maxDepth) {
			// TODO: Show a warning and tip in the UI also
			SPDLOG_WARN("Max directory depth ({}) exceeded", maxDepth);
		}
	}

	if (newImagePaths.size()) {
		LastAtlasInfo info = getLastAtlasInfo(app.db);
		createAtlas(newImagePaths, info, app.db);
	}
}

void loadAtlas() {
	// std::mutex finish;
	// finish.lock();
	// Application &app = Application::get_instance();
	// app.imageTextures.clear();
	// app.db.executeCommand(
	// 	"SELECT * FROM Images;",
	// 	[](void *data, int, char **argv, char **) -> int {
	// 		auto *imageTextues = ( std::vector<ImageData> * )data;
	// 		imageTextues->push_back(
	// 			{ argv[1], argv[2], ( unsigned int )std::stoi(argv[3]) });
	// 		return 0;
	// 	},
	// 	&(app.imageTextures));
	// for (const auto &img : app.imageTextures) {
	// 	if (app.atlasTextures.find(img.atlas_path) == app.atlasTextures.end()) {
	// 		app.atlasTextures[img.atlas_path] = 0;
	// 	}
	// }
	// std::vector<unsigned char *> atlasData(app.atlasTextures.size());
	// int _w, _h, _c;
	// for (auto &path : app.atlasTextures) {
	// 	unsigned char *data = stbi_load(path.first.c_str(), &_w, &_h, &_c, 4);
	// 	std::shared_ptr<GLJob> textureJob =
	// 		std::make_shared<GLJob>([data, &path]() {
	// 			GLuint tex;
	// 			GLCall(glGenTextures(1, &tex));
	// 			GLCall(glBindTexture(GL_TEXTURE_2D, tex));
	// 			GLCall(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
	// 			GLCall(glTexImage2D(GL_TEXTURE_2D,
	// 				0,
	// 				GL_RGBA,
	// 				2240,
	// 				2240,
	// 				0,
	// 				GL_RGBA,
	// 				GL_UNSIGNED_BYTE,
	// 				data));
	// 			GLCall(glTexParameteri(
	// 				GL_TEXTURE_2D,
	// 				GL_TEXTURE_MIN_FILTER,
	// 				GL_LINEAR));
	// 			GLCall(glTexParameteri(
	// 				GL_TEXTURE_2D,
	// 				GL_TEXTURE_MAG_FILTER,
	// 				GL_LINEAR));
	// 			GLCall(glTexParameteri(
	// 				GL_TEXTURE_2D,
	// 				GL_TEXTURE_WRAP_S,
	// 				GL_CLAMP_TO_BORDER));
	// 			GLCall(glTexParameteri(
	// 				GL_TEXTURE_2D,
	// 				GL_TEXTURE_WRAP_T,
	// 				GL_CLAMP_TO_BORDER));
	// 			stbi_image_free(data);
	// 			path.second = tex;
	// 		});
	// 	app.glJobQ.push(textureJob);
	// }
	// std::shared_ptr<GLJob> unlockJob = std::make_shared<GLJob>(
	// 	[]() {
	// 	},
	// 	"",
	// 	false,
	// 	&finish);
	// app.glJobQ.push(unlockJob);
	// finish.lock();

	// for (auto &img : app.imageTextures) {
	// 	if (img.textureID == 0) {
	// 		img.textureID = app.atlasTextures[img.atlas_path];
	// 	}
	// }
}

float *copyAsPlanar(float *data, int image_size, int count) {
	float *imageData = new float[image_size * image_size * 3 * count];
	int	   num_pixels = image_size * image_size;

	for (int i = 0; i < count; i++) {
		float *rPlane = imageData + num_pixels * (3 * i + 0);
		float *gPlane = imageData + num_pixels * (3 * i + 1);
		float *bPlane = imageData + num_pixels * (3 * i + 2);

		for (int pixel = 0; pixel < num_pixels; pixel++) {
			rPlane[pixel] = data[pixel * 3 + 0 + i * 3 * num_pixels];
			gPlane[pixel] = data[pixel * 3 + 1 + i * 3 * num_pixels];
			bPlane[pixel] = data[pixel * 3 + 2 + i * 3 * num_pixels];
		}
	}

	return imageData;
}

float *preprocessNImages(std::vector<std::pair<fs::path, int>> &imagePaths,
	int															start,
	int															count,
	int															image_size) {
	float *imageData = new float[image_size * image_size * 3 * count];
	int	   idx = 0;
	int	   _w, _h, _c;
	float *_data;
	for (int i = start; i < count; i++) {
		_data = stbi_loadf(imagePaths[i].first.c_str(), &_w, &_h, &_c, 3);
		stbir_resize_float_linear(_data,
			_w,
			_h,
			0,
			imageData + (idx * image_size * image_size * 3),
			image_size,
			image_size,
			0,
			STBIR_RGB);
		stbi_image_free(_data);
		idx++;
	}

	float *planarData = copyAsPlanar(imageData, image_size, count);
	// unsigned char* planarUint = new unsigned char[224 * 224 * 3 * 4];
	// for (int i = 0; i < 224 * 224 * 4 * 3; i++) {
	//   planarUint[i] = planarData[i] * 225.0f;
	// }
	delete[] imageData;
	return planarData;
}