#include "ImageUtils.h"
#include "SignalBus.h"
#include "SQLiteHelper.h"
#include "GLJob.h"
#include "Worker.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "nlohmann/json.hpp"

#include <filesystem>
#include <thread>

#include <renderdoc_app.h>
#include <dlfcn.h>
#include <unordered_map>
inline RENDERDOC_API_1_6_0 *rdoc_api = NULL;

using json = nlohmann::json;
namespace fs = std::filesystem;

class Application {
  public:
	static Application &getInstance();
	json getConfig();

	void start();
	void loadImages();

	~Application();

	// TODO: Need a different method to implement atlas texturing
	std::vector<ImageData> imageTextures;
	std::unordered_map<fs::path, GLuint> atlasTextures;

	GLJobQ glJobQ;

	DBWrapper db;

  private:
	json config;
	bool config_dirty;
	fs::path config_path;

	bool running = true;

	Worker discoverThread;
	Worker atlasThread;

	static Application &instance;
	Application();
	Application(const Application &) = delete;
	Application &operator=(const Application &) = delete;

	GLFWwindow *window;
	void load_config();
};
