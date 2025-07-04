/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

// lowkey i just copied these all from editor_ui.c so maybe these are redundant
#include <stdio.h>
#ifdef __linux__
    #include <unistd.h>
#else
    #include <platform/windows/unistd.h>
#endif
#include <p2d/p2d.h>
#include <yoyoengine/yoyoengine.h>
#include "editor.h"
#include "editor_ui.h"
#include "editor_serialize.h"
#include "editor_panels.h"
#include "editor_selection.h"
#include "editor_utils.h"

/*
    Some variables used globally
*/
char proposed_script_path[1024] = "";
char proposed_animation_meta_path[1024] = "";
char proposed_tilemap_meta_path[1024] = "";

float editor_selection_group_x = 0;
float editor_selection_group_y = 0;
float editor_selection_last_group_x = 0;
float editor_selection_last_group_y = 0;

/*
    Takes in the bounds we are going to paint into, and returns
    the pt (int) that the font needs loaded in to maximize its quality

    Both formulas are from chatgpt
*/
int _auto_calculate_font_size(struct ye_rectf bounds){
    // print out recieved bounds
    //ye_logf(debug, "Recieved bounds: x: %f, y: %f, w: %f, h: %f\n", bounds.x, bounds.y, bounds.w, bounds.h);

    const int areaInPixels = bounds.w * bounds.h;

    // Assuming a standard DPI value of 96, adjust as needed
    const float dpi = 96.0;
    
    // Formula for calculating the font size based on area
    float fontSize = sqrt((float)areaInPixels / (72.0 * dpi));
    
    // Convert the calculated font size to points
    int fontSizeInPoints = (int)(fontSize * 72.0);
    
    int calculated = ye_clamp(fontSizeInPoints,1,500);

    ye_logf(debug, "Calculated auto font size: %d\n", calculated);

    return calculated;
}

void _paint_transform(struct nk_context *ctx, struct ye_entity *ent){
    if(ent->transform != NULL){
        if(nk_tree_push(ctx, NK_TREE_TAB, "Transform", NK_MAXIMIZED)){
            nk_layout_row_dynamic(ctx, 25, 4);
            nk_label(ctx, "X:", NK_TEXT_CENTERED);
            nk_property_float(ctx, "#x", -1000000, &ent->transform->x, 1000000, 1, 5);
            nk_label(ctx, "Y:", NK_TEXT_CENTERED);
            nk_property_float(ctx, "#y", -1000000, &ent->transform->y, 1000000, 1, 5);
            
            nk_layout_row_dynamic(ctx, 25, 4);
            nk_label(ctx, "Rotation:", NK_TEXT_CENTERED);
            nk_property_float(ctx, "#r", -1000000, &ent->transform->rotation, 1000000, 1, 5);
            nk_label(ctx, "", NK_TEXT_CENTERED); // empty space
            nk_label(ctx, "", NK_TEXT_CENTERED); // empty space

            // TODO: correct into range of 0-359 with modulo operation

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            if(nk_button_label(ctx, "Remove Component")){
                ye_remove_transform_component(ent);
                editor_unsaved();
                editor_deselect_all();
            }

            nk_tree_pop(ctx);
        }
    }
    else{
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label_colored(ctx, "No transform component", NK_TEXT_CENTERED, nk_rgb(255, 255, 0));
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        if(nk_button_label(ctx, "Add Transform Component")){
            ye_add_transform_component(ent, 0, 0);
            editor_unsaved();
        }
    }
}

static void SDLCALL editor_browse_renderer_image_cb(void* userdata, const char* const* filelist, int filter){
    if(!filelist) {
        ye_logf(YE_LL_ERROR, "An error occured: %s\n", SDL_GetError());
        return;
    }
    else if (!*filelist) {
        ye_logf(YE_LL_DEBUG, "No file selected in engine selector dialog.\n");
        return;
    }

    const char *selected_path = *filelist;
    const char *resources_subpath = strstr(selected_path, "resources/");
    if (resources_subpath) {
        selected_path = resources_subpath + strlen("resources/");
    }
    // TODO: can we just limit the scope of where the file picker can go?

    ye_logf(YE_LL_DEBUG, "Selected engine path: %s\n", selected_path);

    struct ye_entity *ent = (struct ye_entity*)userdata;
    free(ent->renderer->renderer_impl.image->src);
    ent->renderer->renderer_impl.image->src = strdup(selected_path);
    // recomputes the image texture
    ye_update_renderer_component(ent);
    editor_unsaved();
    
    (void)userdata; // unused
    (void)filter; // unused
}

