#ifndef UILAYER_H
#define UILAYER_H

#include "ImageManager.h"
#include "Window/Window.h"
#include "imgui.h"

enum View {
	MENU,
	PREVIEW
};

enum Sort {
	FILENAME,
	SORT_COUNT
};

struct UIState {
	int active_image = 0;

	View view{ MENU };

	bool   menu_sidebar = false;
	float  sidebar_width = 0;
	double fps = 0.0f;

	Sort sorting{ FILENAME };
};

class UILayer {
  public:
	void init(Window *win);
	void shutdown();

	void update(double delta);
	void render();

  private:
	Window *win;

	float	startTime;
	float	scroll_y = 0;
	bool	restore_scroll = false;
	UIState state = {};

	void menu(ImageManager &img_man);
	void preview(ImageManager &img_man);
};

#endif // UILAYER_H