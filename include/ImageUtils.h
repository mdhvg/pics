#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

/*
* SEPARATE THREAD FUNCTIONS
* - Might have to get mutex for glfwcontext if required access
*/
void listImagesHelper(fs::path &dir, std::vector<fs::path> &imagePaths, std::vector<fs::path> &pending);
void listImages(fs::path &rootDir, std::vector<fs::path> &imagePaths);
void loadImage(fs::path imagePath);
// SEPARATE THREAD

void initializeTexture(fs::path imagePath);