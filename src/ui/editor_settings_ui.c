/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include "editor.h"
#include "editor_ui.h"
#include "editor_build.h"
#include "editor_panels.h"
#include "editor_utils.h"
#include <yoyoengine/yoyoengine.h>
#ifdef __linux__
    #include <unistd.h>
#else
    #include <platform/windows/unistd.h>
#endif

/*
    INITIALIZE VARIABLES FOR JUST THIS FILE
*/
bool project_settings_open = false;
json_t *BUILD_FILE;
/*
    VARIABLES INITIALIZED
*/

/*
    Popout panel for the project settings
*/
char project_name[256];
char project_entry_scene[256];
int project_log_level; // 0-4 (debug, info, warning, error, none)
int project_volume; // 0-128
bool project_debug_mode; // true or false
int project_screen_size_UNPROCESSED; // convert 0: 1920x1080, 1: 2560x1440
int project_window_mode_UNPROCESSED; // 0-2 (windowed, fullscreen, borderless) -> 1, 0, SDL_WINDOW_FULLSCREEN_DESKTOP
// we need to process the window mode, 3 should become SDL_WINDOW_FULLSCREEN_DESKTOP
int project_framecap; // -1 for vsync, else 0-MAXINT
int sdl_quality_hint; // 0-2 (nearest, linear, anisotropic)
char _project_framecap_label[10];
char project_window_title[256];
char project_icon_path[256];
bool project_stretch_viewport; // true or false
bool project_stretch_resolution; // true or false

// gravity and physics settings
float gravity_x;
float gravity_y;
float air_density;
int substeps;
// TODO: mass scaling? it breaks mid-sim
int joint_substeps;
int grid_size;
float mass_scale;
bool frustum_sleeping;

/*
    Build settings variables
*/
char build_additional_cflags[128];
char build_platform[32];
int original_build_platform_int; // tracks the target platform when opening settings, to compare with after closing if we need to set the "delete_cache" build.yoyo bool
int build_platform_int;
int build_mode_int;
char build_engine_tag_name[256];
char build_rc_path[256];
bool original_use_local_engine;
bool use_local_engine;
char local_engine_path[512];

/*
    Helper functions
*/
void editor_reload_build_file(){
    json_decref(BUILD_FILE);
    BUILD_FILE = ye_json_read(ye_path("build.yoyo"));
}

static void SDLCALL editor_engine_selector_dialog_cb(void* userdata, const char* const* filelist, int filter){
    if(!filelist) {
        ye_logf(YE_LL_ERROR, "An error occured: %s\n", SDL_GetError());
        return;
    }
    else if (!*filelist) {
        ye_logf(YE_LL_DEBUG, "No file selected in engine selector dialog.\n");
        return;
    }

    ye_logf(YE_LL_DEBUG, "Selected engine path: %s\n", *filelist);
    strncpy(local_engine_path, *filelist, (size_t)sizeof(local_engine_path) - 1);
    // do not free filelist
    
    (void)userdata; // unused
    (void)filter; // unused
}

