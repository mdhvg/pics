#include "GLJob.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "ImageManager.h"
#include "Utils.h"
#include "Application.h"
#include "Debug.h"
#include "profile.h"

#include <cstddef>
#include <string>
#include <memory>
#include <unordered_map>
#include <cstring>
#include <filesystem>
#include <queue>

unsigned int  ImageManager::framebuffer = 0;
unsigned int  ImageManager::atlas_shader = 0;
unsigned int  ImageManager::vao = 0;
unsigned int  ImageManager::vbo = 0;
unsigned int  ImageManager::ebo = 0;
unsigned int  ImageManager::image_array = 0;
unsigned int  ImageManager::mask_texture = 0;
ImageTexture  ImageManager::preview_texture;
unsigned char ImageManager::mask[100] = { 0 };

std::unordered_map<unsigned int, unsigned int> ImageManager::atlas_texture;

struct Atlas {
	std::string	 path;
	int unsigned id, index;
};

bool imageExists(const fs::path &path, DBWrapper &db) {
	bool exists = false;

	db.executeCommand(
		fmt::format("SELECT path FROM Images WHERE path = '{}' LIMIT 1;",
			path.string()),
		[](void *data, int, char **argv, char **) -> int {
			bool *exists = ( bool * )data;
			*exists = true;
			return 0;
		},
		&exists);

	return exists;
}

unsigned char *get_thumbnail_data(const std::string &path) {
	// Might need to put this here
	// #ifdef _WIN32
	// 	auto path = imagePath.u8string();
	// 	auto s =
	// 		std::string(reinterpret_cast<const char *>(path.data()), path.size());
	// #else
	// 	auto s = imagePath.string();
	// #endif

	int			   width, height;
	unsigned char *data = stbi_load(path.c_str(), &width, &height, NULL, 3);
	ASSERT(data != nullptr);

	int			   smaller_side = std::min(width, height);
	unsigned char *cropped = new unsigned char[smaller_side * smaller_side * 3];
	int			   x_off = (width - smaller_side) / 2;
	int			   y_off = (height - smaller_side) / 2;

	unsigned char *src = data + (y_off * width + x_off) * 3;
	unsigned char *dst = cropped;
	for (int i = 0; i < smaller_side; i++) {
		memcpy(dst, src, smaller_side * 3);
		src += width * 3;
		dst += smaller_side * 3;
	}
	stbi_image_free(data);

	unsigned char *resized = stbir_resize_uint8_linear(cropped, smaller_side, smaller_side, 0, NULL, 224, 224, 0, STBIR_RGB);
	delete[] cropped;

	return resized;
}

void create_texture(unsigned int *texture, int count = 1, unsigned char *data = NULL) {
}

unsigned int get_hole_atlas(ImageManager *manager, const Atlas &hole) {
	int id = hole.id;
	if (manager->atlas_texture.find(id) != manager->atlas_texture.end()) {
		return manager->atlas_texture[id];
	}
	std::mutex end;
	end.lock();

	unsigned int   texture;
	int			   width, height;
	unsigned char *data = stbi_load(hole.path.c_str(), &width, &height, NULL, 4);
	auto		   job = std::make_shared<GLJob>([t = &texture, data, width, height]() {
		  GLCall(glGenTextures(1, t));
		  GLCall(glBindTexture(GL_TEXTURE_2D, *t));

		  GLCall(glTexImage2D(GL_TEXTURE_2D,
			  0,
			  GL_RGB,
			  2240,
			  2240,
			  0,
			  GL_RGB,
			  GL_UNSIGNED_BYTE,
			  data));

		  GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		  GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		  GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
		  GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));

		  stbi_image_free(data);
	  },
		  "create texture",
		  &end);
	Application::get_instance().glJobQ.push(job);

	end.lock();
	manager->atlas_texture[id] = texture;
	return texture;
}

void gen_framebuffer(unsigned int *fbo) {
	auto job = std::make_shared<GLJob>([fbo]() {
		if (!glIsFramebuffer(*fbo)) {
			GLCall(glGenFramebuffers(1, fbo));
		}
	});
	Application::get_instance().glJobQ.push(job);
}

