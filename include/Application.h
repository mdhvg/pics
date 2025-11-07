#include "ImageManager.h"
#include "SQLiteHelper.h"
#include "GLJob.h"
#include "Window/Window.h"
#include "UI/UILayer.h"
#include "Thread/threadpool.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "nlohmann/json.hpp"
#include "usearch/index.hpp"
#include "usearch/index_dense.hpp"
#include "clip.h"

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <filesystem>

#include <renderdoc_app.h>
#include <dlfcn.h>
#include <utility>
inline RENDERDOC_API_1_6_0 *rdoc_api = NULL;

using json = nlohmann::json;
namespace usrch = unum::usearch;
namespace fs = std::filesystem;

#define INDEX_PATH ROOT_DIR "/index.usearch"

class Application {
  public:
	json getConfig();

	void start();
	void loadImages();

	template <class F>
	inline void async_job(F &&task, const std::string &name = "") {
		pool.enqueue(std::forward<F>(task), name);
	}

	inline bool is_running() {
		return running;
	}

	inline static Application &get_instance() {
		static Application instance;
		return instance;
	}

	~Application();

	// TODO: Need a different method to implement atlas texturing
	// std::vector<ImageData> imageTextures;
	// std::unordered_map<fs::path, GLuint> atlasTextures;

	GLJobQ glJobQ;

	usrch::index_dense_t index;
	clip_ctx			*clip = nullptr;

	DBWrapper	db;
	const char *dbPath = ROOT_DIR "/pics.sqlite";

	ImageManager img_man;

  private:
	json	 config;
	bool	 config_dirty;
	fs::path config_path;

	bool running = true;

	static Application &instance;
	Application();
	Application(const Application &) = delete;
	Application &operator=(const Application &) = delete;

	Window	window;
	UILayer uiLayer;

	ThreadPool pool;

	void process_GL_job();

	void load_config();
};
