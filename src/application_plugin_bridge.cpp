#include "QuarkCore/QuarkCore.hpp"

#include "application_plugin_bridge.h"

#include "editor/editor.h"
#include "editor/editor_entity.h"
#include "editor/editor_hierarchy_utils.h"
#include "editor/editor_viewers.h"
#include "camera.h"
#include "engine/transform.h"
#include "project.h"
#include "qcImGui.h"

#include "imgui.h"

#include <algorithm>
#include <cstring>
#include <limits>
namespace
{
CEditor* s_pEditor = nullptr;
CFlyCamera* s_pCamera = nullptr;
SPluginContext* s_pPluginContext = nullptr;

bool TryGetImGuiCondition(EPluginUiCondition condition, ImGuiCond* pCondition)
{
    if (!pCondition)
    {
        return false;
    }
    switch (condition)
    {
    case PLUGIN_UI_CONDITION_ALWAYS: *pCondition = ImGuiCond_Always; return true;
    case PLUGIN_UI_CONDITION_ONCE: *pCondition = ImGuiCond_Once; return true;
    case PLUGIN_UI_CONDITION_FIRST_USE:
        *pCondition = ImGuiCond_FirstUseEver;
        return true;
    case PLUGIN_UI_CONDITION_APPEARING:
        *pCondition = ImGuiCond_Appearing;
        return true;
    default: return false;
    }
}

bool TryGetImGuiMouseButton(EPluginUiMouseButton button, ImGuiMouseButton* pButton)
{
    if (!pButton)
    {
        return false;
    }
    switch (button)
    {
    case PLUGIN_UI_MOUSE_LEFT: *pButton = ImGuiMouseButton_Left; return true;
    case PLUGIN_UI_MOUSE_RIGHT: *pButton = ImGuiMouseButton_Right; return true;
    case PLUGIN_UI_MOUSE_MIDDLE: *pButton = ImGuiMouseButton_Middle; return true;
    default: return false;
    }
}

bool TryGetImGuiKey(EPluginUiKey key, ImGuiKey* pImGuiKey)
{
    if (!pImGuiKey)
    {
        return false;
    }
    switch (key)
    {
    case PLUGIN_UI_KEY_TAB: *pImGuiKey = ImGuiKey_Tab; return true;
    case PLUGIN_UI_KEY_LEFT: *pImGuiKey = ImGuiKey_LeftArrow; return true;
    case PLUGIN_UI_KEY_RIGHT: *pImGuiKey = ImGuiKey_RightArrow; return true;
    case PLUGIN_UI_KEY_UP: *pImGuiKey = ImGuiKey_UpArrow; return true;
    case PLUGIN_UI_KEY_DOWN: *pImGuiKey = ImGuiKey_DownArrow; return true;
    case PLUGIN_UI_KEY_PAGE_UP: *pImGuiKey = ImGuiKey_PageUp; return true;
    case PLUGIN_UI_KEY_PAGE_DOWN: *pImGuiKey = ImGuiKey_PageDown; return true;
    case PLUGIN_UI_KEY_HOME: *pImGuiKey = ImGuiKey_Home; return true;
    case PLUGIN_UI_KEY_END: *pImGuiKey = ImGuiKey_End; return true;
    case PLUGIN_UI_KEY_INSERT: *pImGuiKey = ImGuiKey_Insert; return true;
    case PLUGIN_UI_KEY_DELETE: *pImGuiKey = ImGuiKey_Delete; return true;
    case PLUGIN_UI_KEY_BACKSPACE: *pImGuiKey = ImGuiKey_Backspace; return true;
    case PLUGIN_UI_KEY_SPACE: *pImGuiKey = ImGuiKey_Space; return true;
    case PLUGIN_UI_KEY_ENTER: *pImGuiKey = ImGuiKey_Enter; return true;
    case PLUGIN_UI_KEY_ESCAPE: *pImGuiKey = ImGuiKey_Escape; return true;
    case PLUGIN_UI_KEY_A: *pImGuiKey = ImGuiKey_A; return true;
    case PLUGIN_UI_KEY_B: *pImGuiKey = ImGuiKey_B; return true;
    case PLUGIN_UI_KEY_C: *pImGuiKey = ImGuiKey_C; return true;
    case PLUGIN_UI_KEY_D: *pImGuiKey = ImGuiKey_D; return true;
    case PLUGIN_UI_KEY_E: *pImGuiKey = ImGuiKey_E; return true;
    case PLUGIN_UI_KEY_F: *pImGuiKey = ImGuiKey_F; return true;
    case PLUGIN_UI_KEY_G: *pImGuiKey = ImGuiKey_G; return true;
    case PLUGIN_UI_KEY_H: *pImGuiKey = ImGuiKey_H; return true;
    case PLUGIN_UI_KEY_I: *pImGuiKey = ImGuiKey_I; return true;
    case PLUGIN_UI_KEY_J: *pImGuiKey = ImGuiKey_J; return true;
    case PLUGIN_UI_KEY_K: *pImGuiKey = ImGuiKey_K; return true;
    case PLUGIN_UI_KEY_L: *pImGuiKey = ImGuiKey_L; return true;
    case PLUGIN_UI_KEY_M: *pImGuiKey = ImGuiKey_M; return true;
    case PLUGIN_UI_KEY_N: *pImGuiKey = ImGuiKey_N; return true;
    case PLUGIN_UI_KEY_O: *pImGuiKey = ImGuiKey_O; return true;
    case PLUGIN_UI_KEY_P: *pImGuiKey = ImGuiKey_P; return true;
    case PLUGIN_UI_KEY_Q: *pImGuiKey = ImGuiKey_Q; return true;
    case PLUGIN_UI_KEY_R: *pImGuiKey = ImGuiKey_R; return true;
    case PLUGIN_UI_KEY_S: *pImGuiKey = ImGuiKey_S; return true;
    case PLUGIN_UI_KEY_T: *pImGuiKey = ImGuiKey_T; return true;
    case PLUGIN_UI_KEY_U: *pImGuiKey = ImGuiKey_U; return true;
    case PLUGIN_UI_KEY_V: *pImGuiKey = ImGuiKey_V; return true;
    case PLUGIN_UI_KEY_W: *pImGuiKey = ImGuiKey_W; return true;
    case PLUGIN_UI_KEY_X: *pImGuiKey = ImGuiKey_X; return true;
    case PLUGIN_UI_KEY_Y: *pImGuiKey = ImGuiKey_Y; return true;
    case PLUGIN_UI_KEY_Z: *pImGuiKey = ImGuiKey_Z; return true;
    case PLUGIN_UI_KEY_F1: *pImGuiKey = ImGuiKey_F1; return true;
    case PLUGIN_UI_KEY_F2: *pImGuiKey = ImGuiKey_F2; return true;
    case PLUGIN_UI_KEY_F3: *pImGuiKey = ImGuiKey_F3; return true;
    case PLUGIN_UI_KEY_F4: *pImGuiKey = ImGuiKey_F4; return true;
    case PLUGIN_UI_KEY_F5: *pImGuiKey = ImGuiKey_F5; return true;
    case PLUGIN_UI_KEY_F6: *pImGuiKey = ImGuiKey_F6; return true;
    case PLUGIN_UI_KEY_F7: *pImGuiKey = ImGuiKey_F7; return true;
    case PLUGIN_UI_KEY_F8: *pImGuiKey = ImGuiKey_F8; return true;
    case PLUGIN_UI_KEY_F9: *pImGuiKey = ImGuiKey_F9; return true;
    case PLUGIN_UI_KEY_F10: *pImGuiKey = ImGuiKey_F10; return true;
    case PLUGIN_UI_KEY_F11: *pImGuiKey = ImGuiKey_F11; return true;
    case PLUGIN_UI_KEY_F12: *pImGuiKey = ImGuiKey_F12; return true;
    default: return false;
    }
}

bool TryGetImGuiDataType(EPluginUiScalarType type, ImGuiDataType* pDataType)
{
    if (!pDataType)
    {
        return false;
    }
    switch (type)
    {
    case PLUGIN_UI_SCALAR_S8: *pDataType = ImGuiDataType_S8; return true;
    case PLUGIN_UI_SCALAR_U8: *pDataType = ImGuiDataType_U8; return true;
    case PLUGIN_UI_SCALAR_S16: *pDataType = ImGuiDataType_S16; return true;
    case PLUGIN_UI_SCALAR_U16: *pDataType = ImGuiDataType_U16; return true;
    case PLUGIN_UI_SCALAR_S32: *pDataType = ImGuiDataType_S32; return true;
    case PLUGIN_UI_SCALAR_U32: *pDataType = ImGuiDataType_U32; return true;
    case PLUGIN_UI_SCALAR_S64: *pDataType = ImGuiDataType_S64; return true;
    case PLUGIN_UI_SCALAR_U64: *pDataType = ImGuiDataType_U64; return true;
    case PLUGIN_UI_SCALAR_FLOAT: *pDataType = ImGuiDataType_Float; return true;
    case PLUGIN_UI_SCALAR_DOUBLE: *pDataType = ImGuiDataType_Double; return true;
    default: return false;
    }
}

void AssignUiCallbacks(SPluginContext* pCtx)
{
    pCtx->pfnUiBegin        = [](const char* pTitle)
    {
        return ImGui::Begin(pTitle);
    };
    pCtx->pfnUiEnd          = []()
    {
        ImGui::End();
    };
    pCtx->pfnUiBeginMenu   = [](const char* pLabel)
    {
        return ImGui::BeginMenu(pLabel);
    };
    pCtx->pfnUiEndMenu     = []()
    {
        ImGui::EndMenu();
    };
    pCtx->pfnUiMenuItem    = [](const char* pLabel)
    {
        return ImGui::MenuItem(pLabel);
    };
    pCtx->pfnUiText         = [](const char* pTitle)
    {
        ImGui::Text("%s", pTitle);
    };
    pCtx->pfnUiButton       = [](const char* pLabel)
    {
        return ImGui::Button(pLabel);
    };
    pCtx->pfnUiCheckbox     = [](const char* pLabel, bool* pValue)
    {
        return ImGui::Checkbox(pLabel, pValue);
    };
    pCtx->pfnUiSliderFloat = [](const char* pLabel, float* pValue, float min, float max)
    {
        return ImGui::SliderFloat(pLabel, pValue, min, max);
    };
    pCtx->pfnUiInputFloat  = [](const char* pLabel, float* pValue)
    {
        return ImGui::InputFloat(pLabel, pValue);
    };
    pCtx->pfnUiColorEdit3  = [](const char* pLabel, float aColor[3])
    {
        return ImGui::ColorEdit3(pLabel, aColor);
    };
    pCtx->pfnUiSeparator    = []()
    {
        ImGui::Separator();
    };
    pCtx->pfnUiSameLine    = []()
    {
        ImGui::SameLine();
    };
    pCtx->pfnUiPlotLines = [](const char* pLabel, const float* pValues,
                              int valueCount, float minimum, float maximum)
    {
        ImGui::PlotLines(pLabel, pValues, valueCount, 0, nullptr,
            minimum, maximum, ImVec2(0.0f, 60.0f));
    };
    pCtx->pfnUiInputText = [](const char* pLabel, char* pBuffer, size_t bufferSize)
    {
        return pLabel && pBuffer && bufferSize > 0 &&
            ImGui::InputText(pLabel, pBuffer, bufferSize);
    };
    pCtx->pfnUiSliderInt = [](const char* pLabel, int* pValue, int minimum, int maximum)
    {
        return pLabel && pValue && minimum < maximum &&
            ImGui::SliderInt(pLabel, pValue, minimum, maximum);
    };
    pCtx->pfnUiDragFloat = [](const char* pLabel, float* pValue, float speed,
                              float minimum, float maximum)
    {
        return pLabel && pValue &&
            ImGui::DragFloat(pLabel, pValue, speed, minimum, maximum);
    };
    pCtx->pfnUiDragInt = [](const char* pLabel, int* pValue, float speed,
                            int minimum, int maximum)
    {
        return pLabel && pValue &&
            ImGui::DragInt(pLabel, pValue, speed, minimum, maximum);
    };
    pCtx->pfnUiCombo = [](const char* pLabel, const char* const* pItems,
                          int itemCount, int* pSelectedIndex)
    {
        return pLabel && pItems && itemCount > 0 && pSelectedIndex &&
            *pSelectedIndex >= 0 && *pSelectedIndex < itemCount &&
            ImGui::Combo(pLabel, pSelectedIndex, pItems, itemCount);
    };
    pCtx->pfnUiProgressBar = [](float fraction, const char* pOverlay)
    {
        ImGui::ProgressBar(fraction, ImVec2(0.0f, 0.0f), pOverlay);
    };
    pCtx->pfnUiSetTooltip = [](const char* pText)
    {
        if (pText && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", pText);
        }
    };
    pCtx->pfnUiCollapsingHeader = [](const char* pLabel)
    {
        return pLabel && ImGui::CollapsingHeader(pLabel);
    };
    pCtx->pfnUiBeginChild = [](const char* pId, float width, float height)
    {
        return pId && ImGui::BeginChild(pId, ImVec2(width, height));
    };
    pCtx->pfnUiEndChild = []()
    {
        ImGui::EndChild();
    };
    pCtx->pfnUiBeginTabBar = [](const char* pId)
    {
        return pId && ImGui::BeginTabBar(pId);
    };
    pCtx->pfnUiEndTabBar = []()
    {
        ImGui::EndTabBar();
    };
    pCtx->pfnUiBeginTabItem = [](const char* pLabel)
    {
        return pLabel && ImGui::BeginTabItem(pLabel);
    };
    pCtx->pfnUiEndTabItem = []()
    {
        ImGui::EndTabItem();
    };
    pCtx->pfnUiBeginDisabled = [](bool disabled)
    {
        ImGui::BeginDisabled(disabled);
    };
    pCtx->pfnUiEndDisabled = []()
    {
        ImGui::EndDisabled();
    };
    pCtx->pfnUiSelectable = [](const char* pLabel, bool* pSelected)
    {
        return pLabel && pSelected && ImGui::Selectable(pLabel, pSelected);
    };
    pCtx->pfnUiBeginTable = [](const char* pId, int columnCount, bool sortable)
    {
        const ImGuiTableFlags flags = sortable
            ? ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Borders
            : ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders;
        return pId && columnCount > 0 && ImGui::BeginTable(pId, columnCount, flags);
    };
    pCtx->pfnUiEndTable = []()
    {
        ImGui::EndTable();
    };
    pCtx->pfnUiTableSetupColumn = [](const char* pLabel, bool sortable)
    {
        if (pLabel)
        {
            ImGui::TableSetupColumn(pLabel,
                sortable ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_NoSort);
        }
    };
    pCtx->pfnUiTableHeadersRow = []()
    {
        ImGui::TableHeadersRow();
    };
    pCtx->pfnUiTableNextRow = []()
    {
        ImGui::TableNextRow();
    };
    pCtx->pfnUiTableNextColumn = []()
    {
        return ImGui::TableNextColumn();
    };
    pCtx->pfnUiTableGetSortSpec = [](int* pColumnIndex, int* pSortDirection,
                                     bool* pSpecsDirty)
    {
        if (!pColumnIndex || !pSortDirection || !pSpecsDirty)
        {
            return false;
        }
        ImGuiTableSortSpecs* pSpecs = ImGui::TableGetSortSpecs();
        if (!pSpecs || pSpecs->SpecsCount <= 0)
        {
            return false;
        }
        *pColumnIndex = pSpecs->Specs[0].ColumnIndex;
        *pSortDirection = static_cast<int>(pSpecs->Specs[0].SortDirection);
        *pSpecsDirty = pSpecs->SpecsDirty;
        return true;
    };
    pCtx->pfnUiTableClearSortDirty = []()
    {
        if (ImGuiTableSortSpecs* pSpecs = ImGui::TableGetSortSpecs())
        {
            pSpecs->SpecsDirty = false;
        }
    };
    pCtx->pfnUiPushID = [](const char* pId)
    {
        if (pId)
        {
            ImGui::PushID(pId);
        }
    };
    pCtx->pfnUiPopID = []()
    {
        ImGui::PopID();
    };
    pCtx->pfnUiSetNextItemWidth = [](float width)
    {
        ImGui::SetNextItemWidth(width);
    };
    pCtx->pfnUiSpacing = []()
    {
        ImGui::Spacing();
    };
    pCtx->pfnUiIndent = [](float amount)
    {
        ImGui::Indent(amount);
    };
    pCtx->pfnUiUnindent = [](float amount)
    {
        ImGui::Unindent(amount);
    };
    pCtx->pfnUiTextWrapped = [](const char* pText)
    {
        if (pText)
        {
            ImGui::TextWrapped("%s", pText);
        }
    };
    pCtx->pfnUiBulletText = [](const char* pText)
    {
        if (pText)
        {
            ImGui::BulletText("%s", pText);
        }
    };
    pCtx->pfnUiInputInt = [](const char* pLabel, int* pValue)
    {
        return pLabel && pValue && ImGui::InputInt(pLabel, pValue);
    };
    pCtx->pfnUiInputTextMultiline = [](const char* pLabel, char* pBuffer,
                                       size_t bufferSize, float width, float height)
    {
        return pLabel && pBuffer && bufferSize > 0 &&
            ImGui::InputTextMultiline(pLabel, pBuffer, bufferSize, ImVec2(width, height));
    };
    pCtx->pfnUiColorEdit4 = [](const char* pLabel, float aColor[4])
    {
        return pLabel && aColor && ImGui::ColorEdit4(pLabel, aColor);
    };
    pCtx->pfnUiRadioButton = [](const char* pLabel, int* pValue, int buttonValue)
    {
        return pLabel && pValue && ImGui::RadioButton(pLabel, pValue, buttonValue);
    };
    pCtx->pfnUiOpenPopup = [](const char* pId)
    {
        if (pId)
        {
            ImGui::OpenPopup(pId);
        }
    };
    pCtx->pfnUiBeginPopup = [](const char* pId)
    {
        return pId && ImGui::BeginPopup(pId);
    };
    pCtx->pfnUiBeginPopupModal = [](const char* pId, bool* pOpen)
    {
        return pId && ImGui::BeginPopupModal(pId, pOpen);
    };
    pCtx->pfnUiEndPopup = []()
    {
        ImGui::EndPopup();
    };
    pCtx->pfnUiCloseCurrentPopup = []()
    {
        ImGui::CloseCurrentPopup();
    };
    pCtx->pfnUiSetNextWindowSize = [](float width, float height)
    {
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
    };
    pCtx->pfnUiSetNextWindowPos = [](float x, float y)
    {
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_FirstUseEver);
    };
    pCtx->pfnUiImage = [](CAssetLibrary* pAssets, int textureIndex,
                          float width, float height)
    {
        if (!pAssets || textureIndex < 0 ||
            static_cast<size_t>(textureIndex) >= pAssets->TextureCount())
        {
            return false;
        }

        const Texture2D& texture =
            pAssets->TextureByIndex(static_cast<size_t>(textureIndex)).Texture;
        const ImTextureID textureId = QcImGuiGetTextureId(&texture);
        if (textureId == ImTextureID_Invalid)
        {
            return false;
        }

        ImGui::Image(ImTextureRef(textureId), ImVec2(width, height));
        return true;
    };
    pCtx->pfnUiImageButton = [](const char* pLabel, CAssetLibrary* pAssets,
                                int textureIndex, float width, float height)
    {
        if (!pLabel || !pAssets || textureIndex < 0 ||
            static_cast<size_t>(textureIndex) >= pAssets->TextureCount())
        {
            return false;
        }

        const Texture2D& texture =
            pAssets->TextureByIndex(static_cast<size_t>(textureIndex)).Texture;
        const ImTextureID textureId = QcImGuiGetTextureId(&texture);
        return textureId != ImTextureID_Invalid &&
            ImGui::ImageButton(pLabel, ImTextureRef(textureId), ImVec2(width, height));
    };
    pCtx->pfnAssetGetTextureCount = [](CAssetLibrary* pAssets)
    {
        return pAssets ? static_cast<int>(pAssets->TextureCount()) : 0;
    };
    pCtx->pfnAssetGetTextureName = [](CAssetLibrary* pAssets, int textureIndex)
    {
        if (!pAssets || textureIndex < 0 ||
            static_cast<size_t>(textureIndex) >= pAssets->TextureCount())
        {
            return static_cast<const char*>(nullptr);
        }
        return pAssets->TextureByIndex(static_cast<size_t>(textureIndex)).Name.c_str();
    };
    pCtx->pfnUiBeginDragDropSource = []()
    {
        return ImGui::BeginDragDropSource();
    };
    pCtx->pfnUiSetDragDropPayload = [](const char* pType, const void* pData,
                                       size_t dataSize)
    {
        return pType && pData && dataSize > 0 &&
            dataSize <= static_cast<size_t>(std::numeric_limits<int>::max()) &&
            ImGui::SetDragDropPayload(pType, pData, dataSize);
    };
    pCtx->pfnUiEndDragDropSource = []()
    {
        ImGui::EndDragDropSource();
    };
    pCtx->pfnUiBeginDragDropTarget = []()
    {
        return ImGui::BeginDragDropTarget();
    };
    pCtx->pfnUiAcceptDragDropPayload = [](const char* pType, void* pBuffer,
                                          size_t bufferSize, size_t* pPayloadSize)
    {
        if (!pPayloadSize)
        {
            return false;
        }
        *pPayloadSize = 0;
        if (!pType || (!pBuffer && bufferSize > 0))
        {
            return false;
        }

        const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(pType);
        if (!pPayload || !pPayload->IsDelivery() || pPayload->DataSize < 0)
        {
            return false;
        }

        const size_t payloadSize = static_cast<size_t>(pPayload->DataSize);
        *pPayloadSize = payloadSize;
        if (payloadSize > bufferSize || (payloadSize > 0 && !pBuffer))
        {
            return false;
        }
        if (payloadSize > 0)
        {
            std::memcpy(pBuffer, pPayload->Data, payloadSize);
        }
        return true;
    };
    pCtx->pfnUiEndDragDropTarget = []()
    {
        ImGui::EndDragDropTarget();
    };
    pCtx->pfnUiBeginWithMenuBar = [](const char* pTitle)
    {
        return pTitle && ImGui::Begin(pTitle, nullptr, ImGuiWindowFlags_MenuBar);
    };
    pCtx->pfnUiSetNextWindowCollapsed = [](bool collapsed)
    {
        ImGui::SetNextWindowCollapsed(collapsed, ImGuiCond_FirstUseEver);
    };
    pCtx->pfnUiSetNextWindowFocus = []()
    {
        ImGui::SetNextWindowFocus();
    };
    pCtx->pfnUiBeginMenuBar = []()
    {
        return ImGui::BeginMenuBar();
    };
    pCtx->pfnUiEndMenuBar = []()
    {
        ImGui::EndMenuBar();
    };
    pCtx->pfnUiMenuItemEx = [](const char* pLabel, const char* pShortcut,
                               bool* pSelected, bool enabled)
    {
        return pLabel && ImGui::MenuItem(pLabel, pShortcut, pSelected, enabled);
    };
    pCtx->pfnUiBeginPopupContextItem = [](const char* pId)
    {
        return pId && ImGui::BeginPopupContextItem(pId);
    };
    pCtx->pfnUiBeginPopupContextWindow = [](const char* pId)
    {
        return pId && ImGui::BeginPopupContextWindow(pId);
    };
    pCtx->pfnUiBeginTooltip = []()
    {
        return ImGui::BeginTooltip();
    };
    pCtx->pfnUiEndTooltip = []()
    {
        ImGui::EndTooltip();
    };
    pCtx->pfnUiBeginCombo = [](const char* pLabel, const char* pPreview)
    {
        return pLabel && ImGui::BeginCombo(pLabel, pPreview);
    };
    pCtx->pfnUiEndCombo = []()
    {
        ImGui::EndCombo();
    };
    pCtx->pfnUiListBox = [](const char* pLabel, const char* const* pItems,
                             int itemCount, int* pSelectedIndex, int heightInItems)
    {
        return pLabel && pItems && itemCount > 0 && pSelectedIndex &&
            *pSelectedIndex >= 0 && *pSelectedIndex < itemCount &&
            ImGui::ListBox(pLabel, pSelectedIndex, pItems, itemCount, heightInItems);
    };
    pCtx->pfnUiTreeNode = [](const char* pLabel)
    {
        return pLabel && ImGui::TreeNode(pLabel);
    };
    pCtx->pfnUiTreePop = []()
    {
        ImGui::TreePop();
    };
    pCtx->pfnUiInputFloat2 = [](const char* pLabel, float aValues[2])
    {
        return pLabel && aValues && ImGui::InputFloat2(pLabel, aValues);
    };
    pCtx->pfnUiInputFloat3 = [](const char* pLabel, float aValues[3])
    {
        return pLabel && aValues && ImGui::InputFloat3(pLabel, aValues);
    };
    pCtx->pfnUiInputFloat4 = [](const char* pLabel, float aValues[4])
    {
        return pLabel && aValues && ImGui::InputFloat4(pLabel, aValues);
    };
    pCtx->pfnUiInputInt2 = [](const char* pLabel, int aValues[2])
    {
        return pLabel && aValues && ImGui::InputInt2(pLabel, aValues);
    };
    pCtx->pfnUiInputInt3 = [](const char* pLabel, int aValues[3])
    {
        return pLabel && aValues && ImGui::InputInt3(pLabel, aValues);
    };
    pCtx->pfnUiInputInt4 = [](const char* pLabel, int aValues[4])
    {
        return pLabel && aValues && ImGui::InputInt4(pLabel, aValues);
    };
    pCtx->pfnUiDragFloat2 = [](const char* pLabel, float aValues[2], float speed,
                               float minimum, float maximum)
    {
        return pLabel && aValues &&
            ImGui::DragFloat2(pLabel, aValues, speed, minimum, maximum);
    };
    pCtx->pfnUiDragFloat3 = [](const char* pLabel, float aValues[3], float speed,
                               float minimum, float maximum)
    {
        return pLabel && aValues &&
            ImGui::DragFloat3(pLabel, aValues, speed, minimum, maximum);
    };
    pCtx->pfnUiDragFloat4 = [](const char* pLabel, float aValues[4], float speed,
                               float minimum, float maximum)
    {
        return pLabel && aValues &&
            ImGui::DragFloat4(pLabel, aValues, speed, minimum, maximum);
    };
    pCtx->pfnUiColorPicker4 = [](const char* pLabel, float aColor[4])
    {
        return pLabel && aColor && ImGui::ColorPicker4(pLabel, aColor);
    };
    pCtx->pfnUiIsItemHovered = []()
    {
        return ImGui::IsItemHovered();
    };
    pCtx->pfnUiIsItemActive = []()
    {
        return ImGui::IsItemActive();
    };
    pCtx->pfnUiIsItemFocused = []()
    {
        return ImGui::IsItemFocused();
    };
    pCtx->pfnUiIsItemClicked = [](int button)
    {
        return button >= 0 && button <= 2 &&
            ImGui::IsItemClicked(static_cast<ImGuiMouseButton>(button));
    };
    pCtx->pfnUiIsItemVisible = []()
    {
        return ImGui::IsItemVisible();
    };
    pCtx->pfnUiRadioButtonGroup = [](const char* pLabel, const char* const* pItems,
                                     int itemCount, int* pSelectedIndex)
    {
        if (!pLabel || !pItems || itemCount <= 0 || !pSelectedIndex ||
            *pSelectedIndex < 0 || *pSelectedIndex >= itemCount)
        {
            return false;
        }

        ImGui::Text("%s", pLabel);
        ImGui::SameLine();
        ImGui::PushID(pLabel);
        bool changed = false;
        for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
        {
            if (!pItems[itemIndex])
            {
                ImGui::PopID();
                return false;
            }
            ImGui::PushID(itemIndex);
            if (ImGui::RadioButton(pItems[itemIndex], pSelectedIndex, itemIndex))
            {
                changed = true;
            }
            ImGui::PopID();
            if (itemIndex + 1 < itemCount)
            {
                ImGui::SameLine();
            }
        }
        ImGui::PopID();
        return changed;
    };
    pCtx->pfnUiTableSetupScrollFreeze = [](int columns, int rows)
    {
        if (columns >= 0 && rows >= 0)
        {
            ImGui::TableSetupScrollFreeze(columns, rows);
        }
    };
    pCtx->pfnUiTableSetColumnEnabled = [](int columnIndex, bool enabled)
    {
        if (columnIndex >= 0)
        {
            ImGui::TableSetColumnEnabled(columnIndex, enabled);
        }
    };
    pCtx->pfnUiTableIsColumnVisible = [](int columnIndex)
    {
        const ImGuiTableColumnFlags flags = ImGui::TableGetColumnFlags(columnIndex);
        return (flags & ImGuiTableColumnFlags_IsVisible) != 0;
    };
    pCtx->pfnUiTableGetSortSpecs = [](int* aColumnIndices, int* aSortDirections,
                                      int capacity, int* pSortCount, bool* pSpecsDirty)
    {
        if (!pSortCount || !pSpecsDirty || capacity < 0 ||
            (capacity > 0 && (!aColumnIndices || !aSortDirections)))
        {
            return false;
        }
        *pSortCount = 0;
        *pSpecsDirty = false;
        ImGuiTableSortSpecs* pSpecs = ImGui::TableGetSortSpecs();
        if (!pSpecs)
        {
            return false;
        }

        *pSortCount = pSpecs->SpecsCount;
        *pSpecsDirty = pSpecs->SpecsDirty;
        const int copiedCount = std::min(capacity, pSpecs->SpecsCount);
        for (int sortIndex = 0; sortIndex < copiedCount; ++sortIndex)
        {
            aColumnIndices[sortIndex] = pSpecs->Specs[sortIndex].ColumnIndex;
            aSortDirections[sortIndex] =
                static_cast<int>(pSpecs->Specs[sortIndex].SortDirection);
        }
        return pSpecs->SpecsCount > 0;
    };
    pCtx->pfnUiPushStyleColor = [](EPluginUiColor color, float red, float green,
                                   float blue, float alpha)
    {
        ImGuiCol imguiColor;
        switch (color)
        {
        case PLUGIN_UI_COLOR_TEXT: imguiColor = ImGuiCol_Text; break;
        case PLUGIN_UI_COLOR_TEXT_DISABLED: imguiColor = ImGuiCol_TextDisabled; break;
        case PLUGIN_UI_COLOR_WINDOW_BACKGROUND: imguiColor = ImGuiCol_WindowBg; break;
        case PLUGIN_UI_COLOR_CHILD_BACKGROUND: imguiColor = ImGuiCol_ChildBg; break;
        case PLUGIN_UI_COLOR_POPUP_BACKGROUND: imguiColor = ImGuiCol_PopupBg; break;
        case PLUGIN_UI_COLOR_BORDER: imguiColor = ImGuiCol_Border; break;
        case PLUGIN_UI_COLOR_FRAME_BACKGROUND: imguiColor = ImGuiCol_FrameBg; break;
        case PLUGIN_UI_COLOR_FRAME_HOVERED: imguiColor = ImGuiCol_FrameBgHovered; break;
        case PLUGIN_UI_COLOR_FRAME_ACTIVE: imguiColor = ImGuiCol_FrameBgActive; break;
        case PLUGIN_UI_COLOR_BUTTON: imguiColor = ImGuiCol_Button; break;
        case PLUGIN_UI_COLOR_BUTTON_HOVERED: imguiColor = ImGuiCol_ButtonHovered; break;
        case PLUGIN_UI_COLOR_BUTTON_ACTIVE: imguiColor = ImGuiCol_ButtonActive; break;
        case PLUGIN_UI_COLOR_HEADER: imguiColor = ImGuiCol_Header; break;
        case PLUGIN_UI_COLOR_HEADER_HOVERED: imguiColor = ImGuiCol_HeaderHovered; break;
        case PLUGIN_UI_COLOR_HEADER_ACTIVE: imguiColor = ImGuiCol_HeaderActive; break;
        default: return;
        }
        ImGui::PushStyleColor(imguiColor, ImVec4(red, green, blue, alpha));
    };
    pCtx->pfnUiPopStyleColor = [](int count)
    {
        if (count > 0)
        {
            ImGui::PopStyleColor(count);
        }
    };
    pCtx->pfnUiPushStyleVarFloat = [](EPluginUiStyleVar style, float value)
    {
        ImGuiStyleVar imguiStyle;
        switch (style)
        {
        case PLUGIN_UI_STYLE_ALPHA: imguiStyle = ImGuiStyleVar_Alpha; break;
        case PLUGIN_UI_STYLE_WINDOW_ROUNDING: imguiStyle = ImGuiStyleVar_WindowRounding; break;
        case PLUGIN_UI_STYLE_FRAME_ROUNDING: imguiStyle = ImGuiStyleVar_FrameRounding; break;
        case PLUGIN_UI_STYLE_SCROLLBAR_ROUNDING: imguiStyle = ImGuiStyleVar_ScrollbarRounding; break;
        case PLUGIN_UI_STYLE_GRAB_ROUNDING: imguiStyle = ImGuiStyleVar_GrabRounding; break;
        default: return;
        }
        ImGui::PushStyleVar(imguiStyle, value);
    };
    pCtx->pfnUiPushStyleVarVec2 = [](EPluginUiStyleVar style, float x, float y)
    {
        ImGuiStyleVar imguiStyle;
        switch (style)
        {
        case PLUGIN_UI_STYLE_WINDOW_PADDING: imguiStyle = ImGuiStyleVar_WindowPadding; break;
        case PLUGIN_UI_STYLE_FRAME_PADDING: imguiStyle = ImGuiStyleVar_FramePadding; break;
        case PLUGIN_UI_STYLE_ITEM_SPACING: imguiStyle = ImGuiStyleVar_ItemSpacing; break;
        case PLUGIN_UI_STYLE_ITEM_INNER_SPACING: imguiStyle = ImGuiStyleVar_ItemInnerSpacing; break;
        case PLUGIN_UI_STYLE_CELL_PADDING: imguiStyle = ImGuiStyleVar_CellPadding; break;
        default: return;
        }
        ImGui::PushStyleVar(imguiStyle, ImVec2(x, y));
    };
    pCtx->pfnUiPopStyleVar = [](int count)
    {
        if (count > 0)
        {
            ImGui::PopStyleVar(count);
        }
    };
    pCtx->pfnUiGetCursorScreenPos = [](float* pX, float* pY)
    {
        if (!pX || !pY)
        {
            return;
        }
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        *pX = cursor.x;
        *pY = cursor.y;
    };
    pCtx->pfnUiGetItemRect = [](float* pMinX, float* pMinY,
                                float* pMaxX, float* pMaxY)
    {
        if (!pMinX || !pMinY || !pMaxX || !pMaxY)
        {
            return;
        }
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        *pMinX = min.x;
        *pMinY = min.y;
        *pMaxX = max.x;
        *pMaxY = max.y;
    };
    pCtx->pfnUiDrawLine = [](float x1, float y1, float x2, float y2,
                             float red, float green, float blue, float alpha,
                             float thickness)
    {
        if (thickness > 0.0f)
        {
            ImGui::GetWindowDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2),
                ImGui::GetColorU32(ImVec4(red, green, blue, alpha)), thickness);
        }
    };
    pCtx->pfnUiDrawRect = [](float x1, float y1, float x2, float y2,
                             float red, float green, float blue, float alpha,
                             float thickness)
    {
        if (thickness > 0.0f)
        {
            ImGui::GetWindowDrawList()->AddRect(ImVec2(x1, y1), ImVec2(x2, y2),
                ImGui::GetColorU32(ImVec4(red, green, blue, alpha)), 0.0f, 0, thickness);
        }
    };
    pCtx->pfnUiDrawRectFilled = [](float x1, float y1, float x2, float y2,
                                   float red, float green, float blue, float alpha)
    {
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2),
            ImGui::GetColorU32(ImVec4(red, green, blue, alpha)));
    };
    pCtx->pfnUiDrawCircle = [](float x, float y, float radius,
                               float red, float green, float blue, float alpha,
                               float thickness)
    {
        if (radius > 0.0f && thickness > 0.0f)
        {
            ImGui::GetWindowDrawList()->AddCircle(ImVec2(x, y), radius,
                ImGui::GetColorU32(ImVec4(red, green, blue, alpha)), 0, thickness);
        }
    };
    pCtx->pfnUiDrawCircleFilled = [](float x, float y, float radius,
                                     float red, float green, float blue, float alpha)
    {
        if (radius > 0.0f)
        {
            ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(x, y), radius,
                ImGui::GetColorU32(ImVec4(red, green, blue, alpha)));
        }
    };
    pCtx->pfnUiDrawText = [](float x, float y, const char* pText,
                             float red, float green, float blue, float alpha)
    {
        if (pText)
        {
            ImGui::GetWindowDrawList()->AddText(ImVec2(x, y),
                ImGui::GetColorU32(ImVec4(red, green, blue, alpha)), pText);
        }
    };
    pCtx->pfnUiPlotHistogram = [](const char* pLabel, const float* pValues,
                                  int valueCount, float minimum, float maximum)
    {
        if (pLabel && pValues && valueCount > 0)
        {
            ImGui::PlotHistogram(pLabel, pValues, valueCount, 0, nullptr,
                minimum, maximum, ImVec2(0.0f, 60.0f));
        }
    };
    pCtx->pfnUiBeginEx = [](const char* pTitle, bool* pOpen, int options)
    {
        if (!pTitle)
        {
            return false;
        }

        ImGuiWindowFlags flags = ImGuiWindowFlags_None;
        if (options & PLUGIN_UI_WINDOW_NO_TITLE_BAR)
        {
            flags |= ImGuiWindowFlags_NoTitleBar;
        }
        if (options & PLUGIN_UI_WINDOW_NO_RESIZE)
        {
            flags |= ImGuiWindowFlags_NoResize;
        }
        if (options & PLUGIN_UI_WINDOW_NO_MOVE)
        {
            flags |= ImGuiWindowFlags_NoMove;
        }
        if (options & PLUGIN_UI_WINDOW_NO_SCROLLBAR)
        {
            flags |= ImGuiWindowFlags_NoScrollbar;
        }
        if (options & PLUGIN_UI_WINDOW_NO_COLLAPSE)
        {
            flags |= ImGuiWindowFlags_NoCollapse;
        }
        if (options & PLUGIN_UI_WINDOW_NO_BACKGROUND)
        {
            flags |= ImGuiWindowFlags_NoBackground;
        }
        if (options & PLUGIN_UI_WINDOW_NO_SAVED_SETTINGS)
        {
            flags |= ImGuiWindowFlags_NoSavedSettings;
        }
        if (options & PLUGIN_UI_WINDOW_MENU_BAR)
        {
            flags |= ImGuiWindowFlags_MenuBar;
        }
        if (options & PLUGIN_UI_WINDOW_NO_BRING_TO_FRONT)
        {
            flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        }
        return ImGui::Begin(pTitle, pOpen, flags);
    };
    pCtx->pfnUiBeginChildEx = [](const char* pId, float width, float height,
                                 bool border)
    {
        if (!pId)
        {
            return false;
        }
        const ImGuiChildFlags flags = border ? ImGuiChildFlags_Borders
                                             : ImGuiChildFlags_None;
        return ImGui::BeginChild(pId, ImVec2(width, height), flags);
    };
    pCtx->pfnUiTextColored = [](const char* pText, float red, float green,
                                float blue, float alpha)
    {
        if (pText)
        {
            ImGui::TextColored(ImVec4(red, green, blue, alpha), "%s", pText);
        }
    };
    pCtx->pfnUiTextDisabled = [](const char* pText)
    {
        if (pText)
        {
            ImGui::TextDisabled("%s", pText);
        }
    };
    pCtx->pfnUiLabelText = [](const char* pLabel, const char* pValue)
    {
        if (pLabel && pValue)
        {
            ImGui::LabelText(pLabel, "%s", pValue);
        }
    };
    pCtx->pfnUiSeparatorText = [](const char* pLabel)
    {
        if (pLabel)
        {
            ImGui::SeparatorText(pLabel);
        }
    };
    pCtx->pfnUiSmallButton = [](const char* pLabel)
    {
        return pLabel && ImGui::SmallButton(pLabel);
    };
    pCtx->pfnUiInvisibleButton = [](const char* pId, float width, float height)
    {
        return pId && width > 0.0f && height > 0.0f &&
            ImGui::InvisibleButton(pId, ImVec2(width, height));
    };
    pCtx->pfnUiArrowButton = [](const char* pId, EPluginUiDirection direction)
    {
        if (!pId)
        {
            return false;
        }
        ImGuiDir imguiDirection;
        switch (direction)
        {
        case PLUGIN_UI_DIRECTION_LEFT: imguiDirection = ImGuiDir_Left; break;
        case PLUGIN_UI_DIRECTION_RIGHT: imguiDirection = ImGuiDir_Right; break;
        case PLUGIN_UI_DIRECTION_UP: imguiDirection = ImGuiDir_Up; break;
        case PLUGIN_UI_DIRECTION_DOWN: imguiDirection = ImGuiDir_Down; break;
        default: return false;
        }
        return ImGui::ArrowButton(pId, imguiDirection);
    };
    pCtx->pfnUiCheckboxFlags = [](const char* pLabel, int* pValue, int flagsMask)
    {
        return pLabel && pValue && flagsMask != 0 &&
            ImGui::CheckboxFlags(pLabel, pValue, flagsMask);
    };
    pCtx->pfnUiRadioButtonEx = [](const char* pLabel, bool active)
    {
        return pLabel && ImGui::RadioButton(pLabel, active);
    };
    pCtx->pfnUiInputFloatEx = [](const char* pLabel, float* pValue,
                                 float step, float stepFast, const char* pFormat)
    {
        return pLabel && pValue &&
            ImGui::InputFloat(pLabel, pValue, step, stepFast, pFormat);
    };
    pCtx->pfnUiInputIntEx = [](const char* pLabel, int* pValue,
                               int step, int stepFast)
    {
        return pLabel && pValue && ImGui::InputInt(pLabel, pValue, step, stepFast);
    };
    pCtx->pfnUiInputDouble = [](const char* pLabel, double* pValue,
                                double step, double stepFast, const char* pFormat)
    {
        return pLabel && pValue &&
            ImGui::InputDouble(pLabel, pValue, step, stepFast, pFormat);
    };
    pCtx->pfnUiInputTextWithHint = [](const char* pLabel, const char* pHint,
                                      char* pBuffer, size_t bufferSize)
    {
        return pLabel && pHint && pBuffer && bufferSize > 0 &&
            ImGui::InputTextWithHint(pLabel, pHint, pBuffer, bufferSize);
    };
    pCtx->pfnUiDragFloatN = [](const char* pLabel, float* aValues,
                               int componentCount, float speed,
                               float minimum, float maximum)
    {
        if (!pLabel || !aValues ||
            (componentCount != 2 && componentCount != 3 && componentCount != 4))
        {
            return false;
        }
        switch (componentCount)
        {
        case 2: return ImGui::DragFloat2(pLabel, aValues, speed, minimum, maximum);
        case 3: return ImGui::DragFloat3(pLabel, aValues, speed, minimum, maximum);
        case 4: return ImGui::DragFloat4(pLabel, aValues, speed, minimum, maximum);
        default: return false;
        }
    };
    pCtx->pfnUiDragIntRange2 = [](const char* pLabel, int* pCurrentMin,
                                  int* pCurrentMax, float speed,
                                  int minimum, int maximum)
    {
        return pLabel && pCurrentMin && pCurrentMax &&
            ImGui::DragIntRange2(pLabel, pCurrentMin, pCurrentMax, speed,
                                 minimum, maximum);
    };
    pCtx->pfnUiDragFloatRange2 = [](const char* pLabel, float* pCurrentMin,
                                    float* pCurrentMax, float speed,
                                    float minimum, float maximum)
    {
        return pLabel && pCurrentMin && pCurrentMax &&
            ImGui::DragFloatRange2(pLabel, pCurrentMin, pCurrentMax, speed,
                                   minimum, maximum);
    };
    pCtx->pfnUiSliderFloatN = [](const char* pLabel, float* aValues,
                                 int componentCount, float minimum, float maximum)
    {
        if (!pLabel || !aValues || minimum >= maximum ||
            (componentCount != 2 && componentCount != 3 && componentCount != 4))
        {
            return false;
        }
        switch (componentCount)
        {
        case 2: return ImGui::SliderFloat2(pLabel, aValues, minimum, maximum);
        case 3: return ImGui::SliderFloat3(pLabel, aValues, minimum, maximum);
        case 4: return ImGui::SliderFloat4(pLabel, aValues, minimum, maximum);
        default: return false;
        }
    };
    pCtx->pfnUiSliderIntN = [](const char* pLabel, int* aValues,
                               int componentCount, int minimum, int maximum)
    {
        if (!pLabel || !aValues || minimum >= maximum ||
            (componentCount != 2 && componentCount != 3 && componentCount != 4))
        {
            return false;
        }
        switch (componentCount)
        {
        case 2: return ImGui::SliderInt2(pLabel, aValues, minimum, maximum);
        case 3: return ImGui::SliderInt3(pLabel, aValues, minimum, maximum);
        case 4: return ImGui::SliderInt4(pLabel, aValues, minimum, maximum);
        default: return false;
        }
    };
    pCtx->pfnUiSliderAngle = [](const char* pLabel, float* pRadians,
                                float minimumDegrees, float maximumDegrees)
    {
        return pLabel && pRadians && minimumDegrees < maximumDegrees &&
            ImGui::SliderAngle(pLabel, pRadians, minimumDegrees, maximumDegrees);
    };
    pCtx->pfnUiVSliderFloat = [](const char* pLabel, float width, float height,
                                 float* pValue, float minimum, float maximum)
    {
        return pLabel && pValue && width > 0.0f && height > 0.0f &&
            minimum < maximum &&
            ImGui::VSliderFloat(pLabel, ImVec2(width, height), pValue,
                                minimum, maximum);
    };
    pCtx->pfnUiVSliderInt = [](const char* pLabel, float width, float height,
                               int* pValue, int minimum, int maximum)
    {
        return pLabel && pValue && width > 0.0f && height > 0.0f &&
            minimum < maximum &&
            ImGui::VSliderInt(pLabel, ImVec2(width, height), pValue,
                              minimum, maximum);
    };
    pCtx->pfnUiColorPicker3 = [](const char* pLabel, float aColor[3])
    {
        return pLabel && aColor && ImGui::ColorPicker3(pLabel, aColor);
    };
    pCtx->pfnUiTreeNodeEx = [](const char* pLabel, int options)
    {
        if (!pLabel)
        {
            return false;
        }
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
        if (options & PLUGIN_UI_TREE_SELECTED)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (options & PLUGIN_UI_TREE_DEFAULT_OPEN)
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        if (options & PLUGIN_UI_TREE_LEAF)
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (options & PLUGIN_UI_TREE_BULLET)
        {
            flags |= ImGuiTreeNodeFlags_Bullet;
        }
        if (options & PLUGIN_UI_TREE_FRAMED)
        {
            flags |= ImGuiTreeNodeFlags_Framed;
        }
        if (options & PLUGIN_UI_TREE_SPAN_AVAILABLE)
        {
            flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
        }
        return ImGui::TreeNodeEx(pLabel, flags);
    };
    pCtx->pfnUiSelectableEx = [](const char* pLabel, bool* pSelected,
                                 bool allowDoubleClick, bool spanAvailableWidth)
    {
        if (!pLabel || !pSelected)
        {
            return false;
        }
        ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
        if (allowDoubleClick)
        {
            flags |= ImGuiSelectableFlags_AllowDoubleClick;
        }
        if (spanAvailableWidth)
        {
            flags |= ImGuiSelectableFlags_SpanAllColumns;
        }
        return ImGui::Selectable(pLabel, pSelected, flags);
    };
    pCtx->pfnUiIsWindowAppearing = []()
    {
        return ImGui::IsWindowAppearing();
    };
    pCtx->pfnUiIsWindowCollapsed = []()
    {
        return ImGui::IsWindowCollapsed();
    };
    pCtx->pfnUiGetWindowRect = [](float* pX, float* pY,
                                  float* pWidth, float* pHeight)
    {
        if (!pX || !pY || !pWidth || !pHeight)
        {
            return;
        }
        const ImVec2 position = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        *pX = position.x;
        *pY = position.y;
        *pWidth = size.x;
        *pHeight = size.y;
    };
    pCtx->pfnUiGetContentRegionAvail = [](float* pWidth, float* pHeight)
    {
        if (!pWidth || !pHeight)
        {
            return;
        }
        const ImVec2 size = ImGui::GetContentRegionAvail();
        *pWidth = size.x;
        *pHeight = size.y;
    };
    pCtx->pfnUiIsItemEdited = []()
    {
        return ImGui::IsItemEdited();
    };
    pCtx->pfnUiIsItemActivated = []()
    {
        return ImGui::IsItemActivated();
    };
    pCtx->pfnUiIsItemDeactivated = []()
    {
        return ImGui::IsItemDeactivated();
    };
    pCtx->pfnUiIsItemDeactivatedAfterEdit = []()
    {
        return ImGui::IsItemDeactivatedAfterEdit();
    };
    pCtx->pfnUiGetItemRectSize = [](float* pWidth, float* pHeight)
    {
        if (!pWidth || !pHeight)
        {
            return;
        }
        const ImVec2 size = ImGui::GetItemRectSize();
        *pWidth = size.x;
        *pHeight = size.y;
    };
    pCtx->pfnUiSetNextWindowSizeConstraints = [](float minimumWidth,
                                                 float minimumHeight,
                                                 float maximumWidth,
                                                 float maximumHeight)
    {
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(minimumWidth, minimumHeight),
            ImVec2(maximumWidth, maximumHeight));
    };
    pCtx->pfnUiSetNextWindowContentSize = [](float width, float height)
    {
        ImGui::SetNextWindowContentSize(ImVec2(width, height));
    };
    pCtx->pfnUiSetNextWindowScroll = [](float x, float y)
    {
        ImGui::SetNextWindowScroll(ImVec2(x, y));
    };
    pCtx->pfnUiSetNextWindowBgAlpha = [](float alpha)
    {
        ImGui::SetNextWindowBgAlpha(std::clamp(alpha, 0.0f, 1.0f));
    };
    pCtx->pfnUiSetWindowPos = [](float x, float y)
    {
        ImGui::SetWindowPos(ImVec2(x, y));
    };
    pCtx->pfnUiSetWindowSize = [](float width, float height)
    {
        if (width >= 0.0f && height >= 0.0f)
        {
            ImGui::SetWindowSize(ImVec2(width, height));
        }
    };
    pCtx->pfnUiSetWindowCollapsed = [](bool collapsed)
    {
        ImGui::SetWindowCollapsed(collapsed);
    };
    pCtx->pfnUiSetWindowFocus = []()
    {
        ImGui::SetWindowFocus();
    };
    pCtx->pfnUiGetWindowScroll = [](float* pX, float* pY,
                                    float* pMaxX, float* pMaxY)
    {
        if (!pX || !pY || !pMaxX || !pMaxY)
        {
            return;
        }
        *pX = ImGui::GetScrollX();
        *pY = ImGui::GetScrollY();
        *pMaxX = ImGui::GetScrollMaxX();
        *pMaxY = ImGui::GetScrollMaxY();
    };
    pCtx->pfnUiSetWindowScroll = [](float x, float y)
    {
        ImGui::SetScrollX(x);
        ImGui::SetScrollY(y);
    };
    pCtx->pfnUiScrollHere = [](float centerXRatio, float centerYRatio)
    {
        if (centerXRatio >= 0.0f && centerXRatio <= 1.0f)
        {
            ImGui::SetScrollHereX(centerXRatio);
        }
        if (centerYRatio >= 0.0f && centerYRatio <= 1.0f)
        {
            ImGui::SetScrollHereY(centerYRatio);
        }
    };
    pCtx->pfnUiSetCursorScreenPos = [](float x, float y)
    {
        ImGui::SetCursorScreenPos(ImVec2(x, y));
    };
    pCtx->pfnUiGetCursorPos = [](float* pX, float* pY)
    {
        if (!pX || !pY)
        {
            return;
        }
        const ImVec2 position = ImGui::GetCursorPos();
        *pX = position.x;
        *pY = position.y;
    };
    pCtx->pfnUiSetCursorPos = [](float x, float y)
    {
        ImGui::SetCursorPos(ImVec2(x, y));
    };
    pCtx->pfnUiDummy = [](float width, float height)
    {
        if (width >= 0.0f && height >= 0.0f)
        {
            ImGui::Dummy(ImVec2(width, height));
        }
    };
    pCtx->pfnUiNewLine = []()
    {
        ImGui::NewLine();
    };
    pCtx->pfnUiBeginGroup = []()
    {
        ImGui::BeginGroup();
    };
    pCtx->pfnUiEndGroup = []()
    {
        ImGui::EndGroup();
    };
    pCtx->pfnUiAlignTextToFramePadding = []()
    {
        ImGui::AlignTextToFramePadding();
    };
    pCtx->pfnUiGetLayoutMetrics = [](float* pTextLineHeight,
                                     float* pTextLineHeightWithSpacing,
                                     float* pFrameHeight,
                                     float* pFrameHeightWithSpacing)
    {
        if (!pTextLineHeight || !pTextLineHeightWithSpacing ||
            !pFrameHeight || !pFrameHeightWithSpacing)
        {
            return;
        }
        *pTextLineHeight = ImGui::GetTextLineHeight();
        *pTextLineHeightWithSpacing = ImGui::GetTextLineHeightWithSpacing();
        *pFrameHeight = ImGui::GetFrameHeight();
        *pFrameHeightWithSpacing = ImGui::GetFrameHeightWithSpacing();
    };
    pCtx->pfnUiSameLineEx = [](float offsetFromStartX, float spacing)
    {
        ImGui::SameLine(offsetFromStartX, spacing);
    };
    pCtx->pfnUiColorButton = [](const char* pId, float red, float green,
                                 float blue, float alpha, float width, float height)
    {
        return pId && width >= 0.0f && height >= 0.0f &&
            ImGui::ColorButton(pId, ImVec4(red, green, blue, alpha), 0,
                               ImVec2(width, height));
    };
    pCtx->pfnUiSetNextItemOpen = [](bool open, EPluginUiCondition condition)
    {
        ImGuiCond imguiCondition;
        if (TryGetImGuiCondition(condition, &imguiCondition))
        {
            ImGui::SetNextItemOpen(open, imguiCondition);
        }
    };
    pCtx->pfnUiCollapsingHeaderVisible = [](const char* pLabel, bool* pVisible)
    {
        return pLabel && pVisible && ImGui::CollapsingHeader(pLabel, pVisible);
    };
    pCtx->pfnUiTreeNodeIsOpen = [](const char* pId)
    {
        return pId && ImGui::TreeNodeGetOpen(ImGui::GetID(pId));
    };
    pCtx->pfnUiSelectableSized = [](const char* pLabel, bool selected,
                                    float width, float height,
                                    bool allowDoubleClick)
    {
        if (!pLabel || width < 0.0f || height < 0.0f)
        {
            return false;
        }
        const ImGuiSelectableFlags flags = allowDoubleClick
            ? ImGuiSelectableFlags_AllowDoubleClick
            : ImGuiSelectableFlags_None;
        return ImGui::Selectable(pLabel, selected, flags, ImVec2(width, height));
    };
    pCtx->pfnUiOpenPopupOnItemClick = [](const char* pId,
                                         EPluginUiMouseButton mouseButton)
    {
        ImGuiMouseButton imguiButton;
        if (pId && TryGetImGuiMouseButton(mouseButton, &imguiButton))
        {
            ImGui::OpenPopupOnItemClick(pId, imguiButton);
        }
    };
    pCtx->pfnUiBeginPopupContextVoid = [](const char* pId,
                                          EPluginUiMouseButton mouseButton)
    {
        ImGuiMouseButton imguiButton;
        return pId && TryGetImGuiMouseButton(mouseButton, &imguiButton) &&
            ImGui::BeginPopupContextVoid(pId, imguiButton);
    };
    pCtx->pfnUiIsPopupOpen = [](const char* pId)
    {
        return pId && ImGui::IsPopupOpen(pId);
    };
    pCtx->pfnUiTableGetPosition = [](int* pRowIndex, int* pColumnIndex,
                                     int* pColumnCount)
    {
        if (!pRowIndex || !pColumnIndex || !pColumnCount)
        {
            return;
        }
        *pRowIndex = ImGui::TableGetRowIndex();
        *pColumnIndex = ImGui::TableGetColumnIndex();
        *pColumnCount = ImGui::TableGetColumnCount();
    };
    pCtx->pfnUiTableSetColumnIndex = [](int columnIndex)
    {
        return columnIndex >= 0 && ImGui::TableSetColumnIndex(columnIndex);
    };
    pCtx->pfnUiTableSetBgColor = [](EPluginUiTableColorTarget target,
                                    float red, float green, float blue, float alpha,
                                    int columnIndex)
    {
        ImGuiTableBgTarget imguiTarget;
        switch (target)
        {
        case PLUGIN_UI_TABLE_COLOR_ROW_BACKGROUND:
            imguiTarget = ImGuiTableBgTarget_RowBg0;
            break;
        case PLUGIN_UI_TABLE_COLOR_ROW_BACKGROUND_ALT:
            imguiTarget = ImGuiTableBgTarget_RowBg1;
            break;
        case PLUGIN_UI_TABLE_COLOR_CELL_BACKGROUND:
            imguiTarget = ImGuiTableBgTarget_CellBg;
            break;
        default: return;
        }
        ImGui::TableSetBgColor(imguiTarget,
            ImGui::GetColorU32(ImVec4(red, green, blue, alpha)), columnIndex);
    };
    pCtx->pfnUiTableHeader = [](const char* pLabel)
    {
        if (pLabel)
        {
            ImGui::TableHeader(pLabel);
        }
    };
    pCtx->pfnUiTableAngledHeadersRow = []()
    {
        ImGui::TableAngledHeadersRow();
    };
    pCtx->pfnUiTableSetupColumnEx = [](const char* pLabel, bool sortable,
                                       float widthOrWeight)
    {
        if (pLabel && widthOrWeight >= 0.0f)
        {
            const ImGuiTableColumnFlags flags = sortable
                ? ImGuiTableColumnFlags_None
                : ImGuiTableColumnFlags_NoSort;
            ImGui::TableSetupColumn(pLabel, flags, widthOrWeight);
        }
    };
    pCtx->pfnUiGetWindowInteractionState = [](bool* pFocused, bool* pHovered)
    {
        if (!pFocused || !pHovered)
        {
            return;
        }
        *pFocused = ImGui::IsWindowFocused();
        *pHovered = ImGui::IsWindowHovered();
    };
    pCtx->pfnUiSetKeyboardFocusHere = [](int offset)
    {
        ImGui::SetKeyboardFocusHere(offset);
    };
    pCtx->pfnUiSetItemDefaultFocus = []()
    {
        ImGui::SetItemDefaultFocus();
    };
    pCtx->pfnUiIsItemToggledOpen = []()
    {
        return ImGui::IsItemToggledOpen();
    };
    pCtx->pfnUiGetAnyItemState = [](bool* pHovered, bool* pActive,
                                    bool* pFocused)
    {
        if (!pHovered || !pActive || !pFocused)
        {
            return;
        }
        *pHovered = ImGui::IsAnyItemHovered();
        *pActive = ImGui::IsAnyItemActive();
        *pFocused = ImGui::IsAnyItemFocused();
    };
    pCtx->pfnUiIsRectVisible = [](float width, float height)
    {
        return width >= 0.0f && height >= 0.0f &&
            ImGui::IsRectVisible(ImVec2(width, height));
    };
    pCtx->pfnUiCalcTextSize = [](const char* pText, float wrapWidth,
                                 float* pWidth, float* pHeight)
    {
        if (!pText || !pWidth || !pHeight)
        {
            return;
        }
        const ImVec2 size = ImGui::CalcTextSize(pText, nullptr, false, wrapWidth);
        *pWidth = size.x;
        *pHeight = size.y;
    };
    pCtx->pfnUiColorRgbToHsv = [](float red, float green, float blue,
                                  float* pHue, float* pSaturation, float* pValue)
    {
        if (!pHue || !pSaturation || !pValue)
        {
            return;
        }
        ImGui::ColorConvertRGBtoHSV(red, green, blue, *pHue, *pSaturation, *pValue);
    };
    pCtx->pfnUiColorHsvToRgb = [](float hue, float saturation, float value,
                                  float* pRed, float* pGreen, float* pBlue)
    {
        if (!pRed || !pGreen || !pBlue)
        {
            return;
        }
        ImGui::ColorConvertHSVtoRGB(hue, saturation, value, *pRed, *pGreen, *pBlue);
    };
    pCtx->pfnUiGetFrameInfo = [](int* pFrameCount, double* pTimeSeconds)
    {
        if (!pFrameCount || !pTimeSeconds)
        {
            return;
        }
        *pFrameCount = ImGui::GetFrameCount();
        *pTimeSeconds = ImGui::GetTime();
    };
    pCtx->pfnUiGetKeyState = [](EPluginUiKey key, bool* pDown,
                                bool* pPressed, bool* pReleased)
    {
        ImGuiKey imguiKey;
        if (!pDown || !pPressed || !pReleased ||
            !TryGetImGuiKey(key, &imguiKey))
        {
            return;
        }
        *pDown = ImGui::IsKeyDown(imguiKey);
        *pPressed = ImGui::IsKeyPressed(imguiKey, false);
        *pReleased = ImGui::IsKeyReleased(imguiKey);
    };
    pCtx->pfnUiGetMouseButtonState = [](EPluginUiMouseButton button,
                                        bool* pDown, bool* pClicked,
                                        bool* pReleased)
    {
        ImGuiMouseButton imguiButton;
        if (!pDown || !pClicked || !pReleased ||
            !TryGetImGuiMouseButton(button, &imguiButton))
        {
            return;
        }
        *pDown = ImGui::IsMouseDown(imguiButton);
        *pClicked = ImGui::IsMouseClicked(imguiButton);
        *pReleased = ImGui::IsMouseReleased(imguiButton);
    };
    pCtx->pfnUiGetMousePosition = [](float* pX, float* pY, bool* pValid)
    {
        if (!pX || !pY || !pValid)
        {
            return;
        }
        const ImVec2 position = ImGui::GetMousePos();
        *pX = position.x;
        *pY = position.y;
        *pValid = ImGui::IsMousePosValid(&position);
    };
    pCtx->pfnUiPushClipRect = [](float minX, float minY, float maxX, float maxY,
                                 bool intersectCurrent)
    {
        if (minX <= maxX && minY <= maxY)
        {
            ImGui::PushClipRect(ImVec2(minX, minY), ImVec2(maxX, maxY),
                                intersectCurrent);
        }
    };
    pCtx->pfnUiPopClipRect = []()
    {
        ImGui::PopClipRect();
    };
    pCtx->pfnUiSetNextItemAllowOverlap = []()
    {
        ImGui::SetNextItemAllowOverlap();
    };
    pCtx->pfnUiGetLegacyColumnCount = []()
    {
        return ImGui::GetColumnsCount();
    };
    pCtx->pfnUiColumns = [](int count, const char* pId, bool borders)
    {
        if (count >= 0)
        {
            ImGui::Columns(count, pId, borders);
        }
    };
    pCtx->pfnUiNextColumn = []()
    {
        ImGui::NextColumn();
    };
    pCtx->pfnUiLegacyColumnGetSet = [](int columnIndex, float* pWidth,
                                       float* pOffset, bool setWidth, float width,
                                       bool setOffset, float offset)
    {
        if (pWidth)
        {
            *pWidth = ImGui::GetColumnWidth(columnIndex);
        }
        if (pOffset)
        {
            *pOffset = ImGui::GetColumnOffset(columnIndex);
        }
        if (setWidth && width >= 0.0f)
        {
            ImGui::SetColumnWidth(columnIndex, width);
        }
        if (setOffset && offset >= 0.0f)
        {
            ImGui::SetColumnOffset(columnIndex, offset);
        }
    };
    pCtx->pfnUiTabItemButton = [](const char* pLabel)
    {
        return pLabel && ImGui::TabItemButton(pLabel);
    };
    pCtx->pfnUiSetTabItemClosed = [](const char* pLabel)
    {
        if (pLabel)
        {
            ImGui::SetTabItemClosed(pLabel);
        }
    };
    pCtx->pfnUiLogText = [](const char* pText)
    {
        if (pText)
        {
            ImGui::LogText("%s", pText);
        }
    };
    pCtx->pfnUiLogToOutput = [](int autoOpenDepth)
    {
        ImGui::LogToTTY(autoOpenDepth);
    };
    pCtx->pfnUiLogFinish = []()
    {
        ImGui::LogFinish();
    };
    pCtx->pfnUiSetNextWindowPosEx = [](float x, float y, float pivotX,
                                       float pivotY, EPluginUiCondition condition)
    {
        ImGuiCond imguiCondition;
        if (TryGetImGuiCondition(condition, &imguiCondition) &&
            pivotX >= 0.0f && pivotX <= 1.0f &&
            pivotY >= 0.0f && pivotY <= 1.0f)
        {
            ImGui::SetNextWindowPos(ImVec2(x, y), imguiCondition,
                                    ImVec2(pivotX, pivotY));
        }
    };
    pCtx->pfnUiSetNextWindowSizeEx = [](float width, float height,
                                        EPluginUiCondition condition)
    {
        ImGuiCond imguiCondition;
        if (TryGetImGuiCondition(condition, &imguiCondition) &&
            width >= 0.0f && height >= 0.0f)
        {
            ImGui::SetNextWindowSize(ImVec2(width, height), imguiCondition);
        }
    };
    pCtx->pfnUiSetNextWindowCollapsedEx = [](bool collapsed,
                                             EPluginUiCondition condition)
    {
        ImGuiCond imguiCondition;
        if (TryGetImGuiCondition(condition, &imguiCondition))
        {
            ImGui::SetNextWindowCollapsed(collapsed, imguiCondition);
        }
    };
    pCtx->pfnUiGetWindowDpiScale = []()
    {
        return ImGui::GetWindowDpiScale();
    };
    pCtx->pfnUiGetWindowRectDetailed = [](float* pX, float* pY,
                                          float* pWidth, float* pHeight)
    {
        if (!pX || !pY || !pWidth || !pHeight)
        {
            return;
        }
        const ImVec2 position = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        *pX = position.x;
        *pY = position.y;
        *pWidth = size.x;
        *pHeight = size.y;
    };
    pCtx->pfnUiTextLink = [](const char* pLabel)
    {
        return pLabel && ImGui::TextLink(pLabel);
    };
    pCtx->pfnUiBeginItemTooltip = []()
    {
        return ImGui::BeginItemTooltip();
    };
    pCtx->pfnUiSetItemTooltip = [](const char* pText)
    {
        if (pText)
        {
            ImGui::SetItemTooltip("%s", pText);
        }
    };
    pCtx->pfnUiPlotLinesEx = [](const char* pLabel, const float* pValues,
                                int valueCount, int valueOffset,
                                const char* pOverlay, float minimum, float maximum,
                                float width, float height)
    {
        if (pLabel && pValues && valueCount > 0 && height > 0.0f &&
            width >= 0.0f && valueOffset >= 0 && valueOffset < valueCount &&
            minimum <= maximum)
        {
            ImGui::PlotLines(pLabel, pValues, valueCount, valueOffset, pOverlay,
                             minimum, maximum, ImVec2(width, height));
        }
    };
    pCtx->pfnUiPlotHistogramEx = [](const char* pLabel, const float* pValues,
                                    int valueCount, int valueOffset,
                                    const char* pOverlay, float minimum,
                                    float maximum, float width, float height)
    {
        if (pLabel && pValues && valueCount > 0 && height > 0.0f &&
            width >= 0.0f && valueOffset >= 0 && valueOffset < valueCount &&
            minimum <= maximum)
        {
            ImGui::PlotHistogram(pLabel, pValues, valueCount, valueOffset,
                                 pOverlay, minimum, maximum,
                                 ImVec2(width, height));
        }
    };
    pCtx->pfnUiGetMouseButtonDetails = [](EPluginUiMouseButton button,
                                          bool* pDoubleClicked, int* pClickCount,
                                          bool* pDragging, float dragThreshold)
    {
        ImGuiMouseButton imguiButton;
        if (!pDoubleClicked || !pClickCount || !pDragging ||
            !TryGetImGuiMouseButton(button, &imguiButton))
        {
            return;
        }
        *pDoubleClicked = ImGui::IsMouseDoubleClicked(imguiButton);
        *pClickCount = ImGui::GetMouseClickedCount(imguiButton);
        *pDragging = ImGui::IsMouseDragging(imguiButton, dragThreshold);
    };
    pCtx->pfnUiIsMouseHoveringRect = [](float minX, float minY,
                                        float maxX, float maxY, bool clipToUi)
    {
        return minX <= maxX && minY <= maxY &&
            ImGui::IsMouseHoveringRect(ImVec2(minX, minY), ImVec2(maxX, maxY),
                                       clipToUi);
    };
    pCtx->pfnUiGetPopupOpeningMousePosition = [](float* pX, float* pY)
    {
        if (!pX || !pY)
        {
            return;
        }
        const ImVec2 position = ImGui::GetMousePosOnOpeningCurrentPopup();
        *pX = position.x;
        *pY = position.y;
    };
    pCtx->pfnUiIsAnyMouseButtonDown = []()
    {
        return ImGui::IsAnyMouseDown();
    };
    pCtx->pfnUiGetKeyPressedAmount = [](EPluginUiKey key, float repeatDelay,
                                        float repeatRate)
    {
        ImGuiKey imguiKey;
        if (!TryGetImGuiKey(key, &imguiKey) || repeatDelay < 0.0f ||
            repeatRate <= 0.0f)
        {
            return 0;
        }
        return ImGui::GetKeyPressedAmount(imguiKey, repeatDelay, repeatRate);
    };
    pCtx->pfnUiGetKeyName = [](EPluginUiKey key, char* pBuffer,
                               size_t bufferSize)
    {
        ImGuiKey imguiKey;
        if (!pBuffer || bufferSize == 0 || !TryGetImGuiKey(key, &imguiKey))
        {
            return false;
        }
        const char* pName = ImGui::GetKeyName(imguiKey);
        if (!pName)
        {
            return false;
        }
        const size_t nameSize = std::strlen(pName);
        if (nameSize >= bufferSize)
        {
            return false;
        }
        std::memcpy(pBuffer, pName, nameSize + 1);
        return true;
    };
    pCtx->pfnUiShortcut = [](EPluginUiKey key, bool control, bool shift,
                             bool alt, bool super, bool repeat)
    {
        ImGuiKey imguiKey;
        if (!TryGetImGuiKey(key, &imguiKey))
        {
            return false;
        }
        ImGuiKeyChord chord = imguiKey;
        if (control)
        {
            chord |= ImGuiMod_Ctrl;
        }
        if (shift)
        {
            chord |= ImGuiMod_Shift;
        }
        if (alt)
        {
            chord |= ImGuiMod_Alt;
        }
        if (super)
        {
            chord |= ImGuiMod_Super;
        }
        const ImGuiInputFlags flags = repeat ? ImGuiInputFlags_Repeat
                                             : ImGuiInputFlags_None;
        return ImGui::Shortcut(chord, flags);
    };
    pCtx->pfnUiSetNavigationCursorVisible = [](bool visible)
    {
        ImGui::SetNavCursorVisible(visible);
    };
    pCtx->pfnUiDockSpace = [](const char* pId, float width, float height)
    {
        if (!pId || width < 0.0f || height < 0.0f)
        {
            return 0u;
        }
        return ImGui::DockSpace(ImGui::GetID(pId), ImVec2(width, height));
    };
    pCtx->pfnUiSetNextWindowDockId = [](unsigned int dockId,
                                        EPluginUiCondition condition)
    {
        ImGuiCond imguiCondition;
        if (dockId != 0 && TryGetImGuiCondition(condition, &imguiCondition))
        {
            ImGui::SetNextWindowDockID(dockId, imguiCondition);
        }
    };
    pCtx->pfnUiGetWindowDockState = [](unsigned int* pDockId, bool* pDocked)
    {
        if (!pDockId || !pDocked)
        {
            return;
        }
        *pDockId = ImGui::GetWindowDockID();
        *pDocked = ImGui::IsWindowDocked();
    };
    pCtx->pfnUiBeginMainMenuBar = []()
    {
        return ImGui::BeginMainMenuBar();
    };
    pCtx->pfnUiEndMainMenuBar = []()
    {
        ImGui::EndMainMenuBar();
    };
    pCtx->pfnUiBullet = []()
    {
        ImGui::Bullet();
    };
    pCtx->pfnUiValueBool = [](const char* pLabel, bool value)
    {
        if (pLabel)
        {
            ImGui::Value(pLabel, value);
        }
    };
    pCtx->pfnUiValueInt = [](const char* pLabel, int value)
    {
        if (pLabel)
        {
            ImGui::Value(pLabel, value);
        }
    };
    pCtx->pfnUiValueUInt = [](const char* pLabel, unsigned int value)
    {
        if (pLabel)
        {
            ImGui::Value(pLabel, value);
        }
    };
    pCtx->pfnUiValueFloat = [](const char* pLabel, float value,
                               const char* pFormat)
    {
        if (pLabel)
        {
            ImGui::Value(pLabel, value, pFormat);
        }
    };
    pCtx->pfnUiPushFontSize = [](float sizeBaseUnscaled)
    {
        if (sizeBaseUnscaled > 0.0f)
        {
            ImGui::PushFont(nullptr, sizeBaseUnscaled);
        }
    };
    pCtx->pfnUiPopFont = []()
    {
        ImGui::PopFont();
    };
    pCtx->pfnUiGetFontSize = []()
    {
        return ImGui::GetFontSize();
    };
    pCtx->pfnUiPushItemWidth = [](float width)
    {
        ImGui::PushItemWidth(width);
    };
    pCtx->pfnUiPopItemWidth = []()
    {
        ImGui::PopItemWidth();
    };
    pCtx->pfnUiCalcItemWidth = []()
    {
        return ImGui::CalcItemWidth();
    };
    pCtx->pfnUiPushTextWrapPos = [](float localPositionX)
    {
        ImGui::PushTextWrapPos(localPositionX);
    };
    pCtx->pfnUiPopTextWrapPos = []()
    {
        ImGui::PopTextWrapPos();
    };
    pCtx->pfnUiImageWithBg = [](CAssetLibrary* pAssets, int textureIndex,
                                float width, float height,
                                float bgRed, float bgGreen, float bgBlue, float bgAlpha,
                                float tintRed, float tintGreen,
                                float tintBlue, float tintAlpha)
    {
        if (!pAssets || textureIndex < 0 || width <= 0.0f || height <= 0.0f ||
            static_cast<size_t>(textureIndex) >= pAssets->TextureCount())
        {
            return false;
        }
        const Texture2D& texture =
            pAssets->TextureByIndex(static_cast<size_t>(textureIndex)).Texture;
        const ImTextureID textureId = QcImGuiGetTextureId(&texture);
        if (textureId == ImTextureID_Invalid)
        {
            return false;
        }
        ImGui::ImageWithBg(ImTextureRef(textureId), ImVec2(width, height),
                           ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                           ImVec4(bgRed, bgGreen, bgBlue, bgAlpha),
                           ImVec4(tintRed, tintGreen, tintBlue, tintAlpha));
        return true;
    };
    pCtx->pfnUiGetMouseDragDelta = [](EPluginUiMouseButton button,
                                      float lockThreshold,
                                      float* pDeltaX, float* pDeltaY)
    {
        ImGuiMouseButton imguiButton;
        if (!pDeltaX || !pDeltaY ||
            !TryGetImGuiMouseButton(button, &imguiButton))
        {
            return;
        }
        const ImVec2 delta = ImGui::GetMouseDragDelta(imguiButton, lockThreshold);
        *pDeltaX = delta.x;
        *pDeltaY = delta.y;
    };
    pCtx->pfnUiResetMouseDragDelta = [](EPluginUiMouseButton button)
    {
        ImGuiMouseButton imguiButton;
        if (TryGetImGuiMouseButton(button, &imguiButton))
        {
            ImGui::ResetMouseDragDelta(imguiButton);
        }
    };
    pCtx->pfnUiSetMouseCursor = [](EPluginUiMouseCursor cursor)
    {
        ImGuiMouseCursor imguiCursor;
        switch (cursor)
        {
        case PLUGIN_UI_MOUSE_CURSOR_ARROW: imguiCursor = ImGuiMouseCursor_Arrow; break;
        case PLUGIN_UI_MOUSE_CURSOR_TEXT: imguiCursor = ImGuiMouseCursor_TextInput; break;
        case PLUGIN_UI_MOUSE_CURSOR_RESIZE_ALL: imguiCursor = ImGuiMouseCursor_ResizeAll; break;
        case PLUGIN_UI_MOUSE_CURSOR_RESIZE_VERTICAL: imguiCursor = ImGuiMouseCursor_ResizeNS; break;
        case PLUGIN_UI_MOUSE_CURSOR_RESIZE_HORIZONTAL: imguiCursor = ImGuiMouseCursor_ResizeEW; break;
        case PLUGIN_UI_MOUSE_CURSOR_RESIZE_DIAGONAL_NW_SE: imguiCursor = ImGuiMouseCursor_ResizeNWSE; break;
        case PLUGIN_UI_MOUSE_CURSOR_RESIZE_DIAGONAL_NE_SW: imguiCursor = ImGuiMouseCursor_ResizeNESW; break;
        case PLUGIN_UI_MOUSE_CURSOR_HAND: imguiCursor = ImGuiMouseCursor_Hand; break;
        case PLUGIN_UI_MOUSE_CURSOR_NOT_ALLOWED: imguiCursor = ImGuiMouseCursor_NotAllowed; break;
        default: return;
        }
        ImGui::SetMouseCursor(imguiCursor);
    };
    pCtx->pfnUiDockSpaceOverMainViewport = []()
    {
        return ImGui::DockSpaceOverViewport();
    };
    pCtx->pfnUiShowMetricsWindow = [](bool* pOpen)
    {
        ImGui::ShowMetricsWindow(pOpen);
    };
    pCtx->pfnUiShowDebugLogWindow = [](bool* pOpen)
    {
        ImGui::ShowDebugLogWindow(pOpen);
    };
    pCtx->pfnUiShowIdStackToolWindow = [](bool* pOpen)
    {
        ImGui::ShowIDStackToolWindow(pOpen);
    };
    pCtx->pfnUiSetBuiltinStyle = [](EPluginUiBuiltinStyle style)
    {
        switch (style)
        {
        case PLUGIN_UI_BUILTIN_STYLE_DARK: ImGui::StyleColorsDark(); break;
        case PLUGIN_UI_BUILTIN_STYLE_LIGHT: ImGui::StyleColorsLight(); break;
        case PLUGIN_UI_BUILTIN_STYLE_CLASSIC: ImGui::StyleColorsClassic(); break;
        default: break;
        }
    };
    pCtx->pfnUiShowFontSelector = [](const char* pLabel)
    {
        if (pLabel)
        {
            ImGui::ShowFontSelector(pLabel);
        }
    };
    pCtx->pfnUiGetVersion = [](char* pBuffer, size_t bufferSize)
    {
        if (!pBuffer || bufferSize == 0)
        {
            return false;
        }
        const char* pVersion = ImGui::GetVersion();
        const size_t versionSize = std::strlen(pVersion);
        if (versionSize >= bufferSize)
        {
            return false;
        }
        std::memcpy(pBuffer, pVersion, versionSize + 1);
        return true;
    };
    pCtx->pfnUiDragScalar = [](const char* pLabel, EPluginUiScalarType type,
                               void* pValues, int componentCount, float speed,
                               const void* pMinimum, const void* pMaximum,
                               const char* pFormat)
    {
        ImGuiDataType imguiDataType;
        if (!pLabel || !pValues || componentCount <= 0 ||
            !TryGetImGuiDataType(type, &imguiDataType))
        {
            return false;
        }
        if (componentCount == 1)
        {
            return ImGui::DragScalar(pLabel, imguiDataType, pValues, speed,
                                     pMinimum, pMaximum, pFormat);
        }
        return ImGui::DragScalarN(pLabel, imguiDataType, pValues,
                                  componentCount, speed, pMinimum, pMaximum,
                                  pFormat);
    };
    pCtx->pfnUiSliderScalar = [](const char* pLabel, EPluginUiScalarType type,
                                 void* pValues, int componentCount,
                                 const void* pMinimum, const void* pMaximum,
                                 const char* pFormat)
    {
        ImGuiDataType imguiDataType;
        if (!pLabel || !pValues || componentCount <= 0 || !pMinimum ||
            !pMaximum || !TryGetImGuiDataType(type, &imguiDataType))
        {
            return false;
        }
        if (componentCount == 1)
        {
            return ImGui::SliderScalar(pLabel, imguiDataType, pValues,
                                       pMinimum, pMaximum, pFormat);
        }
        return ImGui::SliderScalarN(pLabel, imguiDataType, pValues,
                                    componentCount, pMinimum, pMaximum, pFormat);
    };
    pCtx->pfnUiInputScalar = [](const char* pLabel, EPluginUiScalarType type,
                                void* pValues, int componentCount,
                                const void* pStep, const void* pStepFast,
                                const char* pFormat)
    {
        ImGuiDataType imguiDataType;
        if (!pLabel || !pValues || componentCount <= 0 ||
            !TryGetImGuiDataType(type, &imguiDataType))
        {
            return false;
        }
        if (componentCount == 1)
        {
            return ImGui::InputScalar(pLabel, imguiDataType, pValues,
                                      pStep, pStepFast, pFormat);
        }
        return ImGui::InputScalarN(pLabel, imguiDataType, pValues,
                                   componentCount, pStep, pStepFast, pFormat);
    };
    pCtx->pfnUiVSliderScalar = [](const char* pLabel, EPluginUiScalarType type,
                                  void* pValue, float width, float height,
                                  const void* pMinimum, const void* pMaximum,
                                  const char* pFormat)
    {
        ImGuiDataType imguiDataType;
        return pLabel && pValue && width > 0.0f && height > 0.0f &&
            pMinimum && pMaximum &&
            TryGetImGuiDataType(type, &imguiDataType) &&
            ImGui::VSliderScalar(pLabel, ImVec2(width, height), imguiDataType,
                                 pValue, pMinimum, pMaximum, pFormat);
    };
    pCtx->pfnUiColorPackRgba = [](float red, float green, float blue, float alpha)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(red, green, blue, alpha));
    };
    pCtx->pfnUiColorUnpackRgba = [](unsigned int packedColor,
                                    float* pRed, float* pGreen,
                                    float* pBlue, float* pAlpha)
    {
        if (!pRed || !pGreen || !pBlue || !pAlpha)
        {
            return;
        }
        const ImVec4 color = ImGui::ColorConvertU32ToFloat4(packedColor);
        *pRed = color.x;
        *pGreen = color.y;
        *pBlue = color.z;
        *pAlpha = color.w;
    };
}