unsigned int new_texture(int width, int height, unsigned char *data = NULL) {
	unsigned int texture;
	std::mutex	 end;
	end.lock();

	auto job = std::make_shared<GLJob>([t = &texture, width, height, data]() {
		GLCall(glGenTextures(1, t));
		GLCall(glBindTexture(GL_TEXTURE_2D, *t));
		GLCall(glBindTexture(GL_TEXTURE_2D, *t));

		GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2240, 2240, 0, GL_RGB, GL_UNSIGNED_BYTE, data));

		GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
		GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
	},
		"",
		&end);
	Application::get_instance().glJobQ.push(job);

	end.lock();
	return texture;
}

void bind_tex_to_fbo(unsigned int tex, unsigned int fbo) {
	std::mutex end;
	end.lock();

	auto job = std::make_shared<GLJob>([tex, fbo]() {
		GLCall(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
		GLCall(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0));

		GLuint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		SPDLOG_INFO("Framebuffer status: 0x{:X} ({})",
			status,
			status == GL_FRAMEBUFFER_COMPLETE ? "Complete" : "Incomplete");
		ASSERT(status == GL_FRAMEBUFFER_COMPLETE);
	},
		"",
		&end);
	Application::get_instance().glJobQ.push(job);

	end.lock();
}

void draw_thumbnail(unsigned char *data, int index, unsigned int fbo) {
}

void ImageManager::draw_to_fbo(unsigned int *old_texture) {
	auto job = std::make_shared<GLJob>(
		[this, old_texture]() {
			if (rdoc_api)
				rdoc_api->StartFrameCapture(NULL, NULL);
			ASSERT(glIsVertexArray(vao));
			ASSERT(glIsBuffer(vbo));
			ASSERT(glIsBuffer(ebo));
			ASSERT(glIsFramebuffer(framebuffer));
			ASSERT(glIsProgram(atlas_shader));
			ASSERT(glIsTexture(image_array));
			ASSERT(glIsTexture(mask_texture));

			// if (!glIsTexture(*old_texture)) {
			// 	GLCall(glGenTextures(1, old_texture));
			// }

			GLCall(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));
			GLCall(glUseProgram(atlas_shader));
			GLCall(glBindVertexArray(vao));

			// uniform sampler2D old_texture;
			if (*old_texture != 0) {
				glActiveTexture(GL_TEXTURE0);
				GLCall(glBindTexture(GL_TEXTURE_2D, *old_texture));
				glUniform1i(glGetUniformLocation(atlas_shader, "old_texture"), 0);
			}
			// uniform sampler2DArray images;
			glActiveTexture(GL_TEXTURE1);
			GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, image_array));
			glUniform1i(glGetUniformLocation(atlas_shader, "images"), 1);
			// uniform sampler1D mask;
			glActiveTexture(GL_TEXTURE2);
			GLCall(glBindTexture(GL_TEXTURE_1D, mask_texture));
			glUniform1i(glGetUniformLocation(atlas_shader, "mask"), 2);

			// uniform int atlas_size;
			GLCall(glUniform1i(glGetUniformLocation(atlas_shader, "atlas_size"), 2240));
			// uniform int n_cells;
			GLCall(glUniform1i(glGetUniformLocation(atlas_shader, "n_cells"), 10));
			// uniform int thumb_size;
			GLCall(glUniform1i(glGetUniformLocation(atlas_shader, "thumb_size"), 224));
			// uniform int thumb_count;
			GLCall(glUniform1i(glGetUniformLocation(atlas_shader, "thumb_count"), 100));

			// TODO: Make 224 variable
			GLCall(glViewport(0, 0, 2240, 2240));
			GLCall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL));
			if (rdoc_api)
				rdoc_api->EndFrameCapture(NULL, NULL);
		});
	Application::get_instance().glJobQ.push(job);
}

