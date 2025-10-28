#include "Application.h"
#include "Window/UILayer.h"
#include "ImageManager.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include <algorithm>

void UILayer::init(Window &win) {
	glfwWin = win.handle();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	( void )io;
	io.IniFilename = NULL;
	io.LogFilename = NULL;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	  // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;	  // Enable Multi-Viewport

	io.ConfigViewportsNoTaskBarIcon = true;
	io.ConfigErrorRecoveryEnableTooltip = true;
	io.ConfigErrorRecovery = true;
	io.ConfigErrorRecoveryEnableDebugLog = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	ImGuiStyle &style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(glfwWin, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	io.Fonts->AddFontFromFileTTF(
		ROOT_DIR "/rsc/font/Inter-VariableFont_opsz,wght.ttf",
		16.0f,
		nullptr,
		io.Fonts->GetGlyphRangesDefault());
}

void UILayer::shutdown() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void UILayer::render() {
	Application &app = Application::get_instance();
	if (view == MENU) {
		menu(app.img_man);
	} else {
		preview(app.img_man);
	}
}

void UILayer::menu(ImageManager &img_man) {
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
		ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar);
	ImGui::PopStyleVar(3);

	ImGui::BeginMenuBar();
	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Exit"))
			glfwSetWindowShouldClose(glfwWin, GLFW_TRUE);
		ImGui::EndMenu();
	}
	ImGui::EndMenuBar();

	float curTime = glfwGetTime();
	float fps = 1.0f / (curTime - startTime);
	ImGui::Text("FPS: %.2f", fps);
	startTime = curTime;
	// ImGui::InputTextWithHint("Search", "describe image...", searchField, 2048);
	float widthAvail = ImGui::GetContentRegionAvail().x;
	int cells = ( int )(widthAvail / (224 + ImGui::GetStyle().ItemSpacing.x));
	cells = std::max(cells, 1);

	float requiredWidth = (224 + ImGui::GetStyle().ItemSpacing.x) * cells;
	ImGui::SetCursorPosX((widthAvail - requiredWidth) * 0.5);

	int inRow = cells;

	for (const auto &img : img_man.images) {
		// TODO: Make 224 a variable
		int x = img.second.image_index % 10;
		int y = img.second.image_index / 10;
		if (ImGui::ImageButton(img.second.path.c_str(),
				img_man.atlas_texture[img.second.texture_id],
				{ 224, 224 },
				{ ( float )x / 10, ( float )y / 10 },
				{ ( float )(x + 1) / 10, ( float )(y + 1) / 10 })) {
			state.active_image = img.first;
			ImageManager::load_preview(img.second.path);
			view = PREVIEW;
		}
		if (--inRow) {
			ImGui::SameLine();
		} else {
			ImGui::SetCursorPosX((widthAvail - requiredWidth) * 0.5);
			inRow = cells;
		}
	}

	ImGui::End();

	// Rendering
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// 	// Update and Render additional Platform Windows
	// 	// Platform functions may change the current OpenGL context, so we
	// 	// save/restore it to make it easier to paste this code elsewhere.
	// 	ImGuiIO &io = ImGui::GetIO();
	// 	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
	// 		GLFWwindow *backup_current_context = glfwGetCurrentContext();
	ImGui::UpdatePlatformWindows();
	// 		ImGui::RenderPlatformWindowsDefault();
	// 		glfwMakeContextCurrent(backup_current_context);
	// 	}
}

void UILayer::preview(ImageManager &img_man) {
	if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		view = MENU;
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
		ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar);
	ImGui::PopStyleVar(3);

	ImGui::BeginMenuBar();
	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Exit"))
			glfwSetWindowShouldClose(glfwWin, GLFW_TRUE);
		ImGui::EndMenu();
	}
	ImGui::EndMenuBar();

	ImVec2 dimensions = ImGui::GetContentRegionAvail();

	ImGui::Image(ImageManager::preview_texture.texture_id,
		{ ( float )ImageManager::preview_texture.width, ( float )ImageManager::preview_texture.height },
		{ 0, 0 },
		{ 1, 1 });

	ImGui::End();

	// Rendering
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// 	// Update and Render additional Platform Windows
	// 	// Platform functions may change the current OpenGL context, so we
	// 	// save/restore it to make it easier to paste this code elsewhere.
	// 	ImGuiIO &io = ImGui::GetIO();
	// 	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
	// 		GLFWwindow *backup_current_context = glfwGetCurrentContext();
	ImGui::UpdatePlatformWindows();
	// 		ImGui::RenderPlatformWindowsDefault();
	// 		glfwMakeContextCurrent(backup_current_context);
	// 	}
}