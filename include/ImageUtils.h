#include <filesystem>
#include <vector>
#include <usearch/index.hpp>
#include <usearch/index_dense.hpp>

namespace fs = std::filesystem;

/*
* SEPARATE THREAD FUNCTIONS
* - Might have to get mutex for glfwcontext if required access
*/
void listImagesHelper(fs::path &dir, std::vector<fs::path> &imagePaths, std::vector<fs::path> &pending, int &atlasIndex);
void listImages(fs::path &rootDir, std::vector<fs::path> &imagePaths);
void loadImage(fs::path imagePath);
/* SEPARATE THREAD */

unsigned char *loadAndScaleThumbnail(fs::path &imagePath);
void createThumbnailAtlas(std::vector<fs::path> &imagePaths, int &atlasIndex);
void initializeTexture(fs::path imagePath);