#include "Application.h"
#include "Debug.h"
#include <fstream>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

json create_default_config() {
    return json({
        {"images", {
            {"paths", {
                "test_imgs"
            }}
        }}
        });
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
    }
    else {
        std::ifstream ifs(config_path);
        ifs >> config;
        ifs.close();
    }
}

static void glfw_error_callback(int error, const char* description)
{
    SPDLOG_ERROR("GLFW Error {}: {}", error, description);
}

std::shared_ptr<Application> Application::instance = nullptr;

std::shared_ptr<Application> Application::getInstance() {
    if (!instance) {
        instance = std::shared_ptr<Application>(new Application());
    }
    return instance;
}

Application::Application() {
    config_path = ROOT_DIR "/config.json";
    load_config();

    glfwSetErrorCallback(glfw_error_callback);
    ASSERT(glfwInit() && "Failed to initialize GLFW");

    const char* glsl_version = "#version 400";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
    // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    window = glfwCreateWindow(1280, 720, "Pics", nullptr, nullptr);
    glfwSetWindowCloseCallback(window, [](GLFWwindow* window) {
        Application::getInstance()->running = false;
        });

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    ASSERT(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) && "Failed to initialize OpenGL loader!");

    SPDLOG_INFO("OpenGL Version: {}", (const char*)glGetString(GL_VERSION));
    SPDLOG_INFO("GLSL Version: {}", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
    SPDLOG_INFO("GPU Vendor: {}", (const char*)glGetString(GL_VENDOR));
    SPDLOG_INFO("Renderer: {}", (const char*)glGetString(GL_RENDERER));

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    io.Fonts->AddFontFromFileTTF(ROOT_DIR "/rsc/font/Inter-VariableFont_opsz,wght.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesDefault());
}

Application::~Application() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::start() {
    while (!glfwWindowShouldClose(window) || running) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(main_viewport->Pos);
        ImGui::SetNextWindowSize(main_viewport->Size);
        ImGui::SetNextWindowViewport(main_viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar);
        ImGui::PopStyleVar(3);

        ImGui::BeginMenuBar();
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit"))
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();

        ImGui::BeginChild("Left Page", ImVec2(200, 0), true);
        ImGui::Text("Left Pane");
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("Right Page", ImVec2(0, 0), true);
        ImGui::Text("Right Pane");
        ImGui::EndChild();

        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(1, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }
}

GLFWwindow* Application::getWindow() { return window; }
