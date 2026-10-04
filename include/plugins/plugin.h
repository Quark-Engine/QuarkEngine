#ifndef __PLUGIN_H__
#define __PLUGIN_H__

/**
 * @def PLUGIN_EXPORT
 * @brief Cross-platform DLL-export / C-linkage macro.
 *
 * On Windows expands to `extern "C" __declspec(dllexport)`, which marks the
 * symbol for export from the DLL and suppresses C++ name-mangling.
 * On all other platforms expands to
 * `extern "C" __attribute__((visibility("default")))`, which enables C linkage
 * and explicitly exports the symbol from the shared library.
 *
 * Apply to every symbol that must be discovered at runtime via
 * LoadLibrary / dlopen.
 */
#ifdef _WIN32
    #define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
    #define PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

struct SPluginContext;
class CScene;
class CAssetLibrary;
class CComponentFactoryRegistry;
class CPluginManager;

/**
 * @enum EUIRegion
 * @brief Logical zones of the editor UI where plugins can inject custom UI.
 * 
 * Used to reigster callbacks that will be called only when the corresponding
 * part of the interface is being drawn.
 */
enum EUIRegion
{
    UI_MENU_FILE,
    UI_MENU_EDIT,
    UI_MENU_HELP,
    UI_HIERARCHY,
    UI_INSPECTOR,
    UI_SCENE
};

/**
 * @typedef FPluginUICallback
 * @param pCtx Pointer to the host-provided plugin context.
 * @brief Function pointer type for UI callbacks executed in a specific EUIRegion.
 * 
 * Called by the host when rendering a registered UI region. Receives the
 * current SPluginContext for accessing UI and engine state.
 */
using FPluginUICallback = void(*)(SPluginContext*);

/**
 * @typedef FPluginComponentFactory
 * @param pfnCreate Constructs a default-constructed plugin component. The host
 *               fills it through the component's own deserialize().
 * @return Pointer to a newly allocated component owned by the host, or nullptr
 *         on failure.
 * @brief Function pointer type used by
 *        SPluginContext::pfnRegisterComponentFactory.
 *
 * The plugin owns the component definition but not the instances: the host owns
 * every instance it creates from a factory and frees it with the scene.
 */
using FPluginComponentFactory = void*(*)();

/**
 * @struct SPluginContext
 * @brief Per-frame host state passed into every plugin callback.
 *
 * Contains runtime statistics (timing, entity count, selection state) and a
 * complete set of host-provided function pointers for UI drawing, entity
 * inspection, entity mutation, and scene management.
 *
 * @warning The plugin must **not** store this pointer beyond the call that
 *          received it — the host may reallocate the context between frames.
 */
struct SPluginContext
{
    /** @brief Seconds elapsed since the previous frame.
     *  Use for frame-rate-independent motion and animation. */
    float deltaTime;

    /** @brief Total number of entities currently alive in the scene.
     *  Valid entity indices are in the range [0, entityCount). */
    int entityCount;

    /** @brief Pointer to the index of the currently selected entity,
     *  or nullptr if no entity is selected. Always null-check before
     *  dereferencing. */
    int* pSelected;

    // -------------------------------------------------------------------------
    // UI — thin wrapper over an ImGui-style immediate-mode UI
    // -------------------------------------------------------------------------

    /**
     * @brief Opens a UI window with the given title.
     * @param pTitle Null-terminated window title string.
     * @return true if the window is visible and its contents should be drawn.
     * @note Must always be paired with a call to pfnUiEnd(), even when returning false.
     */
    bool (*pfnUiBegin)(const char* pTitle);

    /**
     * @brief Closes the window opened by the most recent pfnUiBegin() call.
     */
    void (*pfnUiEnd)();

    /**
     * @brief Begins a menu section in the UI (e.g. top menu bar entry).
     * @param pLabel Menu name.
     * @return true if the menu is open and its items should be drawn.
     */
    bool (*pfnUiBeginMenu)(const char* pLabel);
    
    /**
     * @brief Ends the most recently opened menu section.
     */
    void (*pfnUiEndMenu)();

    /**
     * @brief Creates a clickable item inside a menu.
     * @param pLabel Item text.
     * @return true when the item is clicked.
     */
    bool (*pfnUiMenuItem)(const char* pLabel);

