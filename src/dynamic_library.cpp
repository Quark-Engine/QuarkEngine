#include "dynamic_library.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#endif

#include <filesystem>
#include <utility>

CDynamicLibrary::~CDynamicLibrary()
{
    Close();
}

CDynamicLibrary::CDynamicLibrary(CDynamicLibrary&& other) noexcept
    : m_pHandle(other.m_pHandle),
      m_Error(std::move(other.m_Error))
{
    other.m_pHandle = nullptr;
}

CDynamicLibrary& CDynamicLibrary::operator=(CDynamicLibrary&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    Close();

    m_pHandle = other.m_pHandle;
    m_Error = std::move(other.m_Error);
    other.m_pHandle = nullptr;

    return *this;
}

bool CDynamicLibrary::Open(const std::string& path)
{
    Close();
    m_Error.clear();

#ifdef _WIN32
    std::filesystem::path fsPath(path);
    m_pHandle = LoadLibraryW(fsPath.wstring().c_str());
    if (!m_pHandle)
    {
        const DWORD errorCode = GetLastError();
        char aMessage[256] = {};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, errorCode, 0, aMessage, sizeof(aMessage), nullptr);
        m_Error = aMessage;
        return false;
    }
#else
    m_pHandle = dlopen(path.c_str(), RTLD_NOW);
    if (!m_pHandle)
    {
        const char* pError = dlerror();
        m_Error = pError ? pError : "unknown error";
        return false;
    }
#endif

    return true;
}

void CDynamicLibrary::Close()
{
    if (!m_pHandle)
    {
        return;
    }

#ifdef _WIN32
    FreeLibrary(m_pHandle);
#else
    dlclose(m_pHandle);
#endif

    m_pHandle = nullptr;
}

bool CDynamicLibrary::IsOpen() const
{
    return m_pHandle != nullptr;
}

LibHandle CDynamicLibrary::GetHandle() const
{
    return m_pHandle;
}

void* CDynamicLibrary::GetSymbol(const char* pName) const
{
    if (!m_pHandle)
    {
        return nullptr;
    }

#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(m_pHandle, pName));
#else
    return dlsym(m_pHandle, pName);
#endif
}

const std::string& CDynamicLibrary::GetError() const
{
    return m_Error;
}
