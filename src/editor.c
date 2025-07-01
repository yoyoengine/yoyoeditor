/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include <stdio.h>
#ifdef __linux__
    #include <unistd.h>
    #include <sys/wait.h>
    #include <linux/wait.h>
#else
    #include <platform/windows/unistd.h>
    // Windows doesn't have sys/wait.h or linux/wait.h
#endif

#include <SDL.h>
#include <SDL_image.h>

#include <yoyoengine/yoyoengine.h>
#include <yoyoengine/logging.h>

#include <yoyoengine/ye_nk.h>

#include "editor.h"
#include "editor_ui.h"
#include "editor_build.h"
#include "editor_utils.h"
#include "editor_input.h"
#include "editor_panels.h"
#include "editor_selection.h"
#include "editor_settings_ui.h"

// make some editor specific declarations to change engine core behavior
#define YE_EDITOR

/*
    INITIALIZE ALL
*/
struct editor_prefs PREFS = {0};
struct editor_state EDITOR_STATE = {0};

bool unsaved;
bool saving; // in the process of saving
bool quit;
bool lock_viewport_interaction;

struct ye_entity *editor_camera = NULL;
struct ye_entity *origin = NULL;

// this is so yucky to have this as a global, refactor TODO
float screenWidth;
float screenHeight;

struct ye_entity_node *entity_list_head;
struct ye_entity staged_entity;
json_t *SETTINGS;

// holds the path to the editor settings file
char editor_settings_path[1024];

int mouse_world_x = 0;
int mouse_world_y = 0;

// selecting info
SDL_Rect editor_selecting_rect;

// panning info
SDL_Point pan_start;
SDL_Point pan_end;
bool editor_panning = false;

// nk_image icons
struct edicons editor_icons;

char *editor_base_path = NULL;

/*
    ECS HELPERS
*/
void editor_find_or_create_entities(void) {
    editor_camera = ye_get_entity_by_name("editor_camera");
    if (!editor_camera) {
		editor_camera = ye_create_entity_named("editor_camera");
		ye_add_transform_component(editor_camera, 0, 0);
		ye_add_camera_component(editor_camera, 999, (struct ye_rectf){0, 0, 2560, 1440});
		ye_set_camera(editor_camera);
	}

    origin = ye_get_entity_by_name("origin");
	if (!origin) {
		origin = ye_create_entity_named("origin");
		ye_add_transform_component(origin, -50, -50);

		SDL_Texture *orgn_tex = SDL_CreateTextureFromSurface(YE_STATE.runtime.renderer, yep_engine_resource_image("originwhite.png"));
		ye_cache_texture_manual(orgn_tex, "originwhite.png");
		ye_add_image_renderer_component_preloaded(origin, 0, orgn_tex);
		origin->renderer->rect = (struct ye_rectf){0, 0, 100, 100};
	}
}

/*
    ALL GLOBALS INITIALIZED
*/

char * editor_path(const char *subpath) {
    static char path[2048];

    snprintf(path, sizeof(path), "%s/%s", editor_base_path, subpath);

    return path;
}

char * editor_resources_path(const char *subpath) {
    static char path[2048];

    snprintf(path, sizeof(path), "%s/editor_resources/%s", editor_base_path, subpath);

    return path;
}

bool ye_point_in_rect(int x, int y, SDL_Rect rect)
{ // TODO: MOVEME TO ENGINE
    if (x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h)
        return true;
    return false;
}

void editor_reload_settings(){
    if (SETTINGS)
        json_decref(SETTINGS);
    SETTINGS = ye_json_read(ye_path("settings.yoyo"));
}

void editor_load_scene(char * path){
    editor_deselect_all();
    ye_load_scene(path);
    editor_find_or_create_entities();
    editor_re_attach_ecs();
    YE_STATE.engine.target_camera = editor_camera;
}

void editor_re_attach_ecs(){
    entity_list_head = ye_get_entity_list_head();
    editor_find_or_create_entities();
    ye_logf(info, "Re-attatched ECS component pointers.\n");
}

void yoyo_loading_refresh(char * status)
{
    // update status
    snprintf(editor_loading_buffer, sizeof(editor_loading_buffer), "%s", status);

    // handle input
    ye_system_input();

    // clear the screen
    SDL_SetRenderDrawColor(YE_STATE.runtime.renderer, 0, 0, 0, 255);
    SDL_RenderClear(YE_STATE.runtime.renderer);

    SDL_SetRenderViewport(YE_STATE.runtime.renderer, NULL);
    SDL_SetRenderScale(YE_STATE.runtime.renderer, 1.0f, 1.0f);

    // paint just the loading
    editor_panel_loading(YE_STATE.engine.ctx);
    nk_sdl_render(YE_STATE.engine.ctx, NK_ANTI_ALIASING_ON);

    SDL_RenderPresent(YE_STATE.runtime.renderer);

    // SDL_UpdateWindowSurface(YE_STATE.runtime.window); BAD!
}

