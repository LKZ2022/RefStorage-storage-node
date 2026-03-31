#ifndef PLUGIN_API_HPP
#define PLUGIN_API_HPP

#ifdef _WIN32
    #define PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {
    typedef const char* (*GetCommandNameFunc)();
    typedef void (*ExecuteCommandFunc)(const char* args, char* reply_buffer, int max_len);
}

#endif // PLUGIN_API_HPP