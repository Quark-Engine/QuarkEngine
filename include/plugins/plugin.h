#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <cstddef>

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
 * Pass one of these values to pfnRegisterUICallback() to associate a plugin
 * callback with a specific editor region. The host invokes the callback when
 * that region is drawn; it does not invoke region callbacks on unrelated UI
 * passes.
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
 * @enum EPluginEvent
 * @brief Notifications emitted by the host when the editor changes scene state.
 */
enum EPluginEvent
{
    PLUGIN_EVENT_ENTITY_CREATED,    /**< A new entity was appended to the scene. */
    PLUGIN_EVENT_ENTITY_DELETED,    /**< An entity is about to be removed. */
    PLUGIN_EVENT_ENTITY_SELECTED,   /**< The active or multi-selection changed. */
    PLUGIN_EVENT_TRANSFORM_CHANGED, /**< Position, rotation, or scale changed. */
    PLUGIN_EVENT_SCENE_LOADED,      /**< The current scene finished loading. */
    PLUGIN_EVENT_SCENE_SAVED,       /**< The current scene finished saving. */
};

/**
 * @enum EPluginUiColor
 * @brief Stable semantic color slots accepted by the plugin UI style API.
 *
 * These values map to a supported subset of the host ImGui theme. They are
 * intentionally independent of ImGui's numeric enum values.
 */
enum EPluginUiColor
{
    PLUGIN_UI_COLOR_TEXT,               /**< Main text color. */
    PLUGIN_UI_COLOR_TEXT_DISABLED,      /**< Disabled text color. */
    PLUGIN_UI_COLOR_WINDOW_BACKGROUND,  /**< Main window background. */
    PLUGIN_UI_COLOR_CHILD_BACKGROUND,   /**< Child window background. */
    PLUGIN_UI_COLOR_POPUP_BACKGROUND,   /**< Popup and menu background. */
    PLUGIN_UI_COLOR_BORDER,             /**< Window and widget border. */
    PLUGIN_UI_COLOR_FRAME_BACKGROUND,   /**< Input, slider, and checkbox background. */
    PLUGIN_UI_COLOR_FRAME_HOVERED,      /**< Hovered input/frame background. */
    PLUGIN_UI_COLOR_FRAME_ACTIVE,       /**< Active input/frame background. */
    PLUGIN_UI_COLOR_BUTTON,             /**< Button background. */
    PLUGIN_UI_COLOR_BUTTON_HOVERED,     /**< Hovered button background. */
    PLUGIN_UI_COLOR_BUTTON_ACTIVE,      /**< Active button background. */
    PLUGIN_UI_COLOR_HEADER,             /**< Tree, selectable, and section header. */
    PLUGIN_UI_COLOR_HEADER_HOVERED,     /**< Hovered tree/header background. */
    PLUGIN_UI_COLOR_HEADER_ACTIVE       /**< Active tree/header background. */
};

/**
 * @enum EPluginUiStyleVar
 * @brief Stable semantic style variables accepted by the plugin UI style API.
 *
 * Scalar variables are used with pfnUiPushStyleVarFloat(); two-dimensional
 * variables are used with pfnUiPushStyleVarVec2().
 */
enum EPluginUiStyleVar
{
    PLUGIN_UI_STYLE_ALPHA,                /**< Global UI alpha multiplier. */
    PLUGIN_UI_STYLE_WINDOW_ROUNDING,      /**< Window corner rounding. */
    PLUGIN_UI_STYLE_FRAME_ROUNDING,       /**< Widget frame corner rounding. */
    PLUGIN_UI_STYLE_SCROLLBAR_ROUNDING,   /**< Scrollbar grab corner rounding. */
    PLUGIN_UI_STYLE_GRAB_ROUNDING,        /**< Slider and scrollbar grab rounding. */
    PLUGIN_UI_STYLE_WINDOW_PADDING,       /**< Window inner padding (two-dimensional). */
    PLUGIN_UI_STYLE_FRAME_PADDING,        /**< Widget inner padding (two-dimensional). */
    PLUGIN_UI_STYLE_ITEM_SPACING,         /**< Spacing between widgets (two-dimensional). */
    PLUGIN_UI_STYLE_ITEM_INNER_SPACING,   /**< Spacing within compound widgets (two-dimensional). */
    PLUGIN_UI_STYLE_CELL_PADDING          /**< Table cell padding (two-dimensional). */
};

/**
 * @enum EPluginUiDirection
 * @brief Stable directional values used by plugin arrow controls.
 */
enum EPluginUiDirection
{
    PLUGIN_UI_DIRECTION_LEFT,
    PLUGIN_UI_DIRECTION_RIGHT,
    PLUGIN_UI_DIRECTION_UP,
    PLUGIN_UI_DIRECTION_DOWN
};

/**
 * @enum EPluginUiWindowOption
 * @brief Stable window behavior options accepted by pfnUiBeginEx().
 */
enum EPluginUiWindowOption
{
    PLUGIN_UI_WINDOW_NO_TITLE_BAR       = 1 << 0, /**< Hide the title bar. */
    PLUGIN_UI_WINDOW_NO_RESIZE          = 1 << 1, /**< Disable user resizing. */
    PLUGIN_UI_WINDOW_NO_MOVE            = 1 << 2, /**< Disable user moving. */
    PLUGIN_UI_WINDOW_NO_SCROLLBAR       = 1 << 3, /**< Hide the scrollbar. */
    PLUGIN_UI_WINDOW_NO_COLLAPSE        = 1 << 4, /**< Disable title-bar collapse. */
    PLUGIN_UI_WINDOW_NO_BACKGROUND      = 1 << 5, /**< Hide the window background. */
    PLUGIN_UI_WINDOW_NO_SAVED_SETTINGS  = 1 << 6, /**< Do not persist window state. */
    PLUGIN_UI_WINDOW_MENU_BAR           = 1 << 7, /**< Enable the window menu bar. */
    PLUGIN_UI_WINDOW_NO_BRING_TO_FRONT  = 1 << 8  /**< Do not raise on focus. */
};

/**
 * @enum EPluginUiTreeNodeOption
 * @brief Stable tree-node behavior options accepted by pfnUiTreeNodeEx().
 */
enum EPluginUiTreeNodeOption
{
    PLUGIN_UI_TREE_SELECTED          = 1 << 0, /**< Render the node as selected. */
    PLUGIN_UI_TREE_DEFAULT_OPEN      = 1 << 1, /**< Open the node on first use. */
    PLUGIN_UI_TREE_LEAF              = 1 << 2, /**< Render as a leaf without a toggle. */
    PLUGIN_UI_TREE_BULLET            = 1 << 3, /**< Render a bullet instead of an arrow. */
    PLUGIN_UI_TREE_FRAMED            = 1 << 4, /**< Render a full-width framed node. */
    PLUGIN_UI_TREE_SPAN_AVAILABLE    = 1 << 5  /**< Extend the hit area across available width. */
};

/**
 * @enum EPluginUiCondition
 * @brief Stable conditions for one-shot ImGui state changes.
 */
enum EPluginUiCondition
{
    PLUGIN_UI_CONDITION_ALWAYS,
    PLUGIN_UI_CONDITION_ONCE,
    PLUGIN_UI_CONDITION_FIRST_USE,
    PLUGIN_UI_CONDITION_APPEARING
};

/**
 * @enum EPluginUiMouseButton
 * @brief Stable mouse-button identifiers for plugin input queries.
 */
enum EPluginUiMouseButton
{
    PLUGIN_UI_MOUSE_LEFT,
    PLUGIN_UI_MOUSE_RIGHT,
    PLUGIN_UI_MOUSE_MIDDLE
};

/**
 * @enum EPluginUiTableColorTarget
 * @brief Stable table region identifiers for setting a background color.
 */
enum EPluginUiTableColorTarget
{
    PLUGIN_UI_TABLE_COLOR_ROW_BACKGROUND,
    PLUGIN_UI_TABLE_COLOR_ROW_BACKGROUND_ALT,
    PLUGIN_UI_TABLE_COLOR_CELL_BACKGROUND
};

/**
 * @enum EPluginUiKey
 * @brief Stable commonly used keyboard keys for plugin input queries.
 */
enum EPluginUiKey
{
    PLUGIN_UI_KEY_TAB,
    PLUGIN_UI_KEY_LEFT,
    PLUGIN_UI_KEY_RIGHT,
    PLUGIN_UI_KEY_UP,
    PLUGIN_UI_KEY_DOWN,
    PLUGIN_UI_KEY_PAGE_UP,
    PLUGIN_UI_KEY_PAGE_DOWN,
    PLUGIN_UI_KEY_HOME,
    PLUGIN_UI_KEY_END,
    PLUGIN_UI_KEY_INSERT,
    PLUGIN_UI_KEY_DELETE,
    PLUGIN_UI_KEY_BACKSPACE,
    PLUGIN_UI_KEY_SPACE,
    PLUGIN_UI_KEY_ENTER,
    PLUGIN_UI_KEY_ESCAPE,
    PLUGIN_UI_KEY_A,
    PLUGIN_UI_KEY_B,
    PLUGIN_UI_KEY_C,
    PLUGIN_UI_KEY_D,
    PLUGIN_UI_KEY_E,
    PLUGIN_UI_KEY_F,
    PLUGIN_UI_KEY_G,
    PLUGIN_UI_KEY_H,
    PLUGIN_UI_KEY_I,
    PLUGIN_UI_KEY_J,
    PLUGIN_UI_KEY_K,
    PLUGIN_UI_KEY_L,
    PLUGIN_UI_KEY_M,
    PLUGIN_UI_KEY_N,
    PLUGIN_UI_KEY_O,
    PLUGIN_UI_KEY_P,
    PLUGIN_UI_KEY_Q,
    PLUGIN_UI_KEY_R,
    PLUGIN_UI_KEY_S,
    PLUGIN_UI_KEY_T,
    PLUGIN_UI_KEY_U,
    PLUGIN_UI_KEY_V,
    PLUGIN_UI_KEY_W,
    PLUGIN_UI_KEY_X,
    PLUGIN_UI_KEY_Y,
    PLUGIN_UI_KEY_Z,
    PLUGIN_UI_KEY_F1,
    PLUGIN_UI_KEY_F2,
    PLUGIN_UI_KEY_F3,
    PLUGIN_UI_KEY_F4,
    PLUGIN_UI_KEY_F5,
    PLUGIN_UI_KEY_F6,
    PLUGIN_UI_KEY_F7,
    PLUGIN_UI_KEY_F8,
    PLUGIN_UI_KEY_F9,
    PLUGIN_UI_KEY_F10,
    PLUGIN_UI_KEY_F11,
    PLUGIN_UI_KEY_F12
};

/**
 * @enum EPluginUiMouseCursor
 * @brief Stable cursor shapes that plugins may request during UI drawing.
 */
enum EPluginUiMouseCursor
{
    PLUGIN_UI_MOUSE_CURSOR_ARROW,
    PLUGIN_UI_MOUSE_CURSOR_TEXT,
    PLUGIN_UI_MOUSE_CURSOR_RESIZE_ALL,
    PLUGIN_UI_MOUSE_CURSOR_RESIZE_VERTICAL,
    PLUGIN_UI_MOUSE_CURSOR_RESIZE_HORIZONTAL,
    PLUGIN_UI_MOUSE_CURSOR_RESIZE_DIAGONAL_NW_SE,
    PLUGIN_UI_MOUSE_CURSOR_RESIZE_DIAGONAL_NE_SW,
    PLUGIN_UI_MOUSE_CURSOR_HAND,
    PLUGIN_UI_MOUSE_CURSOR_NOT_ALLOWED
};

/**
 * @enum EPluginUiScalarType
 * @brief Fixed-width scalar types supported by generic numeric UI widgets.
 */
enum EPluginUiScalarType
{
    PLUGIN_UI_SCALAR_S8,
    PLUGIN_UI_SCALAR_U8,
    PLUGIN_UI_SCALAR_S16,
    PLUGIN_UI_SCALAR_U16,
    PLUGIN_UI_SCALAR_S32,
    PLUGIN_UI_SCALAR_U32,
    PLUGIN_UI_SCALAR_S64,
    PLUGIN_UI_SCALAR_U64,
    PLUGIN_UI_SCALAR_FLOAT,
    PLUGIN_UI_SCALAR_DOUBLE
};

/**
 * @enum EPluginUiBuiltinStyle
 * @brief Built-in ImGui theme presets that can be selected by a plugin UI.
 */
enum EPluginUiBuiltinStyle
{
    PLUGIN_UI_BUILTIN_STYLE_DARK,
    PLUGIN_UI_BUILTIN_STYLE_LIGHT,
    PLUGIN_UI_BUILTIN_STYLE_CLASSIC
};

/**
 * @typedef FPluginUICallback
 * @brief Function pointer type for drawing plugin UI in a registered editor
 *        region.
 *
 * The host calls this callback while that region's immediate-mode UI pass is
 * active. Use the supplied context only during the callback, and submit UI
 * through its function pointers. Do not retain the context pointer or call UI
 * helpers from a background thread.
 *
 * @param pCtx Current host-provided context; valid only for this invocation.
 */
using FPluginUICallback = void(*)(SPluginContext*);

/**
 * @typedef FPluginEventCallback
 * @brief Function pointer type for receiving host scene/editor notifications.
 *
 * A callback is registered for one event kind and receives notifications of
 * that kind from the host. Entity indices are transient: scene edits can
 * invalidate or shift them, so query the current scene when handling an event
 * and do not retain the context after the callback returns.
 *
 * @param pCtx Current host context; valid only for this invocation.
 * @param event Event kind being reported.
 * @param entityIndex Related entity index, or -1 when the event concerns the
 *                    scene rather than one entity.
 */
using FPluginEventCallback = void(*)(SPluginContext* pCtx, EPluginEvent event, int entityIndex);

/**
 * @typedef FPluginComponentFactory
 * @brief Function pointer type used by
 *        SPluginContext::pfnRegisterComponentFactory.
 *
 * The factory must return a newly allocated instance of the plugin component,
 * converted to void* for the C-compatible plugin boundary. The host assumes
 * ownership of a non-null result, then restores serialized state through the
 * component's deserialize implementation. Return nullptr if construction
 * fails. The plugin owns the component type implementation, but not instances
 * created by the host.
 *
 * @return Newly allocated component instance, or nullptr on failure.
 */
using FPluginComponentFactory = void*(*)();

/**
 * @struct SPluginThreadUsage
 * @brief One host-owned snapshot of a worker or main thread's recent activity.
 *
 * The strings and history array are owned by the host and must not be modified
 * or retained beyond the plugin callback that received this structure.
 */
struct SPluginThreadUsage
{
    /** @brief Human-readable thread role, owned by the host. */
    const char* pName;
    /** @brief Description of the task most recently observed on this thread. */
    const char* pCurrentTask;
    /** @brief Measured busy percentage for the host's current sampling window. */
    float utilizationPercent;
    /** @brief Pointer to the host-maintained recent utilization samples. */
    const float* pHistory;
    /** @brief Number of samples available in @p pHistory. */
    int historyCount;
};

/**
 * @struct SPluginContext
 * @brief Per-frame host state passed into every plugin callback.
 *
 * Contains runtime statistics (timing, entity count, selection state) and a
 * complete set of host-provided function pointers for UI drawing, entity
 * inspection, entity mutation, and scene management.
 *
 * The host refreshes per-frame values before invoking plugin callbacks. Use
 * this structure only during the callback in which it was received; copy any
 * values that must outlive that invocation. Function pointers and host-owned
 * objects are provided for interacting with the editor and current scene.
 *
 * @warning Do not retain this pointer beyond the callback or access it from
 *          another thread. Its address and per-frame fields are host-managed.
 */
struct SPluginContext
{
    /**
     * @brief Elapsed time in seconds between the previous and current frame.
     * @note Use this value to scale frame-dependent plugin updates. It is
     *       refreshed for each frame callback and should not be cached.
     */
    float deltaTime;

    /**
     * @brief Number of entities in the scene associated with this context.
     * @note When this value is positive, valid current entity indices are in
     *       the half-open range [0, entityCount). Re-read it after operations
     *       that add or remove entities.
     */
    int entityCount;

    /**
     * @brief Host-owned pointer to the primary selected entity index.
     *
     * This pointer is null when there is no primary selection. When non-null,
     * its index is only valid against the current scene state.
     *
     * @note Do not free, retain, or modify this pointer; use scene selection
     *       API functions when changing selection.
     */
    int* pSelected;

    // -------------------------------------------------------------------------
    // UI — thin wrapper over an ImGui-style immediate-mode UI
    // -------------------------------------------------------------------------

    /**
     * @brief Begins submitting widgets to a plugin-owned editor window.
     *
     * Call during pfnOnDrawUI() or a registered UI-region callback while the
     * host UI frame is active. The title also identifies the window; append a
     * hidden suffix such as "##settings" when distinct windows need the same
     * visible title.
     *
     * @param pTitle Null-terminated window title and identifier; must not be
     *               null.
     * @return true when the window contents are visible and widgets should be
     *         submitted; false when collapsed or clipped.
     * @note Pair every call with exactly one pfnUiEnd(), even when this
     *       function returns false. Submit child widgets only when it returns
     *       true.
     */
    bool (*pfnUiBegin)(const char* pTitle);

    /**
     * @brief Ends the window opened by the most recent pfnUiBegin() call.
     *
     * Call exactly once after every pfnUiBegin(), regardless of that function's
     * return value. Do not call when no plugin window is open.
     */
    void (*pfnUiEnd)();

    /**
     * @brief Begins a menu in the currently active menu bar or popup.
     *
     * Menu contents should be submitted only while the menu is open. This
     * helper is intended for a menu-region callback or for nesting a submenu
     * inside another open menu.
     *
     * @param pLabel Null-terminated visible menu label; must not be null.
     * @return true when the menu is open and its items may be submitted.
     * @note Pair with pfnUiEndMenu() only when this function returns true.
     */
    bool (*pfnUiBeginMenu)(const char* pLabel);
    
    /**
     * @brief Ends the menu opened by the most recent successful
     *        pfnUiBeginMenu() call.
     * @note Do not call if pfnUiBeginMenu() returned false.
     */
    void (*pfnUiEndMenu)();

