#ifndef UILAYER_H
#define UILAYER_H

#include "ImageManager.h"
#include "Window/Window.h"

#include "GLFW/glfw3.h"

enum View {
	MENU,
	PREVIEW
};

class UILayer {
  public:
	void init(Window &win);
	void shutdown();

	void render();

  private:
	GLFWwindow *glfwWin;
	View view{ MENU };

	float startTime;

	void menu(ImageManager &img_man);
};

#endif // UILAYER_H