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
	vec2 prod = pixel / float(thumb_size);

	int idx = int(prod.y) * n_cells + int(prod.x);
	float update = texture(mask, float(idx) / float(thumb_count)).r;
	if (update > 0.5) {
		color = texture(images, vec3(fract(prod.x), fract(prod.y), float(idx)));
	}

	FragColor = color;
}