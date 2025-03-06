#include <spdlog/spdlog.h>

#ifdef DEBUG
#define ASSERT(x) if (!(x)) \
    { \
        SPDLOG_ERROR("Assertion failed: {}", #x); \
        __builtin_trap(); \
    }

#define GL_CALL(x) if ((x) <= 0) \
    { \
        unsigned long err_code; \
        while ((err_code = ERR_get_error()) != 0) { \
            char err_msg[256]; \
            ERR_error_string_n(err_code, err_msg, sizeof(err_msg)); \
            SPDLOG_ERROR("OpenSSL error: {}", err_msg); \
        } \
        __builtin_trap(); \
    }
#else
#define ASSERT(x) if (!(x)) \
{ \
    SPDLOG_ERROR("Assertion failed: {}", #x); \
}

#define GL_CALL(x) if ((x) <= 0) \
    { \
        unsigned long err_code; \
        while ((err_code = ERR_get_error()) != 0) { \
            char err_msg[256]; \
            ERR_error_string_n(err_code, err_msg, sizeof(err_msg)); \
            SPDLOG_ERROR("OpenSSL error: {}", err_msg); \
        } \
    }
#endif