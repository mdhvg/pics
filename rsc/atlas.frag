#version 460 core

in vec2 TexCoord;
uniform sampler2DArray texArray;
uniform int textureCount;

out vec4 FragColor;

void main() {
	vec2 pos = TexCoord * vec2(10);
	int texIndex = int(pos.y) * 10 + int(pos.x);
	if (texIndex >= textureCount) {
		FragColor = vec4(0.0f);
		return;
	}
	vec2 localCoord = fract(pos);
	FragColor = texture(texArray, vec3(localCoord, texIndex));
}