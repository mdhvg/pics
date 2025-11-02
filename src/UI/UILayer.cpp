#include "IconsLucide.h"
#include "UI/components/Button.h"
#include "UI/components/Sidebar.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "imgui.h"

#include "Application.h"
#include "UI/UILayer.h"
#include "ImageManager.h"
#include "Window/Window.h"
#include "UI/IconButtons.h"

#include <algorithm>
#include <cstddef>
#include <spdlog/spdlog.h>

#define FONT_SIZE 18

void UILayer::init(Window *win) {
	this->win = win;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	( void )io;
	io.IniFilename = NULL;
	io.LogFilename = NULL;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	  // Enable Docking
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;	  // Enable Multi-Viewport

	io.ConfigViewportsNoTaskBarIcon = true;
	io.ConfigErrorRecoveryEnableTooltip = true;
	io.ConfigErrorRecovery = true;
	io.ConfigErrorRecoveryEnableDebugLog = true;

	// Setup Dear ImGui style
	// ImGui::StyleColorsDark();

	ImGuiStyle &style = ImGui::GetStyle();
	style = ImGuiStyle();

	style.WindowPadding = ImVec2(0, 0);
	style.FramePadding = ImVec2(0, 0);
	style.ItemSpacing = ImVec2(0, 0);
	style.ItemInnerSpacing = ImVec2(0, 0);
	style.WindowRounding = 0.0f;
	style.FrameRounding = 0.0f;
	style.ScrollbarRounding = 0.0f;
	style.GrabRounding = 0.0f;
	style.WindowBorderSize = 0.0f;
	style.FrameBorderSize = 0.0f;
	style.TabBorderSize = 0.0f;
	style.IndentSpacing = 0.0f;

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(win->get_handle(), true);
	ImGui_ImplOpenGL3_Init("#version 330");

	ImFontConfig text_font_config;
	text_font_config.RasterizerDensity = 2;
	io.Fonts->AddFontFromFileTTF(
		ROOT_DIR "/fonts/Geist-VariableFont_wght.ttf",
		FONT_SIZE,
		&text_font_config,
		io.Fonts->GetGlyphRangesDefault());

	static const ImWchar icons_ranges[] = { ICON_MIN_LC, ICON_MAX_16_LC, 0 };
	float				 icon_font_size = FONT_SIZE * 2.0f / 3.0f;
	ImFontConfig		 icon_font_config;
	icon_font_config.MergeMode = true;
	icon_font_config.PixelSnapH = true;
	icon_font_config.GlyphMinAdvanceX = icon_font_size;
	io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/lucide.ttf", icon_font_size, &icon_font_config, icons_ranges);
}