void _paint_renderer(struct nk_context *ctx, struct ye_entity *ent){
    if(ent->renderer != NULL){
        if(nk_tree_push(ctx, NK_TREE_TAB, "Renderer", NK_MAXIMIZED)){
            nk_layout_row_dynamic(ctx, 25, 3);
            nk_checkbox_label(ctx, "Active", (nk_bool*)&ent->renderer->active);
            nk_checkbox_label(ctx, "Relative", (nk_bool*)&ent->renderer->relative);
            nk_checkbox_label(ctx, "Lock Aspect Ratio", (nk_bool*)&ent->renderer->lock_aspect_ratio);

            nk_layout_row_dynamic(ctx, 25, 2);
            nk_property_float(ctx, "#x", -1000000, &ent->renderer->rect.x, 1000000, 1, 5);
            nk_property_float(ctx, "#y", -1000000, &ent->renderer->rect.y, 1000000, 1, 5);

            float prev_w = ent->renderer->rect.w;
            float prev_h = ent->renderer->rect.h;

            nk_property_float(ctx, "#w", 0, &ent->renderer->rect.w, 1000000, 1, 5);
            nk_property_float(ctx, "#h", 0, &ent->renderer->rect.h, 1000000, 1, 5);

            if(ent->renderer->lock_aspect_ratio){
                if(prev_w != ent->renderer->rect.w){
                    ent->renderer->rect.h = ent->renderer->rect.w * (prev_h / prev_w);
                }
                else if(prev_h != ent->renderer->rect.h){
                    ent->renderer->rect.w = ent->renderer->rect.h * (prev_w / prev_h);
                }
            }

            nk_checkbox_label(ctx, "Flipped X", (nk_bool*)&ent->renderer->flipped_x);
            nk_checkbox_label(ctx, "Flipped Y", (nk_bool*)&ent->renderer->flipped_y);

            nk_layout_row_dynamic(ctx, 25, 3);
            nk_layout_row_dynamic(ctx, 25, 3);
            nk_property_int(ctx, "center x", -1000000, &ent->renderer->center.x, 1000000, 1, 5);
            nk_property_int(ctx, "center y", -1000000, &ent->renderer->center.y, 1000000, 1, 5);
            if(nk_button_label(ctx, "Auto Center")){
                ent->renderer->center.x = ent->renderer->rect.w / 2;
                ent->renderer->center.y = ent->renderer->rect.h / 2;
                editor_unsaved();
            }

            nk_layout_row_dynamic(ctx, 25, 2);
            nk_layout_row_dynamic(ctx, 25, 2);
            // nk_label(ctx, "Alignment:", NK_TEXT_LEFT); TODO
            nk_property_int(ctx, "#z", -1000000, &ent->renderer->z, 1000000, 1, 5);
            nk_property_float(ctx, "#Rotation", -1000000, &ent->renderer->rotation, 1000000, 1, 5);

            // TODO: correct into range of 0-359 with modulo operation

            nk_layout_row_dynamic(ctx, 25, 2);
            nk_property_int(ctx, "#alpha", 0, &ent->renderer->alpha, 255, 1, 5);
            nk_label(ctx, "", NK_TEXT_CENTERED); // empty space

            if (nk_tree_push(ctx, NK_TREE_TAB, "Alignment", NK_MAXIMIZED))
            {
                int i;
                static int alignment_arr[9] = {0,0,0, 0,0,0, 0,0,0};
                for (int i = 0; i < 9; i++) {alignment_arr[i] = 0;}
                // ^ zero the arr then we load its value from actual comp property

                switch(editor_current_selection->renderer->alignment){
                    case 9:
                        // stretch, nothing selected
                        break;
                    default:
                        alignment_arr[editor_current_selection->renderer->alignment] = 1;
                        // printf("alignment: %d\n", YE_STATE.editor.selected_entity->renderer->alignment);
                        break;
                }

                // preserve original size
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_checkbox_label(ctx, "Preserve Original Size", (nk_bool*)&ent->renderer->preserve_original_size);
                // dont need to observe change since recomputed every frame

                nk_layout_row_static(ctx, 50, 100, 3);
                for (i = 0; i < 9; ++i) {
                    if (nk_selectable_label(ctx, "X", NK_TEXT_CENTERED, (nk_bool *)&alignment_arr[i])) {
                        // // zero any other fields that might have been selected
                        // for(int j = 0; j < 9; j++){
                        //     if(j != i){
                        //         alignment_arr[j] = 0;
                        //     }
                        // } TODO: removeme. leaving here because I think i might have to bugfix this soon
                        
                        // if the entire array is zero
                        int sum = 0;
                        for(int j = 0; j < 9; j++){
                            sum += alignment_arr[j];
                        }
                        if(sum == 0){
                            ent->renderer->alignment = YE_ALIGN_STRETCH;
                        }
                        else{
                            ent->renderer->alignment = (enum ye_alignment)(i);
                        }
                        // printf("Selected alignment %d\n", i);
                        editor_unsaved();
                    }
                }
                switch(ent->renderer->alignment){ // TODO: consolidate, this is yucky
                    case 0:
                        nk_label(ctx, "Top Left", NK_TEXT_CENTERED);
                        break;
                    case 1:
                        nk_label(ctx, "Top Center", NK_TEXT_CENTERED);
                        break;
                    case 2:
                        nk_label(ctx, "Top Right", NK_TEXT_CENTERED);
                        break;
                    case 3:
                        nk_label(ctx, "Middle Left", NK_TEXT_CENTERED);
                        break;
                    case 4:
                        nk_label(ctx, "Middle Center", NK_TEXT_CENTERED);
                        break;
                    case 5:
                        nk_label(ctx, "Middle Right", NK_TEXT_CENTERED);
                        break;
                    case 6:
                        nk_label(ctx, "Bottom Left", NK_TEXT_CENTERED);
                        break;
                    case 7:
                        nk_label(ctx, "Bottom Center", NK_TEXT_CENTERED);
                        break;
                    case 8:
                        nk_label(ctx, "Bottom Right", NK_TEXT_CENTERED);
                        break;
                    case 9:
                        nk_label(ctx, "Stretch", NK_TEXT_CENTERED);
                        break;
                    default:
                        nk_label(ctx, "!!!Corrupted!!!", NK_TEXT_CENTERED);
                        break;
                }
                nk_tree_pop(ctx);
            }

            if (nk_tree_push(ctx, NK_TREE_TAB, "Renderer Data", NK_MAXIMIZED)){
                switch(ent->renderer->type){
                    case YE_RENDERER_TYPE_IMAGE:
                        nk_layout_row_dynamic(ctx, 25, 1);
                        nk_label(ctx, "Image Renderer", NK_TEXT_CENTERED);

                        nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 3);

                        nk_layout_row_push(ctx, 0.3);
                        nk_label(ctx, "Image src:", NK_TEXT_LEFT);

                        // Allocate a temporary buffer that is large enough for user input
                        char temp_src_buffer[1024];
                        strncpy(temp_src_buffer, ent->renderer->renderer_impl.image->src, sizeof(temp_src_buffer));
                        temp_src_buffer[sizeof(temp_src_buffer) - 1] = '\0';  // Ensure null-termination

                        // Allow the user to edit the text in the temporary buffer
                        nk_layout_row_push(ctx, 0.5);
                        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, temp_src_buffer, sizeof(temp_src_buffer), nk_filter_default);

                        // If the text has been changed, replace the old text with the new one
                        if (strcmp(temp_src_buffer, ent->renderer->renderer_impl.image->src) != 0) {
                            free(ent->renderer->renderer_impl.image->src);
                            ent->renderer->renderer_impl.image->src = strdup(temp_src_buffer);
                            // recomputes the image texture
                            ye_update_renderer_component(ent);
                            editor_unsaved();
                        }

                        // browser
                        nk_layout_row_push(ctx, 0.05);
                        nk_layout_row_push(ctx, 0.2);
                        if(nk_button_image_label(ctx, editor_icons.folder, "Browse", NK_TEXT_CENTERED)){
                            ye_pick_resource_file(
                                (struct ye_picker_data){
                                    .filter = ye_picker_image_filters,
                                    .num_filters = &ye_picker_num_image_filters,
                                    .userdata = (void*)ent,
            
                                    .response_mode = YE_PICKER_FWD_CB,
                                    .dest.callback = editor_browse_renderer_image_cb
                                }
                            );
                        }

                        break;
                    case YE_RENDERER_TYPE_TEXT:
                        nk_layout_row_dynamic(ctx, 25, 1);
                        nk_label(ctx, "Text Renderer", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(ctx, 25, 2);
                        nk_label(ctx, "Text:", NK_TEXT_LEFT);

                        /*
                            Ok, this is pretty fucking stupid

                            In order to actually edit fields like this, we need a temp editable larger field
                            and can subsequently update the original if this is changed.
                            Im assuming this is absolutely awful for performance but it only happens in the
                            editor, so who fucking cares

                            The real problem with this that makes it a FIXME TODO is the size cap of the field lengths.
                            This also means that any buffers created in the editor will perma have a 1024 length?
                            actually maybe not, they are encoded into json and read as shortened in the runtime.
                        */

                        // text //

                        // Allocate a temporary buffer that is large enough for user input
                        char temp_buffer[1024];
                        strncpy(temp_buffer, ent->renderer->renderer_impl.text->text, sizeof(temp_buffer));
                        temp_buffer[sizeof(temp_buffer) - 1] = '\0';  // Ensure null-termination

                        // Allow the user to edit the text in the temporary buffer
                        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, temp_buffer, sizeof(temp_buffer), nk_filter_default);

                        // If the text has been changed, replace the old text with the new one
                        if (strcmp(temp_buffer, ent->renderer->renderer_impl.text->text) != 0) {
                            free(ent->renderer->renderer_impl.text->text);
                            ent->renderer->renderer_impl.text->text = strdup(temp_buffer);
                            // recomputes the text texture
                            ye_update_renderer_component(ent);
                            editor_unsaved();
                        }

                        // wrapping //
                        nk_layout_row_dynamic(ctx, 25, 2);
                        nk_label(ctx, "Wrap Width:", NK_TEXT_LEFT);
                        nk_property_int(ctx, "#px", 0, &ent->renderer->renderer_impl.text->wrap_width, 1000000, 1, 5);

                        // color //
                        nk_layout_row_dynamic(ctx, 25, 2);
                        nk_label(ctx, "Color (name):", NK_TEXT_LEFT);

                        // Allocate a temporary buffer that is large enough for user input
                        char temp_buffer_color[1024];
                        strncpy(temp_buffer_color, ent->renderer->renderer_impl.text->color_name, sizeof(temp_buffer_color));
                        temp_buffer_color[sizeof(temp_buffer_color) - 1] = '\0';  // Ensure null-termination

                        // Allow the user to edit the text in the temporary buffer
                        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, temp_buffer_color, sizeof(temp_buffer_color), nk_filter_default);

                        // If the text has been changed, replace the old text with the new one
                        if (strcmp(temp_buffer_color, ent->renderer->renderer_impl.text->color_name) != 0) {
                            free(ent->renderer->renderer_impl.text->color_name);
                            ent->renderer->renderer_impl.text->color_name = strdup(temp_buffer_color);
                            // recomputes the text texture
                            ye_update_renderer_component(ent);
                            editor_unsaved();
                        }

                        // font //
                        nk_layout_row_dynamic(ctx, 25, 2);
                        nk_label(ctx, "Font (name):", NK_TEXT_LEFT);

                        // Allocate a temporary buffer that is large enough for user input
                        char temp_buffer_font[1024];
                        strncpy(temp_buffer_font, ent->renderer->renderer_impl.text->font_name, sizeof(temp_buffer_font));
                        temp_buffer_font[sizeof(temp_buffer_font) - 1] = '\0';  // Ensure null-termination

                        // Allow the user to edit the text in the temporary buffer
                        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, temp_buffer_font, sizeof(temp_buffer_font), nk_filter_default);

                        // If the text has been changed, replace the old text with the new one
                        if (strcmp(temp_buffer_font, ent->renderer->renderer_impl.text->font_name) != 0) {
                            free(ent->renderer->renderer_impl.text->font_name);
                            ent->renderer->renderer_impl.text->font_name = strdup(temp_buffer_font);
                            // recomputes the text texture
                            ye_update_renderer_component(ent);
                            editor_unsaved();
                        }

                        // font size //
                        nk_layout_row_dynamic(ctx, 25, 3);
                        nk_label(ctx, "Font Size:", NK_TEXT_LEFT);
                        int res = nk_propertyi(ctx, "#pt", 1, ent->renderer->renderer_impl.text->font_size, 500, 1, 5);
                        if(res != ent->renderer->renderer_impl.text->font_size){
                            ent->renderer->renderer_impl.text->font_size = res;
                            // recomputes the text texture
                            ye_update_renderer_component(ent);
                            editor_unsaved();
                        }

                        // auto font size //
                        if(nk_button_label(ctx, "Auto Compute")){
                            ent->renderer->renderer_impl.text->font_size = _auto_calculate_font_size(ent->renderer->computed_pos);
                            // recomputes the text texture
                            ye_update_renderer_component(ent);
                            editor_unsaved();
                        }

                        break;
                    /*
                        Todo: rest of the renderer types
                    */
                    case YE_RENDERER_TYPE_TILEMAP_TILE:
                        nk_layout_row_dynamic(ctx, 25, 1);
                        nk_label(ctx, "Tile Renderer", NK_TEXT_CENTERED);
                        nk_layout_row_dynamic(ctx, 25, 2);
                        nk_label(ctx, "Tilemap src:", NK_TEXT_LEFT);

                        // Allocate a temporary buffer that is large enough for user input
                        char temp_tilemap_src_buffer[1024];
                        strncpy(temp_tilemap_src_buffer, ent->renderer->renderer_impl.tile->handle, sizeof(temp_tilemap_src_buffer));
                        temp_tilemap_src_buffer[sizeof(temp_tilemap_src_buffer) - 1] = '\0';  // Ensure null-termination

                        // Allow the user to edit the text in the temporary buffer
                        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, temp_tilemap_src_buffer, sizeof(temp_tilemap_src_buffer), nk_filter_default);

                        // If the text has been changed, replace the old text with the new one
                        if (strcmp(temp_tilemap_src_buffer, ent->renderer->renderer_impl.tile->handle) != 0) {
                            free(ent->renderer->renderer_impl.tile->handle);
                            ent->renderer->renderer_impl.tile->handle = strdup(temp_tilemap_src_buffer);
                            // recomputes the image texture
                            ye_update_renderer_component(ent);
                            editor_unsaved();
                        }

                        nk_layout_row_dynamic(ctx, 25, 1);
                        nk_layout_row_dynamic(ctx, 25, 1);
                        nk_label(ctx, "Tile details:", NK_TEXT_LEFT);
                        nk_layout_row_dynamic(ctx, 25, 2);
                        nk_property_int(ctx, "#x", -1000000, &ent->renderer->renderer_impl.tile->src.x, 1000000, 1, 5);
                        nk_property_int(ctx, "#y", -1000000, &ent->renderer->renderer_impl.tile->src.y, 1000000, 1, 5);
                        nk_property_int(ctx, "#w", 0, &ent->renderer->renderer_impl.tile->src.w, 1000000, 1, 5);
                        nk_property_int(ctx, "#h", 0, &ent->renderer->renderer_impl.tile->src.h, 1000000, 1, 5);
                        break;
                    case YE_RENDERER_TYPE_ANIMATION:
                        nk_layout_row_dynamic(ctx, 25, 1);
                        nk_label(ctx, "Animation Renderer", NK_TEXT_CENTERED);
                        nk_layout_row_dynamic(ctx, 25, 2);
                        nk_label(ctx, "Meta file:", NK_TEXT_LEFT);

                        // Allocate a temporary buffer that is large enough for user input
                        char temp_animation_src_buffer[1024];
                        strncpy(temp_animation_src_buffer, ent->renderer->renderer_impl.animation->meta_file, sizeof(temp_animation_src_buffer));
                        temp_animation_src_buffer[sizeof(temp_animation_src_buffer) - 1] = '\0';  // Ensure null-termination
                        
                        // Allow the user to edit the text in the temporary buffer
                        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, temp_animation_src_buffer, sizeof(temp_animation_src_buffer), nk_filter_default);

                        // If the text has been changed, replace the old text with the new one
                        if (strcmp(temp_animation_src_buffer, ent->renderer->renderer_impl.animation->meta_file) != 0) {
                            free(ent->renderer->renderer_impl.animation->meta_file);
                            ent->renderer->renderer_impl.animation->meta_file = strdup(temp_animation_src_buffer);
                            // recomputes the image texture
                            ye_update_renderer_component(ent);
                            editor_unsaved();
                        }
                        break;
                    default:
                        nk_label(ctx, "!!!Corrupted - NO TYPE!!!", NK_TEXT_CENTERED);
                        break;
                }
                nk_tree_pop(ctx);
            }

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            if(nk_button_label(ctx, "Remove Component")){
                ye_remove_renderer_component(ent);
                editor_unsaved();
                editor_deselect_all();

                nk_tree_pop(ctx);
                return;
            }

            // struct SDL_Rect bounds = ye_get_position_rect(ent, YE_COMPONENT_RENDERER);
            // ye_debug_render_rect(bounds.x, bounds.y, bounds.w, bounds.h, (SDL_Color){0, 255, 0, 128}, 8);

            nk_tree_pop(ctx);
        }
    }
    else{
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label_colored(ctx, "No renderer component", NK_TEXT_CENTERED, nk_rgb(255, 255, 0));
        nk_layout_row_dynamic(ctx, 25, 1);

        /*
            Lets have a combo for which type we would like to add
        */
        static int add_renderer_cb_selected_type = 0;
        const char *add_renderer_cb_items[] = {"Text", "Text Outlined", "Image", "Animation", "Tilemap Tile"};

        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "Add Renderer Type:", NK_TEXT_LEFT);
        add_renderer_cb_selected_type = nk_combo(ctx, add_renderer_cb_items, 5, add_renderer_cb_selected_type, 25, nk_vec2(200,200));

        /*
            In the case of animations and tilemap tiles, lets ask some additional details
            to construct a new renderer with prepopulated info
        */
        if(add_renderer_cb_selected_type == YE_RENDERER_TYPE_ANIMATION){
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Meta file:", NK_TEXT_LEFT);

            // use proposed_animation_meta_path
            // Allocate a temporary buffer that is large enough for user input
            char temp_animation_src_buffer[1024];
            strncpy(temp_animation_src_buffer, proposed_animation_meta_path, sizeof(temp_animation_src_buffer));
            temp_animation_src_buffer[sizeof(temp_animation_src_buffer) - 1] = '\0';  // Ensure null-termination

            // Allow the user to edit the text in the temporary buffer
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, temp_animation_src_buffer, sizeof(temp_animation_src_buffer), nk_filter_default);

            // If the text has been changed, replace the old text with the new one
            if (strcmp(temp_animation_src_buffer, proposed_animation_meta_path) != 0) {
                strncpy(proposed_animation_meta_path, temp_animation_src_buffer, sizeof(proposed_animation_meta_path));
                proposed_animation_meta_path[sizeof(proposed_animation_meta_path) - 1] = '\0';  // Ensure null-termination
            }
        }
        else if(add_renderer_cb_selected_type == YE_RENDERER_TYPE_TILEMAP_TILE){
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Tilemap src:", NK_TEXT_LEFT);

            // use proposed_tilemap_meta_path
            // Allocate a temporary buffer that is large enough for user input
            char temp_tilemap_src_buffer[1024];
            strncpy(temp_tilemap_src_buffer, proposed_tilemap_meta_path, sizeof(temp_tilemap_src_buffer));
            temp_tilemap_src_buffer[sizeof(temp_tilemap_src_buffer) - 1] = '\0';  // Ensure null-termination

            // Allow the user to edit the text in the temporary buffer
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, temp_tilemap_src_buffer, sizeof(temp_tilemap_src_buffer), nk_filter_default);

            // If the text has been changed, replace the old text with the new one
            if (strcmp(temp_tilemap_src_buffer, proposed_tilemap_meta_path) != 0) {
                strncpy(proposed_tilemap_meta_path, temp_tilemap_src_buffer, sizeof(proposed_tilemap_meta_path));
                proposed_tilemap_meta_path[sizeof(proposed_tilemap_meta_path) - 1] = '\0';  // Ensure null-termination
            }
        }

        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        if(nk_button_label(ctx, "Add Renderer Component")){
            switch(add_renderer_cb_selected_type){
                case 0:
                    ye_add_text_renderer_component(ent, 99, "Sample Text", "default", 12, "white",0);
                    editor_unsaved();
                    break;
                case 1:
                    ye_add_text_outlined_renderer_component(ent, 99, "Sample Text", "default", 12, "white", "red", 5,0);
                    editor_unsaved();
                    break;
                case 2:
                    ye_add_image_renderer_component(ent, 1, "fakeimage.png");
                    editor_unsaved();
                    break;
                case 3:
                    ye_add_animation_renderer_component(ent, 1, proposed_animation_meta_path);
                    editor_unsaved();
                    break;
                case 4:
                    ye_add_tilemap_renderer_component(ent, 1, proposed_tilemap_meta_path, (SDL_Rect){0,0,100,100});
                    editor_unsaved();
                    break;
                default:
                    break;
            }
        }
    }
}

