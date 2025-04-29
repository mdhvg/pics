#include <spdlog/spdlog.h>
#include <glad/glad.h>

#ifdef _WIN32
#define TRAP() __debugbreak();
#else
#define TRAP() __builtin_trap();
#endif

#ifdef DEBUG
#define ASSERT(x)                                 \
	if (!(x)) {                                   \
		SPDLOG_ERROR("Assertion failed: {}", #x); \
		TRAP();                                   \
	}

#define GLCall(x)   \
	GLClearError(); \
	x;              \
	ASSERT(GLLogCall());
#else
#define ASSERT(x)                                 \
	if (!(x)) {                                   \
		SPDLOG_ERROR("Assertion failed: {}", #x); \
	}

#define GLCall(x)   \
	GLClearError(); \
	x;              \
	ASSERT(GLLogCall());
#endif

void GLClearError();
bool GLLogCall();