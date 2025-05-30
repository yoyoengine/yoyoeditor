/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include <math.h>

#include <yoyoengine/yoyoengine.h>

#include "editor.h"
#include "editor_serialize.h"
#include "editor_build.h"
#include "editor_selection.h"

/*
    Getting this equation right was exponentially more difficult than you could possibly imagine
*/
void editor_update_mouse_world_pos(int x, int y){
    // y = y - 35; // account for the menu bar

    float scaleX = (float)YE_STATE.engine.screen_width / YE_STATE.engine.target_camera->camera->view_field.w;
    float scaleY = (float)YE_STATE.engine.screen_height / YE_STATE.engine.target_camera->camera->view_field.h;
    struct ye_rectf campos = ye_get_position(YE_STATE.engine.target_camera, YE_COMPONENT_CAMERA);
    mouse_world_x = ((x / scaleX) + campos.x);
    mouse_world_y = ((y / scaleY) + campos.y);
}

/*
    Checks if the mouse is within the editor viewport
*/
bool is_hovering_editor(int x, int y){
    return (x > 0 && x < screenWidth / 1.5 &&
            y > 35 && y < 35 + screenHeight / 1.5);
}

float last_pan_x, last_pan_y;
float camera_zoom = 1.0;
float camera_zoom_sens = 1.0;
float mx, my;

void update_mx_my(){
    // get the current mouse position in window
    float _mx, _my; SDL_GetMouseState(&_mx, &_my);
    mx = _mx;
    my = _my;

    // use the camera size to calculate the world coordinates of the mouse position
    float scaleX = (float)YE_STATE.engine.screen_width / YE_STATE.engine.target_camera->camera->view_field.w;
    float scaleY = (float)YE_STATE.engine.screen_height / YE_STATE.engine.target_camera->camera->view_field.h;
    struct ye_rectf campos = ye_get_position(YE_STATE.engine.target_camera, YE_COMPONENT_CAMERA);
    mx = ((mx / scaleX) + campos.x);
    my = ((my / scaleY) + campos.y);
}

void editor_input_panning(SDL_Event event){

    // get mouse world pos
    update_mx_my();

    float win_mx, win_my; SDL_GetMouseState(&win_mx, &win_my);

    // if middle mouse clicked down, initialize panning
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_MIDDLE) {
            if (!lock_viewport_interaction) {
                editor_panning = true;
                
                SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE));

                // set the starting point for our pan on first frame
                last_pan_x = mx;
                last_pan_y = my;
            }
        }
    }
    // if middle mouse released, stop panning
    else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event.button.button == SDL_BUTTON_MIDDLE) {
            editor_panning = false;
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT));
        }
    }
    // mouse movement, if we are panning move the camera
    else if (event.type == SDL_EVENT_MOUSE_MOTION) {
        if (editor_panning) {
            // get the diff we need to pan by
            float dx = mx - last_pan_x;
            float dy = my - last_pan_y;

            // pan the camera
            editor_camera->transform->x -= dx;
            editor_camera->transform->y -= dy;

            // get the new world location as our next starting point
            update_mx_my();

            last_pan_x = mx;
            last_pan_y = my;
        }
    }
    else if (event.type == SDL_EVENT_MOUSE_WHEEL && is_hovering_editor(win_mx, win_my) && !lock_viewport_interaction)
    {
        float dt = ye_delta_time();
        float zoom_factor = 0.1f; // Adjust this value to control the zoom speed

        if (event.wheel.y > 0)
        {
            camera_zoom *= pow(200.0f, zoom_factor * dt);
            // Ensure camera_zoom doesn't go above 10x
            camera_zoom = fmin(camera_zoom, 10.0f);
        }
        else if (event.wheel.y < 0)
        {
            camera_zoom /= pow(200.0f, zoom_factor * dt);
            // Ensure camera_zoom doesn't go below 0.1x
            camera_zoom = fmax(camera_zoom, 0.1f);
        }

        // save the size before
        float old_w = editor_camera->camera->view_field.w;
        float old_h = editor_camera->camera->view_field.h;

        float old_mx = mx;
        float old_my = my;

        // update the camera zoom
        editor_camera->camera->view_field.w = screenWidth / camera_zoom;
        editor_camera->camera->view_field.h = screenHeight / camera_zoom;

        update_mx_my();

        switch(PREFS.zoom_style){
            case ZOOM_TOP_LEFT:
                // no behavior, zooms top left of camera by default
                break;
            case ZOOM_CENTER:
                // offset position to keep center of the screen in the same place TODO: scale this with viewport size
                editor_camera->transform->x -= (editor_camera->camera->view_field.w - old_w) / 2;
                editor_camera->transform->y -= (editor_camera->camera->view_field.h - old_h) / 2;
                break;
            case ZOOM_MOUSE:
                // move the camera, so that we have zoomed centered on the mouse
                editor_camera->transform->x -= (mx - old_mx);
                editor_camera->transform->y -= (my - old_my);
                break;
        }

        editor_update_mouse_world_pos(mx, my);
    }
}

/*
    Any Misc items that need to happen on editor input
*/
void editor_input_misc(SDL_Event event){
    // update the mouse world position
    if(event.type == SDL_EVENT_MOUSE_MOTION){
        editor_update_mouse_world_pos(event.motion.x, event.motion.y);
    }

    // window events //
    if(event.type == SDL_EVENT_WINDOW_RESIZED) {
        screenWidth = event.window.data1;
        screenHeight = event.window.data2;

        // if we have resized and we are in the editor, we need to update the editor camera
        editor_camera->camera->view_field.w = screenWidth / camera_zoom;
        editor_camera->camera->view_field.h = screenHeight / camera_zoom;
    }
}

/*
    Any keyboard shortcuts for the editor
*/
void editor_input_shortcuts(SDL_Event event){
    if (event.type == SDL_EVENT_KEY_DOWN)
    {
        switch (event.key.key)
        {
        case SDLK_S :
            // CTRL + S ->= save the scene
            if (event.key.mod & SDL_KMOD_CTRL)
            {
                editor_write_scene_to_disk(ye_path_resources(YE_STATE.runtime.scene_file_path));
                editor_saved();
            }
            break;
        case SDLK_R :
            // CTRL + SHIFT + R ->= reload the current scene
            if (event.key.mod & SDL_KMOD_CTRL && event.key.mod & SDL_KMOD_SHIFT)
            {
                ye_logf(debug,"Editor Reloading Scene.\n");
                editor_deselect_all();
                ye_reload_scene();
                editor_re_attach_ecs();
                editor_saved();
            }
            // CTRL + R ->= build and run the project
            else if (event.key.mod & SDL_KMOD_CTRL)
            {
                editor_build_and_run();
            }
            break;
        case SDLK_GRAVE :
            // if we opened or closed the console, lock accordingly
            lock_viewport_interaction = ui_component_exists("console");
            break;
        default:
            break;
        }
    }
}

/*
    Registered input handler

    TODO:
    - zoom in should center on the mouse or the center of the screen
*/
void editor_handle_input(SDL_Event event) {
    if (event.type == SDL_EVENT_QUIT)
        quit = true;

    // misc input handling
    editor_input_panning(event);
    editor_selection_handler(event); // editor_selection.c
    editor_input_misc(event);
    editor_input_shortcuts(event);
}