void _paint_camera(struct nk_context *ctx, struct ye_entity *ent){
    if(ent->camera != NULL){
        if(nk_tree_push(ctx, NK_TREE_TAB, "Camera", NK_MAXIMIZED)){
            nk_layout_row_dynamic(ctx, 25, 3);
            nk_checkbox_label(ctx, "Active", (nk_bool*)&ent->camera->active);
            nk_checkbox_label(ctx, "Relative", (nk_bool*)&ent->camera->relative);
            nk_checkbox_label(ctx, "Lock Aspect Ratio", (nk_bool*)&ent->camera->lock_aspect_ratio);
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_property_float(ctx, "#x", -1000000, &ent->camera->view_field.x, 1000000, 1, 5);
            nk_property_float(ctx, "#y", -1000000, &ent->camera->view_field.y, 1000000, 1, 5);

            float prev_w = ent->camera->view_field.w;
            float prev_h = ent->camera->view_field.h;

            nk_property_float(ctx, "#w", 0, &ent->camera->view_field.w, 1000000, 1, 5);
            nk_property_float(ctx, "#h", 0, &ent->camera->view_field.h, 1000000, 1, 5);
            
            if(ent->camera->lock_aspect_ratio){
                if(prev_w != ent->camera->view_field.w){
                    ent->camera->view_field.h = ent->camera->view_field.w * (prev_h / prev_w);
                }
                else if(prev_h != ent->camera->view_field.h){
                    ent->camera->view_field.w = ent->camera->view_field.h * (prev_w / prev_h);
                }
            }
            
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_property_int(ctx, "#z", -1000000, &ent->camera->z, 1000000, 1, 5);

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            if(nk_button_label(ctx, "Remove Component")){
                ye_remove_camera_component(ent);
                editor_unsaved();
                editor_deselect_all();
                nk_tree_pop(ctx);
                return;
            }
            
            // SDL_Rect bounds = ye_get_position_rect(ent, YE_COMPONENT_CAMERA);
            // ye_debug_render_rect(bounds.x, bounds.y, bounds.w, bounds.h, (SDL_Color){0, 255, 0, 128}, 8);

            nk_tree_pop(ctx);
        }
    }
    else{
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label_colored(ctx, "No camera component", NK_TEXT_CENTERED, nk_rgb(255, 255, 0));
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        if(nk_button_label(ctx, "Add Camera Component")){
            ye_add_camera_component(ent, 9999, (struct ye_rectf){0,0,1920,1080});
            editor_unsaved();
        }
    }
}

