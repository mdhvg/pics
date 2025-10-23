#include "Debug.h"
#include "Window/Window.h"

#include "GLFW/glfw3.h"
#include "spdlog/spdlog.h"

static void glfw_error_callback(int error, const char *description) {
	SPDLOG_ERROR("GLFW Error {}: {}", error, description);
}

bool Window::init() {
	glfwSetErrorCallback(glfw_error_callback);
	ASSERT(glfwInit() && "Failed to initialize GLFW");

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);		   // 3.0+ only

	win = glfwCreateWindow(1280, 720, APP_NAME, NULL, NULL);
	glfwMakeContextCurrent(win);
	glfwSwapInterval(1); // Enable vsync

	ASSERT(gladLoadGLLoader(( GLADloadproc )glfwGetProcAddress) && "Failed to initialize OpenGL loader!");

	SPDLOG_INFO("OpenGL Version: {}", ( const char * )glGetString(GL_VERSION));
	SPDLOG_INFO("GLSL Version: {}", ( const char * )glGetString(GL_SHADING_LANGUAGE_VERSION));
	SPDLOG_INFO("GPU Vendor: {}", ( const char * )glGetString(GL_VENDOR));
	SPDLOG_INFO("Renderer: {}", ( const char * )glGetString(GL_RENDERER));
	int maxTextureUnits;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
	SPDLOG_INFO("Maximum texture units: {}", maxTextureUnits);
	GLint maxLayers;
	glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
	SPDLOG_INFO("Maximum texture array layers supported: {}", maxLayers);
	return true;
}

void Window::close() {
	glfwDestroyWindow(win);
	glfwTerminate();
}

void Window::poll() {
	glfwPollEvents();
	glClearColor(1, 0, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Window::update() {
	GLCall(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	glfwMakeContextCurrent(win);
	glfwSwapBuffers(win);
}