void ImageManager::bind_thumbnails(std::vector<std::pair<unsigned int, std::string>> &index_path) {
	memset(mask, 0, 100);
	for (int i = 0; i < index_path.size() && Application::get_instance().is_running(); i++) { // TODO: Needs something else to check if it's even on
		unsigned char *data = get_thumbnail_data(index_path[i].second);
		auto		   idx = index_path[i].first;
		mask[idx] = 0xFF;

		auto job = std::make_shared<GLJob>([this, data, idx]() {
			ASSERT(data != NULL);
			if (!glIsTexture(image_array)) {
				GLCall(glGenTextures(1, &image_array));
				GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, image_array));
				GLCall(glTexImage3D(GL_TEXTURE_2D_ARRAY,
					0,
					GL_RGB,
					224,
					224,
					100,
					0,
					GL_RGB,
					GL_UNSIGNED_BYTE,
					NULL));
				GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
				GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
				GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
				GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
			}
			GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, image_array));
			GLCall(glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
				0,
				0,
				0,
				idx,
				224,
				224,
				1,
				GL_RGB,
				GL_UNSIGNED_BYTE,
				data));
			stbi_image_free(data);
		});
		Application::get_instance().glJobQ.push(job);
	}
	auto job = std::make_shared<GLJob>([]() {
		if (!glIsTexture(mask_texture)) {
			GLCall(glGenTextures(1, &mask_texture));
			GLCall(glBindTexture(GL_TEXTURE_1D, mask_texture));
			GLCall(glTexImage1D(GL_TEXTURE_1D,
				0,
				GL_RED,
				100,
				0,
				GL_RED,
				GL_UNSIGNED_BYTE,
				NULL));
			GLCall(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
			GLCall(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		}
		GLCall(glBindTexture(GL_TEXTURE_1D, mask_texture));
		GLCall(glTexImage1D(GL_TEXTURE_1D,
			0,
			GL_RED,
			100,
			0,
			GL_RED,
			GL_UNSIGNED_BYTE,
			mask));
	});
	Application::get_instance().glJobQ.push(job);
}

void ImageManager::replace_texture(unsigned int id, unsigned int new_texture) {
	atlas_texture[id] = new_texture;
}

void delete_texture(unsigned int texture) {
	auto job = std::make_shared<GLJob>([t = &texture]() {
		GLCall(glBindTexture(GL_TEXTURE_2D, *t));
		GLCall(glDeleteTextures(1, t));
	});
	Application::get_instance().glJobQ.push(job);
}

void update_incomplete_db(Atlas atlas, int count) {
	Application &app = Application::get_instance();
	app.db.executeCommand(fmt::format(
		R"(INSERT INTO Atlas
			(atlas_path, idx, image_count)
			VALUES ('{}', {}, {})
			ON CONFLICT(atlas_path)
			DO UPDATE SET
			idx = excluded.idx,
			image_count = excluded.imagecount;)",
		atlas.path,
		atlas.index,
		atlas.index + count));
}

void add_images_db(Atlas atlas, std::vector<std::pair<unsigned int, std::string>> &fill) {
	Application &app = Application::get_instance();
	app.db.executeCommand("BEGIN TRANSACTION;");
	for (int i = 0; i < fill.size(); i++) {
		unsigned int id;
		app.db.executeCommand(
			fmt::format(R"(
				INSERT INTO Images(path, atlas_id, atlas_index, filename)
				VALUES
				('{}', {}, {}, '{}')
				RETURNING id;
			)",
				fill[i].second,
				atlas.id,
				fill[i].first,
				fs::path(fill[i].second).filename().string()),
			[](void *data, int, char **argv, char **) {
				unsigned int *id = ( unsigned int * )data;
				*id = std::stoul(argv[0]);
				return 0;
			},
			&id);
		app.img_man.images[id] = { fill[i].second,
			atlas.id,
			fill[i].first };
	}
	app.db.executeCommand("COMMIT;");
}

int find_id_db() {
	Application &app = Application::get_instance();
	int			 id;
	app.db.executeCommand("SELECT COUNT(1) FROM Atlas;", [](void *data, int, char **argv, char **) {
		int *id = ( int * )data;
		*id = std::stoi(argv[0]) + 1;
		return 0;
	},
		&id);
	return id;
}

