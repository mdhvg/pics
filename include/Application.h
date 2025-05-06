#include "ImageTexture.h"
#include "SignalBus.h"
#include "SQLiteHelper.h"
#include "GLJob.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "nlohmann/json.hpp"

#include <filesystem>
#include <thread>

#include <renderdoc_app.h>
#include <dlfcn.h>
inline RENDERDOC_API_1_6_0 *rdoc_api = NULL;

using json = nlohmann::json;
namespace fs = std::filesystem;

class Application {
  public:
	static Application &getInstance();
	json getConfig();

	void start();

	~Application();

	std::unordered_map<fs::path, ImageTexture> atlasTextures;
	GLJobQ glJobQ;
	DBWrapper db;

  private:
	json config;
	bool config_dirty;
	fs::path config_path;

	bool running = true;

	std::thread discoverThread;
	std::thread atlasThread;
	std::vector<fs::path> imagePaths;

	static Application &instance;
	Application();
	Application(const Application &) = delete;
	Application &operator=(const Application &) = delete;

	GLFWwindow *window;
	void load_config();
};