    /**
     * @brief Adds a clickable command to the currently open menu.
     * @param pLabel Null-terminated item text; must not be null.
     * @return true for the frame in which the user activates the item;
     *         otherwise false.
     * @note Call only between a successful pfnUiBeginMenu() and its matching
     *       pfnUiEndMenu().
     */
    bool (*pfnUiMenuItem)(const char* pLabel);

    /**
     * @brief Registers a callback for drawing into an editor UI region.
     *
     * Call during plugin initialization. The host invokes the callback when it
     * draws the selected region; the callback receives the current context and
     * must use it only for that invocation. Registering the same function more
     * than once may result in multiple invocations.
     *
     * @param pCtx Context containing the host UI callback registry. Pass the
     *             context supplied to pfnOnLoad(); do not retain it.
     * @param region Editor region in which to invoke the callback.
     * @param pfnCallback Non-null function to invoke while drawing that region.
     * @note The host associates the registration with the plugin being loaded
     *       and removes it when that plugin is unloaded.
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
     * should call this from OnLoad() for each of them. Unregistration requires
     * a valid context, but pfnOnUnload() does not receive one; do not retain a
     * context pointer beyond a host callback to work around this limitation.
     *
     * @param pCtx Context containing the host component registry. Pass the
     *             context supplied to pfnOnLoad(); do not retain it.
     * @param pTypeName Non-null, null-terminated type name exactly matching the
     *                  component's serialized GetTypeName() value.
     * @param pfnCreate Factory that returns a newly allocated default component
     *                  instance, or nullptr if it cannot construct one. The
     *                  host deserializes the instance after construction.
     * @return true when the factory was registered; false for invalid
     *         arguments or a reserved/already registered type name.
     * @warning The factory registry does not currently associate registrations
     *          with their owning plugin. The plugin must not unload its factory
     *          code while the registration remains active. Since pfnOnUnload()
     *          has no context, this API currently provides no safe way to
     *          unregister a factory from that callback.
     */
    bool (*pfnRegisterComponentFactory)(SPluginContext* pCtx, const char* pTypeName, FPluginComponentFactory pfnCreate);

    /**
     * @brief Removes a component factory from the host registry.
     *
     * Use this to unregister a plugin-owned factory while a valid plugin
     * context is available. This operation is keyed by type name, so pass only
     * a type name that the calling plugin is responsible for. An absent type
     * is left unchanged.
     *
     * @param pCtx Context containing the host component registry; valid only
     *             during the host callback in which it was received.
     * @param pTypeName Null-terminated registered component type name.
     */
    void (*pfnUnregisterComponentFactory)(SPluginContext* pCtx, const char* pTypeName);

    /**
     * @brief Displays plain, read-only text in the current UI region.
     *
     * The text is passed as data, not as a printf-style format string. The
     * host reads it synchronously and does not retain the pointer.
     *
     * @param pText Null-terminated text to display; must not be null.
     */
    void (*pfnUiText)(const char* pText);

    /**
     * @brief Displays a clickable button in the current UI region.
     * @param pLabel Null-terminated visible label and widget identifier. Use a
     *               hidden suffix such as "##apply_1" when IDs must differ
     *               while visible text remains the same.
     * @return true on the frame the button is activated; otherwise false.
     * @note Call only while a plugin UI callback is being drawn.
     */
    bool (*pfnUiButton)(const char* pLabel);

    /**
     * @brief Displays a checkbox and reads or updates a plugin-owned bool.
     * @param pLabel Null-terminated visible label and widget identifier.
     * @param pValue Non-null pointer to the bool state. The host reads the
     *               current value and writes the toggled value when changed.
     * @return true when the value changes; otherwise false.
     * @note The plugin retains ownership of @p pValue and must keep it valid
     *       for the duration of this call.
     */
    bool (*pfnUiCheckbox)(const char* pLabel, bool* pValue);

    /**
     * @brief Displays a horizontal slider for a floating-point value.
     *
     * The value is clamped to the inclusive range and written directly to the
     * plugin-owned storage.
     *
     * @param pLabel Null-terminated visible label and widget identifier.
     * @param pValue Non-null pointer to the value to display and edit.
     * @param fMin Inclusive lower bound; must be less than @p fMax.
     * @param fMax Inclusive upper bound; must be greater than @p fMin.
     * @return true when the value changes; otherwise false.
     */
    bool (*pfnUiSliderFloat)(const char* pLabel, float* pValue, float fMin, float fMax);

    /**
     * @brief Displays a numeric input field for a floating-point value.
     *
     * The user can type a value directly; this control does not impose a
     * minimum or maximum.
     *
     * @param pLabel Null-terminated visible label and widget identifier.
     * @param pValue Non-null pointer to the value to display and edit.
     * @return true when the input value changes; otherwise false.
     * @note The host writes to plugin-owned storage only during this call.
     */
    bool (*pfnUiInputFloat)(const char* pLabel, float* pValue);

    /**
     * @brief Displays an RGB color editor and updates a three-float color.
     *
     * Channels are normalized red, green and blue values; this control does
     * not read or modify alpha.
     *
     * @param pLabel Null-terminated visible label and widget identifier.
     * @param aColor Writable array of at least three floats in R, G, B order.
     *               Each channel is expected to be in the range [0.0, 1.0].
     * @return true when any channel changes; otherwise false.
     * @note The host reads and writes the array during this call only.
     */
    bool (*pfnUiColorEdit3)(const char* pLabel, float aColor[3]);

    /**
     * @brief Adds a horizontal separator at the current layout position.
     *
     * The separator is drawn within the current window or UI region and
     * advances the layout cursor to the next line.
     * @note Call only while a plugin UI callback is active.
     */
    void (*pfnUiSeparator)();

    /**
     * @brief Places the next widget on the current horizontal layout line.
     *
     * This suppresses the normal line break after the previously submitted
     * item. Call immediately before the widget to place beside that item.
     * @note Call only while a plugin UI callback is active.
     */
    void (*pfnUiSameLine)();
    
    /**
     * @brief Gets the display name stored on an entity.
     *
     * The returned string refers directly to the entity's host-owned name.
     *
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @param index Valid zero-based entity index in [0, entityCount).
     * @return Pointer to a null-terminated host-owned name. Do not modify or
     *         free it; renaming or removing the entity, or changing the scene,
     *         may invalidate the pointer.
     * @warning This helper does not validate the scene or index. Supply a
     *          non-null scene and a currently valid index.
     */
    const char* (*pfnEntityGetName)(CScene* pScene, int index);

    /**
     * @brief Reads an entity's stored local position into three output values.
     *
     * For an entity with a parent, these are coordinates relative to that
     * parent; they are not the composed world-space position.
     *
     * @param pScene Scene to query; normally pass the current ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param pX Non-null output pointer for the local X coordinate.
     * @param pY Non-null output pointer for the local Y coordinate.
     * @param pZ Non-null output pointer for the local Z coordinate.
     * @note If the entity has no transform component, the host leaves outputs
     *       unchanged. Initialize outputs before calling if this is possible.
     * @warning This helper does not validate the scene, index, or output
     *          pointers; all must be valid.
     */
    void (*pfnEntityGetPosition)(CScene* pScene, int index, float* pX, float* pY, float* pZ);

    /**
     * @brief Reads an entity's stored Euler rotation into three output values.
     *
     * The values use the same units and component convention as the host
     * transform component. For children, these are local rotations relative to
     * the parent, not a decomposed world rotation.
     *
     * @param pScene Scene to query; normally pass the current ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param pX Non-null output pointer for the X rotation component.
     * @param pY Non-null output pointer for the Y rotation component.
     * @param pZ Non-null output pointer for the Z rotation component.
     * @note If the entity has no transform component, the host leaves outputs
     *       unchanged.
     * @warning This helper does not validate the scene, index, or output
     *          pointers; all must be valid.
     */
    void (*pfnEntityGetRotation)(CScene* pScene, int index, float* pX, float* pY, float* pZ);

    /**
     * @brief Reads an entity's stored local scale into three output values.
     * @param pScene Scene to query; normally pass the current ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param pX Non-null output pointer for the local X scale.
     * @param pY Non-null output pointer for the local Y scale.
     * @param pZ Non-null output pointer for the local Z scale.
     * @note If the entity has no transform component, the host leaves outputs
     *       unchanged.
     * @warning This helper does not validate the scene, index, or output
     *          pointers; all must be valid.
     */
    void (*pfnEntityGetScale)(CScene* pScene, int index, float* pX, float* pY, float* pZ);

    /**
     * @brief Reads an entity's material tint into four output channels.
     * @param pScene Scene to query; normally pass the current ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param pR Non-null output pointer for red, in [0, 255].
     * @param pG Non-null output pointer for green, in [0, 255].
     * @param pB Non-null output pointer for blue, in [0, 255].
     * @param pA Non-null output pointer for alpha, in [0, 255].
     * @note If the entity has no material component, the host leaves outputs
     *       unchanged.
     * @warning This helper does not validate the scene, index, or output
     *          pointers; all must be valid.
     */
    void (*pfnEntityGetColor)(CScene* pScene, int index, unsigned char* pR, unsigned char* pG,
                             unsigned char* pB, unsigned char* pA);

    // -------------------------------------------------------------------------
    // Entity write — mutate entity state by index
    // -------------------------------------------------------------------------

    /**
     * @brief Sets an entity's stored local position.
     *
     * For a child entity, the supplied coordinates are relative to its parent.
     * This clears any local-matrix override and notifies plugins that the
     * transform changed.
     *
     * @param pScene Scene to mutate; normally pass the current ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param x New local X coordinate.
     * @param y New local Y coordinate.
     * @param z New local Z coordinate.
     * @note If the entity has no transform component, no change is made.
     * @warning This helper does not validate the scene or index. Supply a
     *          non-null scene and a currently valid index.
     */
    void (*pfnEntitySetPosition)(CScene* pScene, int index, float x, float y, float z);

    /**
     * @brief Sets an entity's stored local Euler rotation.
     *
     * The values use the same units and component convention as the host
     * transform component. This clears any local-matrix override and notifies
     * plugins that the transform changed.
     *
     * @param pScene Scene to mutate; normally pass the current ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param x New X rotation component.
     * @param y New Y rotation component.
     * @param z New Z rotation component.
     * @note If the entity has no transform component, no change is made.
     * @warning This helper does not validate the scene or index. Supply a
     *          non-null scene and a currently valid index.
     */
    void (*pfnEntitySetRotation)(CScene* pScene, int index, float x, float y, float z);

    /**
     * @brief Sets an entity's stored local scale.
     *
     * For a child entity, the scale is relative to its parent. This clears any
     * local-matrix override and notifies plugins that the transform changed.
     *
     * @param pScene Scene to mutate; normally pass the current ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param x New local X scale.
     * @param y New local Y scale.
     * @param z New local Z scale.
     * @note If the entity has no transform component, no change is made.
     * @warning This helper does not validate the scene or index. Supply a
     *          non-null scene and a currently valid index.
     */
    void (*pfnEntitySetScale)(CScene* pScene, int index, float x, float y, float z);

    /**
     * @brief Sets an entity's material tint color.
     * @param pScene Scene to mutate; normally pass the current ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param r Red channel in [0, 255].
     * @param g Green channel in [0, 255].
     * @param b Blue channel in [0, 255].
     * @param a Alpha channel in [0, 255].
     * @note If the entity has no material component, no change is made. This
     *       helper does not itself create an undo command.
     * @warning This helper does not validate the scene or index. Supply a
     *          non-null scene and a currently valid index.
     */
    void (*pfnEntitySetColor)(CScene* pScene, int index, unsigned char r, unsigned char g,
                             unsigned char b, unsigned char a);

    /**
     * @brief Replaces an entity's display name.
     * @param pScene Scene to mutate; normally pass the current ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param pName Null-terminated name string. The host copies the value
     *              before returning; the caller may release its buffer after
     *              the call. It must not be null.
     * @note This helper does not itself create an undo command or enforce
     *       unique names.
     * @warning This helper does not validate the scene or index. Supply a
     *          non-null scene and a currently valid index.
     */
    void (*pfnEntitySetName)(CScene* pScene, int index, const char* pName);

    /**
     * @brief Pointer to the current scene owned and managed by the host.
     *
     * Use this scene for entity and scene APIs. The pointer is borrowed: plugins
     * must not delete it or retain it beyond the callback. A scene load may
     * replace the host's current scene.
     */
    CScene* pScene;

    /**
     * @brief Serializes the supplied scene to the project's scene file.
     *
     * A scene-saved plugin event is dispatched after the host save call.
     * Invalid null arguments cause the operation to be ignored. This function
     * does not report filesystem errors through its return value.
     *
     * @param pProjectPath Null-terminated project directory/path used by the
     *                     host serialization service.
     * @param pScene Scene to serialize; normally pass the current ctx->pScene.
     * @note The path and scene must remain valid for the duration of this call.
     */
    void (*pfnSceneSave)(const char* pProjectPath, CScene* pScene);

    /**
     * @brief Creates a scene entity from a registered model asset.
     *
     * On success, the entity is appended to the scene and an entity-created
     * event is dispatched. The new entity uses the asset's default transform.
     *
     * @param pAssets Asset library in which to resolve @p pAssetName; normally
     *                pass ctx->pAssets.
     * @param pScene Scene to mutate; normally pass ctx->pScene.
     * @param pAssetName Null-terminated model asset name.
     * @return The appended entity's zero-based index, or -1 if an argument is
     *         null, the asset is unknown, or the asset has no usable mesh.
     * @warning Appending may invalidate references or pointers into the
     *          scene's entity storage. Re-query entity count and indices.
     */
    int (*pfnSceneSpawn)(CAssetLibrary* pAssets, CScene* pScene, const char* pAssetName);

    /**
     * @brief Removes an entity from the scene immediately.
     *
     * The host dispatches an entity-deleted event before erasing the entity.
     * Out-of-range indices are ignored. This function does not provide an
     * operation result.
     *
     * @param pScene Scene to mutate; normally pass ctx->pScene. Must be valid.
     * @param index Zero-based index of the entity to remove.
     * @warning Erasing the entity can shift later indices and invalidate
     *          references into scene storage. Re-query cached indices and
     *          entityCount after the call.
     */
    void (*pfnSceneDelete)(CScene* pScene, int index);

    // -------------------------------------------------------------------------
    // Component registry
    // -------------------------------------------------------------------------

    /**
     * @brief Borrowed pointer to the host's component factory registry.
     *
     * This is the registry used by the component factory registration helpers.
     * The host owns it; plugins must not delete or retain it beyond their
     * loaded lifetime. Check for nullptr when supporting older host versions.
     */
    CComponentFactoryRegistry* pComponentRegistry;

    /**
     * @brief Borrowed pointer to the host plugin manager.
     *
     * The UI and event callback registration helpers use this host-owned
     * manager. Plugins must not delete or retain it beyond their loaded
     * lifetime. Check for nullptr when supporting older host versions.
     */
    CPluginManager* pPluginManager;

    /**
     * @brief Borrowed pointer to the host's model asset library.
     *
     * Asset enumeration and scene-spawn helpers use this library. The host
     * owns it; plugins must not delete it or retain it beyond their loaded
     * lifetime. Check for nullptr when supporting older host versions.
     */
    CAssetLibrary* pAssets;

    /**
     * @brief Borrowed, null-terminated path for the project currently open.
     *
     * Pass this value to pfnSceneSave() when saving the current scene. The
     * string is host-owned; do not modify, free, or retain it beyond the
     * callback. Check for nullptr when supporting older host versions.
     */
    const char* pProjectPath;

    // -------------------------------------------------------------------------
    // ABI extension fields. Keep new function pointers at the end of the
    // context so plugins built against older headers keep their offsets.
    // -------------------------------------------------------------------------

    /**
     * @brief Gets an entity's parent index in the scene hierarchy.
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @return Parent entity index, -1 when the entity is a root, or -1 when
     *         the scene/index is invalid. A returned index is transient and
     *         may change after scene edits.
     */
    int (*pfnEntityGetParent)(CScene* pScene, int index);

    /**
     * @brief Changes an entity's parent in the scene hierarchy.
     *
     * The host reparents through its hierarchy operation, preserving the
     * entity's world transform where the operation succeeds.
     *
     * @param pScene Scene to mutate; normally pass ctx->pScene.
     * @param index Zero-based entity index to reparent.
     * @param parentIndex Zero-based new parent index, or -1 to make the entity
     *                    a root.
     * @return true when the operation changes the hierarchy; false for null
     *         scene, invalid indices, self-parenting, or a rejected hierarchy
     *         operation.
     * @note A parent index must refer to an entity in the same scene.
     */
    bool (*pfnEntitySetParent)(CScene* pScene, int index, int parentIndex);

    /**
     * @brief Counts the components attached to an entity.
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @return Number of attached components, or 0 for a null scene, invalid
     *         entity index, or entity without a component manager.
     */
    int (*pfnEntityGetComponentCount)(CScene* pScene, int index);

    /**
     * @brief Gets the registered type name of one attached component.
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param componentIndex Zero-based position in the component list.
     * @return Pointer to a thread-local copy of the type name, or nullptr for
     *         invalid arguments or a missing component. The pointer is
     *         overwritten by the next call to this function on the same
     *         thread; copy the string if it must be retained.
     */
    const char* (*pfnEntityGetComponentType)(CScene* pScene, int index, int componentIndex);

    /**
     * @brief Checks whether an entity has a component of the requested type.
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param pTypeName Null-terminated, exact registered component type name.
     * @return true when a component of that type is attached; false for a
     *         null scene/name, invalid entity index, or no matching component.
     */
    bool (*pfnEntityHasComponent)(CScene* pScene, int index, const char* pTypeName);

    /**
     * @brief Creates and attaches a component using its registered factory.
     *
     * The entity must have a component manager and the type must have been
     * registered by the host or a plugin. Existing components of the same
     * type are not duplicated by this helper.
     *
     * @param pScene Scene to mutate; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param pTypeName Null-terminated, exact registered component type name.
     * @return true if a new component was created and attached; false if the
     *         arguments are invalid, the type is unavailable, or attachment
     *         cannot be performed.
     */
    bool (*pfnEntityAddComponent)(CScene* pScene, int index, const char* pTypeName);