void add_atlas_db(Atlas atlas, int count) {
	Application &app = Application::get_instance();
	app.db.executeCommand(fmt::format(
		R"(INSERT INTO Atlas
			(atlas_path, idx, image_count)
			VALUES ('{}', {}, {}))",
		atlas.path,
		atlas.id,
		count));
}

void ImageManager::save_framebuffer(std::string &path) {
	std::mutex end;
	end.lock();

	unsigned char *data = new unsigned char[2240 * 2240 * 3];
	auto		   job = std::make_shared<GLJob>([data]() {
		  ASSERT(glIsFramebuffer(framebuffer));
		  GLCall(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));
		  GLCall(glReadPixels(0,
			  0,
			  2240,
			  2240,
			  GL_RGB,
			  GL_UNSIGNED_BYTE,
			  data));
	  },
		  "",
		  &end);
	Application::get_instance().glJobQ.push(job);

	auto atlas_dir = fs::path(path).parent_path();
	if (!fs::exists(atlas_dir)) {
		fs::create_directories(atlas_dir);
	}

	end.lock();

	stbi_write_png(path.c_str(), 2240, 2240, 3, data, 0);
	delete[] data;
}

void ImageManager::create_atlas(std::queue<std::string> &paths) {
	Application &app = Application::get_instance();

	std::vector<std::pair<unsigned int, std::string>> hole_fill;
	// TODO: Find the ones with holes also
	std::vector<Atlas> incomplete;
	app.db.executeCommand(R"(SELECT
			id,
			atlas_path,
			idx
			FROM Atlas WHERE image_count < 100;
		)",
		[](void *data, int, char **argv, char **) {
			auto incomplete = *( std::vector<Atlas> * )data;
			incomplete.push_back({ argv[1],
				( unsigned int )std::stoul(argv[0]),
				( unsigned int )std::stoul(argv[2]) });
			return 0;
		},
		&incomplete);

	for (auto const &atlas : incomplete) {
		int count = 0;
		for (int i = atlas.index; i < 100 && !paths.empty(); i++) {
			hole_fill.push_back({ i, paths.front() });
			count++;
			paths.pop();
		}
		ZoneScoped;
		bind_thumbnails(hole_fill);
		unsigned int old_texture = get_hole_atlas(this, atlas);
		gen_framebuffer(&framebuffer);
		unsigned int empty = new_texture(2240, 2240);
		bind_tex_to_fbo(empty, framebuffer);
		draw_to_fbo(&old_texture);
		replace_texture(atlas.id, empty);
		delete_texture(old_texture);
		update_incomplete_db(atlas, count);
		add_images_db(atlas, hole_fill);
		std::vector<std::pair<unsigned int, std::string>> temp;
		temp.swap(hole_fill);
	}

	SPDLOG_INFO("Found {} images", paths.size());

	// TODO: Now create new ones
	int iters = (paths.size() + 99) / 100;
	for (int i = 0; i < iters && Application::get_instance().is_running(); i++) {
		unsigned int count = 0;
		for (int i = 0; i < 100 && !paths.empty(); i++) {
			hole_fill.push_back({ i, paths.front() });
			count++;
			paths.pop();
		}
		bind_thumbnails(hole_fill);
		gen_framebuffer(&framebuffer);
		unsigned int empty = new_texture(2240, 2240);
		bind_tex_to_fbo(empty, framebuffer);
		unsigned int old_texture = 0;
		draw_to_fbo(&old_texture);
		unsigned int id = find_id_db();
		atlas_texture[id] = empty;
		std::string atlas_path = fmt::format(ROOT_DIR "/.atlas/atlas_{}.png", id);
		save_framebuffer(atlas_path);
		add_atlas_db({ atlas_path, id, count - 1 }, count);
		add_images_db({ atlas_path, id, count - 1 }, hole_fill);
		std::vector<std::pair<unsigned int, std::string>> temp;
		temp.swap(hole_fill);
	}
}

