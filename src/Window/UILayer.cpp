#include "Application.h"
#include "Window/UILayer.h"
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
		int x = img.second.atlas_index % 10;
		int y = img.second.atlas_index / 10;
		ImGui::ImageButton(img.second.path.c_str(),
			img_man.atlas_texture[img.second.atlas_id],
			{ 224, 224 },
			{ ( float )x / 10, ( float )y / 10 },
			{ ( float )(x + 1) / 10, ( float )(y + 1) / 10 });
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

void menu_set_items() {
	// ImageHandle &img_handle = ImageHandle::get_instance();
	// Get the ordering also
	// Also limit number of images being shown depending on visible ones
}

void render_menu() {
	// This is 1 frame of the menu
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

	// ImGui::BeginMenuBar();
	// if (ImGui::BeginMenu("File")) {
	// 	if (ImGui::MenuItem("Exit"))
	// 		glfwSetWindowShouldClose(window, GLFW_TRUE);
	// 	ImGui::EndMenu();
	// }
	// ImGui::EndMenuBar();

	// ImGui::InputTextWithHint(
	// 	"Search", "describe image...", searchField, 2048);
	// #ifdef _WIN32
	// 		ImGui::Text("Scanned Images %lld", imagePaths.size());
	// #else
	// 		ImGui::Text("Scanned Images %ld", imagePaths.size());
	// #endif
	float widthAvail = ImGui::GetContentRegionAvail().x;
	int cells = ( int )(widthAvail / (224 + ImGui::GetStyle().ItemSpacing.x));
	cells = std::max(cells, 1);

	float requiredWidth = (224 + ImGui::GetStyle().ItemSpacing.x) * cells;
	ImGui::SetCursorPosX((widthAvail - requiredWidth) * 0.5);

	int inRow = cells;

	// for (const auto &img : imageTextures) {
	// 	// TODO: Make 224 a variable
	// 	int x = img.atlas_index % 10;
	// 	int y = img.atlas_index / 10;
	// 	ImGui::ImageButton(img.path.c_str(), img.textureID, {224, 224}, {(float)x/10, (float)y/10}, {(float)(x+1)/10, (float)(y+1)/10});
	// 	if (--inRow) {
	// 		ImGui::SameLine();
	// 	} else {
	// 		ImGui::SetCursorPosX((widthAvail - requiredWidth) * 0.5);
	// 		inRow = cells;
	// 	}
	// }

	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}