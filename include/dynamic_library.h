#ifndef __DYNAMIC_LIBRARY_H__
#define __DYNAMIC_LIBRARY_H__
#include <string>

#ifdef _WIN32
   #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOGDI
        #define NOGDI
    #endif
    #ifndef NOUSER
        #define NOUSER
    #endif

    #include <windows.h>

    #undef CloseWindow
    #undef ShowCursor
    #undef Rectangle

    typedef HMODULE LibHandle;
#else
    #include <dlfcn.h>

    typedef void* LibHandle;
#endif

class CDynamicLibrary
{
public:
    CDynamicLibrary() = default;
    ~CDynamicLibrary();

    CDynamicLibrary(const CDynamicLibrary&) = delete;
    CDynamicLibrary& operator=(const CDynamicLibrary&) = delete;
    CDynamicLibrary(CDynamicLibrary&& other) noexcept;
    CDynamicLibrary& operator=(CDynamicLibrary&& other) noexcept;

    bool Open(const std::string& path);
    void Close();

    bool IsOpen() const;
    LibHandle GetHandle() const;

    void* GetSymbol(const char* pName) const;
    const std::string& GetError() const;

private:
    LibHandle m_Handle = nullptr;
    std::string m_Error;
};

#endif // __DYNAMIC_LIBRARY_H__