void ye_editor_paint_project_settings(struct nk_context *ctx){
    if (nk_begin(ctx, "Settings", nk_rect(screenWidth/2 - 250, screenHeight/2 - 250, 500, 500),
        NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
            const struct nk_input *in = &ctx->input;
            struct nk_rect bounds;

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label_colored(ctx, "Project Settings:", NK_TEXT_CENTERED, nk_rgb(255, 255, 255));

            /*
                Project name
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Project Name:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "The name of your game!");
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, project_name, 256, nk_filter_default);

            /*
                Window Title
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Window Title:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "The title of the game window.");
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, project_window_title, 256, nk_filter_default);

            /*
                Icon Path
            */
            nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 2);
            nk_layout_row_push(ctx, 0.5f);

            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Icon Path:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "The path (from inside resources/) to the icon file you want to use for the game window.");
            nk_layout_row_push(ctx, 0.43f);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, project_icon_path, 256, nk_filter_default);

            nk_layout_row_push(ctx, 0.05f);
            nk_layout_row_push(ctx, 0.06f);
            if(nk_button_image(ctx, editor_icons.folder)){
                ye_pick_resource_file(
                    (struct ye_picker_data){
                        .filter = ye_picker_any_filters,
                        .num_filters = &ye_picker_num_any_filters,

                        .response_mode = YE_PICKER_WRITE_CHAR_BUF,
                        .dest.output_buf = {
                            .buffer = project_icon_path,
                            .size = sizeof(project_icon_path) - 1,
                        },
                    }
                );
            }

            /*
                Entry Scene
            */
            nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 2);
            nk_layout_row_push(ctx, 0.5f);

            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Entry Scene:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "The scene file you want to load first when the game runs (include extension and path relative to /resources/).");
            nk_layout_row_push(ctx, 0.43f);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, project_entry_scene, 256, nk_filter_default);

            nk_layout_row_push(ctx, 0.05f);
            nk_layout_row_push(ctx, 0.06f);
            if(nk_button_image(ctx, editor_icons.folder)){
                ye_pick_resource_file(
                    (struct ye_picker_data){
                        .filter = ye_picker_yoyo_filters,
                        .num_filters = &ye_picker_num_yoyo_filters,

                        .response_mode = YE_PICKER_WRITE_CHAR_BUF,
                        .dest.output_buf = {
                            .buffer = project_entry_scene,
                            .size = sizeof(project_entry_scene) - 1,
                        },
                    }
                );
            }

            /*
                Log Level
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Log Threshold:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Controls what logs get written at runtime, anything equivalent to, or more severe than your threshold will be logged.");
            // combo box with options cooresponding
            static const char *log_levels[] = {"Debug", "Info", "Warning", "Error", "None"};
            nk_combobox(ctx, log_levels, NK_LEN(log_levels), &project_log_level, 25, nk_vec2(200,200));

            /*
                Volume (slider: 0-128)
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            char volume_label[256];
            snprintf(volume_label, sizeof(volume_label), "Volume: %d", project_volume);
            nk_label(ctx, volume_label, NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "The volume of the game, 0-128");
            nk_slider_int(ctx, 0, &project_volume, 128, 1);

            /*
                Screen size (combo box) 1920x1080 2560x1440
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Window Size:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "The size of the game window.");
            static const char *screen_sizes[] = {"1920x1080", "2560x1440"};
            nk_combobox(ctx, screen_sizes, NK_LEN(screen_sizes), &project_screen_size_UNPROCESSED, 25, nk_vec2(200,200));

            /*
                Window Mode (combo box) Fullscreen, Windowed, Borderless
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Window Mode:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "The mode of the game window.");
            static const char *window_modes[] = {"Windowed","Fullscreen","Borderless"};
            nk_combobox(ctx, window_modes, NK_LEN(window_modes), &project_window_mode_UNPROCESSED, 25, nk_vec2(200,200));

            /*
                Framecap (int input, -1 for vsync)
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "FPS Cap:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "The maximum number of frames per second the game will run at. -1 for vsync.");
            if(nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, _project_framecap_label, 10, nk_filter_decimal)){
                project_framecap = atoi(_project_framecap_label);
            }

            /*
                Render quality hint
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Pixel Scaling", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "The quality of the pixel scaling. Choose \"nearest\" for pixel art.");
            static const char *pixel_scaling[] = {"nearest", "linear", "anisotropic"};
            nk_combobox(ctx, pixel_scaling, NK_LEN(pixel_scaling), &sdl_quality_hint, 25, nk_vec2(200,200));

            /*
                Physics Settings
            */
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label_colored(ctx, "Physics Settings:", NK_TEXT_CENTERED, nk_rgb(255, 255, 255));

            /*
                Gravity
            */
            nk_layout_row_dynamic(ctx, 25, 3);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Gravity Vec:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Controls the direction and magnitude of gravity.");
            nk_property_float(ctx, "#X:", -1000, &gravity_x, 1000, 0.1f, 0.1f);
            nk_property_float(ctx, "#Y:", -1000, &gravity_y, 1000, 0.1f, 0.1f);

            /*
                Grid Size
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Physics Cell Size:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Controls the size of broad phase detection cells.");
            nk_property_int(ctx, "#Size:", 25, &grid_size, 2000, 1, 5);

            /*
                Air Density (custom float input)
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Air Density:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Controls the density of air in the simulation, used for air resistance.");
            static char air_density_buf[32];
            snprintf(air_density_buf, sizeof(air_density_buf), "%.6f", air_density);
            if (nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, air_density_buf, sizeof(air_density_buf), nk_filter_float)) {
                float tmp = strtof(air_density_buf, NULL);
                if (tmp >= 0.0f && tmp <= 1.0f) air_density = tmp;
            }

            /*
                Substeps
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Substeps:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Controls the number of substeps in the physics simulation, higher values will result in more accurate physics but slower performance.");
            nk_property_int(ctx, "#Substeps:", 1, &substeps, 100, 1, 1);

            /*
                Joint Substeps
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Joint Substeps:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Controls the number of substeps for joint physics simulation, higher values will result in more accurate joint physics but slower performance.");
            nk_property_int(ctx, "#Substeps:", 1, &joint_substeps, 100, 1, 1);

            /*
                Mass scaling (custom float input)
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Mass Scaling:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Controls the scaling of mass in the physics simulation, higher values will result in heavier objects, lower values will result in lighter objects.");
            static char mass_scale_buf[32];
            snprintf(mass_scale_buf, sizeof(mass_scale_buf), "%.6f", mass_scale);
            if (nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, mass_scale_buf, sizeof(mass_scale_buf), nk_filter_float)) {
                float tmp = strtof(mass_scale_buf, NULL);
                if (tmp >= 0.000001f && tmp <= 1.0f) mass_scale = tmp;
            }

            // lay out our booleans
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label_colored(ctx, "Advanced Project Settings:", NK_TEXT_CENTERED, nk_rgb(255, 255, 255));
            nk_layout_row_dynamic(ctx, 25, 2);

            /*
                Checkbox for debug mode
            */
            bounds = nk_widget_bounds(ctx);
            nk_checkbox_label(ctx, "Debug Mode", (nk_bool*)&project_debug_mode);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "If enabled, console and other debug features will be enabled at runtime.");

            /*
                Checkbox for stretch viewport
            */
            bounds = nk_widget_bounds(ctx);
            nk_checkbox_label(ctx, "Stretch Viewport", (nk_bool*)&project_stretch_viewport);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "If checked, the viewport will stretch to the size of the window, if unchecked, the viewport will render from its actual perspective.");

            nk_layout_row_dynamic(ctx, 25, 1);
            /*
                Checkbox for stretch resolution
            */
            bounds = nk_widget_bounds(ctx);
            nk_checkbox_label(ctx, "Stretch Resolution", (nk_bool*)&project_stretch_resolution);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "If checked, the window will stretch the output to fill with no pillar or letter boxing.");

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label_colored(ctx, "Build Settings:", NK_TEXT_CENTERED, nk_rgb(255, 255, 255));

            /*
                Platform (dropdown)
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Platform:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "The platform you are building for.");
            static const char *platforms[] = {"linux", "windows", "emscripten"};
            nk_combobox(ctx, platforms, NK_LEN(platforms), &build_platform_int, 25, nk_vec2(200,200));

            /*
                Additional C Flags (string input)
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Additional C Flags:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Additional C flags to pass to the compiler.");
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, build_additional_cflags, 256, nk_filter_default);

            /*
                .rc path (string input)
            */
            nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 2);
            nk_layout_row_push(ctx, 0.5f);

            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, ".rc Path:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "The RELATIVE (to the project root) path to the .rc file you want to use, defines the .exe icon on windows.");
            nk_layout_row_push(ctx, 0.43f);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, build_rc_path, 256, nk_filter_default);

            nk_layout_row_push(ctx, 0.05f);
            nk_layout_row_push(ctx, 0.06f);
            if(nk_button_image(ctx, editor_icons.folder)){
                ye_pick_resource_file(
                    (struct ye_picker_data){
                        .filter = ye_picker_icon_filters,
                        .num_filters = &ye_picker_num_icon_filters,

                        .response_mode = YE_PICKER_WRITE_CHAR_BUF,
                        .dest.output_buf = {
                            .buffer = build_rc_path,
                            .size = sizeof(build_rc_path) - 1,
                        },
                    }
                );
            }

            /*
                Build Mode (dropdown for debug, release)
            */
            nk_layout_row_dynamic(ctx, 25, 2);
            bounds = nk_widget_bounds(ctx);
            nk_label(ctx, "Build Mode:", NK_TEXT_LEFT);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "The mode you are building for.");
            static const char *build_modes[] = {"Debug", "Release"};
            nk_combobox(ctx, build_modes, NK_LEN(build_modes), &build_mode_int, 25, nk_vec2(200,200));

            /*
                Advanced Build Settings
            */
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);

            nk_label(ctx, "Advanced Build Settings:", NK_TEXT_CENTERED);

            // toggle for using local engine copy
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "", NK_TEXT_LEFT);
            bounds = nk_widget_bounds(ctx);
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "If disabled, the game will fetch the engine at the specified tag. If enabled, the game will use a supplied copy.");
            nk_checkbox_label(ctx, "Use Local Engine Copy", (nk_bool*)&use_local_engine);

            if(use_local_engine) {
                // local engine path selector
                nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 3);
                nk_layout_row_push(ctx, 0.5f);
                nk_label(ctx, "Local Engine Path:", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.43f);
                nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, local_engine_path, 256, nk_filter_default);

                nk_layout_row_push(ctx, 0.05f);
                nk_layout_row_push(ctx, 0.06f);
                if(nk_button_image(ctx, editor_icons.folder)){
                    SDL_ShowOpenFolderDialog(editor_engine_selector_dialog_cb, NULL, YE_STATE.runtime.window, NULL, false);
                }
            }
            else {
                /*
                    Engine tag (string input)

                    Determines the pulled core version of the engine when building
                */
                nk_layout_row_dynamic(ctx, 25, 2);
                bounds = nk_widget_bounds(ctx);
                nk_label(ctx, "Engine Tag Name:", NK_TEXT_LEFT);
                if (nk_input_is_mouse_hovering_rect(in, bounds))
                    nk_tooltip(ctx, "The tag name of the engine you want to build against (ex: build-0).");
                nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, build_engine_tag_name, 256, nk_filter_default);
            }

            nk_layout_row_dynamic(ctx, 25, 1);

            // close button
            nk_layout_row_dynamic(ctx, 25, 1);
            if(nk_button_label(ctx, "Save and Close")){

                editor_saving();

                project_settings_open = false;
                lock_viewport_interaction = false;

                // update all the keys in SETTINGS
                json_object_set_new(SETTINGS, "name", json_string(project_name));
                json_object_set_new(SETTINGS, "entry_scene", json_string(project_entry_scene));
                json_object_set_new(SETTINGS, "window_title", json_string(project_window_title));
                json_object_set_new(SETTINGS, "icon_path", json_string(project_icon_path));
                json_object_set_new(SETTINGS, "log_level", json_integer(project_log_level));
                json_object_set_new(SETTINGS, "volume", json_integer(project_volume));
                json_object_set_new(SETTINGS, "debug_mode", project_debug_mode ? json_true() : json_false());
                json_object_set_new(SETTINGS, "screen_width", project_screen_size_UNPROCESSED == 0 ? json_integer(1920) : json_integer(2560));
                json_object_set_new(SETTINGS, "screen_height", project_screen_size_UNPROCESSED == 0 ? json_integer(1080) : json_integer(1440));
                if (project_window_mode_UNPROCESSED == 2) {
                    json_object_set_new(SETTINGS, "window_mode", json_integer(SDL_WINDOW_FULLSCREEN));
                } else {
                    json_object_set_new(SETTINGS, "window_mode", project_window_mode_UNPROCESSED == 0 ? json_integer(0) : json_integer(1));
                }
                json_object_set_new(SETTINGS, "framecap", json_integer(project_framecap));
                json_object_set_new(SETTINGS, "stretch_viewport", project_stretch_viewport ? json_true() : json_false());
                json_object_set_new(SETTINGS, "stretch_resolution", project_stretch_resolution ? json_true() : json_false());
                json_object_set_new(SETTINGS, "sdl_quality_hint", json_integer(sdl_quality_hint));
                json_object_set_new(SETTINGS, "p2d_gravity_x", json_real(gravity_x));
                json_object_set_new(SETTINGS, "p2d_gravity_y", json_real(gravity_y));
                json_object_set_new(SETTINGS, "p2d_grid_size", json_integer(grid_size));
                json_object_set_new(SETTINGS, "p2d_air_density", json_real(air_density));
                json_object_set_new(SETTINGS, "p2d_substeps", json_integer(substeps));
                json_object_set_new(SETTINGS, "p2d_joint_substeps", json_integer(joint_substeps));
                json_object_set_new(SETTINGS, "p2d_frustum_sleeping", frustum_sleeping ? json_true() : json_false());
                json_object_set_new(SETTINGS, "p2d_mass_scaling", json_real(mass_scale));

                // edge: when saving actually update in p2d_state
                YE_STATE.engine.p2d_state->p2d_gravity.x = gravity_x;
                YE_STATE.engine.p2d_state->p2d_gravity.y = gravity_y;
                YE_STATE.engine.p2d_state->p2d_cell_size= grid_size;
                YE_STATE.engine.p2d_state->p2d_air_density = air_density;
                YE_STATE.engine.p2d_state->p2d_substeps = substeps;
                YE_STATE.engine.p2d_state->p2d_joint_iterations = joint_substeps;
                // TODO: requires us to track active camera or do lookup from p2d (im too lazy rn)
                // YE_STATE.engine.p2d_state->p2d_frustum_sleeping = frustum_sleeping;
                YE_STATE.engine.p2d_state->p2d_mass_scaling = mass_scale;
                
                // save the settings file
                ye_json_write(ye_path("settings.yoyo"),SETTINGS);
                
                // update build keys, then save
                json_object_set_new(BUILD_FILE, "build_mode", json_string(build_modes[build_mode_int]));
                json_object_set_new(BUILD_FILE, "cflags", json_string(build_additional_cflags));
                json_object_set_new(BUILD_FILE, "rc_path", json_string(build_rc_path));
                json_object_set_new(BUILD_FILE, "platform", json_string(platforms[build_platform_int]));
                json_object_set_new(BUILD_FILE, "core_tag", json_string(build_engine_tag_name));
                json_object_set_new(BUILD_FILE, "delete_cache", json_boolean((original_build_platform_int != build_platform_int) || (original_use_local_engine != use_local_engine)));
                json_object_set_new(BUILD_FILE, "use_local_engine", json_boolean(use_local_engine));
                json_object_set_new(BUILD_FILE, "local_engine_path", json_string(local_engine_path));
                ye_json_write(ye_path("build.yoyo"),BUILD_FILE);

                editor_saved();

                /*
                    This should be done in a modular way, engine should be rebuilt smartly also, we will want button for it later in dev when engine changes all the time
                    TODO TODO TODO TODO TODO TODO
                */

                remove_ui_component("project settings");
            }
            if(nk_button_label(ctx, "Cancel")){
                project_settings_open = false;
                lock_viewport_interaction = false;
                remove_ui_component("project settings");
            }
        nk_end(ctx);
    }
}

