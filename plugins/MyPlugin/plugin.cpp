#include "plugins/plugin.h"
#include <cstdio>

void OnLoad(SPluginContext* pCtx)
{
}

void OnUnload()
{
}

void OnUpdate(SPluginContext* pCtx)
{
}

void OnDrawUI(SPluginContext* pCtx)
{
    if (pCtx->pfnUiBegin("My Plugin"))
    {
        int sel = *pCtx->m_pSelected;
        if (sel >= 0)
        {
            pCtx->pfnUiText(pCtx->pfnEntityGetName(pCtx->m_pScene, sel));
            pCtx->pfnUiSeparator();

            float x, y, z;
            pCtx->pfnEntityGetPosition(pCtx->m_pScene, sel, &x, &y, &z);
            if (pCtx->pfnUiSliderFloat("Y Position", &y, -50.0f, 50.0f))
            {
                pCtx->pfnEntitySetPosition(pCtx->m_pScene, sel, x, y, z);
            }

            float aColor[3] = { 1, 0, 0 };
            if (pCtx->pfnUiColorEdit3("Color", aColor))
            {
                pCtx->pfnEntitySetColor(pCtx->m_pScene, sel,
                                        (unsigned char)(aColor[0] * 255),
                                        (unsigned char)(aColor[1] * 255),
                                        (unsigned char)(aColor[2] * 255), 255);
            }
        }

        if (pCtx->pfnUiButton("Spawn Cube"))
        {
            pCtx->pfnSceneSpawn(pCtx->m_pAssets, pCtx->m_pScene, "Cube");
        }

        if (pCtx->pfnUiButton("Save Scene"))
        {
            pCtx->pfnSceneSave(pCtx->m_pProjectPath, pCtx->m_pScene);
        }
    }
    pCtx->pfnUiEnd();
}

static SPlugin s_Info{
    "MyPlugin", "0.1",
    OnLoad, OnUnload, OnUpdate, OnDrawUI
};

PLUGIN_EXPORT SPlugin* GetPlugin()
{
    return &s_Info;
}
