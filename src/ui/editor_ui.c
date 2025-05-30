/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include <stdio.h>
#ifdef __linux__
    #include <unistd.h>
#else
    #include <platform/windows/unistd.h>
#endif
#include <yoyoengine/yoyoengine.h>
#include "editor.h"
#include "editor_ui.h"
#include "editor_serialize.h"
#include "editor_panels.h"
#include "editor_selection.h"
#include "editor_utils.h"

#include <yoyoengine/ye_nk.h>

char search_text[256] = {""};
int matching_results = 0;

const float ratio[] = {0.03f, 0.85f, /* up and down arrows: 0.05, 0.05, */ 0.06, 0.06};
void ye_editor_paint_hiearchy(struct nk_context *ctx){
    // if no selected entity its height will be full height, else its half
    int height = num_editor_selections == 0 ? screenHeight : screenHeight / 2.5;
    if (nk_begin(ctx, "Heiarchy", nk_rect(screenWidth/1.5, 0, screenWidth - screenWidth/1.5, height),
        NK_WINDOW_TITLE | NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Controls:", NK_TEXT_LEFT);
            if(nk_button_label(ctx, "New Entity")){
                ye_create_entity();
                entity_list_head = ye_get_entity_list_head();
                editor_unsaved();
            }

            /*
                Display search bar, with additional text for matching results
            */
            if(strcmp(search_text,"") != 0){
                nk_layout_row_dynamic(ctx, 30, 2);
                nk_label(ctx, "Search:", NK_TEXT_LEFT);
                char txt[64];
                sprintf(txt,"%d matching results",matching_results);
                
                if(matching_results > 0){
                    nk_label_colored(ctx, txt, NK_TEXT_LEFT,nk_rgb(0,255,0));
                }
                else{
                    nk_label_colored(ctx, txt, NK_TEXT_LEFT,nk_rgb(255,0,0));
                }
            }
            else{
                nk_layout_row_dynamic(ctx, 30, 1);
                nk_label(ctx, "Search:", NK_TEXT_LEFT);
            }
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, search_text, 256, nk_filter_default);

            nk_layout_row_dynamic(ctx, 30, 1);
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Entities:", NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 25, 3);
            nk_label(ctx, "Active", NK_TEXT_LEFT);
            nk_label(ctx, "Name", NK_TEXT_CENTERED);
            nk_label(ctx, "Options", NK_TEXT_RIGHT);

            // iterate through entity_list_head and display the names of each entity as a button
            struct ye_entity_node *current = entity_list_head;
            matching_results = 0;
            while(current != NULL){
                /*
                    Honestly, should just leave editor camera in the heiarchy for fun lol
                    Kinda funny that you could just nuke it if you wanted
                */
                if(current->entity == editor_camera || current->entity == origin){
                    current = current->next;
                    continue;
                }

                /*
                    If the search bar has text in it:
                    - check for matches within entity names
                    - then fall back on any tags matching the search text
                */
                if(strcmp(search_text,"") != 0){
                    // check if name has search term
                    char * ret = strstr(current->entity->name,search_text);
                    if(ret == NULL){
                        // if name doesnt, check if we potentially have a tag that matches search term
                        if(current->entity->tag != NULL){
                            // loop over all tags and check for matches
                            bool tag_matches = false;
                            for(int i = 0; i < YE_TAG_MAX_NUMBER; i++){
                                ret = strstr(current->entity->tag->tags[i], search_text);
                                if(ret != NULL){
                                    // printf("matched tag: %s\n",current->entity->tag->tags[i]);
                                    tag_matches = true;
                                }
                            }

                            // if we didnt find a tag match, skip entity
                            if(!tag_matches){
                                current = current->next;
                                continue;
                            }
                        }
                        else{
                            // no name or tag match found, skip rendering this in hiearchy
                            current = current->next;
                            continue;
                        }
                    }
                    // printf("matched %s, %s\n",search_text,ret);
                }
                matching_results++;

                nk_layout_row(ctx, NK_DYNAMIC, 30, /*up and down arrows: 6 */ 4, ratio);

                bool cached_active = current->entity->active;

                nk_checkbox_label(ctx, "", (nk_bool*)&current->entity->active);

                if(current->entity->active != cached_active){
                    editor_unsaved();
                }

                // if the entity is selected, display it as a different color
                bool flag = false; // messy way to do this, but it works
                if(editor_is_selected(current->entity)){
                    nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(nk_rgb(100,100,100))); nk_style_push_style_item(ctx, &ctx->style.button.hover, nk_style_item_color(nk_rgb(75,75,75))); nk_style_push_style_item(ctx, &ctx->style.button.active, nk_style_item_color(nk_rgb(50,50,50))); nk_style_push_vec2(ctx, &ctx->style.button.padding, nk_vec2(2,2));
                    flag = true;
                }
                else if(!current->entity->active){
                    nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(nk_rgb(0,0,0))); nk_style_push_style_item(ctx, &ctx->style.button.hover, nk_style_item_color(nk_rgb(20,20,20))); nk_style_push_style_item(ctx, &ctx->style.button.active, nk_style_item_color(nk_rgb(0,0,0))); nk_style_push_vec2(ctx, &ctx->style.button.padding, nk_vec2(2,2));
                    flag = true;
                }

                if(nk_button_label(ctx, current->entity->name)){
                    if(editor_is_selected(current->entity)){
                        editor_deselect(current->entity);
                        // pop our style items if we pushed them
                        if(flag){ // if we are selected, pop our style items
                            nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_vec2(ctx);
                        }
                        break;
                    }
                    editor_select(current->entity);
                    entity_list_head = ye_get_entity_list_head();
                    // set all our current entity staging fields
                    staged_entity = *current->entity; // TODO this is hard because we have to copy all the components too... maybe we just need to let modification of fields directly and skip them if they are invalid
                    // pop our style items if we pushed them
                    if(flag){ // if we are selected, pop our style items
                        nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_vec2(ctx);
                    }
                    break;
                }

                // pop our style items if we pushed them
                if(flag){ // if we are selected, pop our style items
                    nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_vec2(ctx);
                }

                /*
                    // move up button
                    if(nk_button_symbol(ctx, NK_SYMBOL_TRIANGLE_UP)){
                    }

                    // move down button
                    if(nk_button_symbol(ctx, NK_SYMBOL_TRIANGLE_DOWN)){
                    }
                */

                // push some pretty styles for green button!! (thank you nuklear forum!) :D
                nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(nk_rgb(35,35,35))); nk_style_push_style_item(ctx, &ctx->style.button.hover, nk_style_item_color(nk_rgb(0,255,0))); nk_style_push_style_item(ctx, &ctx->style.button.active, nk_style_item_color(nk_rgb(0,255,0))); nk_style_push_vec2(ctx, &ctx->style.button.padding, nk_vec2(2,2));

                // duplicate button
                if(nk_button_image(ctx, editor_icons.duplicate)){
                    struct ye_entity * new = ye_duplicate_entity(current->entity);
                    entity_list_head = ye_get_entity_list_head();
                    editor_unsaved();

                    // set the active entity as the newly duplicated one
                    editor_deselect(current->entity);
                    editor_select(new);

                    // nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_vec2(ctx);
                }

                // pop green
                nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_vec2(ctx);

                // push some pretty styles for red button!! (thank you nuklear forum!) :D
                nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(nk_rgb(35,35,35))); nk_style_push_style_item(ctx, &ctx->style.button.hover, nk_style_item_color(nk_rgb(255,0,0))); nk_style_push_style_item(ctx, &ctx->style.button.active, nk_style_item_color(nk_rgb(255,0,0))); nk_style_push_vec2(ctx, &ctx->style.button.padding, nk_vec2(2,2));
                
                if(nk_button_image(ctx, editor_icons.trash)){
                    // if our selected entity is the current entity, close the hiearchy
                    if(editor_is_selected(current->entity)){
                        editor_deselect(current->entity);
                    }

                    ye_destroy_entity(current->entity);
                    editor_unsaved();
                    entity_list_head = ye_get_entity_list_head();
                    nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_vec2(ctx);
                    break;
                }

                // pop off our cool red button colors
                nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_style_item(ctx); nk_style_pop_vec2(ctx);

                current = current->next;
            }
            if(matching_results <= 0){
                nk_layout_row_dynamic(ctx, 60, 1);
                nk_label_colored(ctx,"no results!",NK_TEXT_CENTERED,nk_rgb(255, 255, 0));
            }
        nk_end(ctx);
    }
}