bool RegisterComponentFactoryForPlugins(SPluginContext* pCtx, const char* pTypeName, FPluginComponentFactory create)
{
    if (!pCtx || !pCtx->pComponentRegistry || !pTypeName || !create)
    {
        return false;
    }
    return pCtx->pComponentRegistry->Register(pTypeName, [create]() -> std::shared_ptr<IComponent>
    {
        IComponent* pComponent = static_cast<IComponent*>(create());
        return pComponent ? std::shared_ptr<IComponent>(pComponent) : nullptr;
    });
}

void UnregisterComponentFactoryForPlugins(SPluginContext* pCtx, const char* pTypeName)
{
    if (pCtx && pCtx->pComponentRegistry && pTypeName)
    {
        pCtx->pComponentRegistry->Unregister(pTypeName);
    }
}

void RegisterUiCallbackForPlugins(SPluginContext* pCtx, EUIRegion region, FPluginUICallback pfnCallback)
{
    if (pCtx && pCtx->pPluginManager && pfnCallback)
    {
        pCtx->pPluginManager->RegisterUiCallback(region, pfnCallback);
    }
}

const char* EntityGetName(CScene* pScene, int index)
{
    return pScene->m_vEntities[index].m_Name.c_str();
}

void EntityGetPosition(CScene* pScene, int index, float* pX, float* pY, float* pZ)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent())
    {
        *pX = pTransform->m_Position.x; *pY = pTransform->m_Position.y; *pZ = pTransform->m_Position.z;
    }
}

