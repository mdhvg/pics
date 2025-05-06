#include "Debug.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

/**
	Clear all (unrelated) previous errors.
*/
void GLClearError() {
	while (glGetError() != GL_NO_ERROR)
		;
}

/**
	Check for an error and log the error to the console.

	@param function The function where the error happend
	@param file The file where the error happend
	@param line The line number where the error happend
	@return Whether there was an error
*/
bool GLLogCall() {
	GLenum error = glGetError();
	if (error) {
		SPDLOG_ERROR("[OpenGL Error] (0x{:X})", error);
		return false;
	}
	return true;
}