    /**
     * @brief Registers @p pfnCallback to be invoked whenever @p region is drawn.
     *
     * Call from OnLoad() for each region the plugin draws into, and expect the
     * callback to run on every frame in which that region is visible, right after
     * the host has finished its own widgets for the region. Inside the callback,
     * use the context it receives rather than the one captured at registration
     * time, because the host refreshes per-frame fields such as deltaTime in place.
     *
     * @param pCtx         The context this callback was reached through. The host
     *                     reads its pPluginManager to reach the registry that owns
     *                     the registration; never null.
     * @param region       Region that gates the callback.
     * @param pfnCallback  Non-null callback to invoke; the host does not
     *                     null-check it.
     */
    void (*pfnRegisterUICallback)(SPluginContext* pCtx, EUIRegion region, FPluginUICallback pfnCallback);

    // -------------------------------------------------------------------------
    // Components
    // -------------------------------------------------------------------------

    /**
     * @brief Teaches the host how to rebuild one of the plugin's component
     *        types when a scene is loaded or an undo step is applied.
     *
     * The scene document stores every component under the string returned by
     * the component's GetTypeName(). A type the host cannot construct is
     * dropped with a warning, so a plugin that attaches components to entities
     * has to call this from OnLoad() for each of them, and should call
     * pfnUnregisterComponentFactory() from OnUnload().
     *
     * @param pCtx      The context this callback was reached through. The host
     *                  reads its pComponentRegistry to reach the registry that
     *                  owns the registration; never null.
     * @param pTypeName Type name exactly as the component writes it to disk.
     * @param pfnCreate    Constructs a default-constructed component instance; the
     *                  host then fills it through the component's own
     *                  deserialize().
     * @return true when the factory was registered. Built-in type names are
     *         reserved and are never overwritten.
     */
    bool (*pfnRegisterComponentFactory)(SPluginContext* pCtx, const char* pTypeName, FPluginComponentFactory pfnCreate);

    /**
     * @brief Removes a factory previously passed to
     *        pfnRegisterComponentFactory(). Safe to call for a type name that was
     *        never registered.
     * @param pCtx      The context this callback was reached through, used the same
     *                  way as in pfnRegisterComponentFactory().
     * @param pTypeName Type name to remove.
     */
    void (*pfnUnregisterComponentFactory)(SPluginContext* pCtx, const char* pTypeName);

    /**
     * @brief Renders a read-only text label inside the current window.
     * @param pText Null-terminated string to display.
     */
    void (*pfnUiText)(const char* pText);

    /**
     * @brief Renders a clickable button.
     * @param pLabel Null-terminated button label.
     * @return true on the single frame the button is clicked.
     */
    bool (*pfnUiButton)(const char* pLabel);

    /**
     * @brief Renders a checkbox bound to an external bool.
     * @param pLabel  Null-terminated label shown next to the checkbox.
     * @param pValue  Pointer to the bool to read from and write to.
     * @return true when the value changes.
     */
    bool (*pfnUiCheckbox)(const char* pLabel, bool* pValue);

    /**
     * @brief Renders a float slider clamped to [min, max].
     * @param pLabel  Null-terminated label.
     * @param pValue  Pointer to the float to read from and write to.
     * @param fMin    Lower bound of the slider range.
     * @param fMax    Upper bound of the slider range.
     * @return true while the slider is being dragged.
     */
    bool (*pfnUiSliderFloat)(const char* pLabel, float* pValue, float fMin, float fMax);

    /**
     * @brief Renders a direct-entry float input field.
     * @param pLabel  Null-terminated label.
     * @param pValue  Pointer to the float to read from and write to.
     * @return true when the value is committed (Enter key or focus loss).
     */
    bool (*pfnUiInputFloat)(const char* pLabel, float* pValue);

    /**
     * @brief Renders an RGB color picker.
     * @param pLabel  Null-terminated label.
     * @param aColor  Three-element float array in [0.0, 1.0] range (R, G, B).
     * @return true when any color component changes.
     */
    bool (*pfnUiColorEdit3)(const char* pLabel, float aColor[3]);

    /**
     * @brief Draws a horizontal separator line inside the current window.
     */
    void (*pfnUiSeparator)();

    /**
     * @brief Places the next widget on the same horizontal line as the
     *        previous one, suppressing the automatic line break.
     */
    void (*pfnUiSameLine)();

    // -------------------------------------------------------------------------
    // Entity read — query entity state by index
    //
    // These take the scene explicitly rather than reaching it through the context
    // so that every host trampoline stays a plain function and the host does not
    // have to keep a file-scope editor pointer alive for the lambdas to borrow.
    // Always pass ctx->pScene.
    // -------------------------------------------------------------------------