void EntityGetRotation(CScene* pScene, int index, float* pX, float* pY, float* pZ)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent())
    {
        *pX = pTransform->m_Rotation.x; *pY = pTransform->m_Rotation.y; *pZ = pTransform->m_Rotation.z;
    }
}

void EntityGetScale(CScene* pScene, int index, float* pX, float* pY, float* pZ)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent())
    {
        *pX = pTransform->m_Scale.x; *pY = pTransform->m_Scale.y; *pZ = pTransform->m_Scale.z;
    }
}

void EntityGetColor(CScene* pScene, int index, unsigned char* pR, unsigned char* pG,
    unsigned char* pB, unsigned char* pA)
{
    if (auto* pMaterial = pScene->m_vEntities[index].GetMaterialComponent())
    {
        *pR = pMaterial->m_Color.r; *pG = pMaterial->m_Color.g;
        *pB = pMaterial->m_Color.b; *pA = pMaterial->m_Color.a;
    }
}

void EntitySetPosition(CScene* pScene, int index, float x, float y, float z)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent())
    {
        pTransform->ClearLocalMatrixOverride();
        pTransform->m_Position = Vec3(x, y, z);
        DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED, index);
    }
}

void EntitySetRotation(CScene* pScene, int index, float x, float y, float z)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent())
    {
        pTransform->ClearLocalMatrixOverride();
        pTransform->m_Rotation = Vec3(x, y, z);
        DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED, index);
    }
}