// pointers to destroy icon textures on shutdown
SDL_Texture * style_tex             = NULL;
SDL_Texture * gear_tex              = NULL;
SDL_Texture * folder_tex            = NULL;
SDL_Texture * build_tex             = NULL;
SDL_Texture * trick_tex             = NULL;
SDL_Texture * play_tex              = NULL;
SDL_Texture * buildrun_tex          = NULL;
SDL_Texture * pack_tex              = NULL;
SDL_Texture * game_tex              = NULL;
SDL_Texture * eye_tex               = NULL;
SDL_Texture * trash_tex             = NULL;
SDL_Texture * duplicate_tex         = NULL;
SDL_Texture * buildreconfigure_tex  = NULL;
SDL_Texture * refresh_tex           = NULL;

SDL_Texture * lightheader           = NULL;

void editor_pre_handle_input(SDL_Event event){
    if (event.type == SDL_EVENT_QUIT)
        quit = true;

    if(event.type == SDL_EVENT_WINDOW_RESIZED) {
        screenWidth = event.window.data1;
        screenHeight = event.window.data2;
    }
}

void editor_welcome_loop() {
    yoyo_loading_refresh("Loading welcome panel...");

    // create camera so engine doesn't freakout
    struct ye_entity * cam = ye_create_entity_named("project select cam");
    ye_add_transform_component(cam, 0, 0);
    ye_add_camera_component(cam, 999, (struct ye_rectf){0, 0, 2560, 1440});
    ye_set_camera(cam);

    ye_register_event_cb(YE_EVENT_HANDLE_INPUT, editor_pre_handle_input, YE_EVENT_FLAG_PERSISTENT);

    struct ScreenSize ss = ye_get_screen_size();
    screenWidth = ss.width; screenHeight = ss.height;
    
    editor_init_panel_welcome();

    ui_register_component("welcome", editor_panel_welcome);

    editor_update_window_title("Yoyo Engine Editor - Home");

    while(EDITOR_STATE.mode == ESTATE_WELCOME){
        if(quit)
            exit(0);
        
        ye_process_frame();
    }

    remove_ui_component("welcome");

    ye_destroy_entity(cam);
    editor_camera = NULL;
    YE_STATE.engine.target_camera = NULL;

    ye_unregister_event_cb(editor_pre_handle_input);
}

// TODO: dumb hack to supress error from this function. sue me.
struct ye_entity * get_ent_by_name_silent(const char *name) {
    struct ye_entity_node *current = entity_list_head;

    while(current != NULL){
        if(strcmp(current->entity->name, name) == 0){
            return current->entity;
        }
        current = current->next;
    }

    return NULL;
}