void _paint_rigidbody(struct nk_context *ctx, struct ye_entity *ent) {
    if(ent->rigidbody != NULL) {
        if(nk_tree_push(ctx, NK_TREE_TAB, "Rigidbody", NK_MAXIMIZED)) {
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_checkbox_label(ctx, "Active", (nk_bool*)&ent->rigidbody->active);

            nk_layout_row_dynamic(ctx, 25, 2);
            nk_property_float(ctx, "#offset x", -1000000, &ent->rigidbody->transform_offset_x, 1000000, 1, 5);
            nk_property_float(ctx, "#offset y", -1000000, &ent->rigidbody->transform_offset_y, 1000000, 1, 5);
            nk_layout_row_dynamic(ctx, 25, 1);

            // const struct nk_input *in = &ctx->input;
            // struct nk_rect bounds;

            if (nk_tree_push(ctx, NK_TREE_TAB, "Rigidbody Properties", NK_MAXIMIZED)) {
                nk_layout_row_dynamic(ctx, 25, 2);
                
                // bounds = nk_widget_bounds(ctx);
                nk_checkbox_label(ctx, "Static", (nk_bool *)(&ent->rigidbody->p2d_object.is_static));
                // if (nk_input_is_mouse_hovering_rect(in, bounds))
                    // nk_tooltip(ctx, "If checked, the object will not move or be affected by forces");
                
                // bounds = nk_widget_bounds(ctx);
                nk_checkbox_label(ctx, "Trigger", (nk_bool *)(&ent->rigidbody->p2d_object.is_trigger));
                // if (nk_input_is_mouse_hovering_rect(in, bounds))
                    // nk_tooltip(ctx, "Will be uncollidable, but emit events when collided with");

                nk_layout_row_dynamic(ctx, 25, 2);
                nk_property_float(ctx, "#density", P2D_MIN_DENSITY, &ent->rigidbody->p2d_object.density, P2D_MAX_DENSITY, 1, 5);
                nk_property_float(ctx, "#restitution", 0.0000001, &ent->rigidbody->p2d_object.restitution, 1, 0.01, 0.01);

                nk_layout_row_dynamic(ctx, 25, 1);
                nk_layout_row_dynamic(ctx, 25, 3);
                nk_property_float(ctx, "#vel x", -1000000, &ent->rigidbody->p2d_object.vx, 1000000, 1, 5);
                nk_property_float(ctx, "#vel y", -1000000, &ent->rigidbody->p2d_object.vy, 1000000, 1, 5);
                nk_property_float(ctx, "#vel ang", -1000000, &ent->rigidbody->p2d_object.vr, 1000000, 1, 5);

                nk_tree_pop(ctx);
            }

            nk_layout_row_dynamic(ctx, 25, 1);

            if (nk_tree_push(ctx, NK_TREE_TAB, "Rigidbody Collider", NK_MAXIMIZED)) {
                static const char *collider_types[] = {"Rectangle", "Circle"};

                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Collider Type:", NK_TEXT_LEFT);
                int prev_selected_collider_type = ent->rigidbody->p2d_object.type;
                ent->rigidbody->p2d_object.type = nk_combo(ctx, collider_types, 2, ent->rigidbody->p2d_object.type, 25, nk_vec2(200,200));

                if((int)ent->rigidbody->p2d_object.type != prev_selected_collider_type){
                    // reset the collider to default values
                    switch(ent->rigidbody->p2d_object.type) {
                        case P2D_OBJECT_CIRCLE: // Circle
                            ent->rigidbody->p2d_object.circle.radius = 0;
                            break;
                        case P2D_OBJECT_RECTANGLE: // Rectangle
                            ent->rigidbody->p2d_object.rectangle.width = 0;
                            ent->rigidbody->p2d_object.rectangle.height = 0;
                            break;
                    }
                    editor_unsaved();
                }

                nk_layout_row_dynamic(ctx, 25, 1);
                switch(ent->rigidbody->p2d_object.type) {
                    case P2D_OBJECT_CIRCLE: // Circle
                        nk_layout_row_dynamic(ctx, 25, 1);
                        nk_property_float(ctx, "#radius", 0.0000001, &ent->rigidbody->p2d_object.circle.radius, 1000000, 1, 5);
                        break;
                    case P2D_OBJECT_RECTANGLE: // Rectangle
                        nk_layout_row_dynamic(ctx, 25, 2);
                        nk_property_float(ctx, "#width", 0.0000001, &ent->rigidbody->p2d_object.rectangle.width, 1000000, 1, 5);
                        nk_property_float(ctx, "#height", 0.0000001, &ent->rigidbody->p2d_object.rectangle.height, 1000000, 1, 5);
                        break;
                }


                nk_tree_pop(ctx);
            }

            // 4x4 grid of selectable labels for collision layers
            
            // new tree collapsable section for collision layers
            nk_layout_row_dynamic(ctx, 25, 1);
            if(nk_tree_push(ctx, NK_TREE_TAB, "Collision Layers", NK_MAXIMIZED)){

                nk_label(ctx, "Collision Layers:", NK_TEXT_LEFT);
                nk_layout_row_static(ctx, 25, 100, 4);
                static const char *ye_collision_layer_names[16] = {
                    "Layer 1", "Layer 2", "Layer 3", "Layer 4",
                    "Layer 5", "Layer 6", "Layer 7", "Layer 8",
                    "Layer 9", "Layer 10", "Layer 11", "Layer 12",
                    "Layer 13", "Layer 14", "Layer 15", "Layer 16"
                };
                for(int i = 0; i < 16; i++) {
                    nk_bool selected = (ent->rigidbody->p2d_object.mask & (1 << i)) != 0;

                    // if selected, make the text black, else white
                    struct nk_color cache = ctx->style.selectable.text_normal_active;
                    struct nk_color cach2 = ctx->style.selectable.text_hover_active;
                    ctx->style.selectable.text_normal_active = selected ? nk_rgb(0, 0, 0) : nk_rgb(255, 255, 255);
                    ctx->style.selectable.text_hover_active = selected ? nk_rgb(0, 0, 0) : nk_rgb(255, 255, 255);

                    if(nk_selectable_label(ctx, ye_collision_layer_names[i], NK_TEXT_CENTERED, &selected)) {
                        ent->rigidbody->p2d_object.mask ^= (1 << i); // toggle the bit
                        editor_unsaved();
                    }

                    ctx->style.selectable.text_normal_active = cache;
                    ctx->style.selectable.text_hover_active = cach2;
                }
            
                nk_tree_pop(ctx);
            }
        
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            if(nk_button_label(ctx, "Remove Component")){
                ye_remove_rigidbody_component(ent);
                editor_unsaved();
                editor_deselect_all();
                nk_tree_pop(ctx);
                return;
            }

            nk_tree_pop(ctx);
        }
    }
    else {
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label_colored(ctx, "No rigidbody component", NK_TEXT_CENTERED, nk_rgb(255, 255, 0));
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        if(nk_button_label(ctx, "Add Rigidbody Component")){
            ye_add_rigidbody_component(ent, 0, 0, (struct p2d_object){0});
            editor_unsaved();
        }
    }
}