    /**
     * @brief Returns the display name of an entity.
     * @param pScene Scene to read from; the host's ctx->pScene.
     * @param index  Entity index in [0, entityCount).
     * @return Pointer to the entity's name string, owned by the host.
     *         Do not free or modify this pointer.
     */
    const char* (*pfnEntityGetName)(CScene* pScene, int index);

    /**
     * @brief Writes an entity's world-space position into output parameters.
     * @param pScene Scene to read from; the host's ctx->pScene.
     * @param index  Entity index.
     * @param x,y,z  Output pointers for the X, Y, Z position components.
     */
    void (*pfnEntityGetPosition)(CScene* pScene, int index, float* pX, float* pY, float* pZ);

    /**
     * @brief Writes an entity's Euler rotation into output parameters.
     * @param pScene Scene to read from; the host's ctx->pScene.
     * @param index  Entity index.
     * @param x,y,z  Output pointers for the X, Y, Z rotation components
     *               (host-defined unit — degrees or radians).
     */
    void (*pfnEntityGetRotation)(CScene* pScene, int index, float* pX, float* pY, float* pZ);

    /**
     * @brief Writes an entity's scale into output parameters.
     * @param pScene Scene to read from; the host's ctx->pScene.
     * @param index  Entity index.
     * @param x,y,z  Output pointers for the X, Y, Z scale components.
     */
    void (*pfnEntityGetScale)(CScene* pScene, int index, float* pX, float* pY, float* pZ);

    /**
     * @brief Writes an entity's RGBA tint color into output parameters.
     * @param pScene Scene to read from; the host's ctx->pScene.
     * @param index    Entity index.
     * @param r,g,b,a  Output pointers for the red, green, blue, and alpha
     *                 channels as unsigned bytes (0–255).
     */
    void (*pfnEntityGetColor)(CScene* pScene, int index, unsigned char* pR, unsigned char* pG,
                             unsigned char* pB, unsigned char* pA);

    // -------------------------------------------------------------------------
    // Entity write — mutate entity state by index
    // -------------------------------------------------------------------------

    /**
     * @brief Moves an entity to a new world-space position.
     * @param pScene Scene to mutate; the host's ctx->pScene.
     * @param index  Entity index.
     * @param x,y,z  New position components.
     */
    void (*pfnEntitySetPosition)(CScene* pScene, int index, float x, float y, float z);

    /**
     * @brief Sets an entity's Euler rotation.
     * @param pScene Scene to mutate; the host's ctx->pScene.
     * @param index  Entity index.
     * @param x,y,z  New rotation components (host-defined unit).
     */
    void (*pfnEntitySetRotation)(CScene* pScene, int index, float x, float y, float z);

    /**
     * @brief Sets an entity's scale.
     * @param pScene Scene to mutate; the host's ctx->pScene.
     * @param index  Entity index.
     * @param x,y,z  New scale components.
     */
    void (*pfnEntitySetScale)(CScene* pScene, int index, float x, float y, float z);

    /**
     * @brief Sets an entity's RGBA tint color.
     * @param pScene   Scene to mutate; the host's ctx->pScene.
     * @param index    Entity index.
     * @param r,g,b,a  New color channel values as unsigned bytes (0–255).
     */
    void (*pfnEntitySetColor)(CScene* pScene, int index, unsigned char r, unsigned char g,
                             unsigned char b, unsigned char a);

    /**
     * @brief Renames an entity.
     * @param pScene Scene to mutate; the host's ctx->pScene.
     * @param index  Entity index.
     * @param pName  Null-terminated string. The host copies the string
     *               internally; the caller may free its buffer after this
     *               returns.
     */
    void (*pfnEntitySetName)(CScene* pScene, int index, const char* pName);

    // -------------------------------------------------------------------------
    // Scene management
    // -------------------------------------------------------------------------

    /**
     * @brief Current scene state owned by the host.
     */
    CScene* pScene;

    /**
     * @brief Serialises the scene to disk under the given project path.
     * @param pProjectPath Null-terminated path of the project to write into.
     * @param pScene       Scene to serialise; the host's ctx->pScene.
     */
    void (*pfnSceneSave)(const char* pProjectPath, CScene* pScene);

    /**
     * @brief Instantiates an asset and adds it to the scene.
     * @param pAssets    Host asset library to resolve the name against.
     * @param pScene     Scene to add the new entity to; the host's ctx->pScene.
     * @param pAssetName Null-terminated asset identifier recognised by the
     *                   host's asset registry.
     * @return The new entity's index (>= 0) on success, or -1 on failure
     *         (e.g. unknown asset name).
     */
    int (*pfnSceneSpawn)(CAssetLibrary* pAssets, CScene* pScene, const char* pAssetName);