    /**
     * @brief Removes the first component whose type name matches.
     * @param pScene Scene to mutate; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param pTypeName Null-terminated, exact component type name to remove.
     * @return true when a component was removed; false for invalid arguments,
     *         an entity without a component manager, or no match.
     * @warning Removing a component drops the component manager's ownership.
     *          The instance may be destroyed when any other shared ownership
     *          ends; do not rely on a raw pointer remaining valid.
     */
    bool (*pfnEntityRemoveComponent)(CScene* pScene, int index, const char* pTypeName);

    /**
     * @brief Sets the enabled flag on the first matching component.
     *
     * Disabling a component does not remove it from the entity or unregister
     * its type.
     *
     * @param pScene Scene to mutate; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param pTypeName Null-terminated, exact component type name.
     * @param enabled New enabled state.
     * @return true when a matching component was found and updated; false
     *         otherwise.
     */
    bool (*pfnEntitySetComponentEnabled)(CScene* pScene, int index,
                                         const char* pTypeName, bool enabled);

    /**
     * @brief Counts the tags attached to an entity.
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @return Tag count, or 0 for a null scene or invalid entity index.
     */
    int (*pfnEntityGetTagCount)(CScene* pScene, int index);

    /**
     * @brief Gets a tag string by its position in an entity's tag list.
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param tagIndex Zero-based tag index in
     *                 [0, pfnEntityGetTagCount(pScene, index)).
     * @return Pointer to the host-owned, null-terminated tag, or nullptr for
     *         invalid arguments. Do not free or modify it; tag changes or
     *         scene changes may invalidate the pointer.
     */
    const char* (*pfnEntityGetTag)(CScene* pScene, int index, int tagIndex);

    /**
     * @brief Checks whether an entity has an exact matching tag.
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param pTag Null-terminated tag value to find.
     * @return true if the tag is present; false for null arguments, an invalid
     *         index, or when no exact match exists.
     */
    bool (*pfnEntityHasTag)(CScene* pScene, int index, const char* pTag);

    /**
     * @brief Adds a non-empty tag to an entity.
     *
     * Existing tags are not duplicated. The host copies the tag string into
     * scene-owned storage before returning.
     *
     * @param pScene Scene to mutate; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param pTag Null-terminated tag value to add; empty strings are rejected.
     * @return true when the tag was added; false for invalid arguments or
     *         when the entity already has that tag.
     */
    bool (*pfnEntityAddTag)(CScene* pScene, int index, const char* pTag);

    /**
     * @brief Removes an exact matching tag from an entity.
     * @param pScene Scene to mutate; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @param pTag Null-terminated tag value to remove.
     * @return true when the tag was found and removed; false for invalid
     *         arguments or when there is no match.
     */
    bool (*pfnEntityRemoveTag)(CScene* pScene, int index, const char* pTag);

    /**
     * @brief Gets the number of model assets currently registered.
     * @param pAssets Host asset library; normally pass ctx->pAssets.
     * @return Registered model count, or 0 when @p pAssets is null.
     * @note This count describes model assets, not every file in the project.
     */
    int (*pfnAssetGetCount)(CAssetLibrary* pAssets);

    /**
     * @brief Gets a model asset's registered name by index.
     * @param pAssets Host asset library; normally pass ctx->pAssets.
     * @param index Zero-based asset index in [0, pfnAssetGetCount(pAssets)).
     * @return Pointer to a host-owned, null-terminated asset name, or nullptr
     *         for invalid arguments. Do not modify or free the string; asset
     *         library changes may invalidate it.
     */
    const char* (*pfnAssetGetName)(CAssetLibrary* pAssets, int index);

    /**
     * @brief Gets the host's numeric EObjectType for a model asset.
     * @param pAssets Host asset library; normally pass ctx->pAssets.
     * @param index Zero-based asset index in [0, pfnAssetGetCount(pAssets)).
     * @return Integer representation of the asset's EObjectType, or -1 for a
     *         null asset library or invalid index. Interpret the result using
     *         the EObjectType values from the matching engine version.
     */
    int (*pfnAssetGetType)(CAssetLibrary* pAssets, int index);

    /**
     * @brief Checks whether a model asset is registered under a given name.
     * @param pAssets Host asset library; normally pass ctx->pAssets.
     * @param pName Null-terminated asset name to find.
     * @return true when an exact registered model name is found; false when
     *         either argument is null or the name is unknown.
     */
    bool (*pfnAssetExists)(CAssetLibrary* pAssets, const char* pName);

    /**
     * @brief Creates an asset-backed entity and assigns its local position.
     *
     * The asset-backed entity is appended first, then its transform position is
     * updated when it has a transform component. The host dispatches the
     * corresponding entity-created event during creation and a transform
     * changed event when a transform is present.
     *
     * @param pAssets Asset library in which to resolve @p pAssetName; normally
     *                pass ctx->pAssets.
     * @param pScene Scene to mutate; normally pass ctx->pScene.
     * @param pAssetName Null-terminated registered model asset name.
     * @param x Local X position to assign.
     * @param y Local Y position to assign.
     * @param z Local Z position to assign.
     * @return The appended entity's zero-based index, or -1 if asset creation
     *         fails.
     * @warning Appending may invalidate references or pointers into scene
     *          entity storage. Re-query counts and indices after the call.
     */
    int (*pfnSceneSpawnEx)(CAssetLibrary* pAssets, CScene* pScene,
                           const char* pAssetName, float x, float y, float z);

    /**
     * @brief Gets the primary selected entity index.
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @return Primary selected index, or -1 when the scene is null or has no
     *         primary selection. Re-query after scene edits because indices
     *         may shift.
     */
    int (*pfnSceneGetSelected)(CScene* pScene);

    /**
     * @brief Updates the scene's entity selection.
     *
     * A non-additive selection replaces the current selection. An additive
     * selection toggles the specified entity in the multi-selection. Every
     * accepted selection operation dispatches an entity-selected event.
     *
     * @param pScene Scene to mutate; normally pass ctx->pScene.
     * @param index Zero-based entity index, or -1 to clear the full selection
     *              when @p additive is false.
     * @param additive If false, replace selection; if true, toggle the indexed
     *                 entity without clearing other selected entities.
     * @note Null scenes and indices outside the valid range (apart from the
     *       clear-selection case) are ignored.
     */
    void (*pfnSceneSetSelected)(CScene* pScene, int index, bool additive);

    /**
     * @brief Gets the number of entities in the current multi-selection.
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @return Selection count, or 0 when @p pScene is null.
     */
    int (*pfnSceneGetSelectionCount)(CScene* pScene);

    /**
     * @brief Gets a selected entity by its position in the selection list.
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @param selectionIndex Zero-based selection-list index in
     *                      [0, pfnSceneGetSelectionCount(pScene)).
     * @return The selected entity's current index, or -1 for a null scene or
     *         invalid selection index. Re-query after scene edits.
     */
    int (*pfnSceneGetSelectedAt)(CScene* pScene, int selectionIndex);

    /**
     * @brief Begins recording a plugin scene operation as an undoable command.
     *
     * Begin the command before making the scene changes that should be grouped
     * into one undo step, and always pair it with pfnSceneEndCommand(). The
     * host accepts commands only for its currently managed scene.
     *
     * @param pScene Scene to mutate; normally pass ctx->pScene.
     * @param pDescription Reserved for a human-readable command label. The
     *                     current host bridge ignores this value.
     * @note Nested command starts are ignored by the host. The host records
     *       the scene state before subsequent mutations; it does not retain
     *       this pointer.
     */
    void (*pfnSceneBeginCommand)(CScene* pScene, const char* pDescription);

    /**
     * @brief Finishes the active plugin scene command.
     *
     * Call with the same scene passed to pfnSceneBeginCommand(). The host
     * completes the editor's current command if the supplied scene is its
     * managed scene.
     *
     * @param pScene Scene associated with the command; normally pass
     *               ctx->pScene.
     * @note Calling when no matching command is active is safe.
     */
    void (*pfnSceneEndCommand)(CScene* pScene);

    /**
     * @brief Applies the most recent available editor undo operation.
     * @return true when an undo operation was available and applied; false
     *         when the host editor is unavailable or its undo history is empty.
     * @note Operates on the host editor's current scene and undo stack, not an
     *       arbitrary scene pointer.
     */
    bool (*pfnSceneUndo)();

    /**
     * @brief Reapplies the most recently undone editor operation.
     * @return true when a redo operation was available and applied; false
     *         when the host editor is unavailable or its redo history is empty.
     * @note Operates on the host editor's current scene and redo stack, not an
     *       arbitrary scene pointer.
     */
    bool (*pfnSceneRedo)();

    /**
     * @brief Reports the editor's dirty state for the supplied scene.
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @return true when the editor identifies this as its current scene and
     *         that scene has unsaved changes; false otherwise.
     */
    bool (*pfnSceneIsDirty)(CScene* pScene);

    /**
     * @brief Selects an entity and focuses the editor camera on it.
     *
     * The camera is moved to focus the entity's world-space origin. This is an
     * editor view operation and does not alter the entity's saved transform.
     *
     * @param index Zero-based index in the host editor's current scene.
     * @note Invalid indices and an unavailable editor are ignored.
     */
    void (*pfnEditorFocusEntity)(int index);

    /**
     * @brief Replaces or clears the editor status-bar message.
     * @param pMessage Null-terminated message text. Pass nullptr to clear the
     *                 current status message; the host copies non-null text.
     * @note The message is editor UI state and is not stored in the scene.
     */
    void (*pfnEditorSetStatusMessage)(const char* pMessage);

    /**
     * @brief Requests that the host editor refresh its scene view.
     *
     * This is a rendering hint and does not modify scene data. It may be
     * unnecessary in configurations where the host redraws continuously.
     */
    void (*pfnEditorRequestSceneRedraw)();

    /**
     * @brief Selects an asset in the browser and opens a model preview if found.
     * @param pAssetName Null-terminated asset-browser name. The host copies the
     *                   supplied value before returning.
     * @note Unknown names can still become the browser's selected name, but
     *       only a registered model asset opens the model viewer. Null or empty
     *       names are ignored.
     */
    void (*pfnEditorOpenAsset)(const char* pAssetName);

    /**
     * @brief Subscribes a callback to notifications of one event kind.
     *
     * Register during plugin initialization. The callback is invoked by the
     * host when the selected event occurs; registrations are automatically
     * removed when the plugin is unloaded.
     *
     * @param pCtx Current plugin context, used by the host to locate the
     *             callback registry. Do not retain it.
     * @param event Event kind to observe.
     * @param pfnCallback Callback function to register; it must remain loaded
     *                    for as long as the registration exists.
     * @note This field is appended for ABI compatibility with older plugins.
     */
    void (*pfnRegisterEventCallback)(SPluginContext* pCtx, EPluginEvent event,
                                     FPluginEventCallback pfnCallback);

    /**
     * @brief Removes matching callback registration(s) for an event.
     * @param pCtx Current plugin context used to locate the host registry.
     * @param event Event kind originally passed to registration.
     * @param pfnCallback Exact function pointer originally registered.
     * @note If the matching registration does not exist, the host leaves its
     *       event registry unchanged.
     */
    void (*pfnUnregisterEventCallback)(SPluginContext* pCtx, EPluginEvent event,
                                       FPluginEventCallback pfnCallback);

    /**
     * @brief Gets the asset name associated with an entity's mesh component.
     * @param pScene Scene to query; normally pass ctx->pScene.
     * @param index Zero-based entity index in [0, entityCount).
     * @return Pointer to a host-owned, null-terminated asset name, or nullptr
     *         when the scene/index is invalid, no mesh exists, or the mesh has
     *         no asset name. Do not free or modify the returned string.
     * @note This field is appended for ABI compatibility with older plugins.
     */
    const char* (*pfnEntityGetAssetName)(CScene* pScene, int index);

    /**
     * @brief Draws a line chart from a contiguous sequence of float samples.
     *
     * The chart is rendered in the current plugin window or UI region. The
     * host reads the sample array during this call and does not retain it.
     *
     * @param pLabel Non-null, null-terminated widget label. An ImGui hidden ID suffix
     *               such as "##usage" may be used to avoid visible text.
     * @param pValues Pointer to the first sample; must be non-null.
     * @param valueCount Number of samples to draw; must be greater than zero.
     * @param minimum Fixed minimum value shown on the vertical axis.
     * @param maximum Fixed maximum value shown on the vertical axis.
     * @note The graph has a host-defined height. This thin wrapper passes the
     *       sample pointer and count directly to the UI backend, so both must
     *       describe a valid contiguous array for the duration of the call.
     */
    void (*pfnUiPlotLines)(const char* pLabel, const float* pValues,
                           int valueCount, float minimum, float maximum);

    /**
     * @brief Per-thread utilization snapshots supplied by the host.
     * @note The array and its referenced names/history buffers are host-owned
     *       and valid only for the duration of the current plugin callback.
     *       Copy any values a plugin needs to retain.
     */
    const SPluginThreadUsage* pThreadUsages;

    /** @brief Number of entries in @p pThreadUsages. */
    int threadUsageCount;

    /**
     * @brief Displays a single-line editor for a caller-owned UTF-8 string.
     *
     * The string is edited directly in the supplied buffer. The buffer must
     * already contain a null terminator and remain writable for the duration
     * of the call. This API does not support ImGui input callbacks or flags.
     *
     * @param pLabel Null-terminated widget label.
     * @param pBuffer Writable, null-terminated character buffer.
     * @param bufferSize Capacity of @p pBuffer in bytes, including space for
     *                   the terminating null character; must be greater than
     *                   zero.
     * @return true when the text value changes; otherwise false.
     * @note The plugin owns the buffer and is responsible for its lifetime and
     *       correct UTF-8 encoding.
     */
    bool (*pfnUiInputText)(const char* pLabel, char* pBuffer, size_t bufferSize);

    /**
     * @brief Displays a slider for editing an integer within a closed range.
     * @param pLabel Null-terminated widget label.
     * @param pValue Pointer to the integer value; it is updated when the user
     *               changes the slider.
     * @param minimum Inclusive lower bound.
     * @param maximum Inclusive upper bound; must be greater than @p minimum.
     * @return true when the value changes; otherwise false.
     */
    bool (*pfnUiSliderInt)(const char* pLabel, int* pValue, int minimum, int maximum);

    /**
     * @brief Displays a draggable field for editing a floating-point value.
     * @param pLabel Null-terminated widget label.
     * @param pValue Pointer to the value; it is updated while the user drags.
     * @param speed Change in value per horizontal pixel of mouse movement.
     *              Must be positive for normal drag behavior.
     * @param minimum Lower bound. A bound is applied only when
     *                @p minimum is less than @p maximum.
     * @param maximum Upper bound. A bound is applied only when
     *                @p minimum is less than @p maximum.
     * @return true when the value changes; otherwise false.
     */
    bool (*pfnUiDragFloat)(const char* pLabel, float* pValue, float speed,
                           float minimum, float maximum);

    /**
     * @brief Displays a draggable field for editing an integer value.
     * @param pLabel Null-terminated widget label.
     * @param pValue Pointer to the value; it is updated while the user drags.
     * @param speed Change in value per horizontal pixel of mouse movement.
     *              Must be positive for normal drag behavior.
     * @param minimum Lower bound. A bound is applied only when
     *                @p minimum is less than @p maximum.
     * @param maximum Upper bound. A bound is applied only when
     *                @p minimum is less than @p maximum.
     * @return true when the value changes; otherwise false.
     */
    bool (*pfnUiDragInt)(const char* pLabel, int* pValue, float speed,
                         int minimum, int maximum);

    /**
     * @brief Displays a dropdown populated from a caller-owned array of labels.
     *
     * The label array and its strings are read only during this call and are
     * not retained by the host.
     *
     * @param pLabel Null-terminated widget label.
     * @param pItems Array containing @p itemCount non-null, null-terminated
     *               item labels.
     * @param itemCount Number of labels in @p pItems; must be greater than
     *                  zero.
     * @param pSelectedIndex Pointer to the selected item index. On entry it
     *                       must be in the range [0, @p itemCount); the host
     *                       updates it when the user selects another item.
     * @return true when the selected index changes; otherwise false.
     */
    bool (*pfnUiCombo)(const char* pLabel, const char* const* pItems,
                       int itemCount, int* pSelectedIndex);

    /**
     * @brief Displays a progress bar in the current UI region.
     * @param fraction Progress fraction, where 0.0 is empty and 1.0 is
     *                 complete. Values outside this range are clamped by the
     *                 underlying UI implementation.
     * @param pOverlay Optional null-terminated text drawn over the bar.
     *                 Pass nullptr to omit the overlay.
     */
    void (*pfnUiProgressBar)(float fraction, const char* pOverlay);

    /**
     * @brief Shows a tooltip for the most recently submitted UI item.
     * @param pText Null-terminated tooltip text; nullptr is ignored.
     * @note The tooltip is shown only while the last item is hovered. Call
     *       immediately after the widget whose tooltip should be displayed.
     */
    void (*pfnUiSetTooltip)(const char* pText);

    /**
     * @brief Draws a collapsible header and reports whether its section is open.
     * @param pLabel Null-terminated header label. Labels are also used as UI
     *               identifiers; append a hidden ID suffix when labels repeat.
     * @return true while the section is expanded and its contents should be
     *         submitted; false while collapsed.
     * @note This helper does not require a matching end call.
     */
    bool (*pfnUiCollapsingHeader)(const char* pLabel);

    /**
     * @brief Begins a scrollable child region inside the current window.
     *
     * A dimension of zero uses the remaining available space along that axis;
     * a positive dimension requests a fixed size. Negative sizes follow
     * ImGui's alignment-to-boundary behavior.
     *
     * @param pId Null-terminated identifier unique among child regions in the
     *            current window.
     * @param width Requested width in UI units.
     * @param height Requested height in UI units.
     * @return true when the child contents are visible and may be submitted.
     * @note Always call pfnUiEndChild() after this function, even when it
     *       returns false.
     */
    bool (*pfnUiBeginChild)(const char* pId, float width, float height);

    /**
     * @brief Ends the child region opened by the most recent pfnUiBeginChild().
     * @note Must be called exactly once for every pfnUiBeginChild() call,
     *       regardless of that call's return value.
     */
    void (*pfnUiEndChild)();

    /**
     * @brief Begins a tab bar that groups tab items.
     * @param pId Null-terminated identifier unique among tab bars in the
     *            current UI context.
     * @return true when the tab bar is open and tab items should be submitted.
     * @note Call pfnUiEndTabBar() only when this function returns true.
     */
    bool (*pfnUiBeginTabBar)(const char* pId);

