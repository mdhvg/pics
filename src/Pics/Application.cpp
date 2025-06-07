#include "Application.h"
#include "Debug.h"
#include "ImageUtils.h"
#include "ModelHandler.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "spdlog/spdlog.h"

#include <dlfcn.h>
#include <renderdoc_app.h>

#include <algorithm>
#include <cstring>
#include <fstream>

json create_default_config() {
	// clang-format off
    return json({
        {"images", {
            {"paths", {
                "/test_imgs"
            }}
        }}
    });
	// clang-format on
}

void Application::load_config() {
	SPDLOG_INFO("ROOT_DIR: {}", ROOT_DIR);
	if (!fs::exists(ROOT_DIR)) {
		fs::create_directory(ROOT_DIR);
	}
	if (!fs::exists(config_path)) {
		SPDLOG_INFO("Config file not found.");
		// Make default config
		json config = create_default_config();
		// TODO: Instead of saving the config, set it as dirty
		std::ofstream ofs(config_path);
		ofs << config.dump(4);
		ofs.close();
	} else {
		std::ifstream ifs(config_path);
		ifs >> config;
		ifs.close();
	}
}

static void glfw_error_callback(int error, const char *description) {
	SPDLOG_ERROR("GLFW Error {}: {}", error, description);
}

Application &Application::getInstance() {
	static Application instance;
	return instance;
}

Application::Application()
	: db(ROOT_DIR "/pics.sqlite"),
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
	  discoverWorker(discoverImages), atlasWorker(loadAtlas),
	  modelWorker(modelHandler) {

	if (void *mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD)) {
		pRENDERDOC_GetAPI RENDERDOC_GetAPI =
			( pRENDERDOC_GetAPI )dlsym(mod, "RENDERDOC_GetAPI");
		int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0,
								   ( void ** )&rdoc_api);
		ASSERT(ret == 1);
	}

	config_path = ROOT_DIR "/config.json";
	load_config();

	db.executeCommand(R"(
		CREATE TABLE IF NOT EXISTS Images (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			path TEXT NOT NULL UNIQUE,
			atlas_path TEXT NOT NULL,
			atlas_index INTEGER NOT NULL,
      embedding INTEGER NOT NULL DEFAULT 0
		);

		CREATE INDEX IF NOT EXISTS idx_imagepath ON Images(path);

		CREATE TABLE IF NOT EXISTS Atlas (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			atlas_path TEXT NOT NULL UNIQUE,
			idx INTEGER NOT NULL,
			image_count INTEGER NOT NULL DEFAULT 0
		);
		)");

	index = usrch::index_dense_t::make(
		usrch::metric_punned_t(512, usrch::metric_kind_t::cos_k));

	SignalBus::getInstance();

	glfwSetErrorCallback(glfw_error_callback);
	ASSERT(glfwInit() && "Failed to initialize GLFW");

	const char *glsl_version = "#version 330";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);		   // 3.0+ only
	// glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

	window = glfwCreateWindow(1280, 720, "Pics", nullptr, nullptr);
	glfwSetWindowCloseCallback(window, [](GLFWwindow *window) {
		Application::getInstance().running = false;
	});

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	ASSERT(gladLoadGLLoader(( GLADloadproc )glfwGetProcAddress) &&
		   "Failed to initialize OpenGL loader!");

	SPDLOG_INFO("OpenGL Version: {}", ( const char * )glGetString(GL_VERSION));
	SPDLOG_INFO("GLSL Version: {}",
				( const char * )glGetString(GL_SHADING_LANGUAGE_VERSION));
	SPDLOG_INFO("GPU Vendor: {}", ( const char * )glGetString(GL_VENDOR));
	SPDLOG_INFO("Renderer: {}", ( const char * )glGetString(GL_RENDERER));
	int maxTextureUnits;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
	SPDLOG_INFO("Maximum texture units: {}", maxTextureUnits);
	GLint maxLayers;
	glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
	SPDLOG_INFO("Maximum texture array layers supported: {}", maxLayers);

	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	( void )io;
	io.ConfigFlags |=
		ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |=
		ImGuiConfigFlags_NavEnableGamepad;			  // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport
														// / Platform Windows
	io.ConfigViewportsNoTaskBarIcon = true;
	io.ConfigErrorRecoveryEnableTooltip = true;
	io.ConfigErrorRecovery = true;
	io.ConfigErrorRecoveryEnableDebugLog = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform
	// windows can look identical to regular ones.
	ImGuiStyle &style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	io.Fonts->AddFontFromFileTTF(ROOT_DIR
								 "/rsc/font/Inter-VariableFont_opsz,wght.ttf",
								 16.0f,
								 nullptr,
								 io.Fonts->GetGlyphRangesDefault());
}

