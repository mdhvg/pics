#version 330 core

in vec2 TexCoord;
uniform sampler2DArray texArray;
uniform sampler2D fbTex;
uniform int textureCount;
uniform int textureStart;

out vec4 FragColor;

void main() {
	vec2 pos = TexCoord * vec2(10);
	int texIndex = int(pos.y) * 10 + int(pos.x);
	if (texIndex >= textureCount) {
		FragColor = vec4(0.0f);
		return;
	}
	if (texIndex < textureStart) {
		FragColor = texture(fbTex, TexCoord);
		return;
	}
	vec2 localCoord = fract(pos);
	FragColor = texture(texArray, vec3(localCoord, texIndex - textureStart));
}