void ImageManager::discover_images() {
	Application &app = Application::get_instance();
	json		 config = app.getConfig();

	std::queue<std::string> pending;
	for (const auto &path : config["images"]["paths"].get<std::vector<std::string>>()) {
		pending.push(path);
	}

	const int				IMAGE_BATCH_SIZE = 100;
	std::queue<std::string> found_images;

	int depth = 0;
	// TODO: Document about max directory traversal levels = 15
	const int MAX_DEPTH = 15;

	while (pending.size() && depth <= MAX_DEPTH) {
		int n_current = pending.size();
		for (int i = 0; i < n_current; i++) {
			fs::path current = pending.front();
			pending.pop();
			for (const fs::path &entry : fs::directory_iterator(current)) {
				if (fs::is_directory(entry)) {
					pending.push(entry);
					continue;
				}
				if (fs::is_regular_file(entry) && (entry.extension() == ".jpg" || entry.extension() == ".png")) {
					if (imageExists(entry, app.db))
						continue;
					found_images.push(fs::absolute(entry));
				}
			}
		}
	}
	app.async_job([this, images = std::move(found_images)]() mutable {
		create_atlas(images);
	},
		"Atlas create job");
	// app.async_job([this]() {
	// 	prepare_framebuffer();
	// },
	// 	"Prepare framerbuffer");
}

void ImageManager::load_images() {
	Application &app = Application::get_instance();
	app.db.executeCommand("SELECT * FROM Images;", [](void *data, int, char **argv, char **) {
		auto		 images = ( std::unordered_map<unsigned int, ImageTexture> * )data;
		unsigned int id = ( unsigned int )std::stoul(argv[0]);
		if (images->find(id) == images->end()) {
			(*images)[id] = {
				.path = argv[1],
				.texture_id = ( unsigned int )std::stoul(argv[2]),
				.image_index = ( unsigned int )std::stoul(argv[3])
			};
		}
		return 0;
	},
		&app.img_man.images);
}

void ImageManager::cache_atlas() {
	Application &app = Application::get_instance();

	std::vector<std::pair<int, std::string>> atlas_ids;
	app.db.executeCommand("SELECT * FROM Atlas;", [](void *data, int, char **argv, char **) {
		auto atlas_ids = ( std::vector<std::pair<int, std::string>> * )data;
		atlas_ids->push_back({ std::stoi(argv[0]), argv[1] });
		return 0;
	},
		&atlas_ids);

	for (int i = 0; i < atlas_ids.size(); i++) {
		int			   _w, _h;
		unsigned char *data = stbi_load(atlas_ids[i].second.c_str(), &_w, &_h, NULL, 3);
		int			   id = atlas_ids[i].first;
		auto		   atlasJob = std::make_shared<GLJob>([id, data]() {
			  unsigned int tex_ptr;
			  GLCall(glGenTextures(1, &tex_ptr));
			  GLCall(glBindTexture(GL_TEXTURE_2D, tex_ptr));
			  GLCall(glTexImage2D(GL_TEXTURE_2D,
				  0,
				  GL_RGB,
				  2240,
				  2240,
				  0,
				  GL_RGB,
				  GL_UNSIGNED_BYTE,
				  data));
			  GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
			  GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
			  GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
			  GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
			  stbi_image_free(data);

			  atlas_texture[id] = tex_ptr;
		  });
		app.glJobQ.push(atlasJob);
	}
}

void ImageManager::create_buffers() {
	float vertices[] = {
		// clang-format off
		-1.0f, -1.0f, 0.0f, 0.0f,       // Bottom-left
		 1.0f, -1.0f, 1.0f, 0.0f,       // Bottom-right
		 1.0f,  1.0f, 1.0f, 1.0f,		// Top-right
		-1.0f,  1.0f, 0.0f, 1.0f		// Top-left
		// clang-format on
	};
	int indices[] = {
		// clang-format off
		0, 1, 2,
		2, 3, 0
		// clang-format on
	};

	auto job = std::make_shared<GLJob>([vertices, indices]() {
		GLCall(glGenVertexArrays(1, &vao));
		GLCall(glGenBuffers(1, &vbo));
		GLCall(glGenBuffers(1, &ebo));

		GLCall(glBindVertexArray(vao));

		GLCall(glBindBuffer(GL_ARRAY_BUFFER, vbo));
		GLCall(glBufferData(
			GL_ARRAY_BUFFER,
			sizeof(vertices),
			vertices,
			GL_STATIC_DRAW));

		GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));
		GLCall(glBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			sizeof(indices),
			indices,
			GL_STATIC_DRAW));

		GLCall(glVertexAttribPointer(
			0,
			2,
			GL_FLOAT,
			GL_FALSE,
			4 * sizeof(float),
			( void * )0));
		GLCall(glEnableVertexAttribArray(0));

		GLCall(glVertexAttribPointer(1,
			2,
			GL_FLOAT,
			GL_FALSE,
			4 * sizeof(float),
			( void * )(2 * sizeof(float))));
		GLCall(glEnableVertexAttribArray(1))
	});
	Application::get_instance().glJobQ.push(job);
}