    /**
     * @brief Ends the tab bar opened by the most recent successful
     *        pfnUiBeginTabBar() call.
     */
    void (*pfnUiEndTabBar)();

    /**
     * @brief Adds a tab item to the current tab bar and begins its contents.
     * @param pLabel Null-terminated visible tab label. Append a hidden ID
     *               suffix when otherwise-identical tab labels are needed.
     * @return true when this tab is selected and its contents should be
     *         submitted.
     * @note Call pfnUiEndTabItem() only when this function returns true.
     */
    bool (*pfnUiBeginTabItem)(const char* pLabel);

    /**
     * @brief Ends the tab item opened by the most recent successful
     *        pfnUiBeginTabItem() call.
     */
    void (*pfnUiEndTabItem)();

    /**
     * @brief Begins a disabled scope for subsequent widgets.
     * @param disabled When true, widgets in this scope cannot be interacted
     *                 with and are rendered using the disabled style. When
     *                 false, the scope has no disabling effect.
     * @note Always pair this call with pfnUiEndDisabled(). Disabled scopes
     *       can be nested; an inner scope cannot re-enable an outer disabled
     *       scope.
     */
    void (*pfnUiBeginDisabled)(bool disabled);

    /**
     * @brief Ends the disabled scope opened by the most recent
     *        pfnUiBeginDisabled() call.
     */
    void (*pfnUiEndDisabled)();

    /**
     * @brief Displays a selectable row or item and updates its selected state.
     * @param pLabel Null-terminated item label; use a hidden ID suffix when
     *               labels are not unique.
     * @param pSelected Pointer to the current selection state. The value is
     *                  updated when the item is clicked.
     * @return true when the item was activated or its selection changed;
     *         otherwise false.
     * @note The selection state is owned by the plugin. This helper does not
     *       implement list or multi-selection policy.
     */
    bool (*pfnUiSelectable)(const char* pLabel, bool* pSelected);

    /**
     * @brief Begins a table with a fixed number of columns.
     *
     * The table supports resizing and reordering columns. When sorting is
     * enabled, sortable columns display sort controls; plugins are responsible
     * for applying the requested sort to their own data.
     *
     * @param pId Null-terminated identifier unique among tables in the current
     *            UI context.
     * @param columnCount Number of columns; must be greater than zero.
     * @param sortable Enables sort controls for columns declared sortable.
     * @return true when the table is visible and its contents should be
     *         submitted; false when it is clipped or otherwise not visible.
     * @note Call setup, header, row, column, and sort-spec functions only
     *       after a successful begin. Pair successful calls with
     *       pfnUiEndTable().
     */
    bool (*pfnUiBeginTable)(const char* pId, int columnCount, bool sortable);

    /**
     * @brief Ends the table opened by the most recent successful
     *        pfnUiBeginTable() call.
     */
    void (*pfnUiEndTable)();

    /**
     * @brief Declares a column in the current table.
     * @param pLabel Null-terminated header label for the column.
     * @param sortable When false, disables sorting for this column even if
     *                 sorting is enabled on the table.
     * @note Call after pfnUiBeginTable() returns true and before submitting
     *       the header row or any table rows.
     */
    void (*pfnUiTableSetupColumn)(const char* pLabel, bool sortable);

    /**
     * @brief Submits the header row using the labels declared by
     *        pfnUiTableSetupColumn().
     * @note Call at most once per table and before its data rows. Headers are
     *       required for users to interact with column sorting.
     */
    void (*pfnUiTableHeadersRow)();

    /**
     * @brief Starts a new row in the current table.
     * @note The first cell becomes the current cell. Call
     *       pfnUiTableNextColumn() to advance to subsequent cells.
     */
    void (*pfnUiTableNextRow)();

    /**
     * @brief Advances to the next cell in the current table row.
     * @return true when the current cell is visible and may be populated;
     *         false when it is clipped. The cursor still advances either way.
     */
    bool (*pfnUiTableNextColumn)();

    /**
     * @brief Reads the primary sort specification for the current table.
     *
     * This helper exposes only the first active sort key. Sort requests are
     * meaningful only while the current table is inside a successful
     * pfnUiBeginTable()/pfnUiEndTable() scope.
     *
     * @param pColumnIndex Output pointer receiving the zero-based column index;
     *                     must be non-null.
     * @param pSortDirection Output pointer receiving 1 for ascending or 2 for
     *                       descending; must be non-null. A value of 0 means
     *                       no direction is available.
     * @param pSpecsDirty Output pointer receiving whether the specification
     *                    changed since it was last handled; must be non-null.
     * @return true when an active sort specification is available and the
     *         output values were written; false if there is no active sort
     *         specification or an output pointer is null.
     * @note When @p pSpecsDirty is true, apply the requested ordering to the
     *       plugin-owned rows and then call pfnUiTableClearSortDirty().
     */
    bool (*pfnUiTableGetSortSpec)(int* pColumnIndex, int* pSortDirection,
                                  bool* pSpecsDirty);

    /**
     * @brief Marks the current table's sort specification as handled.
     * @note Call after applying a changed specification returned by
     *       pfnUiTableGetSortSpec(). Must be called inside the current table
     *       scope.
     */
    void (*pfnUiTableClearSortDirty)();

    /**
     * @brief Pushes a string onto the current UI identifier stack.
     *
     * Use this around repeated widgets with identical labels, such as rows in
     * a loop, to give each widget a unique ID without changing its visible
     * label.
     *
     * @param pId Non-null, null-terminated identifier component.
     * @note Pair every call with pfnUiPopID(), in last-in/first-out order,
     *       before returning from the drawing callback.
     */
    void (*pfnUiPushID)(const char* pId);

    /**
     * @brief Pops the most recently pushed UI identifier component.
     * @note Call only when a matching pfnUiPushID() has not yet been popped.
     */
    void (*pfnUiPopID)();

    /**
     * @brief Sets the width of the next submitted UI item.
     * @param width Requested width in UI units. Negative values use the
     *              underlying UI backend's special sizing behavior.
     * @note Applies only to the next item. Call before submitting that item.
     */
    void (*pfnUiSetNextItemWidth)(float width);

    /**
     * @brief Adds vertical spacing between items in the current UI layout.
     * @note Call only while a plugin UI drawing callback is active.
     */
    void (*pfnUiSpacing)();

    /**
     * @brief Moves the current layout cursor horizontally to the right.
     * @param amount Horizontal indent in UI units. A non-positive value uses
     *                the UI backend's default indentation amount.
     * @note Pair with pfnUiUnindent() to restore the prior layout position.
     */
    void (*pfnUiIndent)(float amount);

    /**
     * @brief Reverses a preceding layout indentation.
     * @param amount Horizontal distance in UI units. Pass the same positive
     *                value used for the corresponding pfnUiIndent() call, or a
     *                non-positive value to use the backend's default.
     */
    void (*pfnUiUnindent)(float amount);

    /**
     * @brief Displays plain text that wraps at the available content width.
     * @param pText Null-terminated text to display; must not be null.
     * @note The host treats the text as data, not as a printf-style format
     *       string, and does not retain the pointer.
     */
    void (*pfnUiTextWrapped)(const char* pText);

    /**
     * @brief Displays one bulleted line of plain text.
     * @param pText Null-terminated text to display; must not be null.
     * @note The host treats the text as data, not as a printf-style format
     *       string, and does not retain the pointer.
     */
    void (*pfnUiBulletText)(const char* pText);

    /**
     * @brief Displays an integer input field for direct numeric entry.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pValue Non-null pointer to the plugin-owned integer to read and
     *               update.
     * @return true when the value changes; otherwise false.
     */
    bool (*pfnUiInputInt)(const char* pLabel, int* pValue);

    /**
     * @brief Displays a multiline editor for a caller-owned UTF-8 string.
     *
     * The editor modifies the provided buffer in place. This wrapper does not
     * enable resize callbacks or custom input flags.
     *
     * @param pLabel Null-terminated widget label and identifier.
     * @param pBuffer Writable, null-terminated character buffer.
     * @param bufferSize Capacity of @p pBuffer in bytes, including its final
     *                   null terminator; must be greater than zero.
     * @param width Requested widget width in UI units; zero uses available
     *              width.
     * @param height Requested widget height in UI units; zero uses the
     *               backend's default height.
     * @return true when the text changes; otherwise false.
     * @note The plugin owns the buffer and must keep it writable and valid for
     *       the duration of this call.
     */
    bool (*pfnUiInputTextMultiline)(const char* pLabel, char* pBuffer,
                                    size_t bufferSize, float width, float height);

    /**
     * @brief Displays an RGBA color editor and updates four normalized channels.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aColor Writable array of at least four floats in red, green,
     *               blue, alpha order. Channels are expected in [0.0, 1.0].
     * @return true when any channel changes; otherwise false.
     * @note The host reads and writes the array during this call only.
     */
    bool (*pfnUiColorEdit4)(const char* pLabel, float aColor[4]);

    /**
     * @brief Displays one button in a mutually exclusive integer choice.
     * @param pLabel Null-terminated visible label and widget identifier.
     * @param pValue Non-null pointer to the plugin-owned selected value.
     * @param buttonValue Integer value represented by this radio button.
     * @return true when this button is selected by the user and changes the
     *         value; otherwise false.
     * @note Multiple radio buttons sharing the same @p pValue form a choice
     *       group. The host updates @p pValue directly.
     */
    bool (*pfnUiRadioButton)(const char* pLabel, int* pValue, int buttonValue);

    /**
     * @brief Requests that a popup with the given identifier open.
     * @param pId Non-null, null-terminated popup identifier.
     * @note Call before pfnUiBeginPopup() or pfnUiBeginPopupModal() in the
     *       active UI frame. A popup is opened in the current UI context.
     */
    void (*pfnUiOpenPopup)(const char* pId);

    /**
     * @brief Begins drawing a non-modal popup if it is open.
     * @param pId Non-null, null-terminated identifier matching
     *            pfnUiOpenPopup().
     * @return true when the popup is open and its contents may be submitted.
     * @note Pair a true result with exactly one pfnUiEndPopup(). Do not call
     *       the end function when this returns false.
     */
    bool (*pfnUiBeginPopup)(const char* pId);

    /**
     * @brief Begins drawing a modal popup if it is open.
     *
     * A modal blocks interaction with the rest of the UI until dismissed or
     * closed through pfnUiCloseCurrentPopup().
     *
     * @param pId Non-null, null-terminated identifier matching
     *            pfnUiOpenPopup().
     * @param pOpen Optional plugin-owned open-state pointer. If non-null, the
     *              host may set it to false when the modal's close control is
     *              used.
     * @return true when the modal is open and its contents may be submitted.
     * @note Pair a true result with exactly one pfnUiEndPopup().
     */
    bool (*pfnUiBeginPopupModal)(const char* pId, bool* pOpen);

    /**
     * @brief Ends the popup opened by a successful popup begin call.
     * @note Pair with either pfnUiBeginPopup() or pfnUiBeginPopupModal() when
     *       that function returned true.
     */
    void (*pfnUiEndPopup)();

    /**
     * @brief Closes the innermost currently open popup.
     * @note Call from inside the popup's successful begin/end scope, typically
     *       after the user activates a confirmation or cancel control.
     */
    void (*pfnUiCloseCurrentPopup)();

    /**
     * @brief Sets the size of the next window opened through pfnUiBegin().
     * @param width Requested width in UI units.
     * @param height Requested height in UI units.
     * @note The host applies this size the first time that window identifier
     *       is used. Call before pfnUiBegin(); it does not resize an existing
     *       window on every frame.
     */
    void (*pfnUiSetNextWindowSize)(float width, float height);

    /**
     * @brief Sets the position of the next window opened through pfnUiBegin().
     * @param x Requested horizontal position in UI units.
     * @param y Requested vertical position in UI units.
     * @note The host applies this position the first time that window
     *       identifier is used. Call before pfnUiBegin(); it does not move an
     *       existing window on every frame.
     */
    void (*pfnUiSetNextWindowPos)(float x, float y);

    /**
     * @brief Displays a loaded host texture from the asset library.
     *
     * The plugin passes an asset-library index rather than a renderer handle;
     * the host resolves and draws the corresponding texture without
     * transferring ownership.
     *
     * @param pAssets Host asset library; normally pass ctx->pAssets.
     * @param textureIndex Zero-based index in [0, TextureCount()).
     * @param width Requested image width in UI units.
     * @param height Requested image height in UI units.
     * @return true when a valid texture was submitted for drawing; false for
     *         invalid arguments, an invalid index, or an unloaded texture.
     * @warning Refreshing the asset library can change texture indices; obtain
     *          the index again after a refresh.
     */
    bool (*pfnUiImage)(CAssetLibrary* pAssets, int textureIndex,
                       float width, float height);

    /**
     * @brief Displays a clickable image button backed by a loaded host texture.
     * @param pLabel Null-terminated widget label and identifier. Use a unique
     *               identifier when multiple buttons are drawn in a loop.
     * @param pAssets Host asset library; normally pass ctx->pAssets.
     * @param textureIndex Zero-based index in [0, TextureCount()).
     * @param width Requested image width in UI units, excluding button padding.
     * @param height Requested image height in UI units, excluding button
     *               padding.
     * @return true when the image button is activated; false for invalid
     *         arguments, an invalid index, an unloaded texture, or no click.
     * @warning Refreshing the asset library can change texture indices; obtain
     *          the index again after a refresh.
     */
    bool (*pfnUiImageButton)(const char* pLabel, CAssetLibrary* pAssets,
                             int textureIndex, float width, float height);

    /**
     * @brief Gets the number of loaded textures in the host asset library.
     * @param pAssets Host asset library; normally pass ctx->pAssets.
     * @return Number of loaded textures, or zero when @p pAssets is null.
     * @note Texture indices are valid only until the host refreshes the asset
     *       library. Re-query the count and indices after a refresh.
     */
    int (*pfnAssetGetTextureCount)(CAssetLibrary* pAssets);

    /**
     * @brief Gets a loaded texture's display name by index.
     * @param pAssets Host asset library; normally pass ctx->pAssets.
     * @param textureIndex Zero-based index in
     *                     [0, pfnAssetGetTextureCount(pAssets)).
     * @return Pointer to a host-owned, null-terminated name, or nullptr for
     *         invalid arguments. Do not modify or free the string; refreshing
     *         the asset library may invalidate it.
     */
    const char* (*pfnAssetGetTextureName)(CAssetLibrary* pAssets, int textureIndex);

    /**
     * @brief Begins an immediate-mode drag source for the last submitted item.
     * @return true while the item is actively being dragged and source content
     *         may be submitted; otherwise false.
     * @note Pair a true result with exactly one pfnUiEndDragDropSource().
     */
    bool (*pfnUiBeginDragDropSource)();

    /**
     * @brief Publishes typed bytes from the currently active drag source.
     * @param pType Non-null, null-terminated payload type identifier. Source
     *              and target must use the same identifier.
     * @param pData Pointer to the bytes to copy into the drag payload.
     * @param dataSize Number of bytes at @p pData; must be greater than zero.
     * @return true when the UI backend accepted the payload; otherwise false.
     * @note Call only between a successful pfnUiBeginDragDropSource() and its
     *       matching pfnUiEndDragDropSource(). The host copies the bytes and
     *       does not retain @p pData.
     */
    bool (*pfnUiSetDragDropPayload)(const char* pType, const void* pData,
                                    size_t dataSize);

    /**
     * @brief Ends the drag source opened by a successful source begin call.
     */
    void (*pfnUiEndDragDropSource)();

    /**
     * @brief Begins a drag target for the last submitted item.
     * @return true when the item is eligible to accept a payload; otherwise
     *         false.
     * @note Pair a true result with exactly one pfnUiEndDragDropTarget().
     */
    bool (*pfnUiBeginDragDropTarget)();

    /**
     * @brief Accepts a typed payload and copies its bytes into plugin storage.
     *
     * The payload's required byte count is reported even when the destination
     * buffer is too small. In that case no bytes are copied and the function
     * returns false.
     *
     * @param pType Non-null, null-terminated payload type identifier.
     * @param pBuffer Writable destination buffer; must be non-null when
     *                @p bufferSize is greater than zero.
     * @param bufferSize Capacity of @p pBuffer in bytes.
     * @param pPayloadSize Non-null output pointer receiving the payload's byte
     *                     count when a matching payload is present, or zero
     *                     when none is available.
     * @return true when a matching payload was present and completely copied;
     *         false when no matching payload exists, arguments are invalid, or
     *         the destination is too small.
     * @note Call only between a successful pfnUiBeginDragDropTarget() and its
     *       matching pfnUiEndDragDropTarget(). Validate copied bytes according
     *       to the agreed payload type before using them.
     */
    bool (*pfnUiAcceptDragDropPayload)(const char* pType, void* pBuffer,
                                       size_t bufferSize, size_t* pPayloadSize);

    /**
     * @brief Ends the drag target opened by a successful target begin call.
     */
    void (*pfnUiEndDragDropTarget)();

    /**
     * @brief Begins a window with a menu bar enabled.
     *
     * This behaves like pfnUiBegin() but enables the host menu-bar window flag
     * so pfnUiBeginMenuBar() can be used inside the window.
     *
     * @param pTitle Null-terminated window title and identifier.
     * @return true when the window contents are visible and may be submitted.
     * @note Pair every call with pfnUiEnd(), including when this function
     *       returns false.
     */
    bool (*pfnUiBeginWithMenuBar)(const char* pTitle);

    /**
     * @brief Sets whether the next window opened by pfnUiBegin() starts
     *        collapsed.
     * @param collapsed True to request a collapsed title bar, false to request
     *                  an expanded window.
     * @note The host applies this state the first time the window identifier
     *       is used. Call before pfnUiBegin().
     */
    void (*pfnUiSetNextWindowCollapsed)(bool collapsed);

    /**
     * @brief Requests keyboard focus for the next window opened by
     *        pfnUiBegin().
     * @note Call before pfnUiBegin(). The focus request applies to the next
     *       window submission only.
     */
    void (*pfnUiSetNextWindowFocus)();

    /**
     * @brief Begins the menu bar of a window opened with menu-bar support.
     * @return true when menu items may be submitted.
     * @note Pair a true result with pfnUiEndMenuBar().
     */
    bool (*pfnUiBeginMenuBar)();