void _paint_tag(struct nk_context *ctx, struct ye_entity *ent){
    if(ent->tag != NULL){
        if(nk_tree_push(ctx, NK_TREE_TAB, "Tag", NK_MAXIMIZED)){
            // tag components can hold 10 buffers (TODO: sync this somehow with the #define in engine) so we want to just show them all as editable text boxes
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Tag Buffers:", NK_TEXT_LEFT);
            for(int i = 0; i < 10;){
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, ent->tag->tags[i], 20, nk_filter_default); i++;
                nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, ent->tag->tags[i], 20, nk_filter_default); i++;
            }

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            if(nk_button_label(ctx, "Remove Component")){
                ye_remove_tag_component(ent);
                editor_unsaved();
                editor_deselect_all();
                nk_tree_pop(ctx);
                return;
            }

            nk_tree_pop(ctx);
        }
    }
    else{
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label_colored(ctx, "No tag component", NK_TEXT_CENTERED, nk_rgb(255, 255, 0));
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        if(nk_button_label(ctx, "Add Tag Component")){
            ye_add_tag_component(ent);
            editor_unsaved();
        }
    }
}

void _paint_script(struct nk_context *ctx, struct ye_entity *ent){
    if(ent->lua_script != NULL){
        if(nk_tree_push(ctx, NK_TREE_TAB, "Script", NK_MAXIMIZED)){
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Script:", NK_TEXT_LEFT);

            // Allocate a temporary buffer that is large enough for user input
            char temp_buffer[1024];
            strncpy(temp_buffer, ent->lua_script->script_handle, sizeof(temp_buffer));
            temp_buffer[sizeof(temp_buffer) - 1] = '\0';  // Ensure null-termination

            // layout a Nuklear row with 80% for entry and 20% for open button
            nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 2);
            nk_layout_row_push(ctx, 0.77);

            // Allow the user to edit the text in the temporary buffer
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, temp_buffer, sizeof(temp_buffer), nk_filter_default);

            // gap
            nk_layout_row_push(ctx, 0.02);
            nk_label(ctx, "", NK_TEXT_CENTERED);

            // layout a Nuklear row with 20% for open button
            nk_layout_row_push(ctx, 0.21);
            if(nk_button_label(ctx, "Open"))
                editor_open_in_system(ye_path_resources(temp_buffer));

            // If the text has been changed, replace the old text with the new one
            if (strcmp(temp_buffer, ent->lua_script->script_handle) != 0) {
                free(ent->lua_script->script_handle);
                ent->lua_script->script_handle = strdup(temp_buffer);
                // printf("Changed script to %s\n", ent->lua_script->script_handle);
            }

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Globals:", NK_TEXT_LEFT);

            if(ent->lua_script->globals != NULL){
                nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 3);

                nk_layout_row_push(ctx, 0.40);
                nk_label(ctx, "Name", NK_TEXT_LEFT);

                nk_layout_row_push(ctx, 0.07);
                nk_label(ctx, "", NK_TEXT_CENTERED);

                nk_layout_row_push(ctx, 0.40);
                nk_label(ctx, "Value", NK_TEXT_LEFT);
                

                struct ye_lua_script_global *current = ent->lua_script->globals;
                while(current != NULL) {
                    nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 3);

                    nk_layout_row_push(ctx, 0.40);
                    nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, current->name, YE_LUA_SCRIPT_GLOBAL_NAME_MAX_CHARACTERS, nk_filter_default);

                    nk_layout_row_push(ctx, 0.07);
                    nk_label(ctx, "", NK_TEXT_CENTERED);

                    nk_layout_row_push(ctx, 0.40);
                    switch(current->type){
                        case YE_LSG_NUMBER:
                            nk_property_double(ctx, "#", -1000000, &current->value.number, 1000000, 1, 5);
                            break;
                        case YE_LSG_STRING:
                            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, current->value.string, YE_LUA_SCRIPT_GLOBAL_VALUE_STRING_MAX_CHARACTERS, nk_filter_default);
                            break;
                        case YE_LSG_BOOL:
                            nk_checkbox_label(ctx, "", (nk_bool *)(&current->value.boolean));
                            break;
                        default:
                            break;
                    }

                    nk_layout_row_push(ctx, 0.07);
                    nk_label(ctx, "", NK_TEXT_CENTERED);

                    nk_layout_row_push(ctx, 0.05);
                    if(nk_button_image(ctx, editor_icons.trash)) {
                        ye_lua_script_remove_global(ent, current->name);
                        editor_unsaved();
                        break; // do not loop over broken memory
                    }
                    current = current->next;
                }
            }
            else {
                nk_layout_row_dynamic(ctx, 10, 1);
                nk_label_colored(ctx, "No globals", NK_TEXT_CENTERED, nk_rgb(150, 150, 150));
            }

            // padding for add new global section
            nk_layout_row_dynamic(ctx, 10, 1);

            // WARN: THESE INDEXES MUST COORESPOND TO THE ENUM IN YE_LUA_SCRIPT.H
            static const char *nk_label_global_keys[] = {"number", "string", "boolean"};
            static int nk_label_global_selected_key = 0;

            nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 3);
            nk_layout_row_push(ctx, 0.30);
            nk_label(ctx, "Add new global:", NK_TEXT_LEFT);
            
            nk_layout_row_push(ctx, 0.09);
            nk_label(ctx, "", NK_TEXT_CENTERED);

            nk_layout_row_push(ctx, 0.37);
            nk_label_global_selected_key = nk_combo(ctx, nk_label_global_keys, 3, nk_label_global_selected_key, 25, nk_vec2(200,200));

            nk_layout_row_push(ctx, 0.02);
            nk_label(ctx, "", NK_TEXT_CENTERED);

            nk_layout_row_push(ctx, 0.21);
            if(nk_button_label(ctx, "Add")){
                double vd;
                bool vb;
                switch((enum ye_lua_script_global_t)nk_label_global_selected_key){
                    case YE_LSG_NUMBER:
                        vd = 0;
                        ye_lua_script_add_global(ent, YE_LSG_NUMBER, "new_global", (void *)&vd);
                        break;
                    case YE_LSG_STRING:
                        ye_lua_script_add_global(ent, YE_LSG_STRING, "new_global", (void *)"");
                        break;
                    case YE_LSG_BOOL:
                        vb = false;
                        ye_lua_script_add_global(ent, YE_LSG_BOOL, "new_global", (void *)&vb);
                        break;
                    default:
                        break;
                }

                editor_unsaved();
            }

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            if(nk_button_label(ctx, "Remove Component")){
                ye_remove_lua_script_component(ent);
                editor_unsaved();
                editor_deselect_all();
                nk_tree_pop(ctx);
                return;
            }
            
            nk_tree_pop(ctx);
        }
    }
    else{
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label_colored(ctx, "No script component", NK_TEXT_CENTERED, nk_rgb(255, 255, 0));
        nk_layout_row_dynamic(ctx, 25, 1);


        // proposed_script_path is used to hold the path that the user is typing in
        nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 3);

        nk_layout_row_push(ctx, 0.30);
        nk_label(ctx, "Script Path:", NK_TEXT_LEFT);

        nk_layout_row_push(ctx, 0.50);
        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, proposed_script_path, sizeof(proposed_script_path), nk_filter_default);

        // browse
        nk_layout_row_push(ctx, 0.20);
        if(nk_button_image_label(ctx, editor_icons.folder, "Browse", NK_TEXT_CENTERED)){
            ye_pick_resource_file(
                (struct ye_picker_data){
                    .filter = ye_picker_script_filters,
                    .num_filters = &ye_picker_num_script_filters,

                    .response_mode = YE_PICKER_WRITE_CHAR_BUF,
                    .dest.output_buf = {
                        .buffer = proposed_script_path,
                        .size = sizeof(proposed_script_path),
                    },
                }
            );
        }

        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        if(nk_button_label(ctx, "Add Script Component")){

            // if file does not exist, lets create it
            if(!ye_file_exists(ye_path_resources(proposed_script_path))){
                ye_touch_file(ye_path_resources(proposed_script_path), "-- Template yoyoengine Lua script,\n-- provided for your convenience! :)\n\nfunction onMount()\n\t\nend\n\nfunction onUpdate()\n\t\nend\n\nfunction onUnmount()\n\t\nend");
            }

            ye_add_lua_script_component(ent, proposed_script_path, NULL); // TODO: lua system hasnt been updated for yep yet. we need to read from bytecode or just content
            editor_unsaved();
        }
    }
}

