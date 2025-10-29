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
	ImVec4 *colors = style.Colors;

	// DPI
	style.ScaleAllSizes(1.25f);

	int dark = 1;
// Helper macro to create colors easily
#define COL(r, g, b, a) ImVec4((r) / 255.0f, (g) / 255.0f, (b) / 255.0f, (a))

	style.WindowPadding = ImVec2(12, 12);
	style.FramePadding = ImVec2(2, 3);
	style.ItemSpacing = ImVec2(4, 4);
	style.ScrollbarSize = 5.0f;

	style.WindowRounding = 10.0f;
	style.FrameRounding = 6.0f;
	style.PopupRounding = 8.0f;
	style.ChildRounding = 10.0f;
	style.GrabRounding = 4.0f;
	style.TabRounding = 6.0f;

	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;

	auto &c = style.Colors;

	if (dark) {
		// ===== Dark palette (approx from .dark in CSS) =====
		ImVec4 background = COL(9.0204, 9.0165, 11.301, 255);
		ImVec4 card = COL(54, 54, 58, 255);
		ImVec4 accent = COL(70, 70, 75, 255);
		ImVec4 border = COL(255, 255, 255, 25);
		ImVec4 text = COL(250, 250, 250, 255);
		ImVec4 textMuted = COL(180, 180, 190, 255);
		ImVec4 primary = COL(240, 180, 90, 255); // warm yellowish
		ImVec4 primaryActive = COL(255, 210, 120, 255);
		ImVec4 destructive = COL(235, 80, 70, 255);
		ImVec4 highlight = COL(95, 120, 250, 255);

		c[ImGuiCol_Text] = text;
		c[ImGuiCol_TextDisabled] = textMuted;
		c[ImGuiCol_WindowBg] = background;
		c[ImGuiCol_ChildBg] = card;
		c[ImGuiCol_PopupBg] = card;
		c[ImGuiCol_Border] = border;
		c[ImGuiCol_BorderShadow] = COL(0, 0, 0, 0);

		c[ImGuiCol_FrameBg] = accent;
		c[ImGuiCol_FrameBgHovered] = primary;
		c[ImGuiCol_FrameBgActive] = primaryActive;

		c[ImGuiCol_TitleBg] = accent;
		c[ImGuiCol_TitleBgActive] = primary;
		c[ImGuiCol_TitleBgCollapsed] = accent;

		c[ImGuiCol_MenuBarBg] = accent;

		c[ImGuiCol_ScrollbarBg] = accent;
		c[ImGuiCol_ScrollbarGrab] = primary;
		c[ImGuiCol_ScrollbarGrabHovered] = primaryActive;
		c[ImGuiCol_ScrollbarGrabActive] = primaryActive;

		c[ImGuiCol_CheckMark] = highlight;

		c[ImGuiCol_SliderGrab] = primary;
		c[ImGuiCol_SliderGrabActive] = primaryActive;

		c[ImGuiCol_Button] = accent;
		c[ImGuiCol_ButtonHovered] = primary;
		c[ImGuiCol_ButtonActive] = primaryActive;

		c[ImGuiCol_Header] = accent;
		c[ImGuiCol_HeaderHovered] = primary;
		c[ImGuiCol_HeaderActive] = primaryActive;

		c[ImGuiCol_Separator] = border;
		c[ImGuiCol_SeparatorHovered] = primary;
		c[ImGuiCol_SeparatorActive] = primaryActive;

		c[ImGuiCol_ResizeGrip] = accent;
		c[ImGuiCol_ResizeGripHovered] = primary;
		c[ImGuiCol_ResizeGripActive] = primaryActive;

		c[ImGuiCol_Tab] = accent;
		c[ImGuiCol_TabHovered] = primary;
		c[ImGuiCol_TabActive] = primaryActive;
		c[ImGuiCol_TabUnfocused] = accent;
		c[ImGuiCol_TabUnfocusedActive] = accent;

		// c[ImGuiCol_DockingPreview] = primaryActive;
		// c[ImGuiCol_DockingEmptyBg] = background;

		// c[ImGuiCol_TableHeaderBg] = accent;
		// c[ImGuiCol_TableBorderStrong] = border;
		// c[ImGuiCol_TableBorderLight] = border;
		// c[ImGuiCol_TableRowBg] = background;
		// c[ImGuiCol_TableRowBgAlt] = card;

		c[ImGuiCol_TextSelectedBg] = ImVec4(primary.x, primary.y, primary.z, 0.25f);
		c[ImGuiCol_DragDropTarget] = primaryActive;
		c[ImGuiCol_NavHighlight] = primaryActive;
		c[ImGuiCol_NavWindowingHighlight] = primaryActive;
		c[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.6f);
		c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.6f);
	} else {
		// ===== Light palette (approx from :root in CSS) =====
		ImVec4 background = COL(255, 255, 255, 255);
		ImVec4 card = COL(255, 255, 255, 255);
		ImVec4 accent = COL(240, 240, 240, 255);
		ImVec4 border = COL(220, 220, 225, 255);
		ImVec4 text = COL(36, 36, 38, 255);
		ImVec4 textMuted = COL(110, 110, 120, 255);
		ImVec4 primary = COL(240, 180, 90, 255);
		ImVec4 primaryActive = COL(255, 210, 120, 255);
		ImVec4 highlight = COL(80, 110, 240, 255);

		c[ImGuiCol_Text] = text;
		c[ImGuiCol_TextDisabled] = textMuted;
		c[ImGuiCol_WindowBg] = background;
		c[ImGuiCol_ChildBg] = card;
		c[ImGuiCol_PopupBg] = card;
		c[ImGuiCol_Border] = border;

		c[ImGuiCol_FrameBg] = accent;
		c[ImGuiCol_FrameBgHovered] = primary;
		c[ImGuiCol_FrameBgActive] = primaryActive;

		c[ImGuiCol_Button] = accent;
		c[ImGuiCol_ButtonHovered] = primary;
		c[ImGuiCol_ButtonActive] = primaryActive;

		c[ImGuiCol_Header] = accent;
		c[ImGuiCol_HeaderHovered] = primary;
		c[ImGuiCol_HeaderActive] = primaryActive;
	}

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(glfwWin, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	io.Fonts->AddFontFromFileTTF(
		ROOT_DIR "/rsc/font/Geist-VariableFont_wght.ttf",
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

	if (restore_scroll) {
		ImGui::SetScrollY(scroll_y);
		restore_scroll = false;
	}

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
		restore_scroll = true;
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

	ImVec2 avail = ImGui::GetContentRegionAvail();
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