    /**
     * @brief Ends the menu bar opened by a successful pfnUiBeginMenuBar().
     */
    void (*pfnUiEndMenuBar)();

    /**
     * @brief Adds a menu command with shortcut text, enabled state, and an
     *        optional selected indicator.
     * @param pLabel Null-terminated visible command label.
     * @param pShortcut Optional null-terminated shortcut display text; may be
     *                  nullptr. This text is decorative and does not bind keys.
     * @param pSelected Optional pointer to a plugin-owned selected state. When
     *                  non-null, activating the menu item toggles the value.
     * @param enabled When false, the item is displayed disabled and cannot be
     *                activated.
     * @return true when the enabled item is activated; otherwise false.
     * @note Call inside a menu opened with pfnUiBeginMenu() or within a
     *       successful pfnUiBeginMenuBar() scope.
     */
    bool (*pfnUiMenuItemEx)(const char* pLabel, const char* pShortcut,
                            bool* pSelected, bool enabled);

    /**
     * @brief Begins a context popup associated with the most recently submitted
     *        UI item.
     * @param pId Null-terminated popup identifier.
     * @return true while the context popup is open and its content may be
     *         submitted.
     * @note Pair a true result with pfnUiEndPopup(). Submit an item before
     *       calling; use a unique identifier when the item has no stable ID.
     */
    bool (*pfnUiBeginPopupContextItem)(const char* pId);

    /**
     * @brief Begins a context popup associated with the current window.
     * @param pId Null-terminated popup identifier.
     * @return true while the context popup is open and its content may be
     *         submitted.
     * @note Pair a true result with pfnUiEndPopup().
     */
    bool (*pfnUiBeginPopupContextWindow)(const char* pId);

    /**
     * @brief Begins a tooltip window explicitly.
     * @return true when the tooltip scope is active.
     * @note Pair a true result with pfnUiEndTooltip().
     */
    bool (*pfnUiBeginTooltip)();

    /**
     * @brief Ends the tooltip scope opened by a successful
     *        pfnUiBeginTooltip().
     */
    void (*pfnUiEndTooltip)();

    /**
     * @brief Begins a combo popup with a caller-provided preview string.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pPreview Null-terminated preview string; may be nullptr when no
     *                 item is selected.
     * @return true while the popup is open and its contents may be submitted.
     * @note Pair a true result with pfnUiEndCombo().
     */
    bool (*pfnUiBeginCombo)(const char* pLabel, const char* pPreview);

    /**
     * @brief Ends a combo popup opened by a successful pfnUiBeginCombo().
     */
    void (*pfnUiEndCombo)();

    /**
     * @brief Displays a scrollable list box populated by caller-owned labels.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pItems Array of @p itemCount non-null, null-terminated labels.
     * @param itemCount Number of labels; must be greater than zero.
     * @param pSelectedIndex Non-null plugin-owned selected index. On entry it
     *                       must be in [0, @p itemCount); the host updates it
     *                       when another item is selected.
     * @param heightInItems Requested visible row count; non-positive values use
     *                      the backend's default list height.
     * @return true when the selected index changes; otherwise false.
     * @note The labels and strings are read only during this call.
     */
    bool (*pfnUiListBox)(const char* pLabel, const char* const* pItems,
                         int itemCount, int* pSelectedIndex, int heightInItems);

    /**
     * @brief Displays a collapsible tree node.
     * @param pLabel Null-terminated node label and identifier.
     * @return true when the node is open and child items may be submitted.
     * @note Pair a true result with pfnUiTreePop(); do not call TreePop when
     *       this function returns false.
     */
    bool (*pfnUiTreeNode)(const char* pLabel);

    /**
     * @brief Ends the tree node opened by a successful pfnUiTreeNode().
     */
    void (*pfnUiTreePop)();

    /**
     * @brief Displays a two-component floating-point input field.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aValues Writable array of at least two floats.
     * @return true when either component changes; otherwise false.
     */
    bool (*pfnUiInputFloat2)(const char* pLabel, float aValues[2]);

    /**
     * @brief Displays a three-component floating-point input field.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aValues Writable array of at least three floats.
     * @return true when any component changes; otherwise false.
     */
    bool (*pfnUiInputFloat3)(const char* pLabel, float aValues[3]);

    /**
     * @brief Displays a four-component floating-point input field.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aValues Writable array of at least four floats.
     * @return true when any component changes; otherwise false.
     */
    bool (*pfnUiInputFloat4)(const char* pLabel, float aValues[4]);

    /**
     * @brief Displays a two-component integer input field.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aValues Writable array of at least two integers.
     * @return true when either component changes; otherwise false.
     */
    bool (*pfnUiInputInt2)(const char* pLabel, int aValues[2]);

    /**
     * @brief Displays a three-component integer input field.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aValues Writable array of at least three integers.
     * @return true when any component changes; otherwise false.
     */
    bool (*pfnUiInputInt3)(const char* pLabel, int aValues[3]);

    /**
     * @brief Displays a four-component integer input field.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aValues Writable array of at least four integers.
     * @return true when any component changes; otherwise false.
     */
    bool (*pfnUiInputInt4)(const char* pLabel, int aValues[4]);

    /**
     * @brief Displays a two-component draggable floating-point field.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aValues Writable array of at least two floats.
     * @param speed Change in value per horizontal pixel; normally positive.
     * @param minimum Lower bound; bounds are applied only when minimum is less
     *                 than maximum.
     * @param maximum Upper bound; bounds are applied only when minimum is less
     *                 than maximum.
     * @return true when either component changes; otherwise false.
     */
    bool (*pfnUiDragFloat2)(const char* pLabel, float aValues[2], float speed,
                            float minimum, float maximum);

    /**
     * @brief Displays a three-component draggable floating-point field.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aValues Writable array of at least three floats.
     * @param speed Change in value per horizontal pixel; normally positive.
     * @param minimum Lower bound; bounds are applied only when minimum is less
     *                 than maximum.
     * @param maximum Upper bound; bounds are applied only when minimum is less
     *                 than maximum.
     * @return true when any component changes; otherwise false.
     */
    bool (*pfnUiDragFloat3)(const char* pLabel, float aValues[3], float speed,
                            float minimum, float maximum);

    /**
     * @brief Displays a four-component draggable floating-point field.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aValues Writable array of at least four floats.
     * @param speed Change in value per horizontal pixel; normally positive.
     * @param minimum Lower bound; bounds are applied only when minimum is less
     *                 than maximum.
     * @param maximum Upper bound; bounds are applied only when minimum is less
     *                 than maximum.
     * @return true when any component changes; otherwise false.
     */
    bool (*pfnUiDragFloat4)(const char* pLabel, float aValues[4], float speed,
                            float minimum, float maximum);

    /**
     * @brief Displays a color picker for a caller-owned RGBA color.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aColor Writable array of at least four normalized RGBA floats.
     * @return true when any channel changes; otherwise false.
     */
    bool (*pfnUiColorPicker4)(const char* pLabel, float aColor[4]);

    /**
     * @brief Reports whether the most recently submitted item is hovered.
     * @return true when the item is hovered and not blocked by another UI
     *         element; otherwise false.
     * @note Call immediately after the item to inspect.
     */
    bool (*pfnUiIsItemHovered)();

    /**
     * @brief Reports whether the most recently submitted item is active.
     * @return true while the item is being interacted with; otherwise false.
     * @note Call immediately after the item to inspect.
     */
    bool (*pfnUiIsItemActive)();

    /**
     * @brief Reports whether the most recently submitted item has keyboard or
     *        gamepad navigation focus.
     * @return true when focused; otherwise false.
     * @note Call immediately after the item to inspect.
     */
    bool (*pfnUiIsItemFocused)();

    /**
     * @brief Reports whether the most recently submitted item received a
     *        specified mouse-button click.
     * @param button Mouse button index: 0 for left, 1 for right, 2 for middle.
     * @return true when the item is hovered and that button was clicked;
     *         otherwise false.
     * @note Call immediately after the item to inspect.
     */
    bool (*pfnUiIsItemClicked)(int button);

    /**
     * @brief Reports whether the most recently submitted item is visible after
     *        clipping.
     * @return true when any part of the item is visible; otherwise false.
     * @note Call immediately after the item to inspect.
     */
    bool (*pfnUiIsItemVisible)();

    /**
     * @brief Adds a row of radio controls by selecting a label from an array.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pItems Array of @p itemCount non-null, null-terminated labels.
     * @param itemCount Number of labels; must be greater than zero.
     * @param pSelectedIndex Non-null pointer to the selected index, in
     *                       [0, @p itemCount); updated when changed.
     * @return true when the selection changes; otherwise false.
     */
    bool (*pfnUiRadioButtonGroup)(const char* pLabel, const char* const* pItems,
                                  int itemCount, int* pSelectedIndex);

    /**
     * @brief Freezes leading rows and columns in the current table.
     * @param columns Number of leading columns to keep visible while scrolling.
     * @param rows Number of leading rows to keep visible while scrolling.
     * @note Call inside a successfully begun table, before submitting rows.
     */
    void (*pfnUiTableSetupScrollFreeze)(int columns, int rows);

    /**
     * @brief Enables or hides a table column.
     * @param columnIndex Zero-based column index.
     * @param enabled True to show the column; false to hide it.
     * @note Call inside a successfully begun table. The user may be able to
     *       change this state later through the table context menu.
     */
    void (*pfnUiTableSetColumnEnabled)(int columnIndex, bool enabled);

    /**
     * @brief Reports whether a table column is currently visible.
     * @param columnIndex Zero-based column index, or -1 for the current column.
     * @return true if the column is enabled and visible; otherwise false.
     * @note Call inside a successfully begun table.
     */
    bool (*pfnUiTableIsColumnVisible)(int columnIndex);

    /**
     * @brief Gets all active table sort keys, up to the caller's capacity.
     *
     * Output arrays use matching element positions. Direction values are 1 for
     * ascending and 2 for descending. The total active key count is returned
     * through @p pSortCount and may exceed @p capacity.
     *
     * @param aColumnIndices Writable array for zero-based column indices.
     * @param aSortDirections Writable array for direction codes.
     * @param capacity Number of elements available in each output array.
     * @param pSortCount Non-null output receiving the total number of active
     *                   sort keys.
     * @param pSpecsDirty Non-null output receiving whether the sort
     *                    specification changed.
     * @return true when the table has a sort specification; false when it has
     *         no active sort specification or arguments are invalid.
     * @note Call inside a successfully begun sortable table. When dirty, apply
     *       the available keys, then call pfnUiTableClearSortDirty().
     */
    bool (*pfnUiTableGetSortSpecs)(int* aColumnIndices, int* aSortDirections,
                                   int capacity, int* pSortCount, bool* pSpecsDirty);

    /**
     * @brief Pushes a temporary color override for one supported semantic UI
     *        color slot.
     * @param color Semantic color slot to override.
     * @param red Red channel in [0.0, 1.0].
     * @param green Green channel in [0.0, 1.0].
     * @param blue Blue channel in [0.0, 1.0].
     * @param alpha Alpha channel in [0.0, 1.0].
     * @note Pair every call with pfnUiPopStyleColor(), in last-in/first-out
     *       order, before returning from the callback.
     */
    void (*pfnUiPushStyleColor)(EPluginUiColor color, float red, float green,
                                float blue, float alpha);

    /**
     * @brief Pops one or more color overrides from the UI style stack.
     * @param count Number of most recently pushed colors to restore; must be
     *              positive and no greater than the number pushed.
     */
    void (*pfnUiPopStyleColor)(int count);

    /**
     * @brief Pushes a temporary scalar style override.
     * @param style Supported scalar style variable.
     * @param value New value for the duration of the style scope.
     * @note Only scalar values such as alpha and rounding are accepted. Pair
     *       with pfnUiPopStyleVar().
     */
    void (*pfnUiPushStyleVarFloat)(EPluginUiStyleVar style, float value);

    /**
     * @brief Pushes a temporary two-dimensional style override.
     * @param style Supported two-dimensional style variable.
     * @param x Horizontal component in UI units.
     * @param y Vertical component in UI units.
     * @note Only padding and spacing variables are accepted. Pair with
     *       pfnUiPopStyleVar().
     */
    void (*pfnUiPushStyleVarVec2)(EPluginUiStyleVar style, float x, float y);

    /**
     * @brief Pops one or more values from the UI style-variable stack.
     * @param count Number of most recently pushed variables to restore; must
     *              be positive and no greater than the number pushed.
     */
    void (*pfnUiPopStyleVar)(int count);

    /**
     * @brief Gets the current content cursor position in screen coordinates.
     * @param pX Non-null output pointer receiving the horizontal coordinate.
     * @param pY Non-null output pointer receiving the vertical coordinate.
     * @note The returned coordinates can be passed to pfnUiDrawLine,
     *       pfnUiDrawRect, pfnUiDrawCircle, or pfnUiDrawText.
     */
    void (*pfnUiGetCursorScreenPos)(float* pX, float* pY);

    /**
     * @brief Gets the screen-space bounds of the most recently submitted item.
     * @param pMinX Non-null output receiving the left edge.
     * @param pMinY Non-null output receiving the top edge.
     * @param pMaxX Non-null output receiving the right edge.
     * @param pMaxY Non-null output receiving the bottom edge.
     * @note Call immediately after the item to inspect. The item bounds are
     *       useful for positioning custom draw-list overlays.
     */
    void (*pfnUiGetItemRect)(float* pMinX, float* pMinY,
                             float* pMaxX, float* pMaxY);

    /**
     * @brief Draws a line over the current window's UI draw list.
     * @param x1 Start X position in screen coordinates.
     * @param y1 Start Y position in screen coordinates.
     * @param x2 End X position in screen coordinates.
     * @param y2 End Y position in screen coordinates.
     * @param red Red channel in [0.0, 1.0].
     * @param green Green channel in [0.0, 1.0].
     * @param blue Blue channel in [0.0, 1.0].
     * @param alpha Alpha channel in [0.0, 1.0].
     * @param thickness Line thickness in UI units; must be positive.
     */
    void (*pfnUiDrawLine)(float x1, float y1, float x2, float y2,
                          float red, float green, float blue, float alpha,
                          float thickness);

    /**
     * @brief Draws an outlined rectangle over the current window's draw list.
     * @param x1 Top-left X position in screen coordinates.
     * @param y1 Top-left Y position in screen coordinates.
     * @param x2 Bottom-right X position in screen coordinates.
     * @param y2 Bottom-right Y position in screen coordinates.
     * @param red Red channel in [0.0, 1.0].
     * @param green Green channel in [0.0, 1.0].
     * @param blue Blue channel in [0.0, 1.0].
     * @param alpha Alpha channel in [0.0, 1.0].
     * @param thickness Outline thickness in UI units; must be positive.
     */
    void (*pfnUiDrawRect)(float x1, float y1, float x2, float y2,
                          float red, float green, float blue, float alpha,
                          float thickness);

    /**
     * @brief Draws a filled rectangle over the current window's draw list.
     * @param x1 Top-left X position in screen coordinates.
     * @param y1 Top-left Y position in screen coordinates.
     * @param x2 Bottom-right X position in screen coordinates.
     * @param y2 Bottom-right Y position in screen coordinates.
     * @param red Red channel in [0.0, 1.0].
     * @param green Green channel in [0.0, 1.0].
     * @param blue Blue channel in [0.0, 1.0].
     * @param alpha Alpha channel in [0.0, 1.0].
     */
    void (*pfnUiDrawRectFilled)(float x1, float y1, float x2, float y2,
                                float red, float green, float blue, float alpha);

    /**
     * @brief Draws an outlined circle over the current window's draw list.
     * @param x Center X position in screen coordinates.
     * @param y Center Y position in screen coordinates.
     * @param radius Circle radius in UI units; must be positive.
     * @param red Red channel in [0.0, 1.0].
     * @param green Green channel in [0.0, 1.0].
     * @param blue Blue channel in [0.0, 1.0].
     * @param alpha Alpha channel in [0.0, 1.0].
     * @param thickness Outline thickness in UI units; must be positive.
     */
    void (*pfnUiDrawCircle)(float x, float y, float radius,
                            float red, float green, float blue, float alpha,
                            float thickness);

    /**
     * @brief Draws a filled circle over the current window's draw list.
     * @param x Center X position in screen coordinates.
     * @param y Center Y position in screen coordinates.
     * @param radius Circle radius in UI units; must be positive.
     * @param red Red channel in [0.0, 1.0].
     * @param green Green channel in [0.0, 1.0].
     * @param blue Blue channel in [0.0, 1.0].
     * @param alpha Alpha channel in [0.0, 1.0].
     */
    void (*pfnUiDrawCircleFilled)(float x, float y, float radius,
                                  float red, float green, float blue, float alpha);

    /**
     * @brief Draws plain text at a screen-space position on the current window.
     * @param x Text origin X position in screen coordinates.
     * @param y Text origin Y position in screen coordinates.
     * @param pText Null-terminated text to draw; must not be null.
     * @param red Red channel in [0.0, 1.0].
     * @param green Green channel in [0.0, 1.0].
     * @param blue Blue channel in [0.0, 1.0].
     * @param alpha Alpha channel in [0.0, 1.0].
     * @note The host reads the string during this call and does not retain it.
     */
    void (*pfnUiDrawText)(float x, float y, const char* pText,
                          float red, float green, float blue, float alpha);

    /**
     * @brief Draws a line chart from a contiguous sequence of float samples.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pValues Pointer to at least @p valueCount float samples.
     * @param valueCount Number of samples; must be positive.
     * @param minimum Fixed lower bound for the vertical axis.
     * @param maximum Fixed upper bound for the vertical axis.
     * @note The array is read only during this call and must remain valid.
     */
    void (*pfnUiPlotHistogram)(const char* pLabel, const float* pValues,
                               int valueCount, float minimum, float maximum);

    /**
     * @brief Begins a window with a plugin-stable subset of window options.
     * @param pTitle Null-terminated window title and identifier.
     * @param pOpen Optional plugin-owned open state. Pass null to omit the
     *              close control; otherwise the host may set it to false.
     * @param options Bitwise combination of EPluginUiWindowOption values.
     * @return true when the window contents are visible; false when collapsed
     *         or clipped.
     * @note Call pfnUiEnd() exactly once after every call, including when this
     *       function returns false.
     */
    bool (*pfnUiBeginEx)(const char* pTitle, bool* pOpen, int options);