void EntitySetScale(CScene* pScene, int index, float x, float y, float z)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent())
    {
        pTransform->ClearLocalMatrixOverride();
        pTransform->m_Scale = Vec3(x, y, z);
        DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED, index);
    }
}

void EntitySetColor(CScene* pScene, int index, unsigned char r, unsigned char g,
    unsigned char b, unsigned char a)
{
    if (auto* pMaterial = pScene->m_vEntities[index].GetMaterialComponent()) pMaterial->m_Color = {r, g, b, a};
}

void EntitySetName(CScene* pScene, int index, const char* pName)
{
    pScene->m_vEntities[index].m_Name = pName;
}

const char* EntityGetAssetName(CScene* pScene, int index)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return nullptr;
    }
    const CMeshComponent* pMesh = pScene->m_vEntities[index].GetMeshComponent();
    if (!pMesh || pMesh->m_AssetName.empty())
    {
        return nullptr;
    }
    return pMesh->m_AssetName.c_str();
}

void RegisterEventCallbackForPlugins(SPluginContext* pCtx, EPluginEvent event,
                                     FPluginEventCallback pfnCallback)
{
    if (pCtx && pCtx->pPluginManager)
    {
        pCtx->pPluginManager->RegisterEventCallback(event, pfnCallback);
    }
}

