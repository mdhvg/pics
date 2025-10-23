#ifndef WINDOW_H
#define WINDOW_H

#include "GLFW/glfw3.h"

class Window {
  public:
	bool init();
	void close();

	inline GLFWwindow *handle() {
		return win;
	}
	inline bool is_open() {
		return !glfwWindowShouldClose(win);
	}
	void poll();
	void update();

  private:
	GLFWwindow *win{ NULL };
};

#endif // WINDOW_H