void editor_editing_loop() {
    // state init
    // update the games knowledge of where the resources path is, now for all the engine is concerned it is our target game
    if (EDITOR_STATE.opened_project_path != NULL)
        ye_update_base_path(EDITOR_STATE.opened_project_path); // GOD THIS IS SUCH A HEADACHE
    else
        ye_logf(error, "No project path provided. Please provide a path to the project folder as the first argument.");

    // let the engine know we also want to custom handle inputs
    ye_register_event_cb(YE_EVENT_HANDLE_INPUT, editor_handle_input, YE_EVENT_FLAG_PERSISTENT);

    editor_camera = get_ent_by_name_silent("editor_camera");
    if(!editor_camera){ // we hit this because purge ecs recreates editor ents for us
        // create our editor camera and register it with the engine
        editor_camera = ye_create_entity_named("editor_camera");
        ye_add_transform_component(editor_camera, 0, 0);
        ye_add_camera_component(editor_camera, 999, (struct ye_rectf){0, 0, 2560, 1440});
        ye_set_camera(editor_camera);
    }

    // register all editor ui components
    ui_register_component("heiarchy", ye_editor_paint_hiearchy);
    ui_register_component("entity", ye_editor_paint_inspector);
    ui_register_component("options", ye_editor_paint_options);
    ui_register_component("project", ye_editor_paint_project);
    ui_register_component("editor_menu_bar", ye_editor_paint_menu);

    origin = get_ent_by_name_silent("origin");
    if(!origin){
        origin = ye_create_entity_named("origin");
        ye_add_transform_component(origin, -50, -50);

        SDL_Texture *orgn_tex = SDL_CreateTextureFromSurface(YE_STATE.runtime.renderer, yep_engine_resource_image("originwhite.png"));
        ye_cache_texture_manual(orgn_tex, "originwhite.png");
        ye_add_image_renderer_component_preloaded(origin, 0, orgn_tex);
        origin->renderer->rect = (struct ye_rectf){0, 0, 100, 100};
    }

    yoyo_loading_refresh("Loading entry scene...");

    // load the scene out of the project settings::entry_scene
    SETTINGS = ye_json_read(ye_path("settings.yoyo"));
    // ye_json_log(SETTINGS);

    SDL_Color red = {255, 0, 0, 255};
    ye_cache_color("warning", red);

    // get the scene to load from "entry_scene"
    const char *entry_scene;
    if (!ye_json_string(SETTINGS, "entry_scene", &entry_scene))
    {
        ye_logf(error, "entry_scene not found in settings file. No scene has been loaded.");
        // TODO: future me create a text entity easily in the center of the scene alerting this fact
        struct ye_entity *text = ye_create_entity_named("warning text");
        ye_add_transform_component(text, 0, 0);
        ye_add_text_renderer_component(text, 900, "entry_scene not found in settings file. No scene has been loaded.", "default", 128, "warning",0);
        text->renderer->rect = (struct ye_rectf){0, 0, 1920, 500};
    }
    else
    {
        ye_load_scene(entry_scene);
    }

    // TODO: this project pref loading should become its own thing

    // get the p2d gravity and cell size
    float p2d_gravity_x = ye_config_float(SETTINGS, "p2d_gravity_x", 0.0f);
    float p2d_gravity_y = ye_config_float(SETTINGS, "p2d_gravity_y", 20.0f);
    int p2d_grid_size = ye_config_int(SETTINGS, "p2d_grid_size", 250);

    // set the gravity
    YE_STATE.engine.p2d_state->p2d_gravity.x = p2d_gravity_x;
    YE_STATE.engine.p2d_state->p2d_gravity.y = p2d_gravity_y;
    YE_STATE.engine.p2d_state->p2d_cell_size = p2d_grid_size;

    entity_list_head = ye_get_entity_list_head();

    // TODO: remove in future when we serialize editor prefs
    YE_STATE.editor.editor_display_viewport_lines = true;

    editor_find_or_create_entities();

    ye_logf(info, "Editor fully initialized.\n");
    ye_logf(info, "---------- BEGIN RUNTIME OUTPUT ----------\n");

    // core editing loop
    while(EDITOR_STATE.mode == ESTATE_EDITING && !quit) {

        // if we are building, check if the build thread has finished
        while(EDITOR_STATE.is_building) {
            // SDL3 threading: poll build status and message
            SDL_LockMutex(EDITOR_STATE.build_mutex);
            int build_status = EDITOR_STATE.build_status;
            char buf[256];
            strncpy(buf, EDITOR_STATE.build_status_msg, sizeof(buf));
            SDL_UnlockMutex(EDITOR_STATE.build_mutex);

            if (build_status != 0) { // 0 = running, 1 = done, 2 = error
                ye_logf(info, "Build thread status: %s\n", buf);
                EDITOR_STATE.is_building = false;
                SDL_WaitThread(EDITOR_STATE.building_thread, NULL);
                if (build_status == 2) {
                    ye_logf(error, "Build failed. Check the build log for more information.\n");
                }
            }
            yoyo_loading_refresh(buf);
        }

        if(editor_draw_drag_rect)
            ye_debug_render_rect(editor_selecting_rect.x, editor_selecting_rect.y, editor_selecting_rect.w, editor_selecting_rect.h, (SDL_Color){255, 0, 0, 255}, 10);
        if(editor_panning)
            ye_debug_render_line(pan_start.x, pan_start.y, pan_end.x, pan_end.y, (SDL_Color){255, 255, 255, 255}, 10);
        editor_render_selection_rects();
        ye_process_frame();
    }

    // if we have left that loop, cleanup the editor editing state
    editor_deselect_all();
    ye_purge_ecs();
    remove_ui_component("heiarchy");
    remove_ui_component("entity");
    remove_ui_component("options");
    remove_ui_component("project");
    remove_ui_component("editor_menu_bar");

    origin = NULL;
    editor_camera = NULL;
    YE_STATE.engine.target_camera = NULL;

    ye_unregister_event_cb(editor_handle_input);
}