void UnregisterEventCallbackForPlugins(SPluginContext* pCtx, EPluginEvent event,
                                       FPluginEventCallback pfnCallback)
{
    if (pCtx && pCtx->pPluginManager)
    {
        pCtx->pPluginManager->UnregisterEventCallback(event, pfnCallback);
    }
}

int EntityGetParent(CScene* pScene, int index)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return -1;
    }
    return pScene->m_vEntities[index].m_ParentId;
}

bool EntitySetParent(CScene* pScene, int index, int parentIndex)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()) ||
        parentIndex < -1 || parentIndex >= static_cast<int>(pScene->m_vEntities.size()) ||
        parentIndex == index)
    {
        return false;
    }

    const int previousParent = pScene->m_vEntities[index].m_ParentId;
    MoveEntityToParent(*pScene, index, parentIndex);
    return pScene->m_vEntities[index].m_ParentId != previousParent;
}

IComponent* FindEntityComponent(CScene* pScene, int index, const char* pTypeName)
{
    if (!pScene || !pTypeName || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return nullptr;
    }

    CComponentManager* pComponents = pScene->m_vEntities[index].GetComponents();
    if (!pComponents)
    {
        return nullptr;
    }

    for (const std::shared_ptr<IComponent>& pComponent : pComponents->GetAllComponents())
    {
        if (pComponent && pComponent->GetTypeName() == pTypeName)
        {
            return pComponent.get();
        }
    }
    return nullptr;
}