void ImageManager::load_preview(const std::string &path) {
	load_as_texture(&preview_texture, path);
}

void ImageManager::compile_shader() {
	auto vert_src = read_file(ROOT_DIR "/rsc/atlas.vert");
	auto frag_src = read_file(ROOT_DIR "/rsc/atlas.frag");
	auto job = std::make_shared<GLJob>([vert_src = std::move(vert_src), frag_src = std::move(frag_src)]() {
		int	 status;
		char info_log[512];

		unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
		const char	*vert_ptr = vert_src.c_str();
		GLCall(glShaderSource(vert, 1, &vert_ptr, NULL));
		GLCall(glCompileShader(vert));
		GLCall(glGetShaderiv(vert, GL_COMPILE_STATUS, &status));
		if (!status) {
			GLCall(glGetShaderInfoLog(vert, 512, NULL, info_log));
			SPDLOG_ERROR("{} shader compilation failed: {}", "VERTEX", info_log);
			ASSERT(false);
		}

		unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);
		const char	*frag_ptr = frag_src.c_str();
		GLCall(glShaderSource(frag, 1, &frag_ptr, NULL));
		GLCall(glCompileShader(frag));
		GLCall(glGetShaderiv(frag, GL_COMPILE_STATUS, &status));
		if (!status) {
			GLCall(glGetShaderInfoLog(frag, 512, NULL, info_log));
			SPDLOG_ERROR("{} shader compilation failed: {}", "FRAGMENT", info_log);
			ASSERT(false);
		}

		atlas_shader = glCreateProgram();
		GLCall(glAttachShader(atlas_shader, vert));
		GLCall(glAttachShader(atlas_shader, frag));
		GLCall(glLinkProgram(atlas_shader));
		glGetProgramiv(atlas_shader, GL_LINK_STATUS, &status);
		if (!status) {
			glGetProgramInfoLog(atlas_shader, 512, nullptr, info_log);
			SPDLOG_ERROR("Shader program linking failed: {}", info_log);
			ASSERT(false);
		}
		GLCall(glDeleteShader(vert));
		GLCall(glDeleteShader(frag));
	});
	Application::get_instance().glJobQ.push(job);
}

void load_as_texture(ImageTexture *texture, const std::string &path, TextureLoadParams params) {
	Application::get_instance().async_job([texture, &path, params]() {
		unsigned char *data = stbi_load(
			path.c_str(),
			&texture->width,
			&texture->height,
			&texture->channels,
			params.req_comp);
		MAKE_JOB([texture, data, params]() {
			GLenum format = GL_RGB;
			if (texture->channels == 4)
				format = GL_RGBA;

			if (!glIsTexture(texture->texture_id)) {
				GLCall(glGenTextures(1, &(texture->texture_id)));
				GLCall(glBindTexture(GL_TEXTURE_2D, texture->texture_id));
				GLCall(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
				GLCall(glTexImage2D(GL_TEXTURE_2D,
					0,
					format,
					texture->width,
					texture->height,
					0,
					format,
					GL_UNSIGNED_BYTE,
					data));

				GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
				GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
				GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
				GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
			} else {
				GLCall(glBindTexture(GL_TEXTURE_2D, texture->texture_id));
				GLCall(glTexImage2D(GL_TEXTURE_2D,
					0,
					format,
					texture->width,
					texture->height,
					0,
					format,
					GL_UNSIGNED_BYTE,
					data));
			}
			stbi_image_free(data);
		});
	});
}