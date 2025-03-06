#include <memory>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;
namespace fs = std::filesystem;

class Application {
public:
	static std::shared_ptr<Application> getInstance();
	GLFWwindow* getWindow();

	void start();

	~Application();
private:
	json config;
	bool config_dirty;
	fs::path config_path;
	bool running = true;
	std::thread imageListThread;
	std::vector<fs::path> imagePaths;

	static std::shared_ptr<Application> instance;
	GLFWwindow* window;
	Application();
	void load_config();
};