void UILayer::shutdown() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void UILayer::update(double delta) {
	if (state.menu_sidebar) {
		state.sidebar_width = 200;
	} else {
		state.sidebar_width = 50;
	}
	state.fps = 1 / delta;
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

	ImGuiIO &io = ImGui::GetIO();
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize(io.DisplaySize);
	ImGui::Begin("Window",
		NULL,
		ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoCollapse
			| ImGuiWindowFlags_NoBringToFrontOnFocus
			| ImGuiWindowFlags_NoNavFocus
			| ImGuiWindowFlags_NoNav
			| ImGuiWindowFlags_MenuBar);

	ImGui::BeginMenuBar();
	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Exit"))
			glfwSetWindowShouldClose(win->get_handle(), GLFW_TRUE);
		ImGui::EndMenu();
	}
	ImGui::EndMenuBar();

	SIDEBAR({
		ImGui::BeginChild("Sidebar", { state.sidebar_width, ImGui::GetContentRegionAvail().y });
		BUTTON_GHOST(
			{
				if (ImGui::Button(state.menu_sidebar ? ICON_LC_CHEVRON_LEFT : ICON_LC_CHEVRON_RIGHT, { state.sidebar_width, 0 })) {
					state.menu_sidebar = !state.menu_sidebar;
				}
			})
		ImGui::EndChild();
	});
	ImGui::SameLine();
	{
		ImGui::BeginChild("Grid");
		ImGui::Text("FPS: %.2f", state.fps);
		float avail = ImGui::GetContentRegionAvail().x;
		float item_w = 112.0f;
		float spacing = 2 * (ImGui::GetStyle().ItemSpacing.x);

		int cells = ( int )((avail + spacing) / (item_w + spacing));
		cells = std::max(cells, 1);

		float used = cells * item_w + (cells - 1) * spacing;

		float offset_x = (avail - used) * 0.5f;
		if (offset_x < 0.0f)
			offset_x = 0.0f;

		// Apply the offset
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);

		int inRow = cells;

		for (const auto &img : img_man.images) {
			// TODO: Make 224 a variable
			int x = img.second.image_index % 10;
			int y = img.second.image_index / 10;
			if (ImGui::ImageButton(img.second.path.c_str(),
					img_man.atlas_texture[img.second.texture_id],
					{ 112, 112 },
					{ ( float )x / 10, ( float )y / 10 },
					{ ( float )(x + 1) / 10, ( float )(y + 1) / 10 })) {
				state.active_image = img.first;
				ImageManager::load_preview(img.second.path);
				scroll_y = ImGui::GetScrollY();
				view = PREVIEW;
			}
			if (--inRow) {
				ImGui::SameLine();
			} else {
				ImGui::SetCursorPosX((avail - used) * 0.5);
				inRow = cells;
			}
		}

		ImGui::EndChild();
	}
	ImGui::End();

	// if (restore_scroll) {
	// 	ImGui::SetScrollY(scroll_y);
	// 	restore_scroll = false;
	// }

	// ImGui::BeginMenuBar();
	// if (ImGui::BeginMenu("File")) {
	// 	if (ImGui::MenuItem("Exit"))
	// 		glfwSetWindowShouldClose(glfwWin, GLFW_TRUE);
	// 	ImGui::EndMenu();
	// }
	// ImGui::EndMenuBar();

	// ImGui::BeginViewportSideBar("Sidebar", ImGui::GetMainViewport(), ImGuiDir_Right, 500, 0);
	// ImGui::Text("This is the sidebar");
	// ImGui::End();

	// ImGui::Begin("Thumbnails");

	// float curTime = glfwGetTime();
	// float fps = 1.0f / (curTime - startTime);
	// ImGui::Text("FPS: %.2f", fps);
	// startTime = curTime;
	// // ImGui::InputTextWithHint("Search", "describe image...", searchField, 2048);
	// ImGui::End();
	// ImGui::End();

	// Rendering
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// 	// Update and Render additional Platform Windows
	// 	// Platform functions may change the current OpenGL context, so we
	// 	// save/restore it to make it easier to paste this code elsewhere.
	// 	ImGuiIO &io = ImGui::GetIO();
	// if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
	// 	GLFWwindow *backup_current_context = glfwGetCurrentContext();
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();
	// 	glfwMakeContextCurrent(backup_current_context);
	// }
}

void UILayer::preview(ImageManager &img_man) {
	if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		view = MENU;
		restore_scroll = true;
	}

	// if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
	// 	ImageManager::preview_texture =
	// }

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGuiViewport *main_viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(main_viewport->Pos);
	ImGui::SetNextWindowSize(main_viewport->Size);
	ImGui::SetNextWindowViewport(main_viewport->ID);
	ImGui::Begin("Main Window",
		nullptr,
		ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar);

	ImGui::BeginMenuBar();
	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Exit"))
			glfwSetWindowShouldClose(win->get_handle(), GLFW_TRUE);
		ImGui::EndMenu();
	}
	ImGui::EndMenuBar();

	ImVec2		  avail = ImGui::GetContentRegionAvail();
	ImageTexture &tex = ImageManager::preview_texture;

	float iw = ( float )tex.width;
	float ih = ( float )tex.height;

	float scale = std::min(avail.x / iw, avail.y / ih);

	ImVec2 size(iw * scale, ih * scale);

	float offset_x = (avail.x - size.x) * 0.5f;
	if (offset_x > 0)
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);

	ImGui::Image(( ImTextureID )( intptr_t )tex.texture_id, size, ImVec2(0, 0), ImVec2(1, 1));

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