/*
    main function
    accepts one string argument of the path to the project folder
*/
int main(int argc, char **argv) {
    // idk why i put this first but whatever
    editor_selecting_rect = (SDL_Rect){0, 0, 0, 0};

    // build up editor contexts
    editor_settings_ui_init();

    // init the engine. this starts the engine as thinking our editor directory is the game dir. this is ok beacuse we want to configure based off of the editor settings.json
    ye_init_engine();

    // get an initial screen size
    struct ScreenSize ss = ye_get_screen_size();
    screenWidth = ss.width; screenHeight = ss.height;

    // refresh the screen
    yoyo_loading_refresh("Initializing editor window...");

    /*
        Do some custom sdl setup for the editor specifically
    */
    // allow window resizing
    SDL_SetWindowResizable(YE_STATE.runtime.window, true); // maybe expose this in the json later on
    SDL_SetWindowMinimumSize(YE_STATE.runtime.window, 1280, 720); // also maybe expose this as an option.
    /*
        The thing about exposing these in json is that any competant dev (not that I am one) or anyone else (nobody will use this engine but me)
        could easily just add this one line of C code in their init function and get the same result.
    */

    /*
        Set the editor settings path
    */
    editor_base_path = strdup(SDL_GetBasePath());
    snprintf(editor_settings_path, sizeof(editor_settings_path), "%s./editor.yoyo", editor_base_path);

    yoyo_loading_refresh("Reading editor settings...");

    // load editor icons //

    #define INIT_EDITOR_TEXTURE(PATH, TEXTURE_VAR, ICON_FIELD) do {                     \
        SDL_Surface *tmp_sur = IMG_Load(editor_resources_path(PATH));                   \
        TEXTURE_VAR = SDL_CreateTextureFromSurface(YE_STATE.runtime.renderer, tmp_sur); \
        ICON_FIELD = nk_image_ptr(TEXTURE_VAR);                                         \
        SDL_DestroySurface(tmp_sur);                                                       \
    } while(0)

    INIT_EDITOR_TEXTURE("edicons/edicon_style.png", style_tex, editor_icons.style);
    INIT_EDITOR_TEXTURE("edicons/edicon_gear.png", gear_tex, editor_icons.gear);
    INIT_EDITOR_TEXTURE("edicons/edicon_folder.png", folder_tex, editor_icons.folder);
    INIT_EDITOR_TEXTURE("edicons/edicon_build.png", build_tex, editor_icons.build);
    INIT_EDITOR_TEXTURE("edicons/edicon_trick.png", trick_tex, editor_icons.trick);
    INIT_EDITOR_TEXTURE("edicons/edicon_play.png", play_tex, editor_icons.play);
    INIT_EDITOR_TEXTURE("edicons/edicon_buildrun.png", buildrun_tex, editor_icons.buildrun);
    INIT_EDITOR_TEXTURE("edicons/edicon_pack.png", pack_tex, editor_icons.pack);
    INIT_EDITOR_TEXTURE("edicons/edicon_game.png", game_tex, editor_icons.game);
    INIT_EDITOR_TEXTURE("edicons/edicon_eye.png", eye_tex, editor_icons.eye);
    INIT_EDITOR_TEXTURE("edicons/edicon_buildreconfigure.png", buildreconfigure_tex, editor_icons.buildreconfigure);
    INIT_EDITOR_TEXTURE("edicons/edicon_duplicate.png", duplicate_tex, editor_icons.duplicate);
    INIT_EDITOR_TEXTURE("edicons/edicon_trash.png", trash_tex, editor_icons.trash);
    INIT_EDITOR_TEXTURE("edicons/edicon_refresh.png", refresh_tex, editor_icons.refresh);

    INIT_EDITOR_TEXTURE("edicons/lightheader.png", lightheader, editor_icons.lightheader);

    ///////////////////////

    /*
        Open the editor settings config.

        TODO: maybe nest fields to make it more
        readible from text editor later on.
    */
    json_t *EDITOR_SETTINGS = ye_json_read(editor_settings_path);
    if(EDITOR_SETTINGS == NULL){
        ye_logf(warning, "editor config file not found. It will be created with defaults.\n");
        EDITOR_SETTINGS = json_object();
    }

    /*
        Read the editor config into the state struct,
        setting defaults as needed
    */
    PREFS.zoom_style = ye_config_int(EDITOR_SETTINGS, "zoom_style", ZOOM_MOUSE); // zoom to mouse by default
    PREFS.color_scheme_index = ye_config_int(EDITOR_SETTINGS, "color_scheme_index", 5); // amoled by default
    PREFS.min_select_px = ye_config_int(EDITOR_SETTINGS, "min_select_px", 10); // 10px by default

    // close the editor settings file
    json_decref(EDITOR_SETTINGS);
    
    /*
        Actually handle the picked pref initialization
    */
    switch(PREFS.color_scheme_index){
        case 0:
            set_style(YE_STATE.engine.ctx, THEME_BLACK);
            break;
        case 1:
            set_style(YE_STATE.engine.ctx, THEME_DARK);
            break;
        case 2:
            set_style(YE_STATE.engine.ctx, THEME_BLUE);
            break;
        case 3:
            set_style(YE_STATE.engine.ctx, THEME_RED);
            break;
        case 4:
            set_style(YE_STATE.engine.ctx, THEME_WHITE);
            break;
        case 5:
            set_style(YE_STATE.engine.ctx, THEME_AMOLED);
            break;
        case 6:
            set_style(YE_STATE.engine.ctx, THEME_DRACULA);
            break;
        case 7:
            set_style(YE_STATE.engine.ctx, THEME_CATPPUCCIN_LATTE);
            break;
        case 8:
            set_style(YE_STATE.engine.ctx, THEME_CATPPUCCIN_FRAPPE);
            break;
        case 9:
            set_style(YE_STATE.engine.ctx, THEME_CATPPUCCIN_MACCHIATO);
            break;
        case 10:
            set_style(YE_STATE.engine.ctx, THEME_CATPPUCCIN_MOCHA);
            break;
        default:
            set_style(YE_STATE.engine.ctx, THEME_AMOLED);
            break;
    }

    yoyo_loading_refresh("Constructing editor...");

    // update screenWidth and screenHeight
    struct ScreenSize screenSize = ye_get_screen_size();
    screenWidth = screenSize.width;
    screenHeight = screenSize.height;

    /*
        if we have invoked the editor without a path to
        a project, we should enter into a state that allows
        the user to create and select projects at will
    */
    if(argc <= 1){
        EDITOR_STATE.mode = ESTATE_WELCOME;
    }
    else {
        char *path = argv[1];
        ye_logf(info, "Editor recieved path: %s\n",path);

        // remote trailing slash (if it exists)
        if(path[strlen(path) - 1] == '/')
            path[strlen(path) - 1] = '\0';

        EDITOR_STATE.opened_project_path = strdup(path); // TODO: free me :-0
        EDITOR_STATE.opened_project_resources_path = malloc(strlen(path) + strlen("resources/") + 1);
        snprintf(EDITOR_STATE.opened_project_resources_path, strlen(path) + strlen("resources/") + 1, "%s/resources/", path);
        EDITOR_STATE.mode = ESTATE_EDITING;
    }

    // initialize SDL build mutex for cross-platform build thread sync
    EDITOR_STATE.build_mutex = SDL_CreateMutex();

    // core editor loop, depending on state
    while(!quit) {
        if(EDITOR_STATE.mode == ESTATE_WELCOME)
            editor_welcome_loop();
        
        editor_editing_loop();
    }

    /*
        Before we shutdown the editor, lets re-serialize
        the preferences we initially loaded
    */
    EDITOR_SETTINGS = ye_json_read(editor_settings_path);
    
    json_object_set_new(EDITOR_SETTINGS, "color_scheme_index", json_integer(PREFS.color_scheme_index));
    json_object_set_new(EDITOR_SETTINGS, "min_select_px", json_integer(PREFS.min_select_px));    

    ye_json_write(editor_settings_path, EDITOR_SETTINGS);
    json_decref(EDITOR_SETTINGS);

    // free editor icons
    SDL_DestroyTexture(style_tex);
    SDL_DestroyTexture(gear_tex);
    SDL_DestroyTexture(folder_tex);
    SDL_DestroyTexture(build_tex);
    SDL_DestroyTexture(trick_tex);
    SDL_DestroyTexture(play_tex);
    SDL_DestroyTexture(buildrun_tex);
    SDL_DestroyTexture(pack_tex);
    SDL_DestroyTexture(game_tex);
    SDL_DestroyTexture(eye_tex);
    SDL_DestroyTexture(trash_tex);
    SDL_DestroyTexture(duplicate_tex);
    SDL_DestroyTexture(buildreconfigure_tex);
    SDL_DestroyTexture(refresh_tex);

    SDL_DestroyTexture(lightheader);

    ye_shutdown_engine();
    json_decref(SETTINGS);

    free(editor_base_path);

    // shutdown editor and teardown contextx
    editor_settings_ui_shutdown();

    // destroy SDL build mutex
    SDL_DestroyMutex(EDITOR_STATE.build_mutex);

    // exit
    return 0;
}

/*
    Locks the editor viewport from interaction
*/
void lock_viewport(){
    lock_viewport_interaction = true;
}

/*
    Unlocks the editor viewport for interaction
*/
void unlock_viewport(){
    lock_viewport_interaction = false;
}