static void SDLCALL editor_browse_audio_ret(void* userdata, const char* const* filelist, int filter){
    const char *selected_path = *filelist;
    const char *resources_subpath = strstr(selected_path, "resources/");
    if (resources_subpath) {
        selected_path = resources_subpath + strlen("resources/");
    }
    // TODO: can refactor picker to do this on every callback if checked later

    ye_logf(YE_LL_DEBUG, "Selected engine path: %s\n", selected_path);

    struct ye_entity *ent = (struct ye_entity*)userdata;
    if(ent && ent->audiosource && ent->audiosource->handle){
        // free the old path
        free(ent->audiosource->handle);
    }
    else{
        ye_logf(YE_LL_ERROR, "Failed to set audio source path, entity or audiosource is NULL\n");
        return;
    }

    ent->audiosource->handle = strdup(selected_path);
    editor_unsaved();
    
    (void)filter; // unused
}

void _paint_audiosource(struct nk_context *ctx, struct ye_entity *ent){
    if(ent->audiosource != NULL){
        if(nk_tree_push(ctx, NK_TREE_TAB, "Audio Source", NK_MAXIMIZED)){
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_checkbox_label(ctx, "Active", (nk_bool*)&ent->audiosource->active);

            static bool _audiosource_disabled = false;
            if(!ent->audiosource->active){
                _audiosource_disabled = true;
                nk_widget_disable_begin(ctx);
            }
            else{
                _audiosource_disabled = false;
            }

            nk_label(ctx, "Spatial Simulation:", NK_TEXT_LEFT);
            nk_checkbox_label(ctx, "Simulated", (nk_bool*)&ent->audiosource->simulated);
            
            // if simulation is off, gray out these fields
            if(!ent->audiosource->simulated)
                nk_widget_disable_begin(ctx);

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_checkbox_label(ctx, "Relative", (nk_bool*)&ent->audiosource->relative);
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_property_float(ctx, "#x", -1000000, &ent->audiosource->range.x, 1000000, 1, 5);
            nk_property_float(ctx, "#y", -1000000, &ent->audiosource->range.y, 1000000, 1, 5);
            nk_layout_row_dynamic(ctx, 25, 1);
            // save size before
            // float size = ent->audiosource->range.w;
            nk_property_float(ctx, "#size", ent->audiosource->range.h, &ent->audiosource->range.w, 1000000, 1, 5);
            nk_property_float(ctx, "#falloff start", 0, &ent->audiosource->range.h, ent->audiosource->range.w, 1, 5);

            // not needed after Lilith integration
            // // if the size changed, move the x and y to keep the center the same
            // if(size != ent->audiosource->range.w){
            //     ent->audiosource->range.x -= (ent->audiosource->range.w - size) / 2;
            //     ent->audiosource->range.y -= (ent->audiosource->range.w - size) / 2;
            // }

            if(!ent->audiosource->simulated && !_audiosource_disabled)
                nk_widget_disable_end(ctx);

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_checkbox_label(ctx, "Play on Awake", (nk_bool*)&ent->audiosource->play_on_awake);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_property_int(ctx, "#Loops", -1, &ent->audiosource->loops, 100, 1, 5);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_property_float(ctx, "#Volume", 0, &ent->audiosource->volume, 1, 0.01, 0.05);
            
            // Allocate a temporary buffer that is large enough for user input
            char temp_buffer_handle[1024];
            strncpy(temp_buffer_handle, ent->audiosource->handle, sizeof(temp_buffer_handle));
            temp_buffer_handle[sizeof(temp_buffer_handle) - 1] = '\0';  // Ensure null-termination

            // Allow the user to edit the text in the temporary buffer
            nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 3);
            
            nk_layout_row_push(ctx, 0.30);
            nk_label(ctx, "audio handle:", NK_TEXT_LEFT);

            nk_layout_row_push(ctx, 0.50);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, temp_buffer_handle, sizeof(temp_buffer_handle), nk_filter_default);

            // If the text has been changed, replace the old text with the new one
            if (strcmp(temp_buffer_handle, ent->audiosource->handle) != 0) {
                free(ent->audiosource->handle);
                ent->audiosource->handle = strdup(temp_buffer_handle);
                // printf("Changed handle to %s\n", ent->audiosource->handle);
                editor_unsaved();
            }

            // browse
            nk_layout_row_push(ctx, 0.20);
            if(nk_button_image_label(ctx, editor_icons.folder, "Browse", NK_TEXT_CENTERED)){
                ye_pick_resource_file(
                    (struct ye_picker_data){
                        .filter = ye_picker_audio_filters,
                        .num_filters = &ye_picker_num_audio_filters,
    
                        .response_mode = YE_PICKER_FWD_CB,
                        .dest.callback = editor_browse_audio_ret,
                        .userdata = (void*)ent,
                    }
                );
            }

            if(_audiosource_disabled)
                nk_widget_disable_end(ctx);

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            if(nk_button_label(ctx, "Remove Component")){
                ye_remove_audiosource_component(ent);
                editor_unsaved();
                editor_deselect_all();

                nk_tree_pop(ctx);
                return;
            }
            
            // SDL_Rect circ = ye_get_position_rect(ent, YE_COMPONENT_AUDIOSOURCE);
            // ye_debug_render_circle(circ.x + (circ.w / 2), circ.y + (circ.w / 2), circ.w / 2, (SDL_Color){0, 255, 0, 128}, 8);
            // ye_debug_render_circle(circ.x + (circ.w / 2), circ.y + (circ.w / 2), circ.h / 2, (SDL_Color){128, 255, 0, 128}, 8);

            nk_tree_pop(ctx);
        }
    }
    else{
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label_colored(ctx, "No audiosource component", NK_TEXT_CENTERED, nk_rgb(255, 255, 0));
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        if(nk_button_label(ctx, "Add Audiosource Component")){
            ye_add_audiosource_component(ent, "", 0, true, -1, true, (struct ye_rectf){0,0,0,0});
            editor_unsaved();
        }
    }
}

