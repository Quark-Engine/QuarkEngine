#include "scripting/script_engine.h"
#include "QuarkEngine/SDK.h"
#include "editor/editor_hierarchy_utils.h"
#include "engine/component_factory_registry.h"
#include "engine/scene_document.h"
#include "entity.h"
#include "project.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

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

namespace fs = std::filesystem;

namespace quark::script
{

    namespace
    {

#ifdef _WIN32
        std::wstring Utf8ToWide(const std::string& str)
        {
            if (str.empty())
            {
                return L"";
            }
            const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
            std::wstring wstr(sizeNeeded, 0);
            MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], sizeNeeded);
            return wstr;
        }
#endif

        std::string Trim(const std::string& str)
        {
            size_t first = str.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
            {
                return "";
            }
            size_t last = str.find_last_not_of(" \t\r\n");
            return str.substr(first, (last - first + 1));
        }

        std::string StripComments(const std::string& source)
        {
            std::string result;
            result.reserve(source.size());

            bool inBlockComment = false;
            bool inLineComment = false;
            bool inString = false;

            for (size_t i = 0; i < source.size(); ++i)
            {
                char c = source[i];
                char next = (i + 1 < source.size()) ? source[i + 1] : '\0';

                if (inBlockComment)
                {
                    if (c == '*' && next == '/')
                    {
                        inBlockComment = false;
                        ++i;
                    }
                    continue;
                }

                if (inLineComment)
                {
                    if (c == '\n' || c == '\r')
                    {
                        inLineComment = false;
                        result += c;
                    }
                    continue;
                }

                if (inString)
                {
                    result += c;
                    if (c == '\\' && next != '\0')
                    {
                        result += next;
                        ++i;
                    }
                    else if (c == '"')
                    {
                        inString = false;
                    }
                    continue;
                }

                if (c == '/' && next == '*')
                {
                    inBlockComment = true;
                    ++i;
                    continue;
                }

                if (c == '/' && next == '/')
                {
                    inLineComment = true;
                    ++i;
                    continue;
                }

                if (c == '"')
                {
                    inString = true;
                    result += c;
                    continue;
                }

                result += c;
            }

            return result;
        }

        fs::path GetExecutableDirectory()
        {
#ifdef _WIN32
            std::wstring executablePath(MAX_PATH, L'\0');
            DWORD pathLength = GetModuleFileNameW(nullptr, executablePath.data(),
                                                  static_cast<DWORD>(executablePath.size()));
            while (pathLength == executablePath.size())
            {
                executablePath.resize(executablePath.size() * 2);
                pathLength = GetModuleFileNameW(nullptr, executablePath.data(),
                                                static_cast<DWORD>(executablePath.size()));
            }
            executablePath.resize(pathLength);
            return fs::path(executablePath).parent_path();
#elif defined(__APPLE__)
            uint32_t size = 0;
            _NSGetExecutablePath(nullptr, &size);
            std::string path(size, '\0');
            if (_NSGetExecutablePath(&path[0], &size) == 0)
            {
                return fs::canonical(path.c_str()).parent_path();
            }
            return fs::current_path();
#else
            return fs::canonical("/proc/self/exe").parent_path();
#endif
        }

        std::string QuotePath(const fs::path& path)
        {
            return "\"" + path.generic_string() + "\"";
        }

        std::string GetScriptLibraryExtension()
        {
#if defined(_WIN32)
            return ".dll";
#elif defined(__APPLE__)
            return ".dylib";
#else
            return ".so";
#endif
        }

        std::vector<std::string> GetScriptLibraryFileNames(const std::string& baseName)
        {
            const std::string ext = GetScriptLibraryExtension();
            std::vector<std::string> vNames;
            vNames.push_back(baseName + ext);
            vNames.push_back("lib" + baseName + ext);
#if !defined(_WIN32)
            if (ext != ".so")
            {
                vNames.push_back(baseName + ".so");
                vNames.push_back("lib" + baseName + ".so");
            }
            if (ext != ".dylib")
            {
                vNames.push_back(baseName + ".dylib");
                vNames.push_back("lib" + baseName + ".dylib");
            }
#endif
            return vNames;
        }

        bool RunBuildCommand(const std::string& command, std::string& errorMessage,
                             const CScriptEngine::FProgressCallback& progressCallback)
        {
            if (progressCallback)
            {
                progressCallback("> " + command);
            }
#ifdef _WIN32
            std::wstring wideCommand = Utf8ToWide(command + " 2>&1");
            FILE* pPipe = _wpopen(wideCommand.c_str(), L"rt");
#else
            FILE* pPipe = popen((command + " 2>&1").c_str(), "r");
#endif
            if (pPipe == nullptr)
            {
                errorMessage = "Failed to start command: " + command;
                return false;
            }

            char aLine[1024];
            std::string pendingLine;
            std::string fullOutput;
            while (fgets(aLine, sizeof(aLine), pPipe) != nullptr)
            {
                fullOutput += aLine;
                pendingLine += aLine;
                size_t newlinePosition = pendingLine.find('\n');
                while (newlinePosition != std::string::npos)
                {
                    if (progressCallback)
                    {
                        progressCallback(pendingLine.substr(0, newlinePosition));
                    }
                    pendingLine.erase(0, newlinePosition + 1);
                    newlinePosition = pendingLine.find('\n');
                }
            }
            if (!pendingLine.empty() && progressCallback)
            {
                progressCallback(pendingLine);
            }
#ifdef _WIN32
            const int result = _pclose(pPipe);
#else
            const int result = pclose(pPipe);
#endif
            if (result == 0)
            {
                return true;
            }
            errorMessage = fullOutput.empty()
                ? ("Build command failed with exit code " + std::to_string(result))
                : fullOutput;
            return false;
        }

    } // anonymous

    bool CScriptHeaderParser::ParseHeader(const std::string& filePath, SParsedScriptInfo& outInfo)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = StripComments(buffer.str());

        size_t classPos = content.find("class ");
        std::string className;
        size_t classBodyStart = std::string::npos;

        while (classPos != std::string::npos)
        {
            size_t nameStart = content.find_first_not_of(" \t\r\n", classPos + 6);
            if (nameStart == std::string::npos)
            {
                break;
            }

            size_t nameEnd = content.find_first_of(" \t\r\n:{", nameStart);
            if (nameEnd == std::string::npos)
            {
                break;
            }

            std::string candidateName = content.substr(nameStart, nameEnd - nameStart);

            size_t colonPos = content.find(':', nameEnd);
            size_t openBrace = content.find('{', nameEnd);

            if (colonPos != std::string::npos && colonPos < openBrace)
            {
                std::string inheritance = content.substr(colonPos + 1, openBrace - (colonPos + 1));
                if (inheritance.find("IScript") != std::string::npos)
                {
                    className = candidateName;
                    classBodyStart = openBrace + 1;
                    break;
                }
            }

            classPos = content.find("class ", classPos + 6);
        }

        if (className.empty() || classBodyStart == std::string::npos)
        {
            return false;
        }

        outInfo.m_ClassName = className;
        outInfo.m_HeaderPath = filePath;
        outInfo.m_vFields.clear();

        int braceDepth = 1;
        size_t classBodyEnd = std::string::npos;
        for (size_t i = classBodyStart; i < content.size(); ++i)
        {
            if (content[i] == '{')
            {
                ++braceDepth;
            }
            else if (content[i] == '}')
            {
                --braceDepth;
                if (braceDepth == 0)
                {
                    classBodyEnd = i;
                    break;
                }
            }
        }

        if (classBodyEnd == std::string::npos)
        {
            return false;
        }

        std::string body = content.substr(classBodyStart, classBodyEnd - classBodyStart);

        bool isPublic = false;
        size_t pos = 0;
        while (pos < body.size())
        {
            size_t nextSemicolon = body.find(';', pos);
            size_t nextColon = body.find(':', pos);

            if (nextColon != std::string::npos && (nextSemicolon == std::string::npos || nextColon < nextSemicolon))
            {
                std::string label = Trim(body.substr(pos, nextColon - pos));
                if (label == "public" || label.find("public") != std::string::npos)
                {
                    isPublic = true;
                }
                else if (label == "private" || label == "protected" ||
                         label.find("private") != std::string::npos || label.find("protected") != std::string::npos)
                {
                    isPublic = false;
                }
                pos = nextColon + 1;
                continue;
            }

            if (nextSemicolon == std::string::npos)
            {
                break;
            }

            std::string statement = Trim(body.substr(pos, nextSemicolon - pos));
            pos = nextSemicolon + 1;

            if (!isPublic || statement.empty())
            {
                continue;
            }

            if (statement.find('(') != std::string::npos || statement.find(')') != std::string::npos)
            {
                continue;
            }
            if (statement.rfind("using ", 0) == 0 || statement.rfind("typedef ", 0) == 0 ||
                statement.rfind("struct ", 0) == 0 || statement.rfind("class ", 0) == 0 ||
                statement.rfind("enum ", 0) == 0 || statement.rfind("friend ", 0) == 0)
            {
                continue;
            }

            std::string decl = statement;
            std::string defaultValStr;
            size_t eqPos = decl.find('=');
            if (eqPos != std::string::npos)
            {
                defaultValStr = Trim(decl.substr(eqPos + 1));
                decl = Trim(decl.substr(0, eqPos));
            }

            std::istringstream iss(decl);
            std::string typeToken;
            std::string nameToken;
            iss >> typeToken;
            if (typeToken == "unsigned" || typeToken == "signed" || typeToken == "std::")
            {
                std::string subType;
                iss >> subType;
                typeToken += " " + subType;
            }
            iss >> nameToken;

            nameToken = Trim(nameToken);
            if (nameToken.empty())
            {
                continue;
            }

            SScriptField field;
            field.m_Name = nameToken;

            if (typeToken == "float" || typeToken == "double")
            {
                field.m_Type = EScriptFieldType::FLOAT;
                if (!defaultValStr.empty())
                {
                    try
                    {
                        std::string num = defaultValStr;
                        if (!num.empty() && (num.back() == 'f' || num.back() == 'F'))
                        {
                            num.pop_back();
                        }
                        field.m_FloatValue = std::stof(num);
                    }
                    catch (...)
                    {
                        field.m_FloatValue = 0.0f;
                    }
                }
            }
            else if (typeToken == "int" || typeToken == "int32_t" || typeToken == "long")
            {
                field.m_Type = EScriptFieldType::INT;
                if (!defaultValStr.empty())
                {
                    try
                    {
                        field.m_IntValue = std::stoi(defaultValStr);
                    }
                    catch (...)
                    {
                        field.m_IntValue = 0;
                    }
                }
            }
            else if (typeToken == "bool")
            {
                field.m_Type = EScriptFieldType::BOOL;
                if (!defaultValStr.empty())
                {
                    field.m_BoolValue = (defaultValStr == "true" || defaultValStr == "1");
                }
            }
            else if (typeToken == "std::string" || typeToken == "string")
            {
                field.m_Type = EScriptFieldType::STRING;
                if (!defaultValStr.empty())
                {
                    if (defaultValStr.size() >= 2 && defaultValStr.front() == '"' && defaultValStr.back() == '"')
                    {
                        field.m_StringValue = defaultValStr.substr(1, defaultValStr.size() - 2);
                    }
                    else
                    {
                        field.m_StringValue = defaultValStr;
                    }
                }
            }
            else if (typeToken == "Vec3" || typeToken == "Vector3" || typeToken == "qc::Vec3")
            {
                field.m_Type = EScriptFieldType::VEC3;
                if (!defaultValStr.empty())
                {
                    size_t braceStart = defaultValStr.find('{');
                    size_t braceEnd = defaultValStr.find('}');
                    if (braceStart != std::string::npos && braceEnd != std::string::npos)
                    {
                        std::string coords = defaultValStr.substr(braceStart + 1, braceEnd - braceStart - 1);
                        std::replace(coords.begin(), coords.end(), ',', ' ');
                        std::istringstream ciss(coords);
                        ciss >> field.m_Vec3Value.x >> field.m_Vec3Value.y >> field.m_Vec3Value.z;
                    }
                }
            }
            else if (typeToken == "Color" || typeToken == "qc::Color")
            {
                field.m_Type = EScriptFieldType::COLOR;
                if (!defaultValStr.empty())
                {
                    size_t braceStart = defaultValStr.find('{');
                    size_t braceEnd = defaultValStr.find('}');
                    if (braceStart != std::string::npos && braceEnd != std::string::npos)
                    {
                        std::string channels = defaultValStr.substr(braceStart + 1, braceEnd - braceStart - 1);
                        std::replace(channels.begin(), channels.end(), ',', ' ');
                        std::istringstream ciss(channels);
                        int r = 255;
                        int g = 255;
                        int b = 255;
                        int a = 255;
                        ciss >> r >> g >> b;
                        if (!(ciss >> a))
                        {
                            a = 255;
                        }
                        field.m_ColorValue = {
                            static_cast<unsigned char>(std::clamp(r, 0, 255)),
                            static_cast<unsigned char>(std::clamp(g, 0, 255)),
                            static_cast<unsigned char>(std::clamp(b, 0, 255)),
                            static_cast<unsigned char>(std::clamp(a, 0, 255))
                        };
                    }
                }
            }
            else if (typeToken == "SEntityRef" || typeToken == "EntityRef" ||
                     typeToken == "quark::script::SEntityRef")
            {
                field.m_Type = EScriptFieldType::ENTITY_REF;
                if (!defaultValStr.empty())
                {
                    try
                    {
                        field.m_EntityIdValue = std::stoi(defaultValStr);
                    }
                    catch (...)
                    {
                        field.m_EntityIdValue = -1;
                    }
                }
            }
            else if (typeToken == "SAssetRef" || typeToken == "AssetRef" ||
                     typeToken == "quark::script::SAssetRef")
            {
                field.m_Type = EScriptFieldType::ASSET_REF;
                if (!defaultValStr.empty())
                {
                    size_t firstQuote = defaultValStr.find('"');
                    size_t lastQuote = defaultValStr.rfind('"');
                    if (firstQuote != std::string::npos && lastQuote != std::string::npos && lastQuote > firstQuote)
                    {
                        field.m_AssetPathValue = defaultValStr.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                    }
                }
            }
            else
            {
                continue;
            }

            outInfo.m_vFields.push_back(field);
        }

        return true;
    }

    std::vector<SParsedScriptInfo> CScriptHeaderParser::ScanProjectScripts(const std::string& projectPath)
    {
        std::vector<SParsedScriptInfo> vResults;
        const fs::path root(projectPath);
        std::error_code ec;

        if (!fs::exists(root, ec))
        {
            return vResults;
        }

        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec))
        {
            if (ec)
            {
                break;
            }
            if (!entry.is_regular_file())
            {
                continue;
            }

            const fs::path path = entry.path();
            const std::string filename = path.filename().string();
            if (filename.rfind(".", 0) == 0)
            {
                continue;
            }

            const std::string ext = path.extension().string();
            if (ext == ".hpp" || ext == ".h")
            {
                SParsedScriptInfo info;
                if (ParseHeader(path.string(), info))
                {
                    vResults.push_back(info);
                }
            }
        }

        return vResults;
    }

    SInputCallbacks g_InputCallbacks;
    STimeCallbacks g_TimeCallbacks;
    SSceneCallbacks g_SceneCallbacks;
    SPhysicsCallbacks g_PhysicsCallbacks;
    SDebugCallbacks g_DebugCallbacks;

    static float s_ScriptTimeScale = 1.0f;
    static float s_ScriptDeltaTime = 0.0166667f;
    static float s_ScriptFixedDeltaTime = 0.02f;
    static double s_ScriptAccumulatedTime = 0.0;
    static int s_DiscardMouseDeltaFrames = 0;
    static std::unordered_map<int, std::unique_ptr<CScriptEntity>> s_EntityWrappers;

    namespace
    {

        CScriptEntity* GetOrCreateScriptEntityWrapper(CEntity& entity)
        {
            CScriptComponent* pScriptComp = entity.GetScriptComponent();
            if (pScriptComp && pScriptComp->m_pScriptEntity)
            {
                CTransformComponent* pTransform = entity.GetTransformComponent();
                Vec3* pPos = pTransform ? &pTransform->m_Position : nullptr;
                Vec3* pRot = pTransform ? &pTransform->m_Rotation : nullptr;
                Vec3* pScale = pTransform ? &pTransform->m_Scale : nullptr;
                bool* pHasOverride = pTransform ? &pTransform->m_HasLocalMatrixOverride : nullptr;
                pScriptComp->m_pScriptEntity->SetTransformPointers(pPos, pRot, pScale, pHasOverride);
                return pScriptComp->m_pScriptEntity.get();
            }

            CTransformComponent* pTransform = entity.GetTransformComponent();
            Vec3* pPos = pTransform ? &pTransform->m_Position : nullptr;
            Vec3* pRot = pTransform ? &pTransform->m_Rotation : nullptr;
            Vec3* pScale = pTransform ? &pTransform->m_Scale : nullptr;
            bool* pHasOverride = pTransform ? &pTransform->m_HasLocalMatrixOverride : nullptr;

            auto& pWrapper = s_EntityWrappers[entity.m_Id];
            pWrapper = std::make_unique<CScriptEntity>(&entity, pPos, pRot, pScale, pHasOverride);
            return pWrapper.get();
        }

        void RefreshSceneScriptEntityPointers(CScene& scene)
        {
            for (auto& entity : scene.m_vEntities)
            {
                CScriptComponent* pScriptComp = entity.GetScriptComponent();
                if (pScriptComp && pScriptComp->m_pScriptEntity)
                {
                    CTransformComponent* pTransform = entity.GetTransformComponent();
                    Vec3* pPos = pTransform ? &pTransform->m_Position : nullptr;
                    Vec3* pRot = pTransform ? &pTransform->m_Rotation : nullptr;
                    Vec3* pScale = pTransform ? &pTransform->m_Scale : nullptr;
                    bool* pHasOverride = pTransform ? &pTransform->m_HasLocalMatrixOverride : nullptr;
                    pScriptComp->m_pScriptEntity = std::make_unique<CScriptEntity>(
                        &entity, pPos, pRot, pScale, pHasOverride);
                    if (pScriptComp->m_pInstance)
                    {
                        pScriptComp->m_pInstance->m_Entity = pScriptComp->m_pScriptEntity.get();
                    }
                }
            }
        }

    } // anonymous

    CScriptEngine::CScriptEngine()
    {
        g_InputCallbacks.pfnIsKeyDown = [](int key) -> bool
        {
            return ::IsKeyDown(static_cast<KeyboardKey>(key));
        };
        g_InputCallbacks.pfnIsKeyPressed = [](int key) -> bool
        {
            return ::IsKeyPressed(static_cast<KeyboardKey>(key));
        };
        g_InputCallbacks.pfnIsKeyReleased = [](int key) -> bool
        {
            return ::IsKeyReleased(static_cast<KeyboardKey>(key));
        };
        g_InputCallbacks.pfnIsKeyUp = [](int key) -> bool
        {
            return ::IsKeyUp(static_cast<KeyboardKey>(key));
        };
        g_InputCallbacks.pfnIsMouseButtonDown = [](int button) -> bool
        {
            if (!CScriptEngine::Get().IsCursorLocked() && !CScriptEngine::Get().IsViewportHovered())
            {
                return false;
            }
            return ::IsMouseButtonDown(static_cast<MouseButton>(button));
        };
        g_InputCallbacks.pfnIsMouseButtonPressed = [](int button) -> bool
        {
            if (!CScriptEngine::Get().IsCursorLocked() && !CScriptEngine::Get().IsViewportHovered())
            {
                return false;
            }
            return ::IsMouseButtonPressed(static_cast<MouseButton>(button));
        };
        g_InputCallbacks.pfnIsMouseButtonReleased = [](int button) -> bool
        {
            if (!CScriptEngine::Get().IsCursorLocked() && !CScriptEngine::Get().IsViewportHovered())
            {
                return false;
            }
            return ::IsMouseButtonReleased(static_cast<MouseButton>(button));
        };
        g_InputCallbacks.pfnIsMouseButtonUp = [](int button) -> bool
        {
            if (!CScriptEngine::Get().IsCursorLocked() && !CScriptEngine::Get().IsViewportHovered())
            {
                return true;
            }
            return ::IsMouseButtonUp(static_cast<MouseButton>(button));
        };
        g_InputCallbacks.pfnGetMousePosition = []() -> Vec2
        {
            return ::GetMousePosition();
        };
        g_InputCallbacks.pfnGetMouseDelta = []() -> Vec2
        {
            if (!CScriptEngine::Get().IsCursorLocked())
            {
                return Vec2{ 0.0f, 0.0f };
            }
            if (s_DiscardMouseDeltaFrames > 0)
            {
                --s_DiscardMouseDeltaFrames;
                ::GetMouseDelta();
                return Vec2{ 0.0f, 0.0f };
            }
            Vec2 delta = ::GetMouseDelta();
            if (delta.x * delta.x + delta.y * delta.y > 40000.0f)
            {
                return Vec2{ 0.0f, 0.0f };
            }
            return delta;
        };
        g_InputCallbacks.pfnGetMouseWheelMove = []() -> float
        {
            if (!CScriptEngine::Get().IsCursorLocked() && !CScriptEngine::Get().IsViewportHovered())
            {
                return 0.0f;
            }
            return ::GetMouseWheelMove();
        };
        g_InputCallbacks.pfnSetCursorLocked = [](bool locked)
        {
            CScriptEngine::Get().SetCursorLocked(locked);
        };
        g_InputCallbacks.pfnIsCursorLocked = []() -> bool
        {
            return CScriptEngine::Get().IsCursorLocked();
        };
        g_InputCallbacks.pfnSetCursorVisible = [](bool visible)
        {
            try
            {
                if (visible)
                {
                    ::ShowCursor();
                }
                else
                {
                    ::HideCursor();
                }
            }
            catch (...)
            {
            }
        };
        g_InputCallbacks.pfnIsCursorVisible = []() -> bool
        {
            try
            {
                return !::IsCursorHidden();
            }
            catch (...)
            {
                return true;
            }
        };
        g_InputCallbacks.pfnIsViewportHovered = []() -> bool
        {
            return CScriptEngine::Get().IsViewportHovered();
        };

        g_TimeCallbacks.pfnGetDeltaTime = []() -> float
        {
            return s_ScriptDeltaTime * s_ScriptTimeScale;
        };
        g_TimeCallbacks.pfnGetUnscaledDeltaTime = []() -> float
        {
            return s_ScriptDeltaTime;
        };
        g_TimeCallbacks.pfnGetFixedDeltaTime = []() -> float
        {
            return s_ScriptFixedDeltaTime;
        };
        g_TimeCallbacks.pfnSetFixedDeltaTime = [](float fixedStep)
        {
            s_ScriptFixedDeltaTime = std::clamp(fixedStep, 0.001f, 1.0f);
        };
        g_TimeCallbacks.pfnGetTime = []() -> double
        {
            return s_ScriptAccumulatedTime;
        };
        g_TimeCallbacks.pfnGetTimeScale = []() -> float
        {
            return s_ScriptTimeScale;
        };
        g_TimeCallbacks.pfnSetTimeScale = [](float scale)
        {
            s_ScriptTimeScale = std::max(0.0f, scale);
        };

        g_SceneCallbacks.pfnFindEntityByName = [](const char* pName) -> CScriptEntity*
        {
            CScene* pScene = CScriptEngine::Get().GetActiveScene();
            if (!pScene || !pName)
            {
                return nullptr;
            }
            for (auto& entity : pScene->m_vEntities)
            {
                if (entity.m_Name == pName)
                {
                    return GetOrCreateScriptEntityWrapper(entity);
                }
            }
            return nullptr;
        };

        g_SceneCallbacks.pfnFindEntityById = [](int entityId) -> CScriptEntity*
        {
            CScene* pScene = CScriptEngine::Get().GetActiveScene();
            if (!pScene || entityId < 0)
            {
                return nullptr;
            }
            for (auto& entity : pScene->m_vEntities)
            {
                if (entity.m_Id == entityId)
                {
                    return GetOrCreateScriptEntityWrapper(entity);
                }
            }
            if (entityId < static_cast<int>(pScene->m_vEntities.size()))
            {
                return GetOrCreateScriptEntityWrapper(pScene->m_vEntities[entityId]);
            }
            return nullptr;
        };

        g_SceneCallbacks.pfnFindEntitiesWithTag = [](const char* pTag, CScriptEntity** ppOutEntities, int maxCount) -> int
        {
            CScene* pScene = CScriptEngine::Get().GetActiveScene();
            if (!pScene || !pTag || pTag[0] == '\0')
            {
                return 0;
            }
            int foundCount = 0;
            for (auto& entity : pScene->m_vEntities)
            {
                const bool hasTag = std::find(entity.m_vTags.begin(), entity.m_vTags.end(), pTag) != entity.m_vTags.end();
                if (!hasTag)
                {
                    continue;
                }
                if (ppOutEntities && foundCount < maxCount)
                {
                    ppOutEntities[foundCount] = GetOrCreateScriptEntityWrapper(entity);
                }
                ++foundCount;
            }
            return foundCount;
        };

        g_SceneCallbacks.pfnInstantiateEntity = [](CScriptEntity* pSourceEntity) -> CScriptEntity*
        {
            CScene* pScene = CScriptEngine::Get().GetActiveScene();
            if (!pScene || !pSourceEntity || !pSourceEntity->GetRawEntity())
            {
                return nullptr;
            }
            const int sourceId = pSourceEntity->GetId();
            int sourceIndex = -1;
            for (int i = 0; i < static_cast<int>(pScene->m_vEntities.size()); ++i)
            {
                if (pScene->m_vEntities[i].m_Id == sourceId || &pScene->m_vEntities[i] == pSourceEntity->GetRawEntity())
                {
                    sourceIndex = i;
                    break;
                }
            }
            if (sourceIndex < 0)
            {
                return nullptr;
            }

            CEntity cloned = pScene->m_vEntities[sourceIndex];
            cloned.m_Id = static_cast<int>(pScene->m_vEntities.size());
            auto isNameTaken = [pScene](const std::string& candidate)
            {
                for (const auto& entity : pScene->m_vEntities)
                {
                    if (entity.m_Name == candidate)
                    {
                        return true;
                    }
                }
                return false;
            };
            std::string uniqueName = cloned.m_Name;
            for (int suffix = 1; isNameTaken(uniqueName); ++suffix)
            {
                uniqueName = cloned.m_Name + " (" + std::to_string(suffix) + ")";
            }
            cloned.m_Name = uniqueName;
            pScene->m_vEntities.push_back(std::move(cloned));
            RefreshSceneScriptEntityPointers(*pScene);

            CEntity& spawned = pScene->m_vEntities.back();
            if (CScriptComponent* pScriptComp = spawned.GetScriptComponent())
            {
                if (!pScriptComp->m_ScriptName.empty())
                {
                    IScript* pInstance = CScriptEngine::Get().CreateScript(pScriptComp->m_ScriptName);
                    if (pInstance)
                    {
                        pScriptComp->m_pInstance = pInstance;
                        CTransformComponent* pTransform = spawned.GetTransformComponent();
                        Vec3* pPos = pTransform ? &pTransform->m_Position : nullptr;
                        Vec3* pRot = pTransform ? &pTransform->m_Rotation : nullptr;
                        Vec3* pScale = pTransform ? &pTransform->m_Scale : nullptr;
                        bool* pHasOverride = pTransform ? &pTransform->m_HasLocalMatrixOverride : nullptr;
                        pScriptComp->m_pScriptEntity = std::make_unique<CScriptEntity>(
                            &spawned, pPos, pRot, pScale, pHasOverride);
                        pInstance->m_Entity = pScriptComp->m_pScriptEntity.get();
                        pScriptComp->ApplyFieldsToInstance();
                        pInstance->OnCreate();
                        if (pScriptComp->m_Enabled)
                        {
                            pInstance->OnStart();
                        }
                    }
                }
            }
            return GetOrCreateScriptEntityWrapper(spawned);
        };

        g_SceneCallbacks.pfnInstantiatePrefab = [](const char* pPrefabPath) -> CScriptEntity*
        {
            CScene* pScene = CScriptEngine::Get().GetActiveScene();
            if (!pScene || !pPrefabPath || pPrefabPath[0] == '\0')
            {
                return nullptr;
            }

            fs::path resolvedPath(pPrefabPath);
            std::error_code ec;
            if (!fs::is_regular_file(resolvedPath, ec))
            {
                for (const auto& candidate : {
                         fs::path(pPrefabPath),
                         fs::current_path() / pPrefabPath,
                         fs::current_path() / "resources" / pPrefabPath })
                {
                    if (fs::is_regular_file(candidate, ec))
                    {
                        resolvedPath = candidate;
                        break;
                    }
                }
            }

            std::ifstream prefabFile(resolvedPath);
            if (!prefabFile.is_open())
            {
                return nullptr;
            }
            std::stringstream buffer;
            buffer << prefabFile.rdbuf();

            CComponentFactoryRegistry defaultFactories;
            const CComponentFactoryRegistry* pFactories = pScene->m_pComponentRegistry
                ? pScene->m_pComponentRegistry
                : &defaultFactories;
            std::vector<int> vCreatedIndices;
            if (!quark::CSceneDocument::InstantiatePrefab(buffer.str(), *pScene, *pFactories, -1, &vCreatedIndices) ||
                vCreatedIndices.empty())
            {
                return nullptr;
            }

            RefreshSceneScriptEntityPointers(*pScene);
            for (int createdIdx : vCreatedIndices)
            {
                if (createdIdx < 0 || createdIdx >= static_cast<int>(pScene->m_vEntities.size()))
                {
                    continue;
                }
                CEntity& spawned = pScene->m_vEntities[createdIdx];
                if (CScriptComponent* pScriptComp = spawned.GetScriptComponent())
                {
                    if (!pScriptComp->m_ScriptName.empty())
                    {
                        IScript* pInstance = CScriptEngine::Get().CreateScript(pScriptComp->m_ScriptName);
                        if (pInstance)
                        {
                            pScriptComp->m_pInstance = pInstance;
                            CTransformComponent* pTransform = spawned.GetTransformComponent();
                            Vec3* pPos = pTransform ? &pTransform->m_Position : nullptr;
                            Vec3* pRot = pTransform ? &pTransform->m_Rotation : nullptr;
                            Vec3* pScale = pTransform ? &pTransform->m_Scale : nullptr;
                            bool* pHasOverride = pTransform ? &pTransform->m_HasLocalMatrixOverride : nullptr;
                            pScriptComp->m_pScriptEntity = std::make_unique<CScriptEntity>(
                                &spawned, pPos, pRot, pScale, pHasOverride);
                            pInstance->m_Entity = pScriptComp->m_pScriptEntity.get();
                            pScriptComp->ApplyFieldsToInstance();
                            pInstance->OnCreate();
                            if (pScriptComp->m_Enabled)
                            {
                                pInstance->OnStart();
                            }
                        }
                    }
                }
            }

            return GetOrCreateScriptEntityWrapper(pScene->m_vEntities[vCreatedIndices.front()]);
        };

        g_SceneCallbacks.pfnDestroyEntity = [](CScriptEntity* pEntity)
        {
            if (pEntity)
            {
                g_SceneCallbacks.pfnDestroyEntityById(pEntity->GetId());
            }
        };

        g_SceneCallbacks.pfnDestroyEntityById = [](int entityId)
        {
            CScene* pScene = CScriptEngine::Get().GetActiveScene();
            if (!pScene || entityId < 0)
            {
                return;
            }
            for (int i = 0; i < static_cast<int>(pScene->m_vEntities.size()); ++i)
            {
                if (pScene->m_vEntities[i].m_Id == entityId)
                {
                    CEntity& target = pScene->m_vEntities[i];
                    if (CScriptComponent* pScriptComp = target.GetScriptComponent())
                    {
                        if (pScriptComp->m_pInstance)
                        {
                            pScriptComp->m_pInstance->OnDestroy();
                            CScriptEngine::Get().DestroyScript(pScriptComp->m_ScriptName, pScriptComp->m_pInstance);
                            pScriptComp->m_pInstance = nullptr;
                            pScriptComp->m_pScriptEntity.reset();
                        }
                    }
                    std::vector<int> vToErase;
                    std::vector<int> vStack = { i };
                    std::vector<bool> vVisited(pScene->m_vEntities.size(), false);
                    while (!vStack.empty())
                    {
                        const int cur = vStack.back();
                        vStack.pop_back();
                        if (cur < 0 || cur >= static_cast<int>(vVisited.size()) || vVisited[cur])
                        {
                            continue;
                        }
                        vVisited[cur] = true;
                        vToErase.push_back(cur);
                        for (int childIdx = 0; childIdx < static_cast<int>(pScene->m_vEntities.size()); ++childIdx)
                        {
                            if (pScene->m_vEntities[childIdx].m_ParentId == cur)
                            {
                                vStack.push_back(childIdx);
                            }
                        }
                    }
                    std::sort(vToErase.begin(), vToErase.end(), std::greater<int>());
                    for (int eraseIdx : vToErase)
                    {
                        if (eraseIdx != i)
                        {
                            if (CScriptComponent* pChildScript = pScene->m_vEntities[eraseIdx].GetScriptComponent())
                            {
                                if (pChildScript->m_pInstance)
                                {
                                    pChildScript->m_pInstance->OnDestroy();
                                    CScriptEngine::Get().DestroyScript(pChildScript->m_ScriptName, pChildScript->m_pInstance);
                                    pChildScript->m_pInstance = nullptr;
                                    pChildScript->m_pScriptEntity.reset();
                                }
                            }
                        }
                        pScene->m_vEntities.erase(pScene->m_vEntities.begin() + eraseIdx);
                        for (int rem = 0; rem < static_cast<int>(pScene->m_vEntities.size()); ++rem)
                        {
                            CEntity& remaining = pScene->m_vEntities[rem];
                            remaining.m_Id = rem;
                            if (remaining.m_ParentId == eraseIdx)
                            {
                                remaining.m_ParentId = -1;
                            }
                            else if (remaining.m_ParentId > eraseIdx)
                            {
                                --remaining.m_ParentId;
                            }
                        }
                    }
                    pScene->m_Selected = -1;
                    pScene->m_vSelectedEntities.clear();
                    RefreshSceneScriptEntityPointers(*pScene);
                    break;
                }
            }
        };

        g_SceneCallbacks.pfnLoadScene = [](const char* pScenePath) -> bool
        {
            CScene* pScene = CScriptEngine::Get().GetActiveScene();
            if (!pScene || !pScenePath || pScenePath[0] == '\0')
            {
                return false;
            }

            fs::path resolvedPath(pScenePath);
            std::error_code ec;
            if (!fs::is_regular_file(resolvedPath, ec))
            {
                for (const auto& candidate : {
                         fs::path(pScenePath),
                         fs::current_path() / pScenePath,
                         fs::current_path() / "resources" / pScenePath })
                {
                    if (fs::is_regular_file(candidate, ec))
                    {
                        resolvedPath = candidate;
                        break;
                    }
                }
            }

            std::ifstream sceneFile(resolvedPath);
            if (!sceneFile.is_open())
            {
                return false;
            }
            std::stringstream buffer;
            buffer << sceneFile.rdbuf();

            CComponentFactoryRegistry defaultFactories;
            const CComponentFactoryRegistry* pFactories = pScene->m_pComponentRegistry
                ? pScene->m_pComponentRegistry
                : &defaultFactories;

            CScene loadedScene;
            loadedScene.m_pComponentRegistry = pScene->m_pComponentRegistry;
            if (!quark::CSceneDocument::Deserialize(buffer.str(), loadedScene, *pFactories))
            {
                return false;
            }

            for (auto& existing : pScene->m_vEntities)
            {
                if (CScriptComponent* pScriptComp = existing.GetScriptComponent())
                {
                    if (pScriptComp->m_pInstance)
                    {
                        pScriptComp->m_pInstance->OnDestroy();
                        CScriptEngine::Get().DestroyScript(pScriptComp->m_ScriptName, pScriptComp->m_pInstance);
                        pScriptComp->m_pInstance = nullptr;
                        pScriptComp->m_pScriptEntity.reset();
                    }
                }
            }

            for (auto& existing : pScene->m_vEntities)
            {
                if (CMeshComponent* pMesh = existing.GetMeshComponent())
                {
                    pMesh->ReleaseOwnedResources();
                }
                if (CCanvasComponent* pCanvas = existing.GetCanvasComponent())
                {
                    pCanvas->ReleaseOwnedResources();
                }
                if (CImageComponent* pImage = existing.GetImageComponent())
                {
                    pImage->ReleaseOwnedResources();
                }
                if (CPanelComponent* pPanel = existing.GetPanelComponent())
                {
                    pPanel->ReleaseOwnedResources();
                }
            }
            pScene->m_vEntities = std::move(loadedScene.m_vEntities);
            pScene->m_Selected = -1;
            pScene->m_vSelectedEntities.clear();
            s_EntityWrappers.clear();

            for (auto& entity : pScene->m_vEntities)
            {
                CScriptComponent* pScriptComp = entity.GetScriptComponent();
                if (!pScriptComp || pScriptComp->m_ScriptName.empty())
                {
                    continue;
                }
                IScript* pInstance = CScriptEngine::Get().CreateScript(pScriptComp->m_ScriptName);
                if (!pInstance)
                {
                    continue;
                }
                pScriptComp->m_pInstance = pInstance;
                CTransformComponent* pTransform = entity.GetTransformComponent();
                Vec3* pPos = pTransform ? &pTransform->m_Position : nullptr;
                Vec3* pRot = pTransform ? &pTransform->m_Rotation : nullptr;
                Vec3* pScale = pTransform ? &pTransform->m_Scale : nullptr;
                bool* pHasOverride = pTransform ? &pTransform->m_HasLocalMatrixOverride : nullptr;
                pScriptComp->m_pScriptEntity = std::make_unique<CScriptEntity>(
                    &entity, pPos, pRot, pScale, pHasOverride);
                pInstance->m_Entity = pScriptComp->m_pScriptEntity.get();
                pScriptComp->ApplyFieldsToInstance();
                pInstance->OnCreate();
            }

            for (auto& entity : pScene->m_vEntities)
            {
                CScriptComponent* pScriptComp = entity.GetScriptComponent();
                if (pScriptComp && pScriptComp->m_pInstance && pScriptComp->m_Enabled)
                {
                    pScriptComp->m_pInstance->OnStart();
                }
            }
            return true;
        };

        g_PhysicsCallbacks.pfnRaycast = [](Vec3 origin, Vec3 direction, float maxDistance,
                                           bool includeTriggers, int ignoreEntityId,
                                           SRaycastHit* pOutHit) -> bool
        {
            CScene* pScene = CScriptEngine::Get().GetActiveScene();
            if (!pScene || !pOutHit)
            {
                return false;
            }
            quark::physics::SPhysicsRaycastHit rawHit;
            if (!CScriptEngine::Get().GetPhysicsWorld().Raycast(
                    *pScene, origin, direction, maxDistance, rawHit, includeTriggers, ignoreEntityId))
            {
                *pOutHit = {};
                return false;
            }

            pOutHit->m_Hit = true;
            pOutHit->m_EntityId = rawHit.m_EntityId;
            pOutHit->m_Point = rawHit.m_Point;
            pOutHit->m_Normal = rawHit.m_Normal;
            pOutHit->m_Distance = rawHit.m_Distance;
            const int hitIndex = pScene->FindEntityIndexById(rawHit.m_EntityId);
            if (hitIndex >= 0 && hitIndex < static_cast<int>(pScene->m_vEntities.size()))
            {
                pOutHit->m_pEntity = GetOrCreateScriptEntityWrapper(pScene->m_vEntities[static_cast<size_t>(hitIndex)]);
            }
            return true;
        };
        g_PhysicsCallbacks.pfnAddForce = [](CScriptEntity* pEntity, Vec3 force)
        {
            if (!pEntity || !pEntity->GetRawEntity())
            {
                return;
            }
            if (CRigidbodyComponent* pBody = pEntity->GetRawEntity()->GetRigidbodyComponent())
            {
                pBody->AddForce(force);
            }
        };
        g_PhysicsCallbacks.pfnAddImpulse = [](CScriptEntity* pEntity, Vec3 impulse)
        {
            if (!pEntity || !pEntity->GetRawEntity())
            {
                return;
            }
            if (CRigidbodyComponent* pBody = pEntity->GetRawEntity()->GetRigidbodyComponent())
            {
                pBody->AddImpulse(impulse);
            }
        };
        g_PhysicsCallbacks.pfnSetVelocity = [](CScriptEntity* pEntity, Vec3 velocity)
        {
            if (!pEntity || !pEntity->GetRawEntity())
            {
                return;
            }
            if (CRigidbodyComponent* pBody = pEntity->GetRawEntity()->GetRigidbodyComponent())
            {
                pBody->SetVelocity(velocity);
            }
        };
        g_PhysicsCallbacks.pfnGetVelocity = [](CScriptEntity* pEntity) -> Vec3
        {
            if (!pEntity || !pEntity->GetRawEntity())
            {
                return { 0.0f, 0.0f, 0.0f };
            }
            if (const CRigidbodyComponent* pBody = pEntity->GetRawEntity()->GetRigidbodyComponent())
            {
                return pBody->m_Velocity;
            }
            return { 0.0f, 0.0f, 0.0f };
        };
        g_PhysicsCallbacks.pfnIsGrounded = [](CScriptEntity* pEntity) -> bool
        {
            if (!pEntity || !pEntity->GetRawEntity())
            {
                return false;
            }
            if (const CRigidbodyComponent* pBody = pEntity->GetRawEntity()->GetRigidbodyComponent())
            {
                return pBody->m_IsGrounded;
            }
            return false;
        };
        g_PhysicsCallbacks.pfnSetGravity = [](Vec3 gravity)
        {
            CScriptEngine::Get().GetPhysicsWorld().SetGravity(gravity);
        };
        g_PhysicsCallbacks.pfnGetGravity = []() -> Vec3
        {
            return CScriptEngine::Get().GetPhysicsWorld().GetGravity();
        };

        g_DebugCallbacks.pfnLog = [](int level, const char* pMessage)
        {
            if (!pMessage)
            {
                return;
            }
            EScriptLogLevel logLevel = EScriptLogLevel::INFO;
            if (level == 1)
            {
                logLevel = EScriptLogLevel::WARNING;
            }
            else if (level >= 2)
            {
                logLevel = EScriptLogLevel::ERR;
            }
            CScriptEngine::Get().LogMessage(logLevel, pMessage);
        };

        m_pActiveDll = &m_Dll;
    }

    CScriptEngine& CScriptEngine::Get()
    {
        static CScriptEngine s_Instance;
        return s_Instance;
    }

    CScriptEngine::~CScriptEngine()
    {
        m_CursorLocked = false;
        UnloadScriptDll();
    }

    void CScriptEngine::SetCursorLocked(bool locked)
    {
        m_CursorLocked = locked;
        try
        {
            SDL_Window* pWin = ::GetNativeWindow();
            if (locked)
            {
                if (pWin)
                {
                    SDL_SetWindowRelativeMouseMode(pWin, true);
                    float dummyX = 0.0f, dummyY = 0.0f;
                    SDL_GetRelativeMouseState(&dummyX, &dummyY);
                }
                ::DisableCursor();
                s_DiscardMouseDeltaFrames = 2;
            }
            else
            {
                if (pWin)
                {
                    SDL_SetWindowRelativeMouseMode(pWin, false);
                }
                ::EnableCursor();
                ::ShowCursor();
            }
        }
        catch (...)
        {
        }
    }

    bool CScriptEngine::BuildScriptDll(const std::string& projectPath, const std::string& gameName,
                                       std::string& errorMessage,
                                       const FProgressCallback& progressCallback)
    {
        const fs::path projectRoot(projectPath);
        std::error_code ec;

        std::vector<fs::path> vScriptSources;
        for (const auto& entry : fs::recursive_directory_iterator(projectRoot, ec))
        {
            if (ec)
            {
                break;
            }
            if (!entry.is_regular_file())
            {
                continue;
            }

            const fs::path p = entry.path();
            const std::string ext = p.extension().string();
            if (ext == ".cpp")
            {
                const std::string pathStr = p.generic_string();
                if (pathStr.find("/.script_build") != std::string::npos ||
                    pathStr.find("/cmake-build") != std::string::npos ||
                    pathStr.find("/Build") != std::string::npos)
                {
                    continue;
                }
                vScriptSources.push_back(p);
            }
        }

        if (vScriptSources.empty())
        {
            errorMessage = "No script source files (.cpp) found in project";
            return false;
        }

        const fs::path buildDir = projectRoot / ".script_build";
        fs::create_directories(buildDir, ec);

        const fs::path executableDir = GetExecutableDirectory();
        const fs::path engineSourceDir = fs::current_path();

        fs::path quarkCoreLib = executableDir / "QuarkCore.lib";
        if (!fs::is_regular_file(quarkCoreLib))
        {
            quarkCoreLib = executableDir / "QuarkCoreRuntime.lib";
        }

        fs::path quarkEngineLib = executableDir / "QuarkEngineRuntime.lib";
        if (!fs::is_regular_file(quarkEngineLib))
        {
            quarkEngineLib = executableDir / "QuarkEngine.lib";
        }

        fs::path sdl3Lib = executableDir / "SDL3.lib";
        if (!fs::is_regular_file(sdl3Lib))
        {
            sdl3Lib = executableDir / "SDL3-static.lib";
        }

        const fs::path bridgeCpp = buildDir / "script_bridge.cpp";
        std::ofstream bridgeOut(bridgeCpp, std::ios::binary | std::ios::trunc);
        if (bridgeOut.is_open())
        {
            bridgeOut << "#include \"QuarkEngine/SDK.h\"\n\n";
            bridgeOut << "namespace quark::script\n{\n";
            bridgeOut << "    SInputCallbacks g_InputCallbacks;\n";
            bridgeOut << "    STimeCallbacks g_TimeCallbacks;\n";
            bridgeOut << "    SSceneCallbacks g_SceneCallbacks;\n";
            bridgeOut << "    SPhysicsCallbacks g_PhysicsCallbacks;\n";
            bridgeOut << "    SDebugCallbacks g_DebugCallbacks;\n";
            bridgeOut << "}\n\n";
            bridgeOut << "extern \"C\" QUARK_SCRIPT_API void SetScriptInputCallbacks(const ::quark::script::SInputCallbacks* pCallbacks)\n";
            bridgeOut << "{\n";
            bridgeOut << "    if (pCallbacks)\n";
            bridgeOut << "    {\n";
            bridgeOut << "        ::quark::script::g_InputCallbacks = *pCallbacks;\n";
            bridgeOut << "    }\n";
            bridgeOut << "}\n\n";
            bridgeOut << "extern \"C\" QUARK_SCRIPT_API void SetScriptTimeCallbacks(const ::quark::script::STimeCallbacks* pCallbacks)\n";
            bridgeOut << "{\n";
            bridgeOut << "    if (pCallbacks)\n";
            bridgeOut << "    {\n";
            bridgeOut << "        ::quark::script::g_TimeCallbacks = *pCallbacks;\n";
            bridgeOut << "    }\n";
            bridgeOut << "}\n\n";
            bridgeOut << "extern \"C\" QUARK_SCRIPT_API void SetScriptSceneCallbacks(const ::quark::script::SSceneCallbacks* pCallbacks)\n";
            bridgeOut << "{\n";
            bridgeOut << "    if (pCallbacks)\n";
            bridgeOut << "    {\n";
            bridgeOut << "        ::quark::script::g_SceneCallbacks = *pCallbacks;\n";
            bridgeOut << "    }\n";
            bridgeOut << "}\n\n";
            bridgeOut << "extern \"C\" QUARK_SCRIPT_API void SetScriptPhysicsCallbacks(const ::quark::script::SPhysicsCallbacks* pCallbacks)\n";
            bridgeOut << "{\n";
            bridgeOut << "    if (pCallbacks)\n";
            bridgeOut << "    {\n";
            bridgeOut << "        ::quark::script::g_PhysicsCallbacks = *pCallbacks;\n";
            bridgeOut << "    }\n";
            bridgeOut << "}\n\n";
            bridgeOut << "extern \"C\" QUARK_SCRIPT_API void SetScriptDebugCallbacks(const ::quark::script::SDebugCallbacks* pCallbacks)\n";
            bridgeOut << "{\n";
            bridgeOut << "    if (pCallbacks)\n";
            bridgeOut << "    {\n";
            bridgeOut << "        ::quark::script::g_DebugCallbacks = *pCallbacks;\n";
            bridgeOut << "    }\n";
            bridgeOut << "}\n";
            bridgeOut.close();
        }

        const fs::path cmakeFile = buildDir / "CMakeLists.txt";
        std::ofstream cmakeOut(cmakeFile, std::ios::binary | std::ios::trunc);
        if (!cmakeOut.is_open())
        {
            errorMessage = "Failed to create CMakeLists.txt for script DLL";
            return false;
        }

        const std::string targetName = "GameScripts";

        cmakeOut << "cmake_minimum_required(VERSION 3.16)\n";
        cmakeOut << "project(" << targetName << " LANGUAGES CXX)\n\n";
        cmakeOut << "set(CMAKE_CXX_STANDARD 20)\n";
        cmakeOut << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n";

        cmakeOut << "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY " << QuotePath(projectRoot) << ")\n";
        cmakeOut << "set(CMAKE_LIBRARY_OUTPUT_DIRECTORY " << QuotePath(projectRoot) << ")\n";
        cmakeOut << "set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY " << QuotePath(projectRoot) << ")\n";
        cmakeOut << "foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})\n";
        cmakeOut << "    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)\n";
        cmakeOut << "    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} " << QuotePath(projectRoot) << ")\n";
        cmakeOut << "    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} " << QuotePath(projectRoot) << ")\n";
        cmakeOut << "    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} " << QuotePath(projectRoot) << ")\n";
        cmakeOut << "endforeach()\n\n";

        cmakeOut << "add_library(" << targetName << " SHARED\n";
        cmakeOut << "    " << QuotePath(bridgeCpp) << "\n";
        for (const auto& src : vScriptSources)
        {
            cmakeOut << "    " << QuotePath(src) << "\n";
        }
        cmakeOut << ")\n\n";

        cmakeOut << "set_target_properties(" << targetName << " PROPERTIES\n";
        cmakeOut << "    PREFIX \"\"\n";
        cmakeOut << "    OUTPUT_NAME \"" << targetName << "\"\n";
        cmakeOut << "    RUNTIME_OUTPUT_DIRECTORY " << QuotePath(projectRoot) << "\n";
        cmakeOut << "    LIBRARY_OUTPUT_DIRECTORY " << QuotePath(projectRoot) << "\n";
        cmakeOut << "    RUNTIME_OUTPUT_DIRECTORY_DEBUG " << QuotePath(projectRoot) << "\n";
        cmakeOut << "    RUNTIME_OUTPUT_DIRECTORY_RELEASE " << QuotePath(projectRoot) << "\n";
        cmakeOut << "    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO " << QuotePath(projectRoot) << "\n";
        cmakeOut << "    LIBRARY_OUTPUT_DIRECTORY_DEBUG " << QuotePath(projectRoot) << "\n";
        cmakeOut << "    LIBRARY_OUTPUT_DIRECTORY_RELEASE " << QuotePath(projectRoot) << "\n";
        cmakeOut << "    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO " << QuotePath(projectRoot) << "\n";
        cmakeOut << ")\n\n";

        cmakeOut << "target_compile_definitions(" << targetName << " PRIVATE QUARK_SCRIPT_BUILD_DLL)\n\n";

        cmakeOut << "target_include_directories(" << targetName << " PRIVATE\n";
        cmakeOut << "    " << QuotePath(engineSourceDir / "include") << "\n";
        cmakeOut << "    " << QuotePath(engineSourceDir / "quark-libs" / "QuarkCore" / "include") << "\n";
        cmakeOut << "    " << QuotePath(engineSourceDir / "quark-libs" / "QuarkCore" / "external" / "SDL3" / "include") << "\n";
        cmakeOut << "    " << QuotePath(engineSourceDir / "quark-libs" / "QuarkCore" / "external" / "Vulkan" / "include") << "\n";
        cmakeOut << "    " << QuotePath(engineSourceDir / "quark-libs" / "QuarkCore" / "external" / "OpenAL" / "include") << "\n";
        cmakeOut << "    " << QuotePath(engineSourceDir / "quark-libs" / "imgui" / "include") << "\n";
        cmakeOut << "    " << QuotePath(engineSourceDir / "src") << "\n";
        cmakeOut << "    " << QuotePath(projectRoot) << "\n";
        cmakeOut << "    " << QuotePath(projectRoot / "resources") << "\n";
        cmakeOut << ")\n\n";

        cmakeOut << "if(WIN32)\n";
        cmakeOut << "    add_library(QuarkCore SHARED IMPORTED GLOBAL)\n";
        cmakeOut << "    set_target_properties(QuarkCore PROPERTIES\n";
        cmakeOut << "        IMPORTED_IMPLIB " << QuotePath(quarkCoreLib) << "\n";
        cmakeOut << "        IMPORTED_LOCATION " << QuotePath(executableDir / "QuarkCore.dll") << "\n";
        cmakeOut << "    )\n\n";

        cmakeOut << "    add_library(QuarkEngine SHARED IMPORTED GLOBAL)\n";
        cmakeOut << "    set_target_properties(QuarkEngine PROPERTIES\n";
        cmakeOut << "        IMPORTED_IMPLIB " << QuotePath(quarkEngineLib) << "\n";
        cmakeOut << "        IMPORTED_LOCATION " << QuotePath(executableDir / "QuarkEngine.dll") << "\n";
        cmakeOut << "    )\n\n";

        if (fs::is_regular_file(sdl3Lib))
        {
            cmakeOut << "    add_library(SDL3 SHARED IMPORTED GLOBAL)\n";
            cmakeOut << "    set_target_properties(SDL3 PROPERTIES\n";
            cmakeOut << "        IMPORTED_IMPLIB " << QuotePath(sdl3Lib) << "\n";
            cmakeOut << "        IMPORTED_LOCATION " << QuotePath(executableDir / "SDL3.dll") << "\n";
            cmakeOut << "    )\n";
            cmakeOut << "    target_link_libraries(" << targetName << " PRIVATE QuarkEngine QuarkCore SDL3)\n";
        }
        else
        {
            cmakeOut << "    target_link_libraries(" << targetName << " PRIVATE QuarkEngine QuarkCore)\n";
        }
        cmakeOut << "else()\n";
        cmakeOut << "    set(CMAKE_POSITION_INDEPENDENT_CODE ON)\n";
        cmakeOut << "    if(APPLE)\n";
        cmakeOut << "        set(QC_PLATFORM_EXT \".dylib\")\n";
        cmakeOut << "    else()\n";
        cmakeOut << "        set(QC_PLATFORM_EXT \".so\")\n";
        cmakeOut << "    endif()\n\n";

        cmakeOut << "    find_library(QUARK_CORE_LOC NAMES QuarkCore libQuarkCore PATHS " << QuotePath(executableDir) << " NO_DEFAULT_PATH)\n";
        cmakeOut << "    if(NOT QUARK_CORE_LOC)\n";
        cmakeOut << "        set(QUARK_CORE_LOC \"" << executableDir.generic_string() << "/QuarkCore${QC_PLATFORM_EXT}\")\n";
        cmakeOut << "    endif()\n";
        cmakeOut << "    add_library(QuarkCore SHARED IMPORTED GLOBAL)\n";
        cmakeOut << "    set_target_properties(QuarkCore PROPERTIES IMPORTED_LOCATION \"${QUARK_CORE_LOC}\")\n\n";

        cmakeOut << "    find_library(QUARK_ENGINE_LOC NAMES QuarkEngine libQuarkEngine PATHS " << QuotePath(executableDir) << " NO_DEFAULT_PATH)\n";
        cmakeOut << "    if(NOT QUARK_ENGINE_LOC)\n";
        cmakeOut << "        set(QUARK_ENGINE_LOC \"" << executableDir.generic_string() << "/QuarkEngine${QC_PLATFORM_EXT}\")\n";
        cmakeOut << "    endif()\n";
        cmakeOut << "    add_library(QuarkEngine SHARED IMPORTED GLOBAL)\n";
        cmakeOut << "    set_target_properties(QuarkEngine PROPERTIES IMPORTED_LOCATION \"${QUARK_ENGINE_LOC}\")\n\n";

        cmakeOut << "    target_link_libraries(" << targetName << " PRIVATE QuarkEngine QuarkCore)\n";
        cmakeOut << "    find_library(SDL3_LOC NAMES SDL3 libSDL3 PATHS " << QuotePath(executableDir) << " NO_DEFAULT_PATH)\n";
        cmakeOut << "    if(SDL3_LOC)\n";
        cmakeOut << "        add_library(SDL3 SHARED IMPORTED GLOBAL)\n";
        cmakeOut << "        set_target_properties(SDL3 PROPERTIES IMPORTED_LOCATION \"${SDL3_LOC}\")\n";
        cmakeOut << "        target_link_libraries(" << targetName << " PRIVATE SDL3)\n";
        cmakeOut << "    endif()\n\n";

        cmakeOut << "    if(APPLE)\n";
        cmakeOut << "        set_target_properties(" << targetName << " PROPERTIES\n";
        cmakeOut << "            BUILD_RPATH \"@loader_path;" << executableDir.generic_string() << "\"\n";
        cmakeOut << "            INSTALL_RPATH \"@loader_path;" << executableDir.generic_string() << "\"\n";
        cmakeOut << "        )\n";
        cmakeOut << "    elseif(UNIX)\n";
        cmakeOut << "        set_target_properties(" << targetName << " PROPERTIES\n";
        cmakeOut << "            BUILD_RPATH \"$ORIGIN;" << executableDir.generic_string() << "\"\n";
        cmakeOut << "            INSTALL_RPATH \"$ORIGIN;" << executableDir.generic_string() << "\"\n";
        cmakeOut << "        )\n";
        cmakeOut << "    endif()\n";
        cmakeOut << "endif()\n";
        cmakeOut.close();

        const fs::path cmakeBuild = buildDir / "build";
        const std::string configureCmd = "cmake -S " + QuotePath(buildDir) +
            " -B " + QuotePath(cmakeBuild);

        if (progressCallback)
        {
            progressCallback("Configuring script project...");
        }
        if (!RunBuildCommand(configureCmd, errorMessage, progressCallback))
        {
            return false;
        }

#if defined(_DEBUG) || defined(DEBUG)
        const std::string buildConfig = "Debug";
#else
        const std::string buildConfig = "Release";
#endif

        const std::string ext = GetScriptLibraryExtension();
        const std::string buildCmd = "cmake --build " + QuotePath(cmakeBuild) +
            " --config " + buildConfig + " --target " + targetName;
        if (progressCallback)
        {
            progressCallback("Compiling " + targetName + ext + " (" + buildConfig + ")...");
        }
        if (!RunBuildCommand(buildCmd, errorMessage, progressCallback))
        {
            return false;
        }

        std::vector<fs::path> vOutputs;
        for (const auto& name : GetScriptLibraryFileNames(targetName))
        {
            vOutputs.push_back(cmakeBuild / buildConfig / name);
            vOutputs.push_back(cmakeBuild / name);
            vOutputs.push_back(cmakeBuild / "Debug" / name);
            vOutputs.push_back(cmakeBuild / "Release" / name);
            vOutputs.push_back(projectRoot / name);
        }

        std::error_code copyEc;
        for (const auto& outPath : vOutputs)
        {
            if (fs::is_regular_file(outPath, copyEc))
            {
                fs::copy_file(outPath, fs::path(projectPath) / ("GameScripts" + ext), fs::copy_options::overwrite_existing, copyEc);
                fs::copy_file(outPath, fs::path(projectPath) / (gameName + ext), fs::copy_options::overwrite_existing, copyEc);
                break;
            }
        }

        return true;
    }

    bool CScriptEngine::LoadScriptDll(const std::string& dllPath)
    {
        UnloadScriptDll();

        if (!m_Dll.Open(dllPath))
        {
            TraceLog(LogLevel::Error, "SCRIPT",
                     TextFormat("Failed to load script DLL '%s': %s", dllPath.c_str(), m_Dll.GetError().c_str()));
            return false;
        }

        m_LoadedDllPath = dllPath;

        using FSetScriptInputCallbacks = void (*)(const SInputCallbacks*);
        auto pfnSetInput = reinterpret_cast<FSetScriptInputCallbacks>(m_Dll.GetSymbol("SetScriptInputCallbacks"));
        if (pfnSetInput)
        {
            pfnSetInput(&g_InputCallbacks);
            TraceLog(LogLevel::Info, "SCRIPT", "Configured script input callbacks bridge");
        }
        else
        {
            TraceLog(LogLevel::Warn, "SCRIPT", "Script DLL does not export SetScriptInputCallbacks");
        }

        using FSetScriptTimeCallbacks = void (*)(const STimeCallbacks*);
        auto pfnSetTime = reinterpret_cast<FSetScriptTimeCallbacks>(m_Dll.GetSymbol("SetScriptTimeCallbacks"));
        if (pfnSetTime)
        {
            pfnSetTime(&g_TimeCallbacks);
            TraceLog(LogLevel::Info, "SCRIPT", "Configured script time callbacks bridge");
        }

        using FSetScriptSceneCallbacks = void (*)(const SSceneCallbacks*);
        auto pfnSetScene = reinterpret_cast<FSetScriptSceneCallbacks>(m_Dll.GetSymbol("SetScriptSceneCallbacks"));
        if (pfnSetScene)
        {
            pfnSetScene(&g_SceneCallbacks);
            TraceLog(LogLevel::Info, "SCRIPT", "Configured script scene callbacks bridge");
        }

        using FSetScriptPhysicsCallbacks = void (*)(const SPhysicsCallbacks*);
        auto pfnSetPhysics = reinterpret_cast<FSetScriptPhysicsCallbacks>(m_Dll.GetSymbol("SetScriptPhysicsCallbacks"));
        if (pfnSetPhysics)
        {
            pfnSetPhysics(&g_PhysicsCallbacks);
            TraceLog(LogLevel::Info, "SCRIPT", "Configured script physics callbacks bridge");
        }

        using FSetScriptDebugCallbacks = void (*)(const SDebugCallbacks*);
        auto pfnSetDebug = reinterpret_cast<FSetScriptDebugCallbacks>(m_Dll.GetSymbol("SetScriptDebugCallbacks"));
        if (pfnSetDebug)
        {
            pfnSetDebug(&g_DebugCallbacks);
            TraceLog(LogLevel::Info, "SCRIPT", "Configured script debug callbacks bridge");
        }

        TraceLog(LogLevel::Info, "SCRIPT", TextFormat("Loaded script DLL '%s'", dllPath.c_str()));
        return true;
    }

    void CScriptEngine::UnloadScriptDll()
    {
        if (m_Dll.IsOpen())
        {
            m_Dll.Close();
            m_LoadedDllPath.clear();
        }
    }

    bool CScriptEngine::IsDllLoaded() const
    {
        return m_Dll.IsOpen();
    }

    IScript* CScriptEngine::CreateScript(const std::string& scriptName)
    {
        CDynamicLibrary* pDll = m_pActiveDll ? m_pActiveDll : &m_Dll;
        if (pDll->IsOpen())
        {
            std::string symbol = "Create_" + scriptName;
            auto pfnCreate = reinterpret_cast<FScriptCreator>(pDll->GetSymbol(symbol.c_str()));
            if (pfnCreate)
            {
                return pfnCreate();
            }
        }

        return CScriptRegistry::Get().Create(scriptName);
    }

    void CScriptEngine::DestroyScript(const std::string& scriptName, IScript* pScript)
    {
        if (!pScript)
        {
            return;
        }

        CDynamicLibrary* pDll = m_pActiveDll ? m_pActiveDll : &m_Dll;
        if (pDll->IsOpen())
        {
            std::string symbol = "Destroy_" + scriptName;
            auto pfnDestroy = reinterpret_cast<FScriptDestroyer>(pDll->GetSymbol(symbol.c_str()));
            if (pfnDestroy)
            {
                pfnDestroy(pScript);
                return;
            }
        }

        CScriptRegistry::Get().Destroy(scriptName, pScript);
    }

    void CScriptEngine::StartPlay(CScene& scene, const std::string& projectPath,
                                  CDynamicLibrary* pExternalDll)
    {
        if (m_IsPlaying)
        {
            return;
        }

        RefreshAvailableScripts(projectPath);

        std::string gameName = fs::path(projectPath).filename().string();
        if (gameName.empty())
        {
            gameName = "Game";
        }

        s_ScriptTimeScale = 1.0f;
        s_ScriptDeltaTime = 0.0166667f;
        s_ScriptFixedDeltaTime = 0.02f;
        s_ScriptAccumulatedTime = 0.0;
        s_DiscardMouseDeltaFrames = 2;
        s_EntityWrappers.clear();
        m_PhysicsWorld.Reset();
        for (auto& entity : scene.m_vEntities)
        {
            if (CRigidbodyComponent* pBody = entity.GetRigidbodyComponent())
            {
                pBody->ResetRuntimeState();
            }
        }
        m_pActiveScene = &scene;
        m_ActiveProjectPath = projectPath;
        m_FixedAccumulator = 0.0f;

        if (pExternalDll && pExternalDll->IsOpen())
        {
            m_pActiveDll = pExternalDll;
            using FSetScriptInputCallbacks = void (*)(const SInputCallbacks*);
            auto pfnSetInput = reinterpret_cast<FSetScriptInputCallbacks>(m_pActiveDll->GetSymbol("SetScriptInputCallbacks"));
            if (pfnSetInput)
            {
                pfnSetInput(&g_InputCallbacks);
            }
            using FSetScriptTimeCallbacks = void (*)(const STimeCallbacks*);
            auto pfnSetTime = reinterpret_cast<FSetScriptTimeCallbacks>(m_pActiveDll->GetSymbol("SetScriptTimeCallbacks"));
            if (pfnSetTime)
            {
                pfnSetTime(&g_TimeCallbacks);
            }
            using FSetScriptSceneCallbacks = void (*)(const SSceneCallbacks*);
            auto pfnSetScene = reinterpret_cast<FSetScriptSceneCallbacks>(m_pActiveDll->GetSymbol("SetScriptSceneCallbacks"));
            if (pfnSetScene)
            {
                pfnSetScene(&g_SceneCallbacks);
            }
            using FSetScriptPhysicsCallbacks = void (*)(const SPhysicsCallbacks*);
            auto pfnSetPhysics = reinterpret_cast<FSetScriptPhysicsCallbacks>(m_pActiveDll->GetSymbol("SetScriptPhysicsCallbacks"));
            if (pfnSetPhysics)
            {
                pfnSetPhysics(&g_PhysicsCallbacks);
            }
            using FSetScriptDebugCallbacks = void (*)(const SDebugCallbacks*);
            auto pfnSetDebug = reinterpret_cast<FSetScriptDebugCallbacks>(m_pActiveDll->GetSymbol("SetScriptDebugCallbacks"));
            if (pfnSetDebug)
            {
                pfnSetDebug(&g_DebugCallbacks);
            }
        }
        else if (m_Dll.IsOpen())
        {
            m_pActiveDll = &m_Dll;
        }
        else
        {
            m_pActiveDll = &m_Dll;
            const std::string libExt = GetScriptLibraryExtension();
            const fs::path primaryDllPath = fs::path(projectPath) / ("GameScripts" + libExt);
            const fs::path namedDllPath = fs::path(projectPath) / (gameName + libExt);
            std::error_code ec;

            std::vector<fs::path> vSearchPaths;
            auto addSearchDir = [&](const fs::path& dir)
            {
                for (const auto& name : GetScriptLibraryFileNames("GameScripts"))
                {
                    vSearchPaths.push_back(dir / name);
                }
                for (const auto& name : GetScriptLibraryFileNames(gameName))
                {
                    vSearchPaths.push_back(dir / name);
                }
            };

            addSearchDir(fs::path(projectPath));
            addSearchDir(GetExecutableDirectory());
            addSearchDir(fs::path(projectPath) / "Debug");
            addSearchDir(fs::path(projectPath) / "Release");
            addSearchDir(fs::path(projectPath) / "Build" / gameName);
            addSearchDir(fs::current_path());

            const bool isStandaloneRuntime = fs::exists(fs::path(projectPath) / "__settings") ||
                (fs::path(projectPath).filename().string().rfind("QuarkRuntime-", 0) == 0);

            bool needsBuild = false;
            fs::path activeDllPath;
            for (const auto& path : vSearchPaths)
            {
                if (fs::is_regular_file(path, ec))
                {
                    activeDllPath = path;
                    break;
                }
            }

            if (activeDllPath.empty())
            {
                if (!isStandaloneRuntime)
                {
                    needsBuild = true;
                }
                else
                {
                    TraceLog(LogLevel::Warn, "SCRIPT", "Standalone runtime: no script DLL found in search paths");
                }
            }
            else if (!isStandaloneRuntime)
            {
                auto dllWriteTime = fs::last_write_time(activeDllPath, ec);
                if (!ec)
                {
                    for (const auto& entry : fs::recursive_directory_iterator(fs::path(projectPath), fs::directory_options::skip_permission_denied, ec))
                    {
                        if (ec)
                        {
                            break;
                        }
                        if (!entry.is_regular_file())
                        {
                            continue;
                        }
                        const std::string ext = entry.path().extension().string();
                        if (ext == ".cpp" || ext == ".hpp" || ext == ".h")
                        {
                            const std::string pStr = entry.path().generic_string();
                            if (pStr.find("/.script_build") != std::string::npos ||
                                pStr.find("/cmake-build") != std::string::npos ||
                                pStr.find("/Build") != std::string::npos)
                            {
                                continue;
                            }
                            if (fs::last_write_time(entry.path(), ec) > dllWriteTime)
                            {
                                needsBuild = true;
                                break;
                            }
                        }
                    }
                }
            }

            bool loaded = false;
            if (!needsBuild && !activeDllPath.empty())
            {
                if (LoadScriptDll(activeDllPath.string()))
                {
                    if (m_Dll.GetSymbol("SetScriptInputCallbacks"))
                    {
                        loaded = true;
                    }
                    else if (!isStandaloneRuntime)
                    {
                        UnloadScriptDll();
                        needsBuild = true;
                    }
                    else
                    {
                        loaded = true;
                    }
                }
            }

            if (!loaded && needsBuild && !isStandaloneRuntime)
            {
                std::string buildError;
                std::error_code removeEc;
                fs::remove(primaryDllPath, removeEc);
                fs::remove(namedDllPath, removeEc);

                if (BuildScriptDll(projectPath, gameName, buildError))
                {
                    std::vector<fs::path> vCandidates;
                    for (const auto& name : GetScriptLibraryFileNames("GameScripts"))
                    {
                        vCandidates.push_back(fs::path(projectPath) / ".script_build" / "build" / "Debug" / name);
                        vCandidates.push_back(fs::path(projectPath) / ".script_build" / "build" / "Release" / name);
                        vCandidates.push_back(fs::path(projectPath) / ".script_build" / "build" / name);
                        vCandidates.push_back(fs::path(projectPath) / "Debug" / name);
                        vCandidates.push_back(fs::path(projectPath) / "Release" / name);
                        vCandidates.push_back(fs::path(projectPath) / name);
                    }
                    for (const auto& name : GetScriptLibraryFileNames(gameName))
                    {
                        vCandidates.push_back(fs::path(projectPath) / name);
                    }
                    vCandidates.push_back(primaryDllPath);
                    vCandidates.push_back(namedDllPath);
                    for (const auto& cand : vCandidates)
                    {
                        if (fs::is_regular_file(cand, ec))
                        {
                            if (cand != primaryDllPath)
                            {
                                std::error_code copyEc;
                                fs::copy_file(cand, primaryDllPath, fs::copy_options::overwrite_existing, copyEc);
                                fs::copy_file(cand, namedDllPath, fs::copy_options::overwrite_existing, copyEc);
                            }
                            const fs::path loadTarget = fs::is_regular_file(primaryDllPath, ec) ? primaryDllPath : cand;
                            if (LoadScriptDll(loadTarget.string()))
                            {
                                if (!m_Dll.GetSymbol("SetScriptInputCallbacks"))
                                {
                                    UnloadScriptDll();
                                    continue;
                                }
                                loaded = true;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    TraceLog(LogLevel::Error, "SCRIPT",
                             TextFormat("Failed to build script DLL for '%s':\n%s",
                                        gameName.c_str(), buildError.c_str()));
                }
            }
        }

        for (auto& entity : scene.m_vEntities)
        {
            CScriptComponent* pScriptComp = entity.GetScriptComponent();
            if (!pScriptComp || pScriptComp->m_ScriptName.empty())
            {
                continue;
            }

            IScript* pInstance = CreateScript(pScriptComp->m_ScriptName);
            if (!pInstance)
            {
                TraceLog(LogLevel::Warn, "SCRIPT",
                         TextFormat("Could not instantiate script '%s' on entity '%s'",
                                    pScriptComp->m_ScriptName.c_str(), entity.m_Name.c_str()));
                continue;
            }

            pScriptComp->m_pInstance = pInstance;
            CTransformComponent* pTransform = entity.GetTransformComponent();
            if (pTransform)
            {
                pTransform->ClearLocalMatrixOverride();
            }
            Vec3* pPos = pTransform ? &pTransform->m_Position : nullptr;
            Vec3* pRot = pTransform ? &pTransform->m_Rotation : nullptr;
            Vec3* pScale = pTransform ? &pTransform->m_Scale : nullptr;
            bool* pHasOverride = pTransform ? &pTransform->m_HasLocalMatrixOverride : nullptr;
            pScriptComp->m_pScriptEntity = std::make_unique<CScriptEntity>(&entity, pPos, pRot, pScale, pHasOverride);
            pInstance->m_Entity = pScriptComp->m_pScriptEntity.get();
            pScriptComp->ApplyFieldsToInstance();

            pInstance->OnCreate();
        }

        for (auto& entity : scene.m_vEntities)
        {
            CScriptComponent* pScriptComp = entity.GetScriptComponent();
            if (pScriptComp && pScriptComp->m_pInstance && pScriptComp->m_Enabled)
            {
                pScriptComp->m_pInstance->OnStart();
            }
        }

        m_IsPlaying = true;
    }

    void CScriptEngine::UpdatePlay(CScene& scene, float deltaTime)
    {
        if (!m_IsPlaying)
        {
            return;
        }

        m_pActiveScene = &scene;
        s_ScriptDeltaTime = deltaTime;
        const float scaledDelta = deltaTime * s_ScriptTimeScale;
        s_ScriptAccumulatedTime += static_cast<double>(scaledDelta);

        const float fixedStep = std::max(0.001f, s_ScriptFixedDeltaTime);
        m_PhysicsWorld.SyncBeforeFixedSteps(scene);
        m_FixedAccumulator += scaledDelta;
        int fixedIterations = 0;
        constexpr int MAX_FIXED_STEPS_PER_FRAME = 8;
        while (m_FixedAccumulator >= fixedStep && fixedIterations < MAX_FIXED_STEPS_PER_FRAME)
        {
            m_FixedAccumulator -= fixedStep;
            ++fixedIterations;
            for (size_t i = 0; i < scene.m_vEntities.size(); ++i)
            {
                CScriptComponent* pScriptComp = scene.m_vEntities[i].GetScriptComponent();
                if (pScriptComp && pScriptComp->m_pInstance && pScriptComp->m_Enabled)
                {
                    pScriptComp->m_pInstance->OnFixedUpdate(fixedStep);
                }
            }

            std::vector<quark::physics::SPhysicsEvent> vPhysicsEvents;
            m_PhysicsWorld.Step(scene, fixedStep, vPhysicsEvents);

            const auto dispatchToEntity = [&](int targetId, int otherId,
                                              const quark::physics::SPhysicsEvent& ev,
                                              const Vec3& normalForTarget)
            {
                const int targetIdx = scene.FindEntityIndexById(targetId);
                const int otherIdx = scene.FindEntityIndexById(otherId);
                if (targetIdx < 0 || otherIdx < 0)
                {
                    return;
                }
                CEntity& targetEntity = scene.m_vEntities[static_cast<size_t>(targetIdx)];
                CScriptComponent* pScriptComp = targetEntity.GetScriptComponent();
                if (!pScriptComp || !pScriptComp->m_pInstance || !pScriptComp->m_Enabled)
                {
                    return;
                }
                CScriptEntity* pOtherWrapper = GetOrCreateScriptEntityWrapper(
                    scene.m_vEntities[static_cast<size_t>(otherIdx)]);
                if (!pOtherWrapper)
                {
                    return;
                }

                using quark::physics::EPhysicsEventType;
                switch (ev.m_Type)
                {
                    case EPhysicsEventType::COLLISION_ENTER:
                        pScriptComp->m_pInstance->OnCollisionEnter({ pOtherWrapper, ev.m_Point, normalForTarget, ev.m_Penetration });
                        break;
                    case EPhysicsEventType::COLLISION_STAY:
                        pScriptComp->m_pInstance->OnCollisionStay({ pOtherWrapper, ev.m_Point, normalForTarget, ev.m_Penetration });
                        break;
                    case EPhysicsEventType::COLLISION_EXIT:
                        pScriptComp->m_pInstance->OnCollisionExit(pOtherWrapper);
                        break;
                    case EPhysicsEventType::TRIGGER_ENTER:
                        pScriptComp->m_pInstance->OnTriggerEnter(pOtherWrapper);
                        break;
                    case EPhysicsEventType::TRIGGER_STAY:
                        pScriptComp->m_pInstance->OnTriggerStay(pOtherWrapper);
                        break;
                    case EPhysicsEventType::TRIGGER_EXIT:
                        pScriptComp->m_pInstance->OnTriggerExit(pOtherWrapper);
                        break;
                }
            };

            for (const quark::physics::SPhysicsEvent& ev : vPhysicsEvents)
            {
                dispatchToEntity(ev.m_EntityA, ev.m_EntityB, ev, ev.m_Normal * -1.0f);
                dispatchToEntity(ev.m_EntityB, ev.m_EntityA, ev, ev.m_Normal);
            }
        }

        const float interpAlpha = std::clamp(m_FixedAccumulator / fixedStep, 0.0f, 1.0f);
        m_PhysicsWorld.ApplyInterpolation(scene, interpAlpha, m_FixedAccumulator);

        for (size_t i = 0; i < scene.m_vEntities.size(); ++i)
        {
            CScriptComponent* pScriptComp = scene.m_vEntities[i].GetScriptComponent();
            if (pScriptComp && pScriptComp->m_pInstance && pScriptComp->m_Enabled)
            {
                pScriptComp->m_pInstance->OnUpdate(scaledDelta);
            }
        }

        for (size_t i = 0; i < scene.m_vEntities.size(); ++i)
        {
            CScriptComponent* pScriptComp = scene.m_vEntities[i].GetScriptComponent();
            if (pScriptComp && pScriptComp->m_pInstance && pScriptComp->m_Enabled)
            {
                pScriptComp->m_pInstance->OnLateUpdate(scaledDelta);
            }
        }
    }

    void CScriptEngine::StopPlay(CScene& scene)
    {
        if (!m_IsPlaying)
        {
            return;
        }

        for (auto& entity : scene.m_vEntities)
        {
            CScriptComponent* pScriptComp = entity.GetScriptComponent();
            if (pScriptComp && pScriptComp->m_pInstance)
            {
                pScriptComp->m_pInstance->OnDestroy();
                DestroyScript(pScriptComp->m_ScriptName, pScriptComp->m_pInstance);
                pScriptComp->m_pInstance = nullptr;
                pScriptComp->m_pScriptEntity.reset();
            }
        }

        SetCursorLocked(false);
        m_PhysicsWorld.Reset();
        for (auto& entity : scene.m_vEntities)
        {
            if (CRigidbodyComponent* pBody = entity.GetRigidbodyComponent())
            {
                pBody->ResetRuntimeState();
            }
        }
        s_ScriptTimeScale = 1.0f;
        s_DiscardMouseDeltaFrames = 0;
        s_EntityWrappers.clear();
        m_pActiveScene = nullptr;
        m_FixedAccumulator = 0.0f;

        UnloadScriptDll();
        m_pActiveDll = &m_Dll;
        m_IsPlaying = false;
    }

    void CScriptEngine::LogMessage(EScriptLogLevel level, const std::string& message, bool emitToTraceLog)
    {
        static thread_local bool s_InLogMessage = false;
        if (s_InLogMessage)
        {
            return;
        }
        s_InLogMessage = true;

        SScriptLogEntry entry;
        entry.m_Level = level;
        entry.m_Message = message;
        entry.m_Timestamp = s_ScriptAccumulatedTime;
        m_vLogs.push_back(std::move(entry));
        if (m_vLogs.size() > 2000)
        {
            m_vLogs.erase(m_vLogs.begin());
        }

        if (emitToTraceLog)
        {
            LogLevel coreLevel = LogLevel::Info;
            if (level == EScriptLogLevel::WARNING)
            {
                coreLevel = LogLevel::Warn;
            }
            else if (level == EScriptLogLevel::ERR)
            {
                coreLevel = LogLevel::Error;
            }
            TraceLog(coreLevel, "SCRIPT", message.c_str());
        }

        s_InLogMessage = false;
    }

    std::vector<SScriptLogEntry> CScriptEngine::GetLogs() const
    {
        return m_vLogs;
    }

    void CScriptEngine::ClearLogs()
    {
        m_vLogs.clear();
    }

    void CScriptEngine::RefreshAvailableScripts(const std::string& projectPath)
    {
        m_vAvailableScripts = CScriptHeaderParser::ScanProjectScripts(projectPath);
    }

    std::string CScriptEngine::FindScriptHeaderPath(const std::string& scriptName) const
    {
        for (const auto& info : m_vAvailableScripts)
        {
            if (info.m_ClassName == scriptName)
            {
                return info.m_HeaderPath;
            }
        }
        return "";
    }

} // quark::script