Application::~Application() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Application::start() {
	discoverWorker.run();
	atlasWorker.run();
	modelWorker.run();

	float startTime = 0.0f;
	char  searchField[2048];
	memset(searchField, 0, 2048);
	double maxLoadTime = 0.0f;

	float  fps = 0.0f;
	float  minFPS = float(0b11111111111111111111111111111111);
	ImVec2 cursor;

	while (!glfwWindowShouldClose(window) || running) {
		glfwPollEvents();
		if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGuiViewport *main_viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(main_viewport->Pos);
		ImGui::SetNextWindowSize(main_viewport->Size);
		ImGui::SetNextWindowViewport(main_viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Main Window",
					 nullptr,
					 ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize |
						 ImGuiWindowFlags_NoCollapse |
						 ImGuiWindowFlags_MenuBar |
						 ImGuiWindowFlags_NoTitleBar);
		ImGui::PopStyleVar(3);

		ImGui::BeginMenuBar();
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Exit"))
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();

		float curTime = glfwGetTime();
		fps = 1.0f / (curTime - startTime);
		minFPS = std::min(minFPS, fps);
		ImGui::Text("FPS: %.2f", fps);
		ImGui::Text("Min FPS: %.2f", minFPS);
		startTime = curTime;
		ImGui::InputTextWithHint(
			"Search", "describe image...", searchField, 2048);
		ImGui::Text("Max load time %.4f", maxLoadTime);
		// #ifdef _WIN32
		// 		ImGui::Text("Scanned Images %lld", imagePaths.size());
		// #else
		// 		ImGui::Text("Scanned Images %ld", imagePaths.size());
		// #endif
		ImGui::Text("Cursor pos: %.2fx, %2fy", cursor.x, cursor.y);
		float widthAvail = ImGui::GetContentRegionAvail().x;
		int	  cells =
			( int )(widthAvail / (224 + ImGui::GetStyle().ItemSpacing.x));
		cells = std::max(cells, 1);

		float requiredWidth = (224 + ImGui::GetStyle().ItemSpacing.x) * cells;
		ImGui::SetCursorPosX((widthAvail - requiredWidth) * 0.5);

		int inRow = cells;

		for (const auto &img : imageTextures) {
			// TODO: Make 224 a variable
			int x = img.atlas_index % 10;
			int y = img.atlas_index / 10;
			ImGui::Image(( ImTextureID )(img.textureID),
						 ImVec2(224, 224),
						 // BUG: Something's wrong causing images to show
						 // upside-down, currently solving by flipping coords
						 // TODO: Fix the image flipping issue
						 ImVec2(( float )x / 10, ( float )(y + 1) / 10),
						 ImVec2(( float )(x + 1) / 10, ( float )y / 10));
			if (--inRow) {
				ImGui::SameLine();
			} else {
				ImGui::SetCursorPosX((widthAvail - requiredWidth) * 0.5);
				inRow = cells;
			}
		}

		// for (auto const &[image, texture] : atlasTextures) {
		// #ifdef _WIN32
		// 			ImGui::Text("Path: %ls", image.c_str());
		// #else
		// 			ImGui::Text("Path: %s", image.c_str());
		// #endif
		// if (!texture.initialized) {
		// 	auto load_start = std::chrono::high_resolution_clock::now();
		// 	initializeTexture(image);
		// 	auto load_end = std::chrono::high_resolution_clock::now();
		// 	std::chrono::duration<double> duration = load_end - load_start;
		// 	maxLoadTime = std::max(maxLoadTime, duration.count());
		// }
		// ImGui::Image((ImTextureID)(texture), ImVec2(224, 224), texture.uv0,
		// texture.uv1); if (--inRow) { 	ImGui::SameLine(); } else {
		// 	ImGui::SetCursorPosX((widthAvail - requiredWidth) * 0.5);
		// 	inRow = cells;
		// }
		// }

		ImGui::End();

		// Rendering
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(1, 0, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (!glJobQ.empty()) {
			glJobQ.pop()->execute();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		// Update and Render additional Platform Windows
		// Platform functions may change the current OpenGL context, so we
		// save/restore it to make it easier to paste this code elsewhere.
		ImGuiIO &io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow *backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}

		glfwSwapBuffers(window);
	}

	while (!glJobQ.empty()) {
		glJobQ.pop()->reject();
	}

	SignalBus::getInstance().appRunningM = false;
}

json Application::getConfig() {
	return config;
}

void Application::loadImages() {
	atlasWorker.run();
}