void _paint_button(struct nk_context *ctx, struct ye_entity *ent){
    if(ent->button != NULL){
        if(nk_tree_push(ctx, NK_TREE_TAB, "Button", NK_MAXIMIZED)){
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_checkbox_label(ctx, "Active", (nk_bool*)&ent->button->active);
            nk_checkbox_label(ctx, "Relative", (nk_bool*)&ent->button->relative);
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_property_float(ctx, "#x", -1000000, &ent->button->rect.x, 1000000, 1, 5);
            nk_property_float(ctx, "#y", -1000000, &ent->button->rect.y, 1000000, 1, 5);
            nk_property_float(ctx, "#w", 0, &ent->button->rect.w, 1000000, 1, 5);
            nk_property_float(ctx, "#h", 0, &ent->button->rect.h, 1000000, 1, 5);
            

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_layout_row_dynamic(ctx, 25, 1);
            if(nk_button_label(ctx, "Remove Component")){
                ye_remove_button_component(ent);
                editor_unsaved();
                editor_deselect_all();
                nk_tree_pop(ctx);
                return;
            }

            // SDL_Rect bounds = ye_get_position_rect(ent, YE_COMPONENT_BUTTON);
            // ye_debug_render_rect(bounds.x, bounds.y, bounds.w, bounds.h, (SDL_Color){0, 255, 0, 128}, 8);
            
            nk_tree_pop(ctx);
        }
    }
    else{
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_label_colored(ctx, "No button component", NK_TEXT_CENTERED, nk_rgb(255, 255, 0));
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_layout_row_dynamic(ctx, 25, 1);
        if(nk_button_label(ctx, "Add Button Component")){
            ye_add_button_component(ent,(struct ye_rectf){0,0,0,0});
            editor_unsaved();
        }
    }
}