    /**
     * @brief Permanently removes an entity from the scene.
     * @param pScene Scene to mutate; the host's ctx->pScene.
     * @param index  Entity index to delete.
     * @warning After this call, indices for all entities above @p index may
     *          be shifted. Re-query entityCount and any cached indices.
     */
    void (*pfnSceneDelete)(CScene* pScene, int index);

    // -------------------------------------------------------------------------
    // Component registry
    // -------------------------------------------------------------------------

    /**
     * @brief The host's component factory registry, used by
     *        pfnRegisterComponentFactory() and pfnUnregisterComponentFactory().
     *
     * Owned by the host and valid for as long as the context itself. Plugins
     * must not cache it across OnUnload(). Always non-null, but a plugin written
     * against an older header can observe null here, so null-check before use.
     */
    CComponentFactoryRegistry* pComponentRegistry;

    /**
     * @brief The host's UI callback registry, used by pfnRegisterUICallback().
     *
     * Owned by the host and valid for as long as the context itself. Plugins must
     * not cache it across OnUnload(). Always non-null, but a plugin written
     * against an older header can observe null here, so null-check before use.
     */
    CPluginManager* pPluginManager;

    /**
     * @brief The host's asset library, used by pfnSceneSpawn() to resolve an
     *        asset name into geometry.
     *
     * Owned by the host and valid for as long as the context itself. Plugins must
     * not cache it across OnUnload(). Always non-null, but a plugin written
     * against an older header can observe null here, so null-check before use.
     */
    CAssetLibrary* pAssets;

    /**
     * @brief Null-terminated path of the project the host has open, used by
     *        pfnSceneSave() as the destination.
     *
     * Owned by the host and valid for as long as the context itself, and the path
     * never changes once the context is handed to plugins. Plugins must not cache
     * it across OnUnload(). Always non-null, but a plugin written against an older
     * header can observe null here, so null-check before use.
     */
    const char* pProjectPath;
};

/**
 * @struct SPlugin
 * @brief Descriptor returned by GetPlugin(). Identifies the plugin and
 *        provides the four lifecycle callbacks the host will invoke.
 *
 * All pointer fields must be non-null; the host does not null-check before
 * calling them.
 */
struct SPlugin
{
    /** @brief Human-readable plugin name shown in the host's plugin manager.
     *  Must be a static string literal — the host will not free it. */
    const char* pName;

    /** @brief Semantic version string (e.g. "1.0.0"). Displayed alongside
     *  @ref name for diagnostics. Must be a static string literal. */
    const char* pVersion;

    /**
     * @brief Called once after the shared library is loaded.
     *
     * Use for one-time initialisation: allocating state, registering
     * resources, reading configuration. @p ctx is valid only for the
     * duration of this call.
     *
     * @param pCtx  Pointer to the host-provided plugin context.
     */
    void (*pfnOnLoad)(SPluginContext* pCtx);

    /**
     * @brief Called once just before the shared library is unloaded.
     *
     * Free all heap allocations and release external resources here.
     * The SPluginContext is no longer available at this point.
     */
    void (*pfnOnUnload)();

    /**
     * @brief Called every frame during the simulation tick, before rendering.
     *
     * Use @c ctx->deltaTime for frame-rate-independent logic. Keep this
     * path fast — avoid heavy I/O or blocking calls.
     *
     * @param pCtx  Pointer to the current frame's plugin context.
     */
    void (*pfnOnUpdate)(SPluginContext* pCtx);

    /**
     * @brief Called every frame during the UI pass.
     *
     * Use the @c ui_* function pointers on @p ctx to draw controls.
     * Every successful @c pfnUiBegin() call must be matched by @c pfnUiEnd().
     *
     * @param pCtx  Pointer to the current frame's plugin context.
     */
    void (*pfnOnDrawUI)(SPluginContext* pCtx);
};

/**
 * @brief SPlugin entry point — the only symbol the host loads from the DLL.
 *
 * The host calls this immediately after loading the shared library to obtain
 * the plugin descriptor. The returned pointer must remain valid for the entire
 * lifetime of the loaded library; use a static or heap-allocated SPlugin
 * instance. Returning nullptr causes the host to abort loading and unload the
 * library.
 *
 * @return Non-null pointer to a fully initialised SPlugin descriptor.
 *         All four function pointer fields must be non-null.
 */
PLUGIN_EXPORT SPlugin* GetPlugin();

#endif // __PLUGIN_H__
