#pragma once
#include "../headers/scene.h"

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

struct PluginContext;

/**
 * @enum UIRegion
 * @brief Logical zones of the editor UI where plugins can inject custom UI.
 * 
 * Used to reigster callbacks that will be called only when the corresponding
 * part of the interface is being drawn.
 */
enum UIRegion {
    UI_MENU_FILE,
    UI_MENU_EDIT,
    UI_MENU_HELP,
    UI_HIERARCHY,
    UI_INSPECTOR,
    UI_SCENE
};

/**
 * @typedef PluginUICallback
 * @param ctx Pointer to the host-provided plugin context.
 * @brief Function pointer type for UI callbacks executed in a specific UIRegion.
 * 
 * Called by the host when rendering a registered UI region. Receives the
 * current PluginContext for accessing UI and engine state.
 */
using PluginUICallback = void(*)(PluginContext*);

/**
 * @struct PluginContext
 * @brief Per-frame host state passed into every plugin callback.
 *
 * Contains runtime statistics (timing, entity count, selection state) and a
 * complete set of host-provided function pointers for UI drawing, entity
 * inspection, entity mutation, and scene management.
 *
 * @warning The plugin must **not** store this pointer beyond the call that
 *          received it — the host may reallocate the context between frames.
 */
struct PluginContext {
    /** @brief Seconds elapsed since the previous frame.
     *  Use for frame-rate-independent motion and animation. */
    float delta_time;

    /** @brief Total number of entities currently alive in the scene.
     *  Valid entity indices are in the range [0, entity_count). */
    int entity_count;

    /** @brief Pointer to the index of the currently selected entity,
     *  or nullptr if no entity is selected. Always null-check before
     *  dereferencing. */
    int* selected;

    // -------------------------------------------------------------------------
    // UI — thin wrapper over an ImGui-style immediate-mode UI
    // -------------------------------------------------------------------------

    /**
     * @brief Opens a UI window with the given title.
     * @param title Null-terminated window title string.
     * @return true if the window is visible and its contents should be drawn.
     * @note Must always be paired with a call to ui_end(), even when returning false.
     */
    bool (*ui_begin)(const char* title);

    /**
     * @brief Closes the window opened by the most recent ui_begin() call.
     */
    void (*ui_end)();

    /**
     * @brief Begins a menu section in the UI (e.g. top menu bar entry).
     * @param label Menu name.
     * @return true if the menu is open and its items should be drawn.
     */
    bool (*ui_begin_menu)(const char* label);
    
    /**
     * @brief Ends the most recently opened menu section.
     */
    void (*ui_end_menu)();

    /**
     * @brief Creates a clickable item inside a menu.
     * @param label Item text.
     * @return true when the item is clicked.
     */
    bool (*ui_menu_item)(const char* label);

    void (*register_ui_callback)(UIRegion region, PluginUICallback callback);

    /**
     * @brief Renders a read-only text label inside the current window.
     * @param text Null-terminated string to display.
     */
    void (*ui_text)(const char* text);

    /**
     * @brief Renders a clickable button.
     * @param label Null-terminated button label.
     * @return true on the single frame the button is clicked.
     */
    bool (*ui_button)(const char* label);

    /**
     * @brief Renders a checkbox bound to an external bool.
     * @param label  Null-terminated label shown next to the checkbox.
     * @param value  Pointer to the bool to read from and write to.
     * @return true when the value changes.
     */
    bool (*ui_checkbox)(const char* label, bool* value);

    /**
     * @brief Renders a float slider clamped to [min, max].
     * @param label  Null-terminated label.
     * @param value  Pointer to the float to read from and write to.
     * @param min    Lower bound of the slider range.
     * @param max    Upper bound of the slider range.
     * @return true while the slider is being dragged.
     */
    bool (*ui_slider_float)(const char* label, float* value, float min, float max);

    /**
     * @brief Renders a direct-entry float input field.
     * @param label  Null-terminated label.
     * @param value  Pointer to the float to read from and write to.
     * @return true when the value is committed (Enter key or focus loss).
     */
    bool (*ui_input_float)(const char* label, float* value);

    /**
     * @brief Renders an RGB color picker.
     * @param label  Null-terminated label.
     * @param color  Three-element float array in [0.0, 1.0] range (R, G, B).
     * @return true when any color component changes.
     */
    bool (*ui_color_edit3)(const char* label, float color[3]);

    /**
     * @brief Draws a horizontal separator line inside the current window.
     */
    void (*ui_separator)();

    /**
     * @brief Places the next widget on the same horizontal line as the
     *        previous one, suppressing the automatic line break.
     */
    void (*ui_same_line)();

    // -------------------------------------------------------------------------
    // Entity read — query entity state by index
    // -------------------------------------------------------------------------

    /**
     * @brief Returns the display name of an entity.
     * @param index Entity index in [0, entity_count).
     * @return Pointer to the entity's name string, owned by the host.
     *         Do not free or modify this pointer.
     */
    const char* (*entity_get_name)(int index);

    /**
     * @brief Writes an entity's world-space position into output parameters.
     * @param index  Entity index.
     * @param x,y,z  Output pointers for the X, Y, Z position components.
     */
    void (*entity_get_position)(int index, float* x, float* y, float* z);

