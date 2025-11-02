#include "Application.h"
#include "Debug.h"
#include "ImageManager.h"
#include "ModelHandler.h"

#include <dlfcn.h>
#include <renderdoc_app.h>

json create_default_config() {
	// clang-format off
    auto config = json({
        {"images", {
            {"paths", {
                ROOT_DIR "/wallpapers"
            }}
        }}
    });
	return config;
	// clang-format on
}

void db_init() {
	Application &app = Application::get_instance();
	app.db.init(app.dbPath); // TODO: Can make a config parser and have db path come from it
	// TODO: Make an entry for mtime, ctime (UNIX epoch both), size and other stuff in Images table.
	app.db.executeCommand(R"(
		CREATE TABLE IF NOT EXISTS Images (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			path TEXT NOT NULL UNIQUE,
			atlas_id INTEGER NOT NULL,
			atlas_index INTEGER NOT NULL,
			filename TEXT NOT NULL,
      		embedding INTEGER NOT NULL DEFAULT 0
		);

		CREATE INDEX IF NOT EXISTS idx_imagepath ON Images(path);

		CREATE TABLE IF NOT EXISTS Atlas (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			atlas_path TEXT NOT NULL UNIQUE,
			idx INTEGER NOT NULL,
			image_count INTEGER NOT NULL DEFAULT 0
		);

		CREATE TABLE IF NOT EXISTS Holes (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			atlas_id INTEGER NOT NULL UNIQUE,
			atlas_index INTEGER NOT NULL
		);
	)");
}

// void Application::load_config() {
// 	SPDLOG_INFO("ROOT_DIR: {}", ROOT_DIR);
// 	if (!fs::exists(ROOT_DIR)) {
// 		fs::create_directory(ROOT_DIR);
// 	}
// 	if (!fs::exists(config_path)) {
// 		SPDLOG_INFO("Config file not found.");
// 		// Make default config
// 		json config = create_default_config();
// 		// TODO: Instead of saving the config, set it as dirty
// 		std::ofstream ofs(config_path);
// 		ofs << config.dump(4);
// 		ofs.close();
// 	} else {
// 		std::ifstream ifs(config_path);
// 		ifs >> config;
// 		ifs.close();
// 	}
// }

Application &Application::get_instance() {
	static Application instance;
	return instance;
}

Application::Application()
	: pool(4), running(true), config(create_default_config())
/*
	  Functions to run on other threads:
			  - Look for new images, create their atlas and add them to the
			  database: discoverImages();
			  - Load the atlases and create some data structure to represent how
			  they will display: loadAtlas();
			  - Load model and (vector) index any images that haven't been
			  indexed yet and keep model graphs for image and text in
	  memory:createModelGraph();
								- Load the image+text embedding model into
	  desired hardware, query database for pending images to be indexed, index
	  them and save those indexes. Wait for *AI* tasks and send response when
	  required
		  */
//   discoverWorker(discoverImages), atlasWorker(loadAtlas)
//   modelWorker(modelHandler)
{

	// This kinda API
	// db_init(); - Run all commands to create tables, etc (Different thread)
	pool.enqueue(db_init, "Database init");
	// db_init();

	// load_atlases(); - Load existing atlases (Different thread, GLJob way)
	// compile_shader(); - Compile all the different shaders that will be used (Different thread, GLJob way)
	// discover_images(); - Find all new images in the selected dirs (Different thread)
	// 		create_atlases(); - Create images atlases

	// ui_init(); - Do OpenGL and ImGUI things (Same thread)
	// ui_init();

	if (void *mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD)) {
		pRENDERDOC_GetAPI RENDERDOC_GetAPI = ( pRENDERDOC_GetAPI )dlsym(mod, "RENDERDOC_GetAPI");
		int				  ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, ( void ** )&rdoc_api);
		ASSERT(ret == 1);
	}

	// config_path = ROOT_DIR "/config.json";
	// load_config();

	// index = usrch::index_dense_t::make(
	// 	usrch::metric_punned_t(512, usrch::metric_kind_t::cos_k));
}

Application::~Application() {}

void Application::start() {
	if (!window.init()) {
		running = false;
		return;
	}

	pool.enqueue([this]() {
		img_man.discover_images();
	},
		"Discover images");
	pool.enqueue(ImageManager::compile_shader);
	pool.enqueue(ImageManager::create_buffers);
	pool.enqueue(ImageManager::cache_atlas);
	pool.enqueue(ImageManager::load_images);

	uiLayer.init(&window);
	while (running && window.is_open()) {
		window.poll();
		uiLayer.update(window.get_delta());
		uiLayer.render();
		window.update();
		process_GL_job();
	}

	running = false;

	while (!glJobQ.empty()) {
		glJobQ.pop()->reject();
	}

	uiLayer.shutdown();
	window.close();

	// float startTime = 0.0f;
	// char searchField[2048];
	// memset(searchField, 0, 2048);
	// double maxLoadTime = 0.0f;

	// float fps = 0.0f;
	// float minFPS = float(0b11111111111111111111111111111111);
	// SPDLOG_INFO("Min FPS in start: {}", minFPS);
	// ImVec2 cursor;

	// while (!glfwWindowShouldClose(window) || running) {
	// 	if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
	// 		ImGui_ImplGlfw_Sleep(10);
	// 		continue;
	// 	}

	// 	if (!glJobQ.empty()) {
	// 		glJobQ.pop()->execute();
	// 		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// 	}

	// 	// Update and Render additional Platform Windows
	// 	// Platform functions may change the current OpenGL context, so we
	// 	// save/restore it to make it easier to paste this code elsewhere.
	// 	ImGuiIO &io = ImGui::GetIO();
	// 	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
	// 		GLFWwindow *backup_current_context = glfwGetCurrentContext();
	// 		ImGui::UpdatePlatformWindows();
	// 		ImGui::RenderPlatformWindowsDefault();
	// 		glfwMakeContextCurrent(backup_current_context);
	// 	}

	// 	glfwSwapBuffers(window);
	// }
}

void Application::process_GL_job() {
	if (!glJobQ.empty()) {
		auto job = glJobQ.pop();
		if (job)
			job->execute();
	}
}

json Application::getConfig() {
	return config;
}

void Application::loadImages() {
	// atlasWorker.run();
}