int EntityGetComponentCount(CScene* pScene, int index)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return 0;
    }
    CComponentManager* pComponents = pScene->m_vEntities[index].GetComponents();
    return pComponents ? static_cast<int>(pComponents->GetComponentCount()) : 0;
}

const char* EntityGetComponentType(CScene* pScene, int index, int componentIndex)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()) ||
        componentIndex < 0)
    {
        return nullptr;
    }

    CComponentManager* pComponents = pScene->m_vEntities[index].GetComponents();
    if (!pComponents || componentIndex >= static_cast<int>(pComponents->GetComponentCount()))
    {
        return nullptr;
    }

    const std::shared_ptr<IComponent> pComponent = pComponents->GetComponent(componentIndex);
    if (!pComponent)
    {
        return nullptr;
    }

    static thread_local std::string s_TypeName;
    s_TypeName = pComponent->GetTypeName();
    return s_TypeName.c_str();
}

bool EntityHasComponent(CScene* pScene, int index, const char* pTypeName)
{
    return FindEntityComponent(pScene, index, pTypeName) != nullptr;
}

bool EntityAddComponent(CScene* pScene, int index, const char* pTypeName)
{
    if (!pScene || !pScene->m_pComponentRegistry || !pTypeName ||
        FindEntityComponent(pScene, index, pTypeName))
    {
        return false;
    }

    if (index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return false;
    }

    std::shared_ptr<IComponent> pComponent = pScene->m_pComponentRegistry->Create(pTypeName);
    if (!pComponent)
    {
        return false;
    }

    CComponentManager* pComponents = pScene->m_vEntities[index].GetComponents();
    if (!pComponents)
    {
        return false;
    }
    pComponents->AddComponent(std::move(pComponent));
    return true;
}