    /**
     * @brief Begins a child region with an optional border and scrolling.
     * @param pId Stable null-terminated child identifier.
     * @param width Requested width in UI units; zero uses available width.
     * @param height Requested height in UI units; zero uses available height.
     * @param border True to draw a border around the child region.
     * @return true when the child contents are visible; false when clipped.
     * @note Call pfnUiEndChild() exactly once after every call, even when
     *       this function returns false.
     */
    bool (*pfnUiBeginChildEx)(const char* pId, float width, float height,
                              bool border);

    /**
     * @brief Displays text using an explicit normalized RGBA color.
     * @param pText Null-terminated text; interpreted literally, not as a
     *              format string.
     * @param red Red channel in [0.0, 1.0].
     * @param green Green channel in [0.0, 1.0].
     * @param blue Blue channel in [0.0, 1.0].
     * @param alpha Alpha channel in [0.0, 1.0].
     */
    void (*pfnUiTextColored)(const char* pText, float red, float green,
                             float blue, float alpha);

    /**
     * @brief Displays text using the host's disabled-text color.
     * @param pText Null-terminated text; interpreted literally, not as a
     *              format string.
     */
    void (*pfnUiTextDisabled)(const char* pText);

    /**
     * @brief Displays a label and literal value on one row.
     * @param pLabel Null-terminated descriptive label.
     * @param pValue Null-terminated value text; interpreted literally.
     */
    void (*pfnUiLabelText)(const char* pLabel, const char* pValue);

    /**
     * @brief Draws a horizontal separator with a centered section label.
     * @param pLabel Null-terminated separator label.
     */
    void (*pfnUiSeparatorText)(const char* pLabel);

    /**
     * @brief Displays a compact button.
     * @param pLabel Null-terminated button label and identifier.
     * @return true on activation; otherwise false.
     */
    bool (*pfnUiSmallButton)(const char* pLabel);

    /**
     * @brief Displays an invisible button with a caller-selected hit-box size.
     * @param pId Null-terminated button identifier.
     * @param width Hit-box width in UI units; must be non-negative.
     * @param height Hit-box height in UI units; must be non-negative.
     * @return true when clicked by the primary mouse button; otherwise false.
     */
    bool (*pfnUiInvisibleButton)(const char* pId, float width, float height);

    /**
     * @brief Displays a standard directional arrow button.
     * @param pId Null-terminated button identifier.
     * @param direction One of the EPluginUiDirection values.
     * @return true on activation; otherwise false.
     */
    bool (*pfnUiArrowButton)(const char* pId, EPluginUiDirection direction);

    /**
     * @brief Edits selected bits of an integer value using a checkbox.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pValue Non-null integer value whose selected bits are edited.
     * @param flagsMask Non-zero bit mask controlled by this checkbox.
     * @return true when the value changes; otherwise false.
     */
    bool (*pfnUiCheckboxFlags)(const char* pLabel, int* pValue, int flagsMask);

    /**
     * @brief Displays a radio control with an explicit selected state.
     * @param pLabel Null-terminated widget label and identifier.
     * @param active True to display the radio control as selected.
     * @return true when activated; otherwise false.
     */
    bool (*pfnUiRadioButtonEx)(const char* pLabel, bool active);

    /**
     * @brief Edits a float with caller-selected step sizes and display format.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pValue Non-null value to edit.
     * @param step Small-step amount; zero disables the step buttons.
     * @param stepFast Large-step amount; zero disables the fast step.
     * @param pFormat Optional printf-style float format, such as "%.3f".
     * @return true when the value changes; otherwise false.
     */
    bool (*pfnUiInputFloatEx)(const char* pLabel, float* pValue,
                              float step, float stepFast, const char* pFormat);

    /**
     * @brief Edits an integer with caller-selected step sizes.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pValue Non-null value to edit.
     * @param step Amount added or removed by the normal step buttons.
     * @param stepFast Amount added or removed by the fast step buttons.
     * @return true when the value changes; otherwise false.
     */
    bool (*pfnUiInputIntEx)(const char* pLabel, int* pValue,
                            int step, int stepFast);

    /**
     * @brief Edits a double-precision value.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pValue Non-null value to edit.
     * @param step Small-step amount; zero disables the step buttons.
     * @param stepFast Large-step amount; zero disables the fast step.
     * @param pFormat Optional printf-style double format.
     * @return true when the value changes; otherwise false.
     */
    bool (*pfnUiInputDouble)(const char* pLabel, double* pValue,
                             double step, double stepFast, const char* pFormat);

    /**
     * @brief Edits a writable text buffer with a placeholder hint.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pHint Null-terminated hint shown while the buffer is empty.
     * @param pBuffer Writable null-terminated UTF-8 buffer.
     * @param bufferSize Buffer capacity in bytes, including space for '\0'.
     * @return true when the buffer contents change; otherwise false.
     * @note The host uses the buffer only during the call and retains no
     *       pointer to it.
     */
    bool (*pfnUiInputTextWithHint)(const char* pLabel, const char* pHint,
                                   char* pBuffer, size_t bufferSize);

    /**
     * @brief Edits two, three, or four float values with drag controls.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aValues Writable array containing at least @p componentCount
     *                float values.
     * @param componentCount Number of values; supported values are 2, 3, and 4.
     * @param speed Sensitivity multiplier for dragging.
     * @param minimum Lower bound; bounds are disabled when it is greater than
     *                or equal to @p maximum.
     * @param maximum Upper bound; bounds are disabled when it is less than or
     *                equal to @p minimum.
     * @return true when any component changes; otherwise false.
     */
    bool (*pfnUiDragFloatN)(const char* pLabel, float* aValues,
                            int componentCount, float speed,
                            float minimum, float maximum);

    /**
     * @brief Edits two integer endpoints as a bounded draggable range.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pCurrentMin Non-null writable lower endpoint.
     * @param pCurrentMax Non-null writable upper endpoint.
     * @param speed Sensitivity multiplier for dragging.
     * @param minimum Allowed lower bound; bounds are disabled when it is
     *                greater than or equal to @p maximum.
     * @param maximum Allowed upper bound; bounds are disabled when it is less
     *                than or equal to @p minimum.
     * @return true when either endpoint changes; otherwise false.
     */
    bool (*pfnUiDragIntRange2)(const char* pLabel, int* pCurrentMin,
                               int* pCurrentMax, float speed,
                               int minimum, int maximum);

    /**
     * @brief Edits two float endpoints as a bounded draggable range.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pCurrentMin Non-null writable lower endpoint.
     * @param pCurrentMax Non-null writable upper endpoint.
     * @param speed Sensitivity multiplier for dragging.
     * @param minimum Allowed lower bound; bounds are disabled when it is
     *                greater than or equal to @p maximum.
     * @param maximum Allowed upper bound; bounds are disabled when it is less
     *                than or equal to @p minimum.
     * @return true when either endpoint changes; otherwise false.
     */
    bool (*pfnUiDragFloatRange2)(const char* pLabel, float* pCurrentMin,
                                 float* pCurrentMax, float speed,
                                 float minimum, float maximum);

    /**
     * @brief Edits two, three, or four float values with slider controls.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aValues Writable array containing at least @p componentCount
     *                float values.
     * @param componentCount Number of values; supported values are 2, 3, and 4.
     * @param minimum Inclusive slider lower bound.
     * @param maximum Inclusive slider upper bound; must exceed @p minimum.
     * @return true when any component changes; otherwise false.
     */
    bool (*pfnUiSliderFloatN)(const char* pLabel, float* aValues,
                              int componentCount, float minimum, float maximum);

    /**
     * @brief Edits two, three, or four integer values with slider controls.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aValues Writable array containing at least @p componentCount
     *                integer values.
     * @param componentCount Number of values; supported values are 2, 3, and 4.
     * @param minimum Inclusive slider lower bound.
     * @param maximum Inclusive slider upper bound; must exceed @p minimum.
     * @return true when any component changes; otherwise false.
     */
    bool (*pfnUiSliderIntN)(const char* pLabel, int* aValues,
                            int componentCount, int minimum, int maximum);

    /**
     * @brief Edits an angle stored in radians using a degree-labelled slider.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pRadians Non-null writable angle in radians.
     * @param minimumDegrees Minimum displayed angle in degrees.
     * @param maximumDegrees Maximum displayed angle in degrees.
     * @return true when the angle changes; otherwise false.
     */
    bool (*pfnUiSliderAngle)(const char* pLabel, float* pRadians,
                             float minimumDegrees, float maximumDegrees);

    /**
     * @brief Edits a float using a vertical slider.
     * @param pLabel Null-terminated widget label and identifier.
     * @param width Slider width in UI units; must be positive.
     * @param height Slider height in UI units; must be positive.
     * @param pValue Non-null writable value.
     * @param minimum Inclusive slider lower bound.
     * @param maximum Inclusive slider upper bound; must exceed @p minimum.
     * @return true when the value changes; otherwise false.
     */
    bool (*pfnUiVSliderFloat)(const char* pLabel, float width, float height,
                              float* pValue, float minimum, float maximum);

    /**
     * @brief Edits an integer using a vertical slider.
     * @param pLabel Null-terminated widget label and identifier.
     * @param width Slider width in UI units; must be positive.
     * @param height Slider height in UI units; must be positive.
     * @param pValue Non-null writable value.
     * @param minimum Inclusive slider lower bound.
     * @param maximum Inclusive slider upper bound; must exceed @p minimum.
     * @return true when the value changes; otherwise false.
     */
    bool (*pfnUiVSliderInt)(const char* pLabel, float width, float height,
                             int* pValue, int minimum, int maximum);

    /**
     * @brief Edits a three-channel color with a full color picker.
     * @param pLabel Null-terminated widget label and identifier.
     * @param aColor Writable RGB array containing at least three normalized
     *               channel values.
     * @return true when any channel changes; otherwise false.
     */
    bool (*pfnUiColorPicker3)(const char* pLabel, float aColor[3]);

    /**
     * @brief Creates a tree node with stable plugin-defined display options.
     * @param pLabel Null-terminated node label and identifier.
     * @param options Bitwise combination of EPluginUiTreeNodeOption values.
     * @return true when the node is open and its contents may be submitted.
     * @note Pair every true result with pfnUiTreePop().
     */
    bool (*pfnUiTreeNodeEx)(const char* pLabel, int options);

    /**
     * @brief Selects or deselects a row using common selectable options.
     * @param pLabel Null-terminated widget label and identifier.
     * @param pSelected Non-null selected state, updated when activated.
     * @param allowDoubleClick True to activate only on a double click.
     * @param spanAvailableWidth True to extend the hit area across the row.
     * @return true when activated; otherwise false.
     */
    bool (*pfnUiSelectableEx)(const char* pLabel, bool* pSelected,
                              bool allowDoubleClick, bool spanAvailableWidth);

    /**
     * @brief Reports whether the current window is being shown for the first
     *        frame after appearing.
     * @return true on the window's appearing frame; otherwise false.
     */
    bool (*pfnUiIsWindowAppearing)();

    /**
     * @brief Reports whether the current window is collapsed.
     * @return true if collapsed; otherwise false.
     */
    bool (*pfnUiIsWindowCollapsed)();

    /**
     * @brief Gets the current window's screen-space position and size.
     * @param pX Non-null output receiving the horizontal position.
     * @param pY Non-null output receiving the vertical position.
     * @param pWidth Non-null output receiving the window width.
     * @param pHeight Non-null output receiving the window height.
     * @note Every output pointer must be valid for writing.
     */
    void (*pfnUiGetWindowRect)(float* pX, float* pY,
                               float* pWidth, float* pHeight);

    /**
     * @brief Gets the available content-region width and height.
     * @param pWidth Non-null output receiving available width.
     * @param pHeight Non-null output receiving available height.
     * @note Call inside a window or child region.
     */
    void (*pfnUiGetContentRegionAvail)(float* pWidth, float* pHeight);

    /**
     * @brief Reports whether the last submitted item was edited.
     * @return true if the item value changed; otherwise false.
     * @note Call immediately after submitting the item.
     */
    bool (*pfnUiIsItemEdited)();

    /**
     * @brief Reports whether the last submitted item was activated.
     * @return true on the frame the item became active; otherwise false.
     * @note Call immediately after submitting the item.
     */
    bool (*pfnUiIsItemActivated)();

    /**
     * @brief Reports whether the last submitted item was deactivated.
     * @return true on the frame the item became inactive; otherwise false.
     * @note Call immediately after submitting the item.
     */
    bool (*pfnUiIsItemDeactivated)();

    /**
     * @brief Reports whether the last submitted item was deactivated after
     *        its value changed.
     * @return true when an edit ended this frame; otherwise false.
     * @note Call immediately after submitting the item.
     */
    bool (*pfnUiIsItemDeactivatedAfterEdit)();

    /**
     * @brief Gets the width and height of the last submitted item.
     * @param pWidth Non-null output receiving item width.
     * @param pHeight Non-null output receiving item height.
     * @note Call immediately after submitting the item.
     */
    void (*pfnUiGetItemRectSize)(float* pWidth, float* pHeight);

    /**
     * @brief Applies size limits to the next window submitted.
     * @param minimumWidth Minimum window width in UI units.
     * @param minimumHeight Minimum window height in UI units.
     * @param maximumWidth Maximum window width; use FLT_MAX-like values for
     *                     an effectively unbounded dimension.
     * @param maximumHeight Maximum window height.
     * @note Call before pfnUiBegin() for the window to constrain. Callback
     *       based constraints are intentionally unavailable across the plugin
     *       ABI.
     */
    void (*pfnUiSetNextWindowSizeConstraints)(float minimumWidth,
                                              float minimumHeight,
                                              float maximumWidth,
                                              float maximumHeight);

    /**
     * @brief Sets the next window's scrollable content dimensions.
     * @param width Requested content width; zero leaves it automatic.
     * @param height Requested content height; zero leaves it automatic.
     * @note Call before pfnUiBegin().
     */
    void (*pfnUiSetNextWindowContentSize)(float width, float height);

    /**
     * @brief Sets the next window's scroll position.
     * @param x Horizontal scroll offset; a negative value leaves it unchanged.
     * @param y Vertical scroll offset; a negative value leaves it unchanged.
     * @note Call before pfnUiBegin().
     */
    void (*pfnUiSetNextWindowScroll)(float x, float y);

    /**
     * @brief Sets the next window background opacity.
     * @param alpha Opacity in [0.0, 1.0].
     * @note Call before pfnUiBegin().
     */
    void (*pfnUiSetNextWindowBgAlpha)(float alpha);

    /**
     * @brief Changes the current window position.
     * @param x Screen-space horizontal position.
     * @param y Screen-space vertical position.
     * @note Prefer pfnUiSetNextWindowPos() before beginning a window to avoid
     *       visual movement during the frame.
     */
    void (*pfnUiSetWindowPos)(float x, float y);

    /**
     * @brief Changes the current window dimensions.
     * @param width Requested window width in UI units.
     * @param height Requested window height in UI units.
     * @note Prefer pfnUiSetNextWindowSize() before beginning a window.
     */
    void (*pfnUiSetWindowSize)(float width, float height);

    /**
     * @brief Changes the current window collapsed state.
     * @param collapsed True to collapse the current window.
     */
    void (*pfnUiSetWindowCollapsed)(bool collapsed);

    /**
     * @brief Requests focus for the current window.
     */
    void (*pfnUiSetWindowFocus)();

    /**
     * @brief Reads the current window scroll offset and maximum.
     * @param pX Non-null output receiving horizontal scroll position.
     * @param pY Non-null output receiving vertical scroll position.
     * @param pMaxX Non-null output receiving maximum horizontal scroll.
     * @param pMaxY Non-null output receiving maximum vertical scroll.
     */
    void (*pfnUiGetWindowScroll)(float* pX, float* pY,
                                 float* pMaxX, float* pMaxY);

    /**
     * @brief Sets the current window scroll offset.
     * @param x Horizontal scroll position.
     * @param y Vertical scroll position.
     */
    void (*pfnUiSetWindowScroll)(float x, float y);

    /**
     * @brief Scrolls the current cursor position into view.
     * @param centerXRatio Horizontal placement ratio in [0.0, 1.0].
     * @param centerYRatio Vertical placement ratio in [0.0, 1.0].
     * @note Call after submitting the content that should be brought into view.
     */
    void (*pfnUiScrollHere)(float centerXRatio, float centerYRatio);

    /**
     * @brief Sets the cursor to a screen-space position.
     * @param x Absolute horizontal screen coordinate.
     * @param y Absolute vertical screen coordinate.
     */
    void (*pfnUiSetCursorScreenPos)(float x, float y);

    /**
     * @brief Gets the cursor position relative to the current window.
     * @param pX Non-null output receiving the horizontal coordinate.
     * @param pY Non-null output receiving the vertical coordinate.
     */
    void (*pfnUiGetCursorPos)(float* pX, float* pY);

    /**
     * @brief Sets the cursor position relative to the current window.
     * @param x Window-local horizontal coordinate.
     * @param y Window-local vertical coordinate.
     */
    void (*pfnUiSetCursorPos)(float x, float y);

    /**
     * @brief Submits an inert layout item with the requested dimensions.
     * @param width Reserved width in UI units; zero is permitted.
     * @param height Reserved height in UI units; zero is permitted.
     * @note Unlike an invisible button, this item does not react to input.
     */
    void (*pfnUiDummy)(float width, float height);

    /**
     * @brief Starts a new layout line.
     */
    void (*pfnUiNewLine)();

    /**
     * @brief Begins a group whose submitted items act as one layout item.
     * @note Pair with pfnUiEndGroup() before leaving the callback.
     */
    void (*pfnUiBeginGroup)();

    /**
     * @brief Ends the most recently begun layout group.
     */
    void (*pfnUiEndGroup)();

    /**
     * @brief Aligns the next text baseline with framed widgets.
     */
    void (*pfnUiAlignTextToFramePadding)();

    /**
     * @brief Gets common text and frame layout dimensions.
     * @param pTextLineHeight Non-null output receiving the text line height.
     * @param pTextLineHeightWithSpacing Non-null output receiving line height
     *                                   plus vertical item spacing.
     * @param pFrameHeight Non-null output receiving the frame height.
     * @param pFrameHeightWithSpacing Non-null output receiving frame height
     *                                plus vertical item spacing.
     */
    void (*pfnUiGetLayoutMetrics)(float* pTextLineHeight,
                                  float* pTextLineHeightWithSpacing,
                                  float* pFrameHeight,
                                  float* pFrameHeightWithSpacing);

