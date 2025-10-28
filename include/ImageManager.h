#ifndef IMAGEMANAGER_H
#define IMAGEMANAGER_H

#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

struct ImageTexture {
	std::string path;
	unsigned int texture_id = 0;  // For thumbnails, this is atlas_id
	unsigned int image_index = 0; // For thumbnails only
	size_t width = 0,
		   height = 0;
};

/*
***Atlas***

The atlas is a continuous 10x10 grid of images (Think of it like a jigsaw puzzle).
When images aren't exactly a multiple of 100, there will be space left in the atlas.
Also, when any existing image is deleted, it will leave holes (Marked with X'es)
in the atlas and those indices will be recorded in the holes table and deleted
from Images table.

Both of these are later filled with image thumbnails when new images are
discovered.

In this example, we have these holes (This grid is 6x7)
[0]: { index =  3, size =  2 }
[1]: { index =  8, size =  1 }
[2]: { index = 17, size =  3 }
[3]: { index = 22, size = 20 }

┌────────────────────────────────┬─────────────────────┬─────────────────────┐
│                                │xxxxxxxxxxxxxxxxxxxxx│                     │
│                                │xxxxxxxxxxxxxxxxxxxxx│                     │
│                                │xxxxxxxxxxxxxxxxxxxxx│                     │
│                                │xxxxxxxxxxxxxxxxxxxxx│                     │
│                                │xxxxxxxxxxxxxxxxxxxxx│                     │
│          ┌──────────┐          └─────────────────────┘                     │
│          │xxxxxxxxxx│                                                      │
│          │xxxxxxxxxx│                                                      │
│          │xxxxxxxxxx│                                                      │
│          │xxxxxxxxxx│                                                      │
│          │xxxxxxxxxx│                                                      │
│          └──────────┘          ┌────────────────────────────────┐          │
│                                │xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx│          │
│                                │xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx│          │
│                                │xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx│          │
│                                │xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx│          │
│                                │xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx│          │
│          ┌─────────────────────┴────────────────────────────────┴──────────┤
│          │                                                                 │
│          │                                                                 │
│          │                                                                 │
│          │                                                                 │
│          │                                                                 │
├──────────┘                                                                 │
│                                                                            │
│                                                                            │
│                                                                            │
│                                                                            │
│                         Space pending for new images                       │
│                                                                            │
│                                                                            │
│                                                                            │
│                                                                            │
│                                                                            │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘
 */

// Data necessary to store hole location
struct AtlasHole {
	std::string path;
	int atlas_id, hole_index, size;
};

class ImageManager {
  public:
	void discover_images();

	static void load_images();
	static void cache_atlas();
	static void compile_shader();
	static void create_buffers();
	static void load_preview(const std::string &path);

	std::unordered_map<unsigned int, ImageTexture> images;
	static std::unordered_map<unsigned int, unsigned int> atlas_texture;

	static ImageTexture preview_texture;

  private:
	void create_atlas(std::queue<std::string> &paths);
	void put_thumbnail(unsigned char *data, AtlasHole hole, unsigned int offset);
	void refill_holes(std::vector<std::string> &paths, const std::vector<AtlasHole> &holes);
	void bind_thumbnails(std::vector<std::pair<unsigned int, std::string>> &index_path);
	void draw_to_fbo(unsigned int *old_texture);
	void replace_texture(unsigned int id, unsigned int new_texture);
	void save_framebuffer(std::string &path);
	void prepare_framebuffer();

	static unsigned int
		atlas_shader,
		framebuffer,
		image_array,
		mask_texture,
		vao, vbo, ebo;
	static unsigned char mask[100];
};

#endif // IMAGEMANAGER_H