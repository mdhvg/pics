#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include "usearch/index.hpp"
#include "usearch/index_dense.hpp"
#include "SQLiteHelper.h"

#include <filesystem>
#include <vector>
#include <queue>

namespace fs = std::filesystem;

struct LastAtlasInfo {
	int index = 0;
	uint16_t imageCount = 0;
	bool complete = true;
};

/*
* SEPARATE THREAD FUNCTIONS
* - Might have to get mutex for glfwcontext if required access
*/
void listImagesHelper(fs::path &dir,
					  std::vector<fs::path> &imagePaths,
					  std::vector<fs::path> &pending,
					  int &atlasIndex);
// void listImages(fs::path &rootDir, std::vector<fs::path> &imagePaths);
void loadImage(fs::path imagePath);
void discoverImages();
/* SEPARATE THREAD */

unsigned char *loadAndScaleThumbnail(fs::path &imagePath);
void createAtlas(std::queue<fs::path> &newImagePaths,
				 int newImageCount,
				 LastAtlasInfo &info,
				 DBWrapper &db);
void loadAtlas(std::vector<std::string> atlasPaths = {});
// void initializeTexture(fs::path imagePath);

#endif // IMAGE_UTILS_H