/*
    Paint some info on the current scene
*/
void ye_editor_paint_info_overlay(struct nk_context *ctx){
    if (nk_begin(ctx, "Info", nk_rect(0, 40, 200, 200),
        NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE)) {
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Mouse World Pos:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            char buf[32];
            sprintf(buf, "%d,%d", mouse_world_x, mouse_world_y);
            nk_label(ctx, buf, NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Scene Name:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, YE_STATE.runtime.scene_name, NK_TEXT_LEFT);
        nk_end(ctx);
    }
}

/*
    TODO:
    - paintbounds somehow starts out checked even though its not, so have to double click to turn on
    - if console is opened when we tick one of these, closing the console crashes
*/
bool show_info_overlay = false;
bool show_camera_overlay = false;
bool show_debug_overlay = false;

bool dummy_show_physics_overlay = false;

void ye_editor_paint_options(struct nk_context *ctx){
    if (nk_begin(ctx, "Options", nk_rect(screenWidth/1.5 / 2, 40 + screenHeight/1.5, screenWidth - screenWidth/1.5, (screenHeight - screenHeight/1.5) - 40),
        NK_WINDOW_TITLE | NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(ctx, 25, 1);
            
            nk_label(ctx, "Overlays:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 4);
            if(nk_checkbox_label(ctx, "Debug", (nk_bool*)&show_debug_overlay)){
                if(show_debug_overlay){
                    ui_register_component("debug",ui_paint_debug_overlay);
                }
                else{
                    remove_ui_component("debug");
                }
            }
            if(nk_checkbox_label(ctx, "Info", (nk_bool*)&show_info_overlay)){
                if(show_info_overlay){
                    ui_register_component("info_overlay",ye_editor_paint_info_overlay);
                }
                else{
                    remove_ui_component("info_overlay");
                }
            }
            if(nk_checkbox_label(ctx, "Camera", (nk_bool*)&show_camera_overlay)){
                if(show_camera_overlay){
                    ui_register_component("cam_info",ui_paint_cam_info);
                }
                else{
                    remove_ui_component("cam_info");
                }
            }

            if(nk_checkbox_label(ctx, "Physics", (nk_bool*)&dummy_show_physics_overlay)){
                ye_set_overlay_state("ye_overlay_physics",dummy_show_physics_overlay);
            }
            // dumb hack: manual sync it in case scene reload messed with it
            dummy_show_physics_overlay = ye_get_overlay_state("ye_overlay_physics");

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Visual Debugging:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_checkbox_label(ctx, "Display Names", (nk_bool*)&YE_STATE.editor.display_names);
            nk_checkbox_label(ctx, "Paintbounds", (nk_bool*)&YE_STATE.editor.paintbounds_visible);
            nk_checkbox_label(ctx, "Wireframe", (nk_bool*)&YE_STATE.editor.wireframe_visible);
            nk_checkbox_label(ctx, "Colliders", (nk_bool*)&YE_STATE.editor.colliders_visible);
            nk_checkbox_label(ctx, "Scene Camera Viewport", (nk_bool*)&YE_STATE.editor.scene_camera_bounds_visible);
            nk_checkbox_label(ctx, "Audio Range", (nk_bool*)&YE_STATE.editor.audiorange_visible);
            nk_checkbox_label(ctx, "Button Bounds", (nk_bool*)&YE_STATE.editor.button_bounds_visible);

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Preferences:", NK_TEXT_LEFT);
            nk_checkbox_label(ctx, "Draw Lines", (nk_bool*)&YE_STATE.editor.editor_display_viewport_lines);

            nk_label(ctx, "Extra:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_checkbox_label(ctx, "Stretch Viewport", (nk_bool*)&YE_STATE.engine.stretch_viewport);
            nk_checkbox_label(ctx, "Lock Viewport", (nk_bool*)&lock_viewport_interaction);
        nk_end(ctx);
    }
}

/*
    Editor settings window
*/
void ye_editor_paint_editor_settings(struct nk_context *ctx){
    if (nk_begin(ctx, "Editor Settings", nk_rect(screenWidth/2 - 250, screenHeight/2 - 100, 500,300),
        NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label(ctx, "Yoyo Editor Settings", NK_TEXT_CENTERED);
        nk_label(ctx, "", NK_TEXT_CENTERED);
        
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "UI Color Theme:", NK_TEXT_CENTERED);
        static const char *color_schemes[] = {"black", "dark", "blue", "red", "white", "amoled", "dracula", "catppuccin latte", "catppuccin frappe", "catppuccin macchiato", "catppuccin mocha"};
        nk_combobox(ctx, color_schemes, NK_LEN(color_schemes), &PREFS.color_scheme_index, 25, nk_vec2(200,200));

        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "Zoom style:", NK_TEXT_CENTERED);
        static const char *zoom_styles[] = {"top left", "center", "mouse"};
        nk_combobox(ctx, zoom_styles, NK_LEN(zoom_styles), (int *)(&PREFS.zoom_style), 25, nk_vec2(200,200));
        nk_label(ctx, "", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "Min select threshold (px):", NK_TEXT_CENTERED);
        nk_property_int(ctx, "px", 0, &PREFS.min_select_px, 10000, 1, 5);
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label(ctx, "", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(ctx, 25, 2);
        if(nk_button_label(ctx, "Cancel")){
            remove_ui_component("editor_settings");
            lock_viewport_interaction = !lock_viewport_interaction;
        }
        if(nk_button_label(ctx, "Apply")){

            /*
                Load the editor settings file, and change "preferences":"theme" to the name of the scheme
            */
            json_t *settings = ye_json_read(editor_settings_path);
            json_object_set_new(json_object_get(settings, "preferences"), "theme", json_string(color_schemes[PREFS.color_scheme_index]));
            ye_json_write(editor_settings_path, settings);
            json_decref(settings);

            if(strcmp(color_schemes[PREFS.color_scheme_index], "dark") == 0)
                set_style(YE_STATE.engine.ctx, THEME_DARK);
            else if(strcmp(color_schemes[PREFS.color_scheme_index], "red") == 0)
                set_style(YE_STATE.engine.ctx, THEME_RED);
            else if(strcmp(color_schemes[PREFS.color_scheme_index], "black") == 0)
                set_style(YE_STATE.engine.ctx, THEME_BLACK);
            else if(strcmp(color_schemes[PREFS.color_scheme_index], "white") == 0)
                set_style(YE_STATE.engine.ctx, THEME_WHITE);
            else if(strcmp(color_schemes[PREFS.color_scheme_index], "blue") == 0)
                set_style(YE_STATE.engine.ctx, THEME_BLUE);
            else if(strcmp(color_schemes[PREFS.color_scheme_index], "amoled") == 0)
                set_style(YE_STATE.engine.ctx, THEME_AMOLED);
            else if(strcmp(color_schemes[PREFS.color_scheme_index], "dracula") == 0)
                set_style(YE_STATE.engine.ctx, THEME_DRACULA);
            else if(strcmp(color_schemes[PREFS.color_scheme_index], "catppuccin latte") == 0)
                set_style(YE_STATE.engine.ctx, THEME_CATPPUCCIN_LATTE);
            else if(strcmp(color_schemes[PREFS.color_scheme_index], "catppuccin frappe") == 0)
                set_style(YE_STATE.engine.ctx, THEME_CATPPUCCIN_FRAPPE);
            else if(strcmp(color_schemes[PREFS.color_scheme_index], "catppuccin macchiato") == 0)
                set_style(YE_STATE.engine.ctx, THEME_CATPPUCCIN_MACCHIATO);
            else if(strcmp(color_schemes[PREFS.color_scheme_index], "catppuccin mocha") == 0)
                set_style(YE_STATE.engine.ctx, THEME_CATPPUCCIN_MOCHA);
            remove_ui_component("editor_settings");
            lock_viewport_interaction = !lock_viewport_interaction;
        }
        nk_end(ctx);
    }
}

/*
    Editor top menu bar

    This function is actually so yucky and should be greatly refactored but tbh this editor code
    should never be seen by another living human being so im ok with living with it.

    TODO: locking viewport should be handled better. It would be nice if when any submenu item is 
    dropped down clicking on it does not select an entity (since its over the viewport)
*/
bool scene_deletion_popup_open = false;

bool new_scene_popup_open = false;
char new_scene_name[256];

bool open_scene_popup_open = false;
char open_scene_name[256];

void ye_editor_paint_menu(struct nk_context *ctx){
    if (nk_begin(ctx, "Menu", nk_rect(0, 0, screenWidth / 1.5, 35), 0)) {
        
        /*
            Popup for getting information to create new scenes
        */
        if(new_scene_popup_open){
            struct nk_rect s = { 0, 0, 400, 175 };
            if (nk_popup_begin(ctx, NK_POPUP_STATIC, "About", NK_WINDOW_MOVABLE, s)) {
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "Create a new scene", NK_TEXT_CENTERED);
                nk_label(ctx, "(do not include path or extension)", NK_TEXT_CENTERED);
                nk_layout_row_dynamic(ctx, 25, 2) ;
                nk_label(ctx, "Scene Name", NK_TEXT_CENTERED);
                nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, new_scene_name, 256, nk_filter_default);
                nk_layout_row_dynamic(ctx, 25, 1); // spacer
                nk_layout_row_dynamic(ctx, 25, 2);
                if(nk_button_label(ctx, "Abort")){
                    new_scene_popup_open = false;
                    lock_viewport_interaction = !lock_viewport_interaction;
                }
                if(nk_button_label(ctx, "Create")){
                    
                    /*
                        Build the new scene file and write it to disk
                    */

                    json_t *new_scene = json_object();
                    json_object_set_new(new_scene, "name", json_string(new_scene_name));
                    json_object_set_new(new_scene, "version", json_integer(YOYO_ENGINE_SCENE_VERSION));
                    json_object_set_new(new_scene, "styles", json_array());
                    json_object_set_new(new_scene, "prefabs", json_array());

                    json_t *scene = json_object();
                    json_object_set_new(scene, "default camera", json_string("camera"));
                    json_object_set_new(scene, "entities", json_array());
                    json_object_set_new(new_scene, "scene", scene);

                    // In entities, let's make the "camera" default camera
                    json_t *camera = json_object();
                    json_object_set_new(camera, "name", json_string("camera"));
                    json_object_set_new(camera, "active", json_true());

                    json_t *components = json_object();
                    json_object_set_new(components, "transform", json_object());
                    json_object_set_new(components, "camera", json_object());

                    json_object_set_new(json_object_get(components, "transform"), "x", json_real(0));
                    json_object_set_new(json_object_get(components, "transform"), "y", json_real(0));

                    json_object_set_new(json_object_get(components, "camera"), "active", json_true());
                    json_object_set_new(json_object_get(components, "camera"), "z", json_integer(999));

                    json_t *view_field = json_object();
                    json_object_set_new(view_field, "w", json_real(1920));
                    json_object_set_new(view_field, "h", json_real(1080));
                    json_object_set_new(json_object_get(components, "camera"), "view field", view_field);

                    json_object_set_new(camera, "components", components);
                    json_array_append_new(json_object_get(scene, "entities"), camera);

                    // get the path of our new scene, it will be scenes/NAME.yoyo
                    char new_scene_path[256 + 12];
                    snprintf(new_scene_path, sizeof(new_scene_path), "scenes/%s.yoyo", new_scene_name);

                    // check if this scene already exists
                    if(access(ye_path_resources(new_scene_path), F_OK) != -1){
                        // file exists
                        ye_logf(error, "Scene already exists.\n");
                    }
                    else{
                        ye_json_write(ye_path_resources(new_scene_path), new_scene);
                        new_scene_popup_open = false;
                        editor_load_scene(new_scene_path);
                        editor_saved();
                    }
                    
                    // cleanup
                    json_decref(new_scene); // TODO: check for memroy leak, need to free camera?
                    lock_viewport_interaction = !lock_viewport_interaction;
                }
                nk_popup_end(ctx);
            }
        }
        
        /*
            Popup for confirming scene deletion
        */
        if(scene_deletion_popup_open){
            struct nk_rect s = { 0, 0, 450, 150 };
            if (nk_popup_begin(ctx, NK_POPUP_STATIC, "About", NK_WINDOW_MOVABLE, s)) {
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "Are you sure you want to delete this scene?", NK_TEXT_CENTERED);
                nk_label(ctx, "This action is irreversible.", NK_TEXT_CENTERED);
                nk_label(ctx, "", NK_TEXT_CENTERED);
                nk_layout_row_dynamic(ctx, 25, 2);
                if(nk_button_label(ctx, "No")){
                    scene_deletion_popup_open = false;
                    lock_viewport_interaction = !lock_viewport_interaction;
                }
                if(nk_button_label(ctx, "Yes")){
                    if(ye_delete_file(YE_STATE.runtime.scene_file_path)){
                        ye_logf(info, "Deleted scene %s\n", YE_STATE.runtime.scene_file_path);
                    }
                    else{
                        ye_logf(error, "Failed to delete scene %s\n", YE_STATE.runtime.scene_file_path);
                    }

                    scene_deletion_popup_open = false;
                    lock_viewport_interaction = !lock_viewport_interaction;

                    // this should get the point across
                    struct ye_entity * warning = ye_create_entity_named("warning");
                    ye_add_text_renderer_component(warning, 0, "Destroyed Scene. Please create or open a different one.", "default", 128, "red",0);
                }
                nk_popup_end(ctx);
            }
        }

        /*
            Popup for opening scene
        */
        if(open_scene_popup_open){
            struct nk_rect s = { 0, 0, 450, 200 };
            if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Open Scene", 0, s)) {
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "What is the path (relative to resources/)?", NK_TEXT_CENTERED);
                nk_label(ctx, "Ex: \"entry.yoyo\"", NK_TEXT_CENTERED);
                nk_label(ctx, "", NK_TEXT_CENTERED);
                nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 2);
                nk_layout_row_push(ctx, 0.65f);
                nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, open_scene_name, 256, nk_filter_default);
                nk_layout_row_push(ctx, 0.35f);
                
                if(nk_button_image_label(ctx, editor_icons.folder, "Browse", NK_TEXT_LEFT)) {
                    ye_pick_resource_file(
                        (struct ye_picker_data){
                            .filter = ye_picker_yoyo_filters,
                            .num_filters = &ye_picker_num_yoyo_filters,

                            .response_mode = YE_PICKER_WRITE_CHAR_BUF,
                            .dest.output_buf = {
                                .buffer = open_scene_name,
                                .size = sizeof(open_scene_name) - 1,
                            },
                        }
                    );
                }
                
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "", NK_TEXT_CENTERED);
                nk_layout_row_dynamic(ctx, 25, 2);
                if(nk_button_label(ctx, "Abort")){
                    open_scene_popup_open = false;
                    unlock_viewport();
                }
                if(nk_button_label(ctx, "Open")){
                    open_scene_popup_open = false;
                    unlock_viewport();

                    editor_load_scene(open_scene_name);
                    editor_saved();
                }
                nk_popup_end(ctx);
            }
        }

        nk_menubar_begin(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 25, 9); // TO ADD NEW ITEMS: CHANGE THIS VALUE
        /*
            File
            Scene
            Settings
            Help
            Quit
            ---SPACER---
            error count
            warning count
            saved status
        */
        
        nk_layout_row_push(ctx, 45);
        if (nk_menu_begin_label(ctx, "File", NK_TEXT_LEFT, nk_vec2(120, 200))) {
            nk_layout_row_dynamic(ctx, 25, 1);
            if (nk_menu_item_label(ctx, "Save", NK_TEXT_LEFT)) {    
                editor_write_scene_to_disk(ye_path_resources(YE_STATE.runtime.scene_file_path));
            }
            if (nk_menu_item_label(ctx, "Exit", NK_TEXT_LEFT)) {
                // OBLITERATE any overlays
                ye_set_all_overlays(false);

                EDITOR_STATE.mode = ESTATE_WELCOME;
                editor_update_window_title("Yoyo Engine Editor - Home");
                
                free(EDITOR_STATE.opened_project_path);
                EDITOR_STATE.opened_project_path = NULL;
                free(EDITOR_STATE.opened_project_resources_path);
                EDITOR_STATE.opened_project_resources_path = NULL;
            }
            nk_menu_end(ctx);

            /*
                TODO:
                build, build and run, build and debug, build and run and debug
            */
        }
        nk_layout_row_push(ctx, 55);
        if (nk_menu_begin_label(ctx, "Scene", NK_TEXT_LEFT, nk_vec2(200, 200))) {
            nk_layout_row_dynamic(ctx, 25, 1);
            
            if (nk_menu_item_label(ctx, "Open Scene", NK_TEXT_LEFT)) { // TODO: save prompt if unsaved
                if(!open_scene_popup_open){
                    open_scene_popup_open = !open_scene_popup_open;
                    lock_viewport();
                    // reset fields
                    strcpy(open_scene_name, "scenes/");
                }
                else{
                    open_scene_popup_open = !open_scene_popup_open;
                    unlock_viewport();
                }
            }

            if (nk_menu_item_label(ctx, "New Scene", NK_TEXT_LEFT)) {
                    new_scene_popup_open = !new_scene_popup_open;
                    lock_viewport_interaction = !lock_viewport_interaction;
                    // reset fields
                    strcpy(new_scene_name, "");
            }
            
            if (nk_menu_item_label(ctx, "Delete Current Scene", NK_TEXT_LEFT)) {
                scene_deletion_popup_open = !scene_deletion_popup_open;
                lock_viewport_interaction = !lock_viewport_interaction;
            }

            nk_layout_row_dynamic(ctx, 25, 1);
            // TODO: warning popup lose changes
            if(nk_menu_item_label(ctx, "Reload Scene", NK_TEXT_LEFT)){
                ye_reload_scene();
                editor_re_attach_ecs();
            }

            if(nk_menu_item_label(ctx, "Scene Settings", NK_TEXT_LEFT)){
                if(!ui_component_exists("scene_settings")){
                    ui_register_component("scene_settings", editor_panel_scene_settings);
                    lock_viewport();
                }
                else{
                    remove_ui_component("scene_settings");
                    unlock_viewport();
                }
            }

            nk_menu_end(ctx);
        }
        nk_layout_row_push(ctx, 85);
        if (nk_menu_begin_label(ctx, "Settings", NK_TEXT_LEFT, nk_vec2(160, 200))) {
            nk_layout_row_dynamic(ctx, 25, 1);
            if(nk_menu_item_label(ctx, "Editor Settings", NK_TEXT_LEFT)){
                if(!ui_component_exists("editor_settings")){
                    ui_register_component("editor_settings", ye_editor_paint_editor_settings);
                }
                else{
                    remove_ui_component("editor_settings");
                }
                lock_viewport_interaction = !lock_viewport_interaction;
            }

            nk_menu_end(ctx);
        }
        nk_layout_row_push(ctx, 45);
        if (nk_menu_begin_label(ctx, "Help", NK_TEXT_LEFT, nk_vec2(200, 200))) {
            nk_layout_row_dynamic(ctx, 25, 1);
            if (nk_menu_item_label(ctx, "Shortcuts", NK_TEXT_LEFT)) {
                if(!ui_component_exists("editor keybinds")){
                    ui_register_component("editor keybinds", editor_panel_keybinds);
                    lock_viewport();
                } else {
                    remove_ui_component("editor keybinds");
                    unlock_viewport();
                }
            }
            if (nk_menu_item_label(ctx, "Documentation", NK_TEXT_LEFT)) {
                editor_open_in_system("https://zoogies.github.io/yoyoengine");
            }
            if (nk_menu_item_label(ctx, "Source Code", NK_TEXT_LEFT)) {
                editor_open_in_system("https://github.com/zoogies/yoyoengine");
            }
            if(nk_menu_item_label(ctx, "Credits", NK_TEXT_LEFT)) {
                if(!ui_component_exists("credits")){
                    ui_register_component("editor_credits", editor_panel_credits);
                    lock_viewport();
                } else {
                    remove_ui_component("editor_credits");
                    unlock_viewport();
                }
            }
            nk_menu_end(ctx);
        }
        nk_layout_row_push(ctx, 45);
        if (nk_menu_begin_label(ctx, "Quit", NK_TEXT_LEFT, nk_vec2(200, 200))) {
            nk_layout_row_dynamic(ctx, 25, 1);
            if (nk_menu_item_label(ctx, "Exit (without saving)", NK_TEXT_LEFT)) { // TODO: save prompt if unsaved
                quit = true;
            }
            nk_menu_end(ctx);
        }
        
        // spacer //
        // get the amount of space (less on small screens more on large)
        int barlength = screenWidth / 1.5;
        int dropdown_length = 45 + 55 + 85 + 45 + 45;
        int status_length = 110 + 110 + 110;
        int margin = 50;
        int spacepx = ye_clamp(screenWidth - 1280,0,barlength - (dropdown_length + status_length) - margin);
        // int spacepx = ye_clamp(screenWidth - 1280,0,(45 + 55 + 85 + 45 + 45) - (110 + 110 + 110));
        // printf("spacepx: %d\n", spacepx);
        nk_layout_row_push(ctx, spacepx);
        nk_label(ctx, "                                                                                                                        ", NK_TEXT_LEFT);
        ///////////

        // used for tooltips
        const struct nk_input *in = &ctx->input;

        if(YE_STATE.runtime.error_count > 0){
            char buf[64];
            sprintf(buf, "%d errors", YE_STATE.runtime.error_count);
            nk_layout_row_push(ctx, 110);
            struct nk_rect bounds = nk_widget_bounds(ctx);
            nk_label_colored(ctx, buf, NK_TEXT_CENTERED, nk_rgb(255, 0, 0));
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Open the console to view. (Help > Shortcuts) to see keybind.");
        }
        if(YE_STATE.runtime.warning_count > 0){
            char buf[64];
            sprintf(buf, "%d warnings", YE_STATE.runtime.warning_count);
            nk_layout_row_push(ctx, 110);
            struct nk_rect bounds = nk_widget_bounds(ctx);
            nk_label_colored(ctx, buf, NK_TEXT_CENTERED, nk_rgb(255, 255, 0));
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Open the console to view. (Help > Shortcuts) to see keybind.");
        }

        // saved status
        nk_layout_row_push(ctx, 110);
        struct nk_rect bounds = nk_widget_bounds(ctx);
        if(saving){
            nk_label_colored(ctx, "Saving", NK_TEXT_CENTERED, nk_rgb(255, 255, 0));
            if (nk_input_is_mouse_hovering_rect(in, bounds))
                nk_tooltip(ctx, "Scene is being saved to disk. Please do not terminate the editor.");
        }
        else{
            if(unsaved){
                nk_label_colored(ctx, "Unsaved", NK_TEXT_CENTERED, nk_rgb(255, 0, 255));
                if (nk_input_is_mouse_hovering_rect(in, bounds))
                    nk_tooltip(ctx, "You have unsaved changes. (Help > Shortcuts) to see keybind.");
            }
            else{
                nk_label_colored(ctx, "Saved", NK_TEXT_CENTERED, nk_rgb(0, 255, 0));
                if (nk_input_is_mouse_hovering_rect(in, bounds))
                    nk_tooltip(ctx, "Scene is up to date with disk.");
            }
        }
        nk_menubar_end(ctx);
        nk_end(ctx);
    }
}