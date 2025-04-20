#ifndef IMAGE_TEXTURE
#define IMAGE_TEXTURE

#include "imgui.h"

struct ImageTexture {
	unsigned int textureID;
	int width, height;
	ImVec2 uv0, uv1;
	bool initialized = false;
	unsigned char *data;
};

#endif // !IMAGE_TEXTURE