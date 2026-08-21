#ifndef __APPLICATION_H__
#define __APPLICATION_H__
#include "QuarkCore/QuarkCore.hpp"
#include "editor/editor.h"
#include "camera.h"
#include "command_line.h"
#include <array>
#include <string>

struct Application {
    CommandLineOptions options;

    Editor editor;
    FlyCamera camera;

    Shader lighting_shader{};
    Shader shadow_shader{};

    int shadows_enabled_loc = -1;
    int shadow_bias_loc = -1;
    int shadow_filter_loc = -1;
    int use_tex_loc = -1;
    int ambient_loc = -1;
    int emission_color_loc = -1;
    int emission_power_loc = -1;

    std::array<RenderTexture2D, QC_MAX_LIGHTS> shadow_maps{};
    std::array<Camera3D, QC_MAX_LIGHTS> shadow_cameras{};
    std::array<int, QC_MAX_LIGHTS> light_view_locations{};
    std::array<int, QC_MAX_LIGHTS> light_projection_locations{};

    std::string project_path;
    std::string active_font_language;
    double last_autosave_time = 0.0;
    int last_selected_entity = -1;

    bool headless = false;
    bool ready_to_run = false;

    explicit Application(const CommandLineOptions& options);

    void initialize();
    void run();
    void shutdown();

    void update_frame();
    void render_frame();
};

#endif // __APPLICATION_H__
