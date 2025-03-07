#include "Debug.h"

#include <iostream>

void GLClearError()
{
    while (glGetError() != GL_NO_ERROR);
}

bool GLLogCall(const char* function, const char* file, int line)
{
    GLenum error = glGetError();

    if (error)
    {
        SPDLOG_ERROR("[OpenGL Error] (%X) in {} at {}:{}", error, function, file, line);
        return false;
    }

    return true;
}