bool EntityRemoveComponent(CScene* pScene, int index, const char* pTypeName)
{
    if (!pScene || !pTypeName || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return false;
    }

    CComponentManager* pComponents = pScene->m_vEntities[index].GetComponents();
    if (!pComponents)
    {
        return false;
    }

    for (size_t componentIndex = 0; componentIndex < pComponents->GetComponentCount(); ++componentIndex)
    {
        const std::shared_ptr<IComponent> pComponent = pComponents->GetComponent(componentIndex);
        if (pComponent && pComponent->GetTypeName() == pTypeName)
        {
            pComponents->RemoveComponent(componentIndex);
            return true;
        }
    }
    return false;
}

bool EntitySetComponentEnabled(CScene* pScene, int index, const char* pTypeName, bool enabled)
{
    IComponent* pComponent = FindEntityComponent(pScene, index, pTypeName);
    if (!pComponent)
    {
        return false;
    }
    pComponent->m_Enabled = enabled;
    return true;
}

int EntityGetTagCount(CScene* pScene, int index)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return 0;
    }
    return static_cast<int>(pScene->m_vEntities[index].m_vTags.size());
}

const char* EntityGetTag(CScene* pScene, int index, int tagIndex)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()) ||
        tagIndex < 0 || tagIndex >= static_cast<int>(pScene->m_vEntities[index].m_vTags.size()))
    {
        return nullptr;
    }
    return pScene->m_vEntities[index].m_vTags[tagIndex].c_str();
}

bool EntityHasTag(CScene* pScene, int index, const char* pTag)
{
    if (!pScene || !pTag || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return false;
    }

    const auto& vTags = pScene->m_vEntities[index].m_vTags;
    return std::find(vTags.begin(), vTags.end(), pTag) != vTags.end();
}

bool EntityAddTag(CScene* pScene, int index, const char* pTag)
{
    if (!pScene || !pTag || pTag[0] == '\0' ||
        index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()) ||
        EntityHasTag(pScene, index, pTag))
    {
        return false;
    }

    pScene->m_vEntities[index].m_vTags.emplace_back(pTag);
    return true;
}

