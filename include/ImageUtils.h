#include "ImageTexture.h"

#include <filesystem>
#include <vector>
#include <unordered_map>

namespace fs = std::filesystem;

/*
* SEPARATE THREAD FUNCTIONS
* - Might have to get mutex for glfwcontext if required access
*/
void listImagesHelper(fs::path& dir, std::vector<fs::path>& imagePaths, std::vector<fs::path>& pending);
void listImages(fs::path& rootDir, std::vector<fs::path>& imagePaths);
// SEPARATE THREAD

void loadImage(fs::path imagePath, std::unordered_map<fs::path, ImageTexture>& imageTextures);