    /**
     * @brief Writes an entity's Euler rotation into output parameters.
     * @param index  Entity index.
     * @param x,y,z  Output pointers for the X, Y, Z rotation components
     *               (host-defined unit — degrees or radians).
     */
    void (*entity_get_rotation)(int index, float* x, float* y, float* z);

    /**
     * @brief Writes an entity's scale into output parameters.
     * @param index  Entity index.
     * @param x,y,z  Output pointers for the X, Y, Z scale components.
     */
    void (*entity_get_scale)(int index, float* x, float* y, float* z);

    /**
     * @brief Writes an entity's RGBA tint color into output parameters.
     * @param index    Entity index.
     * @param r,g,b,a  Output pointers for the red, green, blue, and alpha
     *                 channels as unsigned bytes (0–255).
     */
    void (*entity_get_color)(int index, unsigned char* r, unsigned char* g,
                             unsigned char* b, unsigned char* a);

    // -------------------------------------------------------------------------
    // Entity write — mutate entity state by index
    // -------------------------------------------------------------------------

    /**
     * @brief Moves an entity to a new world-space position.
     * @param index  Entity index.
     * @param x,y,z  New position components.
     */
    void (*entity_set_position)(int index, float x, float y, float z);

    /**
     * @brief Sets an entity's Euler rotation.
     * @param index  Entity index.
     * @param x,y,z  New rotation components (host-defined unit).
     */
    void (*entity_set_rotation)(int index, float x, float y, float z);

    /**
     * @brief Sets an entity's scale.
     * @param index  Entity index.
     * @param x,y,z  New scale components.
     */
    void (*entity_set_scale)(int index, float x, float y, float z);

    /**
     * @brief Sets an entity's RGBA tint color.
     * @param index    Entity index.
     * @param r,g,b,a  New color channel values as unsigned bytes (0–255).
     */
    void (*entity_set_color)(int index, unsigned char r, unsigned char g,
                             unsigned char b, unsigned char a);

    /**
     * @brief Renames an entity.
     * @param index  Entity index.
     * @param name   Null-terminated string. The host copies the string
     *               internally; the caller may free its buffer after this
     *               returns.
     */
    void (*entity_set_name)(int index, const char* name);

    // -------------------------------------------------------------------------
    // Scene management
    // -------------------------------------------------------------------------

    /**
     * @brief Current scene state owned by the host.
     */
    Scene scene;

    /**
     * @brief Serialises the current scene to disk using the host's default
     *        save path. Equivalent to the user pressing Ctrl-S.
     */
    void (*scene_save)();

    /**
     * @brief Instantiates an asset and adds it to the scene.
     * @param asset_name  Null-terminated asset identifier recognised by the
     *                    host's asset registry.
     * @return The new entity's index (>= 0) on success, or -1 on failure
     *         (e.g. unknown asset name).
     */
    int (*scene_spawn)(const char* asset_name);

    /**
     * @brief Permanently removes an entity from the scene.
     * @param index  Entity index to delete.
     * @warning After this call, indices for all entities above @p index may
     *          be shifted. Re-query entity_count and any cached indices.
     */
    void (*scene_delete)(int index);
};

/**
 * @struct Plugin
 * @brief Descriptor returned by get_plugin(). Identifies the plugin and
 *        provides the four lifecycle callbacks the host will invoke.
 *
 * All pointer fields must be non-null; the host does not null-check before
 * calling them.
 */
struct Plugin {
    /** @brief Human-readable plugin name shown in the host's plugin manager.
     *  Must be a static string literal — the host will not free it. */
    const char* name;

    /** @brief Semantic version string (e.g. "1.0.0"). Displayed alongside
     *  @ref name for diagnostics. Must be a static string literal. */
    const char* version;

    /**
     * @brief Called once after the shared library is loaded.
     *
     * Use for one-time initialisation: allocating state, registering
     * resources, reading configuration. @p ctx is valid only for the
     * duration of this call.
     *
     * @param ctx  Pointer to the host-provided plugin context.
     */
    void (*on_load)(PluginContext* ctx);

    /**
     * @brief Called once just before the shared library is unloaded.
     *
     * Free all heap allocations and release external resources here.
     * The PluginContext is no longer available at this point.
     */
    void (*on_unload)();

    /**
     * @brief Called every frame during the simulation tick, before rendering.
     *
     * Use @c ctx->delta_time for frame-rate-independent logic. Keep this
     * path fast — avoid heavy I/O or blocking calls.
     *
     * @param ctx  Pointer to the current frame's plugin context.
     */
    void (*on_update)(PluginContext* ctx);

    /**
     * @brief Called every frame during the UI pass.
     *
     * Use the @c ui_* function pointers on @p ctx to draw controls.
     * Every successful @c ui_begin() call must be matched by @c ui_end().
     *
     * @param ctx  Pointer to the current frame's plugin context.
     */
    void (*on_draw_ui)(PluginContext* ctx);
};

/**
 * @brief Plugin entry point — the only symbol the host loads from the DLL.
 *
 * The host calls this immediately after loading the shared library to obtain
 * the plugin descriptor. The returned pointer must remain valid for the entire
 * lifetime of the loaded library; use a static or heap-allocated Plugin
 * instance. Returning nullptr causes the host to abort loading and unload the
 * library.
 *
 * @return Non-null pointer to a fully initialised Plugin descriptor.
 *         All four function pointer fields must be non-null.
 */
PLUGIN_EXPORT Plugin* get_plugin();