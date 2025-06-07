#include "ImageUtils.h"
#include "SignalBus.h"
#include "SQLiteHelper.h"
#include "GLJob.h"
#include "Worker.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "nlohmann/json.hpp"
#include "usearch/index.hpp"
#include "usearch/index_dense.hpp"
#include "clip.h"

#include <cstddef>
#include <filesystem>

#include <renderdoc_app.h>
#include <dlfcn.h>
#include <unordered_map>
inline RENDERDOC_API_1_6_0 *rdoc_api = NULL;

using json = nlohmann::json;
namespace usrch = unum::usearch;
namespace fs = std::filesystem;

#define INDEX_PATH ROOT_DIR "/index.usearch"

class Application {
  public:
	static Application &getInstance();
	json				getConfig();

	void start();
	void loadImages();

	~Application();

	// TODO: Need a different method to implement atlas texturing
	std::vector<ImageData>				 imageTextures;
	std::unordered_map<fs::path, GLuint> atlasTextures;

	GLJobQ glJobQ;

	DBWrapper			 db;
	usrch::index_dense_t index;
	clip_ctx			*clip = nullptr;

  private:
	json	 config;
	bool	 config_dirty;
	fs::path config_path;

	bool running = true;

	Worker discoverWorker;
	Worker atlasWorker;
	Worker modelWorker;

	static Application &instance;
	Application();
	Application(const Application &) = delete;
	Application &operator=(const Application &) = delete;

	GLFWwindow *window;
	void		load_config();
};
