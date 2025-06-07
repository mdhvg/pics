#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include "SQLiteHelper.h"

#include <filesystem>
#include <queue>
#include <vector>

namespace fs = std::filesystem;

struct LastAtlasInfo {
	int		 index = 0;
	uint16_t imageCount = 0;
	bool	 complete = true;
};

struct ImageData {
	// Variable names formatting here is same as in database
	// unsigned int id;
	std::string	 path;
	std::string	 atlas_path;
	unsigned int atlas_index = 0;
	unsigned int textureID = 0;
};

/*
 * SEPARATE THREAD FUNCTIONS
 * - Might have to get mutex for glfwcontext if required access
 */
void listImagesHelper(fs::path				&dir,
					  std::vector<fs::path> &imagePaths,
					  std::vector<fs::path> &pending,
					  int					&atlasIndex);
// void listImages(fs::path &rootDir, std::vector<fs::path> &imagePaths);
void loadImage(fs::path imagePath);
void discoverImages();
/* SEPARATE THREAD */

unsigned char *loadAndScaleThumbnail(const fs::path &imagePath);
void		   createAtlas(std::queue<fs::path> &newImagePaths,
						   LastAtlasInfo		&info,
						   DBWrapper			&db);
void		   loadAtlas();
// void initializeTexture(fs::path imagePath);

void   interleave2Planar(unsigned char *data);
float *preprocessNImages(std::vector<std::pair<fs::path, int>> &imagePaths,
						 int									start,
						 int									count,
						 int									image_size);

#endif // IMAGE_UTILS_H