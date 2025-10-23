#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D old_texture;
uniform sampler2DArray images;
uniform sampler1D mask;

uniform int atlas_size;
uniform int n_cells;
uniform int thumb_size;
uniform int thumb_count;

void main() {
	vec4 color = texture(old_texture, TexCoord);
	vec2 pixel = TexCoord * float(atlas_size);

	int cellX = int(pixel.x) / thumb_size;
	int cellY = int(pixel.y) / thumb_size;

	int idx = cellY * n_cells + cellX;

	if (idx < thumb_count) {
		float update = texture(mask, float(idx) / float(thumb_count)).r;
		if (update > 0.0) {
			vec2 local = fract(pixel / float(thumb_size));
			color = texture(images, vec3(local, float(idx)));
		}
	}

	FragColor = color;
}