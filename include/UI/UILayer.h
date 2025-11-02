#ifndef UILAYER_H
#define UILAYER_H

#include "ImageManager.h"
#include "Window/Window.h"

enum View {
	MENU,
	PREVIEW
};

struct UIState {
	int active_image = 0;

	bool menu_sidebar = false;
	float sidebar_width = 0;
	double fps = 0.0f;
};

class UILayer {
  public:
	void init(Window *win);
	void shutdown();

	void update(double delta);
	void render();

  private:
	Window* win;
	View view{ MENU };

	float startTime;
	float scroll_y = 0;
	bool restore_scroll = false;
	UIState state = {0};

	void menu(ImageManager &img_man);
	void preview(ImageManager &img_man);
};

#endif // UILAYER_H