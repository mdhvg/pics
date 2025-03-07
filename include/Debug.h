#include <spdlog/spdlog.h>
#include <glad/glad.h>

#ifdef _WIN32
#define TRAP() __debugbreak();
#else
#define TRAP() __builtin_trap();
#endif

#ifdef DEBUG
#define ASSERT_IMPL(x, msg) if (!(x)) \
    { \
        SPDLOG_ERROR("Assertion failed: {} | {}", #x, msg); TRAP(); \
    }

#define ASSERT_NO_MSG(x) if (!(x)) \
    { \
        SPDLOG_ERROR("Assertion failed: {}", #x); TRAP(); \
    }

#define GET_MACRO(_1,_2,NAME,...) NAME

#define ASSERT(...) \
    GET_MACRO(__VA_ARGS__, ASSERT_IMPL, ASSERT_NO_MSG)(__VA_ARGS__)

#define GL_CALL(x) GLClearError(); x; ASSERT(GLLogCall(#x, __FILE__, __LINE__));
    void GLClearError();
    bool GLLogCall(const char* function, const char* file, int line);
#else
#define ASSERT_IMPL(x, msg) if (!(x)) \
    { \
        SPDLOG_ERROR("Assertion failed: {} | {}", #x, msg); \
    }

#define ASSERT_NO_MSG(x) if (!(x)) \
    { \
        SPDLOG_ERROR("Assertion failed: {}", #x); \
    }

#define GET_MACRO(_1,_2,NAME,...) NAME

#define ASSERT(...) \
    GET_MACRO(__VA_ARGS__, ASSERT_IMPL, ASSERT_NO_MSG)(__VA_ARGS__)

#define GL_CALL(x) x
#endif