bool EntityRemoveTag(CScene* pScene, int index, const char* pTag)
{
    if (!pScene || !pTag || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return false;
    }

    auto& vTags = pScene->m_vEntities[index].m_vTags;
    const auto found = std::find(vTags.begin(), vTags.end(), pTag);
    if (found == vTags.end())
    {
        return false;
    }

    vTags.erase(found);
    return true;
}

void SceneSave(const char* pProjectPath, CScene* pScene)
{
    if (!pProjectPath || !pScene)
    {
        return;
    }
    CProjectService::Save(pProjectPath, *pScene);
    DispatchPluginEvent(PLUGIN_EVENT_SCENE_SAVED);
}

int AssetGetCount(CAssetLibrary* pAssets)
{
    return pAssets ? static_cast<int>(pAssets->ModelCount()) : 0;
}

const char* AssetGetName(CAssetLibrary* pAssets, int index)
{
    if (!pAssets || index < 0 || index >= static_cast<int>(pAssets->ModelCount()))
    {
        return nullptr;
    }
    return pAssets->Models()[index].m_Name.c_str();
}

int AssetGetType(CAssetLibrary* pAssets, int index)
{
    if (!pAssets || index < 0 || index >= static_cast<int>(pAssets->ModelCount()))
    {
        return -1;
    }
    return static_cast<int>(pAssets->Models()[index].m_Type);
}

bool AssetExists(CAssetLibrary* pAssets, const char* pName)
{
    return pAssets && pName && pAssets->FindModelByName(pName) != nullptr;
}

int SceneSpawn(CAssetLibrary* pAssets, CScene* pScene, const char* pAssetName)
{
    if (!pAssets || !pScene || !pAssetName)
    {
        return -1;
    }

    for (auto& a : pAssets->Models())
    {
        if (a.m_Name == pAssetName)
        {
            CEntity e = CEntityFactory::FromAsset(*pScene, a);
            const CMeshComponent* pMesh = e.GetMeshComponent();
            if (!pMesh || !pMesh->m_Model.meshCount)
            {
                return -1;
            }
            pScene->m_vEntities.push_back(std::move(e));
            const int entityIndex = static_cast<int>(pScene->m_vEntities.size()) - 1;
            DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, entityIndex);
            return entityIndex;
        }
    }
    return -1;
}

int SceneSpawnEx(CAssetLibrary* pAssets, CScene* pScene, const char* pAssetName,
                float x, float y, float z)
{
    const int entityIndex = SceneSpawn(pAssets, pScene, pAssetName);
    if (entityIndex < 0)
    {
        return -1;
    }

    if (CTransformComponent* pTransform = pScene->m_vEntities[entityIndex].GetTransformComponent())
    {
        pTransform->ClearLocalMatrixOverride();
        pTransform->m_Position = Vec3(x, y, z);
        DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED, entityIndex);
    }
    return entityIndex;
}

void SceneDelete(CScene* pScene, int index)
{
    if (index < 0 || index >= (int)pScene->m_vEntities.size())
    {
        return;
    }
    DispatchPluginEvent(PLUGIN_EVENT_ENTITY_DELETED, index);
    pScene->m_vEntities.erase(pScene->m_vEntities.begin() + index);
    if (pScene->m_Selected >= (int)pScene->m_vEntities.size())
    {
        pScene->m_Selected = -1;
    }
}

int SceneGetSelected(CScene* pScene)
{
    return pScene ? pScene->m_Selected : -1;
}

void SceneSetSelected(CScene* pScene, int index, bool additive)
{
    if (!pScene)
    {
        return;
    }
    if (index == -1 && !additive)
    {
        pScene->m_Selected = -1;
        pScene->m_vSelectedEntities.clear();
        DispatchPluginEvent(PLUGIN_EVENT_ENTITY_SELECTED, -1);
        return;
    }
    if (index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return;
    }
    pScene->SelectEntity(index, additive);
    DispatchPluginEvent(PLUGIN_EVENT_ENTITY_SELECTED, index);
}

int SceneGetSelectionCount(CScene* pScene)
{
    return pScene ? static_cast<int>(pScene->m_vSelectedEntities.size()) : 0;
}

int SceneGetSelectedAt(CScene* pScene, int selectionIndex)
{
    if (!pScene || selectionIndex < 0 ||
        selectionIndex >= static_cast<int>(pScene->m_vSelectedEntities.size()))
    {
        return -1;
    }
    return pScene->m_vSelectedEntities[selectionIndex];
}

void SceneBeginCommand(CScene* pScene, const char* pDescription)
{
    if (s_pEditor && pScene == &s_pEditor->m_Scene)
    {
        s_pEditor->BeginPluginCommand(pDescription);
    }
}

void SceneEndCommand(CScene* pScene)
{
    if (s_pEditor && pScene == &s_pEditor->m_Scene)
    {
        s_pEditor->EndPluginCommand();
    }
}

bool SceneUndo()
{
    if (!s_pEditor || s_pEditor->m_UndoStack.empty())
    {
        return false;
    }
    s_pEditor->Undo();
    return true;
}

bool SceneRedo()
{
    if (!s_pEditor || s_pEditor->m_RedoStack.empty())
    {
        return false;
    }
    s_pEditor->Redo();
    return true;
}

bool SceneIsDirty(CScene* pScene)
{
    return s_pEditor && pScene == &s_pEditor->m_Scene && s_pEditor->m_SceneDirty;
}

void EditorFocusEntity(int index)
{
    if (!s_pEditor || !s_pCamera || index < 0 ||
        index >= static_cast<int>(s_pEditor->m_Scene.m_vEntities.size()))
    {
        return;
    }

    s_pEditor->m_Scene.SelectEntity(index, false);
    const Mat4 world = quark::ComposeWorld(s_pEditor->m_Scene, index);
    const Vec3 position = Vec3(world * Vec3{0.0f, 0.0f, 0.0f});
    s_pCamera->FocusOn(position);
}

void EditorSetStatusMessage(const char* pMessage)
{
    if (s_pEditor)
    {
        s_pEditor->SetStatusMessage(pMessage);
    }
}

void EditorRequestSceneRedraw()
{
    if (s_pEditor)
    {
        s_pEditor->RequestSceneRedraw();
    }
}

void EditorOpenAsset(const char* pAssetName)
{
    if (!s_pEditor || !pAssetName || pAssetName[0] == '\0')
    {
        return;
    }

    s_pEditor->m_SelectedAssetName = pAssetName;
    s_pEditor->m_SelectedAssetIndex = -1;
    if (CModelAsset* pAsset = s_pEditor->m_Assets.FindModelByName(pAssetName))
    {
        OpenModelViewerForAsset(s_pEditor->m_Ui.m_ModelViewer, *pAsset);
    }
}

void AssignEntityAndSceneCallbacks(SPluginContext* pCtx)
{
    pCtx->pfnEntityGetName     = EntityGetName;
    pCtx->pfnEntityGetPosition = EntityGetPosition;
    pCtx->pfnEntityGetRotation = EntityGetRotation;
    pCtx->pfnEntityGetScale    = EntityGetScale;
    pCtx->pfnEntityGetAssetName = EntityGetAssetName;
    pCtx->pfnEntityGetColor    = EntityGetColor;

    pCtx->pfnEntitySetPosition = EntitySetPosition;
    pCtx->pfnEntitySetRotation = EntitySetRotation;
    pCtx->pfnEntitySetScale    = EntitySetScale;
    pCtx->pfnEntitySetColor    = EntitySetColor;
    pCtx->pfnEntitySetName     = EntitySetName;
    pCtx->pfnEntityGetParent   = EntityGetParent;
    pCtx->pfnEntitySetParent   = EntitySetParent;
    pCtx->pfnEntityGetComponentCount = EntityGetComponentCount;
    pCtx->pfnEntityGetComponentType = EntityGetComponentType;
    pCtx->pfnEntityHasComponent = EntityHasComponent;
    pCtx->pfnEntityAddComponent = EntityAddComponent;
    pCtx->pfnEntityRemoveComponent = EntityRemoveComponent;
    pCtx->pfnEntitySetComponentEnabled = EntitySetComponentEnabled;
    pCtx->pfnEntityGetTagCount = EntityGetTagCount;
    pCtx->pfnEntityGetTag = EntityGetTag;
    pCtx->pfnEntityHasTag = EntityHasTag;
    pCtx->pfnEntityAddTag = EntityAddTag;
    pCtx->pfnEntityRemoveTag = EntityRemoveTag;

    pCtx->pfnSceneSave = SceneSave;
    pCtx->pfnAssetGetCount = AssetGetCount;
    pCtx->pfnAssetGetName = AssetGetName;
    pCtx->pfnAssetGetType = AssetGetType;
    pCtx->pfnAssetExists = AssetExists;
    pCtx->pfnSceneSpawn = SceneSpawn;
    pCtx->pfnSceneSpawnEx = SceneSpawnEx;
    pCtx->pfnSceneDelete = SceneDelete;
    pCtx->pfnSceneGetSelected = SceneGetSelected;
    pCtx->pfnSceneSetSelected = SceneSetSelected;
    pCtx->pfnSceneGetSelectionCount = SceneGetSelectionCount;
    pCtx->pfnSceneGetSelectedAt = SceneGetSelectedAt;
    pCtx->pfnSceneBeginCommand = SceneBeginCommand;
    pCtx->pfnSceneEndCommand = SceneEndCommand;
    pCtx->pfnSceneUndo = SceneUndo;
    pCtx->pfnSceneRedo = SceneRedo;
    pCtx->pfnSceneIsDirty = SceneIsDirty;
    pCtx->pfnEditorFocusEntity = EditorFocusEntity;
    pCtx->pfnEditorSetStatusMessage = EditorSetStatusMessage;
    pCtx->pfnEditorRequestSceneRedraw = EditorRequestSceneRedraw;
    pCtx->pfnEditorOpenAsset = EditorOpenAsset;
}

void AssignPluginHostCallbacks(SPluginContext* pCtx, CEditor& editor, CPluginManager& pluginManager)
{
    s_pEditor = &editor;
    pCtx->pfnRegisterUICallback = RegisterUiCallbackForPlugins;
    pCtx->pfnRegisterEventCallback = RegisterEventCallbackForPlugins;
    pCtx->pfnUnregisterEventCallback = UnregisterEventCallbackForPlugins;
    pCtx->pfnRegisterComponentFactory = RegisterComponentFactoryForPlugins;
    pCtx->pfnUnregisterComponentFactory = UnregisterComponentFactoryForPlugins;
    editor.m_Scene.m_pComponentRegistry = &editor.m_ComponentFactories;
    pCtx->pComponentRegistry = &editor.m_ComponentFactories;
    pCtx->pPluginManager = &pluginManager;
}
}

void DispatchPluginEvent(EPluginEvent event, int entityIndex)
{
    if (s_pEditor && s_pEditor->m_pPluginManager && s_pPluginContext)
    {
        s_pEditor->m_pPluginManager->DispatchEvent(event, *s_pPluginContext, entityIndex);
    }
}

void CPluginBridge::SyncContext(CEditor& editor, CPluginManager& pluginManager)
{
    if (!m_pContext)
    {
        return;
    }

    SPluginContext& ctx = *m_pContext;
    CScene& scene = editor.m_Scene;

    ctx.pScene       = &scene;
    ctx.entityCount  = (int)scene.m_vEntities.size();
    ctx.pSelected    = &scene.m_Selected;
    ctx.pAssets      = &editor.m_Assets;
    ctx.pProjectPath = editor.m_ProjectPath.c_str();

    m_vThreadUsageSnapshots = editor.m_CpuTaskPool.GetThreadUsageSnapshot();
    m_vPluginThreadUsage.clear();
    m_vPluginThreadUsage.reserve(m_vThreadUsageSnapshots.size());
    for (const SThreadUsageSnapshot& snapshot : m_vThreadUsageSnapshots)
    {
        m_vPluginThreadUsage.push_back({
            snapshot.name.c_str(),
            snapshot.currentTask.empty() ? nullptr : snapshot.currentTask.c_str(),
            snapshot.utilizationPercent,
            snapshot.vHistory.empty() ? nullptr : snapshot.vHistory.data(),
            static_cast<int>(snapshot.vHistory.size())
        });
    }
    ctx.pThreadUsages = m_vPluginThreadUsage.empty()
        ? nullptr
        : m_vPluginThreadUsage.data();
    ctx.threadUsageCount = static_cast<int>(m_vPluginThreadUsage.size());

    AssignUiCallbacks(&ctx);
    AssignEntityAndSceneCallbacks(&ctx);
    AssignPluginHostCallbacks(&ctx, editor, pluginManager);
}

void CPluginBridge::Initialize(CEditor& editor, CPluginManager& pluginManager)
{
    m_pContext = std::make_unique<SPluginContext>();
    s_pPluginContext = m_pContext.get();
    SyncContext(editor, pluginManager);
}

void CPluginBridge::SetCamera(CFlyCamera& camera)
{
    m_pCamera = &camera;
    s_pCamera = &camera;
}

void CPluginBridge::Update(CEditor& editor, CPluginManager& pluginManager)
{
    if (!m_pContext)
    {
        return;
    }

    m_pContext->deltaTime = GetFrameTime();
    SyncContext(editor, pluginManager);

    pluginManager.UpdateAll(*m_pContext);
    pluginManager.DrawUiAll(*m_pContext);
}

void CPluginBridge::Reset()
{
    s_pCamera = nullptr;
    s_pEditor = nullptr;
    s_pPluginContext = nullptr;
    m_pCamera = nullptr;
    m_pContext.reset();
}
