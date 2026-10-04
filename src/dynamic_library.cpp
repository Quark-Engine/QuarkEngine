#include "dynamic_library.h"

#include <utility>

CDynamicLibrary::~CDynamicLibrary()
{
    Close();
}

CDynamicLibrary::CDynamicLibrary(CDynamicLibrary&& other) noexcept
    : m_Handle(other.m_Handle),
      m_Error(std::move(other.m_Error))
{
    other.m_Handle = nullptr;
}

CDynamicLibrary& CDynamicLibrary::operator=(CDynamicLibrary&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    Close();

    m_Handle = other.m_Handle;
    m_Error = std::move(other.m_Error);
    other.m_Handle = nullptr;

    return *this;
}

bool CDynamicLibrary::Open(const std::string& path)
{
    Close();
    m_Error.clear();

#ifdef _WIN32
    m_Handle = LoadLibraryA(path.c_str());
    if (!m_Handle)
    {
        const DWORD errorCode = GetLastError();
        char aMessage[256] = {};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, errorCode, 0, aMessage, sizeof(aMessage), nullptr);
        m_Error = aMessage;
        return false;
    }
#else
    m_Handle = dlopen(path.c_str(), RTLD_NOW);
    if (!m_Handle)
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
    if (!m_Handle)
    {
        return;
    }

#ifdef _WIN32
    FreeLibrary(m_Handle);
#else
    dlclose(m_Handle);
#endif

    m_Handle = nullptr;
}

bool CDynamicLibrary::IsOpen() const
{
    return m_Handle != nullptr;
}

LibHandle CDynamicLibrary::GetHandle() const
{
    return m_Handle;
}

void* CDynamicLibrary::GetSymbol(const char* pName) const
{
    if (!m_Handle)
    {
        return nullptr;
    }

#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(m_Handle, pName));
#else
    return dlsym(m_Handle, pName);
#endif
}

const std::string& CDynamicLibrary::GetError() const
{
    return m_Error;
}