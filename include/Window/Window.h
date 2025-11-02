#ifndef WINDOW_H
#define WINDOW_H

#include "GLFW/glfw3.h"
#include "imgui.h"

class Window {
  public:
	bool init();
	void close();

	inline GLFWwindow *get_handle() {
		return win;
	}
	inline bool is_open() {
		return !glfwWindowShouldClose(win);
	}
	inline ImVec2 get_pos() {
		return { ( float )xpos, ( float )ypos };
	}
	inline ImVec2 get_size() {
		return { ( float )xpos, ( float )ypos };
	}
	inline double get_delta() {
		double cur = glfwGetTime();
		double delta = cur - start_time;
		start_time = cur;
		return delta;
	}
	void poll();
	void update();

  private:
	GLFWwindow *win{ NULL };
	int			width, height;
	int			xpos, ypos;
	double		start_time;
};

#endif // WINDOW_H