    /**
     * @brief Places widgets on the same line with explicit offset and spacing.
     * @param offsetFromStartX Horizontal offset from the line start; zero
     *                         continues after the previous item.
     * @param spacing Spacing after the previous item; a negative value uses
     *                the host default.
     */
    void (*pfnUiSameLineEx)(float offsetFromStartX, float spacing);

    /**
     * @brief Creates a clickable color swatch.
     * @param pId Null-terminated identifier.
     * @param red Red channel in [0.0, 1.0].
     * @param green Green channel in [0.0, 1.0].
     * @param blue Blue channel in [0.0, 1.0].
     * @param alpha Alpha channel in [0.0, 1.0].
     * @param width Requested swatch width; zero uses the default.
     * @param height Requested swatch height; zero uses the default.
     * @return true when clicked; otherwise false.
     */
    bool (*pfnUiColorButton)(const char* pId, float red, float green,
                             float blue, float alpha, float width, float height);

    /**
     * @brief Sets whether the next tree node or collapsing header starts open.
     * @param open True to open, false to close.
     * @param condition Condition controlling when the state is applied.
     * @note Applies to the next tree node or collapsing header only.
     */
    void (*pfnUiSetNextItemOpen)(bool open, EPluginUiCondition condition);

    /**
     * @brief Displays a collapsing header with a plugin-owned visibility flag.
     * @param pLabel Null-terminated header label and identifier.
     * @param pVisible Non-null state; false hides the header, and the close
     *                 control can set it to false.
     * @return true when the header is open; otherwise false.
     */
    bool (*pfnUiCollapsingHeaderVisible)(const char* pLabel, bool* pVisible);

    /**
     * @brief Reads whether the identified tree node is currently open.
     * @param pId Null-terminated tree node label/identifier.
     * @return true if open; otherwise false.
     */
    bool (*pfnUiTreeNodeIsOpen)(const char* pId);

    /**
     * @brief Selects a row with explicit selected state and dimensions.
     * @param pLabel Null-terminated label and identifier.
     * @param selected Current selected state, not modified by this call.
     * @param width Requested width; zero uses available width.
     * @param height Requested height; zero uses label height.
     * @param allowDoubleClick True to activate on double click.
     * @return true when activated; otherwise false.
     */
    bool (*pfnUiSelectableSized)(const char* pLabel, bool selected,
                                 float width, float height,
                                 bool allowDoubleClick);

    /**
     * @brief Opens a popup when the last submitted item is clicked.
     * @param pId Null-terminated popup identifier.
     * @param mouseButton Mouse button from EPluginUiMouseButton.
     * @note Call immediately after the item that should open the popup.
     */
    void (*pfnUiOpenPopupOnItemClick)(const char* pId,
                                      EPluginUiMouseButton mouseButton);

    /**
     * @brief Begins a context popup opened by clicking empty UI space.
     * @param pId Null-terminated popup identifier.
     * @param mouseButton Mouse button from EPluginUiMouseButton.
     * @return true when the popup is open and its contents may be submitted.
     * @note Pair a true result with pfnUiEndPopup().
     */
    bool (*pfnUiBeginPopupContextVoid)(const char* pId,
                                       EPluginUiMouseButton mouseButton);

    /**
     * @brief Reports whether a named popup is open in the current popup level.
     * @param pId Null-terminated popup identifier.
     * @return true when open; otherwise false.
     */
    bool (*pfnUiIsPopupOpen)(const char* pId);

    /**
     * @brief Reads the current table's row and column indices and column count.
     * @param pRowIndex Non-null output receiving the current row index.
     * @param pColumnIndex Non-null output receiving the current column index.
     * @param pColumnCount Non-null output receiving the table column count.
     * @note Call inside a successfully begun table.
     */
    void (*pfnUiTableGetPosition)(int* pRowIndex, int* pColumnIndex,
                                  int* pColumnCount);

    /**
     * @brief Selects a specific table column for subsequent widgets.
     * @param columnIndex Zero-based column index.
     * @return true when the column is visible; otherwise false.
     * @note Call inside a successfully begun table.
     */
    bool (*pfnUiTableSetColumnIndex)(int columnIndex);

    /**
     * @brief Sets the background color of a table row, cell, or alternate row.
     * @param target Region from EPluginUiTableColorTarget.
     * @param red Red channel in [0.0, 1.0].
     * @param green Green channel in [0.0, 1.0].
     * @param blue Blue channel in [0.0, 1.0].
     * @param alpha Alpha channel in [0.0, 1.0].
     * @param columnIndex Column index for a cell target, or -1 for current.
     * @note Call inside a successfully begun table.
     */
    void (*pfnUiTableSetBgColor)(EPluginUiTableColorTarget target,
                                 float red, float green, float blue, float alpha,
                                 int columnIndex);

    /**
     * @brief Submits one table header cell.
     * @param pLabel Null-terminated header label.
     * @note Call inside a table, typically after beginning a header row.
     */
    void (*pfnUiTableHeader)(const char* pLabel);

    /**
     * @brief Submits angled table headers.
     * @note Call inside a table as its first row, with angled headers enabled
     *       by the host's table setup.
     */
    void (*pfnUiTableAngledHeadersRow)();

    /**
     * @brief Adds a table column with width/weight configuration.
     * @param pLabel Null-terminated column label.
     * @param sortable True to allow sorting on the column.
     * @param widthOrWeight Initial width or stretch weight; zero uses defaults.
     */
    void (*pfnUiTableSetupColumnEx)(const char* pLabel, bool sortable,
                                    float widthOrWeight);

    /**
     * @brief Queries whether the current window is focused or hovered.
     * @param pFocused Non-null output receiving the current-window focus state.
     * @param pHovered Non-null output receiving the current-window hover state.
     * @note Hover is false when another UI layer blocks interaction.
     */
    void (*pfnUiGetWindowInteractionState)(bool* pFocused, bool* pHovered);

    /**
     * @brief Sets keyboard focus to the next widget or a nearby widget.
     * @param offset Relative item offset; zero selects the next widget, -1 the
     *               previous widget, and positive values skip forward.
     */
    void (*pfnUiSetKeyboardFocusHere)(int offset);

    /**
     * @brief Requests default navigation focus for the last submitted item.
     */
    void (*pfnUiSetItemDefaultFocus)();

    /**
     * @brief Reports whether the last item open/closed a tree node.
     * @return true when its open state changed; otherwise false.
     */
    bool (*pfnUiIsItemToggledOpen)();

    /**
     * @brief Reports whether any item is hovered, active, or navigation-focused.
     * @param pHovered Non-null output receiving the any-item hovered state.
     * @param pActive Non-null output receiving the any-item active state.
     * @param pFocused Non-null output receiving the any-item focused state.
     */
    void (*pfnUiGetAnyItemState)(bool* pHovered, bool* pActive, bool* pFocused);

    /**
     * @brief Reports whether a rectangle at the current cursor is visible.
     * @param width Rectangle width in UI units.
     * @param height Rectangle height in UI units.
     * @return true if any part is visible after clipping; otherwise false.
     */
    bool (*pfnUiIsRectVisible)(float width, float height);

    /**
     * @brief Measures literal text using the active UI font.
     * @param pText Null-terminated UTF-8 string, treated literally.
     * @param wrapWidth Maximum line width; zero disables wrapping.
     * @param pWidth Non-null output receiving measured width.
     * @param pHeight Non-null output receiving measured height.
     */
    void (*pfnUiCalcTextSize)(const char* pText, float wrapWidth,
                              float* pWidth, float* pHeight);

    /**
     * @brief Converts a normalized RGB color to HSV.
     * @param red Input red channel in [0.0, 1.0].
     * @param green Input green channel in [0.0, 1.0].
     * @param blue Input blue channel in [0.0, 1.0].
     * @param pHue Non-null output hue in [0.0, 1.0].
     * @param pSaturation Non-null output saturation in [0.0, 1.0].
     * @param pValue Non-null output value in [0.0, 1.0].
     */
    void (*pfnUiColorRgbToHsv)(float red, float green, float blue,
                               float* pHue, float* pSaturation, float* pValue);

    /**
     * @brief Converts a normalized HSV color to RGB.
     * @param hue Input hue in [0.0, 1.0].
     * @param saturation Input saturation in [0.0, 1.0].
     * @param value Input value in [0.0, 1.0].
     * @param pRed Non-null output red channel in [0.0, 1.0].
     * @param pGreen Non-null output green channel in [0.0, 1.0].
     * @param pBlue Non-null output blue channel in [0.0, 1.0].
     */
    void (*pfnUiColorHsvToRgb)(float hue, float saturation, float value,
                               float* pRed, float* pGreen, float* pBlue);

    /**
     * @brief Gets the current frame number and elapsed UI time.
     * @param pFrameCount Non-null output receiving the frame count.
     * @param pTimeSeconds Non-null output receiving elapsed seconds.
     */
    void (*pfnUiGetFrameInfo)(int* pFrameCount, double* pTimeSeconds);

    /**
     * @brief Queries a keyboard key using stable plugin key identifiers.
     * @param key Key from EPluginUiKey.
     * @param pDown Non-null output receiving whether the key is held.
     * @param pPressed Non-null output receiving whether it was pressed this
     *                 frame.
     * @param pReleased Non-null output receiving whether it was released this
     *                  frame.
     * @note Queries observe input owned by the host UI and do not change input
     *       routing or capture behavior.
     */
    void (*pfnUiGetKeyState)(EPluginUiKey key, bool* pDown,
                             bool* pPressed, bool* pReleased);

    /**
     * @brief Queries a mouse button using stable plugin mouse-button identifiers.
     * @param button Mouse button from EPluginUiMouseButton.
     * @param pDown Non-null output receiving whether it is held.
     * @param pClicked Non-null output receiving whether it was pressed this
     *                 frame.
     * @param pReleased Non-null output receiving whether it was released this
     *                  frame.
     */
    void (*pfnUiGetMouseButtonState)(EPluginUiMouseButton button,
                                     bool* pDown, bool* pClicked,
                                     bool* pReleased);

    /**
     * @brief Reads the mouse position in screen coordinates.
     * @param pX Non-null output receiving horizontal position.
     * @param pY Non-null output receiving vertical position.
     * @param pValid Non-null output receiving whether a mouse position exists.
     */
    void (*pfnUiGetMousePosition)(float* pX, float* pY, bool* pValid);

    /**
     * @brief Begins a clipping scope in screen coordinates.
     * @param minX Left edge of clipping rectangle.
     * @param minY Top edge of clipping rectangle.
     * @param maxX Right edge of clipping rectangle.
     * @param maxY Bottom edge of clipping rectangle.
     * @param intersectCurrent True to intersect with the current clip region.
     * @note Pair with pfnUiPopClipRect() in last-in/first-out order.
     */
    void (*pfnUiPushClipRect)(float minX, float minY, float maxX, float maxY,
                              bool intersectCurrent);

    /**
     * @brief Restores the clipping rectangle preceding the latest push.
     */
    void (*pfnUiPopClipRect)();

    /**
     * @brief Permits the next item to overlap with the following item.
     * @note Call before submitting the item that should permit overlap.
     */
    void (*pfnUiSetNextItemAllowOverlap)();

    /**
     * @brief Reads the number of columns in the current legacy columns layout.
     * @return Column count, or zero when no legacy columns layout is active.
     */
    int (*pfnUiGetLegacyColumnCount)();

    /**
     * @brief Begins a legacy columns layout.
     * @param count Number of columns; zero or one ends multi-column behavior.
     * @param pId Optional stable identifier; null uses the default identifier.
     * @param borders True to draw column separators.
     * @note Pair with further column calls as required by ImGui's legacy
     *       layout; new code should prefer tables.
     */
    void (*pfnUiColumns)(int count, const char* pId, bool borders);

    /**
     * @brief Advances to the next legacy column.
     */
    void (*pfnUiNextColumn)();

    /**
     * @brief Gets or sets legacy column width and offset.
     * @param columnIndex Zero-based index, or -1 for the current column.
     * @param pWidth Optional output width; may be null to skip.
     * @param pOffset Optional output offset; may be null to skip.
     * @param setWidth True to apply @p width.
     * @param width New width when @p setWidth is true.
     * @param setOffset True to apply @p offset.
     * @param offset New horizontal offset when @p setOffset is true.
     */
    void (*pfnUiLegacyColumnGetSet)(int columnIndex, float* pWidth,
                                    float* pOffset, bool setWidth, float width,
                                    bool setOffset, float offset);

    /**
     * @brief Submits a tab-shaped button inside the current tab bar.
     * @param pLabel Null-terminated tab label and identifier.
     * @return true when clicked; otherwise false.
     */
    bool (*pfnUiTabItemButton)(const char* pLabel);

    /**
     * @brief Notifies ImGui that a tab was closed before it is submitted.
     * @param pLabel Null-terminated tab or window label.
     * @note Call inside a tab bar before submitting the tab, or with a window
     *       name to notify the docking system.
     */
    void (*pfnUiSetTabItemClosed)(const char* pLabel);

    /**
     * @brief Logs text to the active ImGui log target without displaying it.
     * @param pText Null-terminated text, treated literally.
     * @note This function does nothing unless a host-controlled ImGui log
     *       session is active.
     */
    void (*pfnUiLogText)(const char* pText);

    /**
     * @brief Begins an ImGui log session writing to the host standard output.
     * @param autoOpenDepth Tree depth automatically opened in the captured log;
     *                      negative uses the ImGui default.
     */
    void (*pfnUiLogToOutput)(int autoOpenDepth);

    /**
     * @brief Finishes the current ImGui log session.
     */
    void (*pfnUiLogFinish)();

    /**
     * @brief Sets the position of the next window with a one-shot condition.
     * @param x Screen-space horizontal anchor coordinate.
     * @param y Screen-space vertical anchor coordinate.
     * @param pivotX Horizontal pivot in [0.0, 1.0].
     * @param pivotY Vertical pivot in [0.0, 1.0].
     * @param condition Condition controlling when the position is applied.
     * @note Call before pfnUiBegin().
     */
    void (*pfnUiSetNextWindowPosEx)(float x, float y, float pivotX,
                                    float pivotY, EPluginUiCondition condition);

    /**
     * @brief Sets the dimensions of the next window with a one-shot condition.
     * @param width Requested width; zero permits automatic sizing.
     * @param height Requested height; zero permits automatic sizing.
     * @param condition Condition controlling when the size is applied.
     * @note Call before pfnUiBegin().
     */
    void (*pfnUiSetNextWindowSizeEx)(float width, float height,
                                     EPluginUiCondition condition);

    /**
     * @brief Sets the collapsed state of the next window.
     * @param collapsed True to collapse the window.
     * @param condition Condition controlling when the state is applied.
     * @note Call before pfnUiBegin().
     */
    void (*pfnUiSetNextWindowCollapsedEx)(bool collapsed,
                                          EPluginUiCondition condition);

    /**
     * @brief Gets the current window's DPI scale.
     * @return Positive scale factor, or 1.0 when no window is active.
     */
    float (*pfnUiGetWindowDpiScale)();

    /**
     * @brief Gets the position and dimensions of the current window.
     * @param pX Non-null output receiving screen-space X.
     * @param pY Non-null output receiving screen-space Y.
     * @param pWidth Non-null output receiving width.
     * @param pHeight Non-null output receiving height.
     */
    void (*pfnUiGetWindowRectDetailed)(float* pX, float* pY,
                                       float* pWidth, float* pHeight);

    /**
     * @brief Adds a text hyperlink-style widget.
     * @param pLabel Null-terminated visible link label.
     * @return true when activated; otherwise false.
     */
    bool (*pfnUiTextLink)(const char* pLabel);

    /**
     * @brief Begins a tooltip for the most recently submitted item.
     * @return true when tooltip contents may be submitted.
     * @note Pair a true result with pfnUiEndTooltip().
     */
    bool (*pfnUiBeginItemTooltip)();

    /**
     * @brief Attaches literal tooltip text to the last submitted item.
     * @param pText Null-terminated tooltip text; treated literally.
     */
    void (*pfnUiSetItemTooltip)(const char* pText);

    /**
     * @brief Sets a plot's size and optional descriptive overlay.
     * @param pLabel Null-terminated plot label and identifier.
     * @param pValues Pointer to at least @p valueCount samples.
     * @param valueCount Number of samples; must be positive.
     * @param valueOffset Starting sample index, wrapped within the sample array.
     * @param pOverlay Optional null-terminated overlay text.
     * @param minimum Fixed vertical lower bound.
     * @param maximum Fixed vertical upper bound.
     * @param width Plot width; zero uses available width.
     * @param height Plot height; must be positive.
     * @note Sample memory is read only for the duration of the call.
     */
    void (*pfnUiPlotLinesEx)(const char* pLabel, const float* pValues,
                             int valueCount, int valueOffset,
                             const char* pOverlay, float minimum, float maximum,
                             float width, float height);

    /**
     * @brief Displays a histogram with caller-selected dimensions and overlay.
     * @param pLabel Null-terminated plot label and identifier.
     * @param pValues Pointer to at least @p valueCount samples.
     * @param valueCount Number of samples; must be positive.
     * @param valueOffset Starting sample index, wrapped within the sample array.
     * @param pOverlay Optional null-terminated overlay text.
     * @param minimum Fixed vertical lower bound.
     * @param maximum Fixed vertical upper bound.
     * @param width Plot width; zero uses available width.
     * @param height Plot height; must be positive.
     */
    void (*pfnUiPlotHistogramEx)(const char* pLabel, const float* pValues,
                                 int valueCount, int valueOffset,
                                 const char* pOverlay, float minimum,
                                 float maximum, float width, float height);

    /**
     * @brief Queries additional mouse-button interaction details.
     * @param button Mouse button from EPluginUiMouseButton.
     * @param pDoubleClicked Non-null output receiving double-click state.
     * @param pClickCount Non-null output receiving successive click count.
     * @param pDragging Non-null output receiving drag state.
     * @param dragThreshold Distance threshold; negative selects host default.
     */
    void (*pfnUiGetMouseButtonDetails)(EPluginUiMouseButton button,
                                       bool* pDoubleClicked, int* pClickCount,
                                       bool* pDragging, float dragThreshold);