bool comp_exists(int i, struct ye_entity *ent){
    switch(i){
        case 0:
            return ent->transform != NULL;
            break;
        case 1:
            return ent->renderer != NULL;
            break;
        case 2:
            return ent->camera != NULL;
            break;
        case 3:
            return ent->rigidbody != NULL;
            break;
        case 4:
            return ent->tag != NULL;
            break;
        case 5:
            return ent->lua_script != NULL;
            break;
        case 6:
            return ent->audiosource != NULL;
            break;
        case 7:
            return ent->button != NULL;
            break;
        default:
            return false;
            break;
    }
}

/*
    inspector panel

    defined in editor_ui.h
*/
void ye_editor_paint_inspector(struct nk_context *ctx){
    if(num_editor_selections == 0){
        return;
    }

    struct ye_entity *ent = editor_current_selection;
    if (nk_begin(ctx, "Entity", nk_rect(screenWidth/1.5, screenHeight / 2.5, screenWidth - screenWidth/1.5, screenHeight - screenHeight/2.5),
        NK_WINDOW_TITLE | NK_WINDOW_BORDER)) {
            if (num_editor_selections > 1){
                nk_layout_row_dynamic(ctx, 25, 1);

                char buffer[50];
                snprintf(buffer, 50, "Multiple (%d) entities selected", num_editor_selections);
                nk_label_colored(ctx, buffer, NK_TEXT_CENTERED, nk_rgb(255, 255, 255));
                
                nk_label(ctx, "Actions:", NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 25, 3);

                if(nk_button_label(ctx, "Toggle Active")){
                    struct editor_selection_node *current = editor_selections;

                    while(current != NULL){
                        current->ent->active = !current->ent->active;
                        current = current->next;
                    }

                    editor_unsaved();
                }

                if(nk_button_label(ctx, "Delete All")){
                    struct editor_selection_node *current = editor_selections;

                    while(current != NULL){
                        ye_destroy_entity(current->ent);
                        current = current->next;
                    }

                    editor_deselect_all();
                    editor_unsaved();
                }

                if(nk_button_label(ctx, "Duplicate All")){
                    /*
                        Will duplicate all selected entities,
                        and change the selection list to be only the
                        newly created entities
                    */

                    struct editor_selection_node *cur = editor_selections;
                    struct editor_selection_node *new = NULL;
                    int new_selections = 0;

                    while(cur != NULL) {
                        struct editor_selection_node *new_node = malloc(sizeof(struct editor_selection_node));
                        new_node->ent = ye_duplicate_entity(cur->ent);
                        new_node->next = new;
                        new_selections++;

                        new = new_node;

                        cur = cur->next;
                    }
                    editor_deselect_all();
                    editor_selections = new;
                    num_editor_selections = new_selections; 

                    editor_unsaved();
                }

                // TODO: could have an option to generate a prefab or new scene based on selections

                /*
                    Provide X and Y transformation for groups.
                    Every entity without a transform component will recieve one, and
                    we store a zerod offset variable for the group x and y, as well as a "last_updated" copy.
                    Each time we move the global offset, we will add the difference from the last updated and update accordingly.
                */

                if( editor_selection_group_x != editor_selection_last_group_x ||
                    editor_selection_group_y != editor_selection_last_group_y    ) {
                    
                    struct editor_selection_node *current = editor_selections;

                    while(current != NULL){
                        // make sure every selection has a transform
                        if(current->ent->transform == NULL){
                            ye_add_transform_component(current->ent, 0, 0);
                        }

                        current->ent->transform->x += editor_selection_group_x - editor_selection_last_group_x;
                        current->ent->transform->y += editor_selection_group_y - editor_selection_last_group_y;

                        current = current->next;
                    }

                    editor_selection_last_group_x = editor_selection_group_x;
                    editor_selection_last_group_y = editor_selection_group_y;

                }

                nk_layout_row_dynamic(ctx, 25, 1);
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "Transform Group:", NK_TEXT_LEFT);

                nk_layout_row_dynamic(ctx, 25, 2);
                nk_property_float(ctx, "#Group X", -1000000, &editor_selection_group_x, 1000000, 1, 5);
                nk_property_float(ctx, "#Group Y", -1000000, &editor_selection_group_y, 1000000, 1, 5);
                
                nk_end(ctx);
                return;
            }
            else if (num_editor_selections == 1) { // do not handle 0 selections because this panel is unrendered
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Name:", NK_TEXT_LEFT);
                // TODO: bugfix name editing, setting to zero len is unhappy, also should be 99 for str term?
                nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, ent->name, 100, nk_filter_default);

                nk_layout_row_dynamic(ctx, 25, 1);
                nk_checkbox_label(ctx, "Active", (nk_bool*)&ent->active);

                nk_layout_row_dynamic(ctx, 25, 1);
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "Components:", NK_TEXT_LEFT);

                /*
                    Selector tile layout thing that shows all components in list
                */
                static int current_component_inspector_tab = 0;
                const char *names[] = {"Transform", "Renderer", "Camera","Rigidbody","Tag","Script","Audio Source","Button"};
                static int num_components = sizeof(names) / sizeof(names[0]); // does this mean its only computed here once?

                nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0,0));
                nk_style_push_float(ctx, &ctx->style.button.rounding, 0);
                nk_layout_row_dynamic(ctx, 50, num_components/1.75);
                for (int i = 0; i < num_components; ++i) {
                    // Check if the component is not null
                    if (comp_exists(i, ent)) {
                        // Change the text color to green
                        ctx->style.button.text_normal = nk_rgb(0,255,0);
                    } else {
                        // Reset the text color
                        ctx->style.button.text_normal = nk_rgb(255,255,255); // TODO: THIS SHOULD BE READ FROM STYLE TABLE NOT HARDCODED
                    }

                    if (current_component_inspector_tab == i) {
                        /* active tab gets highlighted */
                        struct nk_style_item button_color = ctx->style.button.normal;
                        ctx->style.button.normal = ctx->style.button.active;
                        current_component_inspector_tab = nk_button_label(ctx, names[i]) ? i: current_component_inspector_tab;
                        ctx->style.button.normal = button_color;
                    } else current_component_inspector_tab = nk_button_label(ctx, names[i]) ? i: current_component_inspector_tab;

                    // reset button text color
                    ctx->style.button.text_normal = nk_rgb(255,255,255); // TODO: THIS SHOULD BE READ FROM STYLE TABLE NOT HARDCODED
                }
                nk_style_pop_float(ctx);
                nk_style_pop_vec2(ctx);

                /* Im going to switch with the integers here, not because strcmp is too much overhead but because I cant be fucked to type allat */
                switch(current_component_inspector_tab){
                    case 0: // transform //
                        _paint_transform(ctx,ent);
                        break;
                    case 1: // renderer //
                        _paint_renderer(ctx,ent);
                        break;
                    case 2: // camera //
                        _paint_camera(ctx,ent);
                        break;
                    case 3: // rigidbody //
                        _paint_rigidbody(ctx,ent);
                        break;
                    case 4: // tag //
                        _paint_tag(ctx,ent);
                        break;
                    case 5: // script //
                        _paint_script(ctx,ent);
                        break;
                    case 6: // audiosource //
                        _paint_audiosource(ctx,ent);
                        break;
                    case 7: // button //
                        _paint_button(ctx,ent);
                        break;
                }
            }
        nk_end(ctx);
    }
}