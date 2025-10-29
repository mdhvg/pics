#ifndef UILAYER_H
#define UILAYER_H

#include "ImageManager.h"
#include "Window/Window.h"

#include "GLFW/glfw3.h"

enum View {
	MENU,
	PREVIEW
};

struct UIState {
	int active_image = 0;
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
	float scroll_y = 0;
	bool restore_scroll = false;
	UIState state;

	void menu(ImageManager &img_man);
	void preview(ImageManager &img_man);
};

#endif // UILAYER_H