    /**
     * @brief Tests whether the mouse is over a screen-space rectangle.
     * @param minX Left edge.
     * @param minY Top edge.
     * @param maxX Right edge.
     * @param maxY Bottom edge.
     * @param clipToUi True to respect current UI clipping.
     * @return true when the mouse is inside the rectangle; otherwise false.
     */
    bool (*pfnUiIsMouseHoveringRect)(float minX, float minY,
                                     float maxX, float maxY, bool clipToUi);

    /**
     * @brief Gets the mouse position recorded when the current popup opened.
     * @param pX Non-null output receiving screen-space X.
     * @param pY Non-null output receiving screen-space Y.
     */
    void (*pfnUiGetPopupOpeningMousePosition)(float* pX, float* pY);

    /**
     * @brief Reports whether any mouse button is held.
     * @return true when at least one button is down; otherwise false.
     */
    bool (*pfnUiIsAnyMouseButtonDown)();

    /**
     * @brief Queries how many times a key repeated this frame.
     * @param key Key from EPluginUiKey.
     * @param repeatDelay Seconds before the first repeat.
     * @param repeatRate Seconds between repeats; must be positive.
     * @return Number of presses/repeats this frame, or zero for invalid input.
     */
    int (*pfnUiGetKeyPressedAmount)(EPluginUiKey key, float repeatDelay,
                                    float repeatRate);

    /**
     * @brief Copies the localized display name of a key into caller storage.
     * @param key Key from EPluginUiKey.
     * @param pBuffer Writable output buffer.
     * @param bufferSize Buffer capacity in bytes, including the terminator.
     * @return true when the name was copied; false for invalid arguments or
     *         insufficient capacity.
     */
    bool (*pfnUiGetKeyName)(EPluginUiKey key, char* pBuffer, size_t bufferSize);

    /**
     * @brief Tests whether a keyboard shortcut is routed to the current scope.
     * @param key Key from EPluginUiKey.
     * @param control True when Control is required.
     * @param shift True when Shift is required.
     * @param alt True when Alt is required.
     * @param super True when the platform Super/Command modifier is required.
     * @param repeat True to permit key repeat.
     * @return true when the shortcut is active for this scope; otherwise false.
     */
    bool (*pfnUiShortcut)(EPluginUiKey key, bool control, bool shift,
                          bool alt, bool super, bool repeat);

    /**
     * @brief Sets whether the current window's contents may use the navigation
     *        cursor.
     * @param visible True to display it.
     */
    void (*pfnUiSetNavigationCursorVisible)(bool visible);

    /**
     * @brief Creates an ImGui dockspace inside the current window.
     * @param pId Null-terminated stable dockspace identifier.
     * @param width Requested width; zero uses remaining width.
     * @param height Requested height; zero uses remaining height.
     * @return Stable dockspace identifier, or zero for invalid input.
     * @note Docking must be enabled by the host. Dock node flags and window
     *       class internals are deliberately not exposed through the ABI.
     */
    unsigned int (*pfnUiDockSpace)(const char* pId, float width, float height);

    /**
     * @brief Requests that the next plugin window be docked into a dock node.
     * @param dockId Identifier returned by pfnUiDockSpace().
     * @param condition Condition controlling when the docking request applies.
     * @note Call before pfnUiBegin().
     */
    void (*pfnUiSetNextWindowDockId)(unsigned int dockId,
                                     EPluginUiCondition condition);

    /**
     * @brief Gets the current window's docking state and dockspace identifier.
     * @param pDockId Non-null output receiving the dock node identifier.
     * @param pDocked Non-null output receiving whether it is docked.
     */
    void (*pfnUiGetWindowDockState)(unsigned int* pDockId, bool* pDocked);

    /**
     * @brief Begins the host's full-width main menu bar.
     * @return true when the menu bar is available for contents.
     * @note Pair a true result with pfnUiEndMainMenuBar().
     */
    bool (*pfnUiBeginMainMenuBar)();

    /**
     * @brief Ends the main menu bar opened by pfnUiBeginMainMenuBar().
     */
    void (*pfnUiEndMainMenuBar)();

    /**
     * @brief Displays a bullet marker at the current layout position.
     */
    void (*pfnUiBullet)();

    /**
     * @brief Displays a formatted-style boolean value without interpreting a
     *        plugin string as a format string.
     * @param pLabel Null-terminated value label.
     * @param value Boolean value to display.
     */
    void (*pfnUiValueBool)(const char* pLabel, bool value);

    /**
     * @brief Displays a signed integer value.
     * @param pLabel Null-terminated value label.
     * @param value Value to display.
     */
    void (*pfnUiValueInt)(const char* pLabel, int value);

    /**
     * @brief Displays an unsigned integer value.
     * @param pLabel Null-terminated value label.
     * @param value Value to display.
     */
    void (*pfnUiValueUInt)(const char* pLabel, unsigned int value);

    /**
     * @brief Displays a floating-point value.
     * @param pLabel Null-terminated value label.
     * @param value Value to display.
     * @param pFormat Optional printf-style float format, such as "%.3f".
     */
    void (*pfnUiValueFloat)(const char* pLabel, float value,
                            const char* pFormat);

    /**
     * @brief Pushes a temporary font size using the host's current font.
     * @param sizeBaseUnscaled Requested size before host DPI scaling; must be
     *                         positive.
     * @note Pair with pfnUiPopFont(). Custom font pointers are not exposed.
     */
    void (*pfnUiPushFontSize)(float sizeBaseUnscaled);

    /**
     * @brief Restores the font size pushed by pfnUiPushFontSize().
     */
    void (*pfnUiPopFont)();

    /**
     * @brief Gets the current scaled font size.
     * @return Font height in UI units.
     */
    float (*pfnUiGetFontSize)();

    /**
     * @brief Pushes a temporary default width for labeled widgets.
     * @param width Width in UI units; negative values align to the right edge.
     * @note Pair with pfnUiPopItemWidth().
     */
    void (*pfnUiPushItemWidth)(float width);

    /**
     * @brief Restores the item width preceding the latest push.
     */
    void (*pfnUiPopItemWidth)();

    /**
     * @brief Gets the width the next standard item will use.
     * @return Width in UI units.
     */
    float (*pfnUiCalcItemWidth)();

    /**
     * @brief Pushes a text wrapping position for subsequent text widgets.
     * @param localPositionX Window-local X position; zero wraps at the current
     *                       content edge, negative disables wrapping.
     * @note Pair with pfnUiPopTextWrapPos().
     */
    void (*pfnUiPushTextWrapPos)(float localPositionX);

    /**
     * @brief Restores the text wrapping position preceding the latest push.
     */
    void (*pfnUiPopTextWrapPos)();

    /**
     * @brief Displays an existing host texture with background and tint colors.
     * @param pAssets Host asset library supplied in SPluginContext.
     * @param textureIndex Current texture index from pfnAssetGetTextureCount().
     * @param width Display width in UI units; must be positive.
     * @param height Display height in UI units; must be positive.
     * @param bgRed Background red channel in [0.0, 1.0].
     * @param bgGreen Background green channel in [0.0, 1.0].
     * @param bgBlue Background blue channel in [0.0, 1.0].
     * @param bgAlpha Background alpha channel in [0.0, 1.0].
     * @param tintRed Tint red channel in [0.0, 1.0].
     * @param tintGreen Tint green channel in [0.0, 1.0].
     * @param tintBlue Tint blue channel in [0.0, 1.0].
     * @param tintAlpha Tint alpha channel in [0.0, 1.0].
     * @return true when the texture exists and the image was submitted.
     */
    bool (*pfnUiImageWithBg)(CAssetLibrary* pAssets, int textureIndex,
                             float width, float height,
                             float bgRed, float bgGreen, float bgBlue, float bgAlpha,
                             float tintRed, float tintGreen,
                             float tintBlue, float tintAlpha);

    /**
     * @brief Gets the mouse cursor movement delta for a mouse button.
     * @param button Mouse button from EPluginUiMouseButton.
     * @param lockThreshold Drag threshold; negative selects host default.
     * @param pDeltaX Non-null output receiving horizontal movement.
     * @param pDeltaY Non-null output receiving vertical movement.
     */
    void (*pfnUiGetMouseDragDelta)(EPluginUiMouseButton button,
                                   float lockThreshold,
                                   float* pDeltaX, float* pDeltaY);

    /**
     * @brief Resets accumulated drag delta for a mouse button.
     * @param button Mouse button from EPluginUiMouseButton.
     */
    void (*pfnUiResetMouseDragDelta)(EPluginUiMouseButton button);

    /**
     * @brief Requests a cursor shape for the current UI frame.
     * @param cursor Shape from EPluginUiMouseCursor.
     */
    void (*pfnUiSetMouseCursor)(EPluginUiMouseCursor cursor);

    /**
     * @brief Creates a dockspace over the host's main viewport.
     * @return Dockspace identifier, or zero when docking is unavailable.
     * @note Docking must be enabled by the host. The viewport itself is never
     *       exposed to plugin code.
     */
    unsigned int (*pfnUiDockSpaceOverMainViewport)();

    /**
     * @brief Displays the Dear ImGui metrics/debugger window.
     * @param pOpen Optional plugin-owned visibility state; may be null.
     */
    void (*pfnUiShowMetricsWindow)(bool* pOpen);

    /**
     * @brief Displays the Dear ImGui debug log window.
     * @param pOpen Optional plugin-owned visibility state; may be null.
     */
    void (*pfnUiShowDebugLogWindow)(bool* pOpen);

    /**
     * @brief Displays the Dear ImGui ID stack inspection window.
     * @param pOpen Optional plugin-owned visibility state; may be null.
     */
    void (*pfnUiShowIdStackToolWindow)(bool* pOpen);

    /**
     * @brief Applies a built-in theme preset to the host's shared ImGui style.
     * @param style Preset from EPluginUiBuiltinStyle.
     */
    void (*pfnUiSetBuiltinStyle)(EPluginUiBuiltinStyle style);

    /**
     * @brief Displays the built-in selector for currently loaded fonts.
     * @param pLabel Null-terminated selector label.
     */
    void (*pfnUiShowFontSelector)(const char* pLabel);

    /**
     * @brief Copies the active Dear ImGui version string into caller storage.
     * @param pBuffer Writable output buffer.
     * @param bufferSize Capacity in bytes, including the null terminator.
     * @return true when the complete version string was copied.
     */
    bool (*pfnUiGetVersion)(char* pBuffer, size_t bufferSize);

    /**
     * @brief Edits one or more plugin-owned values using a generic drag widget.
     * @param pLabel Null-terminated widget label and identifier.
     * @param type Value representation from EPluginUiScalarType.
     * @param pValues Writable storage for one or more values of @p type.
     * @param componentCount Number of contiguous values; must be positive.
     * @param speed Drag sensitivity multiplier.
     * @param pMinimum Optional pointer to one lower-bound value of @p type.
     * @param pMaximum Optional pointer to one upper-bound value of @p type.
     * @param pFormat Optional printf-style display format compatible with
     *                @p type.
     * @return true when at least one value changes; otherwise false.
     * @note All data pointers are read/written synchronously and must point to
     *       storage matching @p type.
     */
    bool (*pfnUiDragScalar)(const char* pLabel, EPluginUiScalarType type,
                            void* pValues, int componentCount, float speed,
                            const void* pMinimum, const void* pMaximum,
                            const char* pFormat);

    /**
     * @brief Edits one or more plugin-owned values using a generic slider.
     * @param pLabel Null-terminated widget label and identifier.
     * @param type Value representation from EPluginUiScalarType.
     * @param pValues Writable storage for one or more values of @p type.
     * @param componentCount Number of contiguous values; must be positive.
     * @param pMinimum Non-null pointer to one lower-bound value of @p type.
     * @param pMaximum Non-null pointer to one upper-bound value of @p type.
     * @param pFormat Optional printf-style display format compatible with
     *                @p type.
     * @return true when at least one value changes; otherwise false.
     */
    bool (*pfnUiSliderScalar)(const char* pLabel, EPluginUiScalarType type,
                              void* pValues, int componentCount,
                              const void* pMinimum, const void* pMaximum,
                              const char* pFormat);

    /**
     * @brief Edits one or more plugin-owned values using generic keyboard input.
     * @param pLabel Null-terminated widget label and identifier.
     * @param type Value representation from EPluginUiScalarType.
     * @param pValues Writable storage for one or more values of @p type.
     * @param componentCount Number of contiguous values; must be positive.
     * @param pStep Optional pointer to the normal step value of @p type.
     * @param pStepFast Optional pointer to the fast step value of @p type.
     * @param pFormat Optional printf-style display format compatible with
     *                @p type.
     * @return true when at least one value changes; otherwise false.
     */
    bool (*pfnUiInputScalar)(const char* pLabel, EPluginUiScalarType type,
                             void* pValues, int componentCount,
                             const void* pStep, const void* pStepFast,
                             const char* pFormat);

    /**
     * @brief Edits a plugin-owned scalar value using a vertical slider.
     * @param pLabel Null-terminated widget label and identifier.
     * @param type Value representation from EPluginUiScalarType.
     * @param pValue Writable storage for one value of @p type.
     * @param width Slider width in UI units; must be positive.
     * @param height Slider height in UI units; must be positive.
     * @param pMinimum Non-null pointer to a lower-bound value of @p type.
     * @param pMaximum Non-null pointer to an upper-bound value of @p type.
     * @param pFormat Optional printf-style display format compatible with
     *                @p type.
     * @return true when the value changes; otherwise false.
     */
    bool (*pfnUiVSliderScalar)(const char* pLabel, EPluginUiScalarType type,
                               void* pValue, float width, float height,
                               const void* pMinimum, const void* pMaximum,
                               const char* pFormat);

    /**
     * @brief Converts a normalized RGBA color to packed ImGui-compatible RGBA.
     * @param red Red channel in [0.0, 1.0].
     * @param green Green channel in [0.0, 1.0].
     * @param blue Blue channel in [0.0, 1.0].
     * @param alpha Alpha channel in [0.0, 1.0].
     * @return Packed color value suitable for plugin-facing color utilities.
     */
    unsigned int (*pfnUiColorPackRgba)(float red, float green,
                                       float blue, float alpha);

    /**
     * @brief Converts a packed color value to normalized RGBA channels.
     * @param packedColor Color returned by pfnUiColorPackRgba().
     * @param pRed Non-null output receiving red in [0.0, 1.0].
     * @param pGreen Non-null output receiving green in [0.0, 1.0].
     * @param pBlue Non-null output receiving blue in [0.0, 1.0].
     * @param pAlpha Non-null output receiving alpha in [0.0, 1.0].
     */
    void (*pfnUiColorUnpackRgba)(unsigned int packedColor,
                                 float* pRed, float* pGreen,
                                 float* pBlue, float* pAlpha);
};

/**
 * @struct SPlugin
 * @brief Immutable plugin descriptor returned from GetPlugin().
 *
 * The host keeps and uses this descriptor while the plugin library is loaded.
 * Its name, version, and callback pointers must remain valid for that entire
 * period. The host expects every field to be initialized, including callbacks
 * that a plugin does not otherwise need; provide an empty callback rather than
 * a null pointer.
 */
struct SPlugin
{
    /**
     * @brief Human-readable plugin name shown in the plugin manager.
     * @note The string is borrowed by the host and must remain valid until the
     *       library is unloaded. A string literal or static storage is
     *       recommended.
     */
    const char* pName;

    /**
     * @brief Human-readable plugin version, conventionally semantic versioning.
     * @note Displayed with @ref pName and borrowed by the host; keep the string
     *       valid until the library is unloaded.
     */
    const char* pVersion;

    /**
     * @brief Called once after the shared library is loaded.
     *
     * Use this callback for initialization that should happen once per plugin
     * load, such as setting up plugin-owned state, registering UI/event
     * callbacks, and registering component factories. The supplied context is
     * borrowed and valid only for this invocation; do not store it for use in
     * later callbacks. Complete required initialization before returning.
     *
     * @param pCtx Host-provided context for the current initialization call.
     */
    void (*pfnOnLoad)(SPluginContext* pCtx);

    /**
     * @brief Called once just before the shared library is unloaded.
     *
     * Release plugin-owned allocations, resources, and external handles here.
     * The callback intentionally receives no context, so use plugin-owned
     * handles or state saved during initialization; do not access host objects
     * through a retained SPluginContext pointer.
     *
     * This callback must complete before unloading code or data belonging to
     * the plugin. The host removes plugin-associated UI and event callbacks as
     * part of unloading.
     */
    void (*pfnOnUnload)();

    /**
     * @brief Called by the host once per simulation frame before rendering.
     *
     * Use the per-frame @c pCtx->deltaTime value for time-based updates. Keep
     * the callback bounded and non-blocking; perform expensive file I/O or
     * independent CPU work through appropriate asynchronous facilities, and
     * avoid concurrently mutating the host scene.
     *
     * @param pCtx Current frame context, borrowed for the duration of this
     *             callback only.
     */
    void (*pfnOnUpdate)(SPluginContext* pCtx);

    /**
     * @brief Called by the host during the plugin UI pass.
     *
     * Submit widgets through the UI helpers on @p pCtx while the host UI frame
     * is active. Every pfnUiBegin() must be paired with pfnUiEnd(), including
     * when begin reports that its contents are not visible. Respect the
     * corresponding Begin/End pairing rules for menus, children, tables, tabs,
     * and disabled blocks.
     *
     * @param pCtx Current UI-pass context, borrowed for the duration of this
     *             callback only.
     */
    void (*pfnOnDrawUI)(SPluginContext* pCtx);
};

/**
 * @brief Plugin-library entry point — the only symbol the host resolves.
 *
 * The host resolves and calls this after loading the shared library, before
 * invoking any lifecycle callback. Return a fully initialized descriptor whose
 * storage and strings remain valid for the full loaded-library lifetime.
 * Returning nullptr causes plugin loading to fail.
 *
 * @return Pointer to a persistent SPlugin descriptor, or nullptr to reject
 *         loading. The name, version, and all lifecycle function pointers
 *         must be valid and non-null.
 */
PLUGIN_EXPORT SPlugin* GetPlugin();

#endif // __PLUGIN_H__