void ye_editor_paint_project(struct nk_context *ctx){
    if (nk_begin(ctx, "Project", nk_rect(0, 40 + screenHeight/1.5, screenWidth/1.5 / 2, screenHeight - (screenHeight/1.5) - 40),
        NK_WINDOW_TITLE | NK_WINDOW_BORDER)) {
            const struct nk_input *in = &ctx->input;
            struct nk_rect bounds;

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label_colored(ctx, "Build Options:", NK_TEXT_LEFT, nk_rgb(255, 255, 255));
            nk_layout_row_static(ctx, 55, 55, 5);

            bounds = nk_widget_bounds(ctx);
            if(nk_button_image(ctx, editor_icons.pack)){
                editor_build_packs(true);
            }
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Rebuild (FORCED) the yep packs");

            bounds = nk_widget_bounds(ctx);
            if(nk_button_image(ctx, editor_icons.buildreconfigure)){
                editor_build_reconfigure();
            }
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Reconfigure and build the project");
            
            bounds = nk_widget_bounds(ctx);
            if(nk_button_image(ctx, editor_icons.build)){
                editor_build(false, false);
            }
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Build the project");

            bounds = nk_widget_bounds(ctx);
            if(nk_button_image(ctx, editor_icons.buildrun)){
                editor_build_and_run();
            }
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Build the project and run it");

            bounds = nk_widget_bounds(ctx);
            if(nk_button_image(ctx, editor_icons.play)){
                editor_run();
            }
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Run the project");

            nk_layout_row_dynamic(ctx, 35, 1);
            nk_label_colored(ctx, "Additional Actions:", NK_TEXT_LEFT, nk_rgb(255, 255, 255));
            nk_layout_row_dynamic(ctx, 35, 2);
            // button to open project settings
            if(nk_button_image_label(ctx, editor_icons.gear, "Project Settings", NK_TEXT_CENTERED)){
                if(project_settings_open){
                    project_settings_open = false;
                    lock_viewport_interaction = false;
                    remove_ui_component("project settings");
                }
                else{
                    editor_reload_settings();
                    editor_reload_build_file();
                    project_settings_open = true;
                    lock_viewport_interaction = true;
                    ui_register_component("project settings",ye_editor_paint_project_settings);
                    // ye_json_log(SETTINGS);
                    // setup a bunch of stack variables for showing current settings
                    
                    /*
                        Project Name
                    */
                    const char * tmp_project_name; 
                    if(!ye_json_string(SETTINGS, "name", &tmp_project_name)){
                        strcpy((char*)tmp_project_name, "Yoyo Engine Game");
                    }
                    strncpy(project_name, (char*)tmp_project_name, (size_t)sizeof(project_name) - 1);
                    project_name[(size_t)sizeof(project_name) - 1] = '\0'; // null terminate just in case TODO: write helper?


                    /*
                        Entry Scene
                    */
                    const char * tmp_project_entry_scene;
                    if(!ye_json_string(SETTINGS,"entry_scene", &tmp_project_entry_scene)){
                        strcpy((char*)tmp_project_entry_scene, "scenes/entry.yoyo");
                    }
                    strncpy(project_entry_scene, (char*)tmp_project_entry_scene, (size_t)sizeof(project_entry_scene) - 1);
                    project_entry_scene[(size_t)sizeof(project_entry_scene) - 1] = '\0'; // null terminate just in case TODO: write helper?

                    /*
                        Window Title
                    */
                    const char * tmp_project_window_title;
                    if(!ye_json_string(SETTINGS, "window_title", &tmp_project_window_title)){
                        strcpy((char*)tmp_project_window_title, "Yoyo Engine Game");
                    }
                    strncpy(project_window_title, (char*)tmp_project_window_title, (size_t)sizeof(project_window_title) - 1);
                    project_window_title[(size_t)sizeof(project_window_title) - 1] = '\0'; // null terminate just in case TODO: write helper?

                    // TODO: this whole section is full of memory errors.
                    // idk what type of crack you were smoking when you
                    // wrote this lmao.

                    /*
                        Icon Path
                    */
                    const char * tmp_project_icon_path;
                    if(!ye_json_string(SETTINGS, "icon_path", &tmp_project_icon_path)){
                        tmp_project_icon_path = strdup("");
                    }
                    strncpy(project_icon_path, (char*)tmp_project_icon_path, (size_t)sizeof(project_icon_path) - 1);

                    /*
                        Log Level
                    */
                    if(!ye_json_int(SETTINGS, "log_level", &project_log_level)){
                        project_log_level = 0;
                    }

                    /*
                        Volume
                    */
                    if(!ye_json_int(SETTINGS, "volume", &project_volume)){
                        project_volume = 128;
                    }

                    /*
                        Debug Mode
                    */
                    if(!ye_json_bool(SETTINGS, "debug_mode", &project_debug_mode)){
                        project_debug_mode = false;
                    }

                    /*
                        Window Size
                    */
                    int sw;
                    if(!ye_json_int(SETTINGS, "screen_width", &sw)){
                        project_screen_size_UNPROCESSED = 0;
                    }
                    else{
                        if(sw == 1920){
                            project_screen_size_UNPROCESSED = 0;
                        }
                        else if(sw == 2560){
                            project_screen_size_UNPROCESSED = 1;
                        }
                        else{
                            project_screen_size_UNPROCESSED = 0; // 1920x1080 defualt
                        }
                    }

                    /*
                        Window Mode
                    */
                    if(!ye_json_int(SETTINGS, "window_mode", &project_window_mode_UNPROCESSED)){
                        project_window_mode_UNPROCESSED = 0;
                    }else{
                        if(project_window_mode_UNPROCESSED == SDL_WINDOW_FULLSCREEN){
                            project_window_mode_UNPROCESSED = 2;
                        }
                    }

                    /*
                        Framecap
                    */
                    if(!ye_json_int(SETTINGS, "framecap", &project_framecap)){
                        project_framecap = -1;
                        sprintf(_project_framecap_label, "-1");
                    }
                    else{
                        sprintf(_project_framecap_label, "%d", project_framecap);
                    }

                    /*
                        Stretch Viewport
                    */
                    if(!ye_json_bool(SETTINGS, "stretch_viewport", &project_stretch_viewport)){
                        project_stretch_viewport = false;
                    }

                    /*
                        Stretch Resolution
                    */
                    if(!ye_json_bool(SETTINGS, "stretch_resolution", &project_stretch_resolution)){
                        project_stretch_resolution = false;
                    }

                    /*
                        sdl_quality_hint
                    */
                    if(!ye_json_int(SETTINGS, "sdl_quality_hint", &sdl_quality_hint)){
                        sdl_quality_hint = 1;
                    }

                    /*
                        Gravity
                    */
                    if(!ye_json_float(SETTINGS, "p2d_gravity_x", &gravity_x)){
                        gravity_x = 0.0f;
                    }
                    if(!ye_json_float(SETTINGS, "p2d_gravity_y", &gravity_y)){
                        gravity_y = 20.0f;
                    }

                    /*
                        Grid Size
                    */
                    if(!ye_json_int(SETTINGS, "p2d_grid_size", &grid_size)){
                        grid_size = 200;
                    }

                    /*
                        Air Density
                    */
                    if(!ye_json_float(SETTINGS, "p2d_air_density", &air_density)){
                        air_density = P2D_DEFAULT_AIR_DENSITY;
                    }

                    /*
                        Substeps
                    */
                    if(!ye_json_int(SETTINGS, "p2d_substeps", &substeps)){
                        substeps = P2D_DEFAULT_SUBSTEPS;
                    }

                    /*
                        Joint Substeps
                    */
                    if(!ye_json_int(SETTINGS, "p2d_joint_substeps", &joint_substeps)){
                        joint_substeps = P2D_DEFAULT_JOINT_SUBSTEPS;
                    }

                    /*
                        Frustum Sleeping
                    */
                    if(!ye_json_bool(SETTINGS, "p2d_frustum_sleeping", &frustum_sleeping)){
                        frustum_sleeping = false;
                    }

                    /*
                        Mass Scaling
                    */
                    if(!ye_json_float(SETTINGS, "p2d_mass_scaling", &mass_scale)){
                        mass_scale = P2D_DEFAULT_MASS_SCALE;
                    }
                    

                    /*
                        Open our build file
                    */
                    BUILD_FILE = ye_json_read(ye_path("build.yoyo"));
                    if(BUILD_FILE){
                        /*
                            Additional C Flags
                        */
                        const char * tmp_build_additional_cflags;
                        if(!ye_json_string(BUILD_FILE, "cflags", &tmp_build_additional_cflags)){
                            strcpy((char*)tmp_build_additional_cflags,"");
                        }
                        strncpy(build_additional_cflags, (char*)tmp_build_additional_cflags, (size_t)sizeof(build_additional_cflags) - 1);
                        build_additional_cflags[(size_t)sizeof(build_additional_cflags) - 1] = '\0'; // null terminate just in case TODO: write helper?

                        /*
                            .rc path
                        */
                        const char * tmp_build_rc_path;
                        if(!ye_json_string(BUILD_FILE, "rc_path", &tmp_build_rc_path)){
                            strcpy((char*)tmp_build_rc_path,"");
                        }
                        strncpy(build_rc_path, (char*)tmp_build_rc_path, (size_t)sizeof(build_rc_path) - 1);
                        build_rc_path[(size_t)sizeof(build_rc_path) - 1] = '\0'; // null terminate just in case TODO: write helper?

                        /*
                            Build mode
                        */
                        const char * tmp_build_mode;
                        if(!ye_json_string(BUILD_FILE, "build_mode", &tmp_build_mode)){
                            build_mode_int = 0;
                        }
                        else{
                            if(strcmp(tmp_build_mode, "release") == 0){
                                build_mode_int = 1;
                            }
                            else{
                                build_mode_int = 0;
                            }
                        }

                        /*
                            Engine tag name
                        */
                        const char * tmp_build_engine_tag_name;
                        char tmp_buffer[512];
                        if(ye_json_string(BUILD_FILE, "core_tag", &tmp_build_engine_tag_name)){
                            strncpy(tmp_buffer, tmp_build_engine_tag_name, sizeof(tmp_buffer) - 1);
                            tmp_buffer[sizeof(tmp_buffer) - 1] = '\0'; // null terminate just in case
                        } else {
                            strncpy(tmp_buffer, YOYO_ENGINE_VERSION_STRING, sizeof(tmp_buffer) - 1);
                            tmp_buffer[sizeof(tmp_buffer) - 1] = '\0'; // null terminate just in case
                        }
                        ye_version_tagify(tmp_buffer);
                        strncpy(build_engine_tag_name, tmp_buffer, sizeof(build_engine_tag_name) - 1);
                        build_engine_tag_name[sizeof(build_engine_tag_name) - 1] = '\0'; // null terminate just in case TODO: write helper?

                        /*
                            Platform
                        */
                        const char * tmp_build_platform;
                        if(!ye_json_string(BUILD_FILE, "platform", &tmp_build_platform)){
                            strcpy((char*)tmp_build_platform,"linux");
                            build_platform_int = 0;
                        }
                        else{
                            if(strcmp(tmp_build_platform, "windows") == 0){
                                build_platform_int = 1;
                            }
                            else if(strcmp(tmp_build_platform, "emscripten") == 0){
                                build_platform_int = 2;
                            }
                            else{
                                build_platform_int = 0;
                            }
                        }
                        original_build_platform_int = build_platform_int;
                        strncpy(build_platform, (char*)tmp_build_platform, (size_t)sizeof(build_platform) - 1);
                        build_platform[(size_t)sizeof(build_platform) - 1] = '\0'; // null terminate just in case TODO: write helper?

                        /*
                            Use local engine
                        */
                        if(!ye_json_bool(BUILD_FILE, "use_local_engine", &use_local_engine)){
                            use_local_engine = false;
                        }
                        original_use_local_engine = use_local_engine;

                        /*
                            Local engine path
                        */
                        const char * tmp_local_engine_path;
                        if(!ye_json_string(BUILD_FILE, "local_engine_path", &tmp_local_engine_path)){
                            strcpy((char*)tmp_local_engine_path,"");
                        }
                        strncpy(local_engine_path, (char*)tmp_local_engine_path, (size_t)sizeof(local_engine_path) - 1);
                        local_engine_path[(size_t)sizeof(local_engine_path) - 1] = '\0'; // null terminate just in case TODO: write helper?
                    }
                    else{
                        ye_logf(error, "build.yoyo not found.");
                        strcpy((char*)build_additional_cflags,"");
                        strcpy((char*)build_rc_path,"");
                        strcpy((char*)build_platform,"linux");
                        build_platform_int = 0;
                        ye_version_tagify(build_engine_tag_name);
                    }
                }
            }
            if(nk_button_image_label(ctx, editor_icons.folder, "Browse Project Files", NK_TEXT_CENTERED)){
                editor_open_in_system(EDITOR_STATE.opened_project_path);
            }
            if(nk_button_image_label(ctx, editor_icons.style, "Edit styles.yoyo", NK_TEXT_CENTERED)){
                /*
                    Open a popout editor for the editor styles
                */
                if(!ui_component_exists("editor styles")){
                    ui_register_component("editor styles", editor_panel_styles);
                    lock_viewport();
                } else {
                    remove_ui_component("editor styles");
                    unlock_viewport();
                }
            }
            // if(nk_button_image_label(ctx, editor_icons.trick, "Manage/Install Tricks", NK_TEXT_CENTERED)){
            //     /*
            //         Open a popout editor for managing tricks
            //     */
            //     if(!ui_component_exists("trick panel")){
            //         ui_register_component("trick panel", editor_panel_tricks);
            //         lock_viewport();
            //     }// else {
            //     //     remove_ui_component("trick panel");
            //     //     unlock_viewport();
            //     // } this would not clean up memory, so just force closing through the panel for now
            // }
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label_colored(ctx, "Copyright (c) Ryan Zmuda 2023-2025", NK_TEXT_CENTERED, nk_rgb(255, 255, 255));
        nk_end(ctx);
    }
}

void editor_settings_ui_init(){
    /*
        For lack of a better memory management solution... im tempted to just make all the fields rely on internal jansson strings.. but then what about others proceduarl during runtime?? arghh
    */

    /*
        dumbass, you could just have all of these malloced and then strcpy a temp var extracted from json to them.
    */
}

void editor_settings_ui_shutdown(){
    json_decref(BUILD_FILE);
}