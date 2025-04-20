#include "ImageTexture.h"
#include "SignalBus.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;
namespace fs = std::filesystem;

class Application {
  public:
	static Application &getInstance();
	GLFWwindow *getWindow();

	void start();

	~Application();

	std::unordered_map<fs::path, ImageTexture> imageTextures;

  private:
	json config;
	bool config_dirty;
	fs::path config_path;

	bool running = true;

	std::thread imageListThread;
	std::vector<fs::path> imagePaths;

	static Application &instance;
	Application();

	Application(const Application &) = delete;
	Application &operator=(const Application &) = delete;

	GLFWwindow *window;
	void load_config();
};
