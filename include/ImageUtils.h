#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

void listImagesHelper(fs::path& dir, std::vector<fs::path>& imagePaths, std::vector<fs::path>& pending);

void listImages(fs::path& rootDir, std::vector<fs::path>& imagePaths);