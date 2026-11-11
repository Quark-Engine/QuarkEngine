#ifndef __GUI_RENDERER_BRIDGE_H__
#define __GUI_RENDERER_BRIDGE_H__

#include "QuarkGUI/QuarkGUI.h"

class CScene;

namespace quark
{
    namespace gui
    {

        void EnsureGuiBackendInstalled();
        void ShutdownGuiBackend();
        void SetGuiScissorCamera2D(const Camera2D* pCamera);

        SGuiInputState GatherGuiInputState(Vec2 mousePosInViewport);

        void RenderSceneGui(CScene& scene, Vec2 mousePosInViewport, bool interactive);

        bool SceneHasActiveUiWidgets(const CScene& scene);
        bool IsPointOverSceneGui(CScene& scene, Vec2 mousePosInViewport);

    } // gui
} // quark

#endif // __GUI_RENDERER_BRIDGE_H__
