/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include <stdbool.h>

#include <p2d/p2d.h>

#include <yoyoengine/debug_renderer.h>
#include <yoyoengine/ecs/ecs.h>
#include <yoyoengine/ecs/rigidbody.h>
#include <yoyoengine/utils.h>

#include "editor.h"
#include "editor_input.h"
#include "editor_ui.h"
#include "editor_selection.h"

bool is_dragging = false;
SDL_Point drag_start;

bool editor_draw_drag_rect = false;

struct editor_selection_node * editor_selections = NULL;
int num_editor_selections = 0;

void editor_deselect_all(){
    ye_reset_editor_selection_group();

    struct editor_selection_node * itr = editor_selections;
    while(itr != NULL){
        struct editor_selection_node * temp = itr;
        itr = itr->next;
        free(temp);
    }
    editor_selections = NULL;
    num_editor_selections = 0;
}

bool already_selected(struct ye_entity * ent){
    struct editor_selection_node * itr = editor_selections;
    while(itr != NULL){
        if(itr->ent == ent) return true;
        itr = itr->next;
    }
    return false;
}

void add_selection(struct ye_entity * ent){
    if(already_selected(ent)) return;

    ye_reset_editor_selection_group();

    // discard selections of editor entities, like editor camera, origin, etc
    if(ent == editor_camera || ent == origin) return;

    struct editor_selection_node * new_node = malloc(sizeof(struct editor_selection_node));
    new_node->ent = ent;
    new_node->next = editor_selections;
    editor_selections = new_node;
    num_editor_selections++;
}

/*
    Only works with entities having transform components
*/
void select_within(SDL_Rect zone){

    /*
        Check if editor selection rect is big
        enough to be considered a selection
    */
    if(abs(zone.w) < PREFS.min_select_px && abs(zone.h) < PREFS.min_select_px){
        return;
    }

    /*
        If w or h is negative, we need to normalize the rect
    */
    if(zone.w < 0){ zone.x += zone.w; zone.w = abs(zone.w); }
    if(zone.h < 0){ zone.y += zone.h; zone.h = abs(zone.h); }

    struct ye_entity_node * itr = transform_list_head;
    while(itr != NULL){
        struct ye_entity * ent = itr->entity;

        if(!ent->active) {
            itr = itr->next;
            continue;
        }

        struct ye_rectf pos = ye_get_position(ent, YE_COMPONENT_TRANSFORM);
        if(pos.x > zone.x && pos.y > zone.y &&
            pos.x + pos.w < zone.x + zone.w &&
            pos.y + pos.h < zone.y + zone.h){
            add_selection(ent);
        }
        else{
            // check renderer for fallback
            if(ye_component_exists(ent, YE_COMPONENT_RENDERER)){
                struct ye_point_rectf sel = ye_rect_to_point_rectf(ye_convert_rect_rectf(zone));
                struct p2d_obb_verts sel_obb = ye_prect2obbverts(sel);
                struct p2d_obb_verts world_obb = ye_prect2obbverts(ent->renderer->_world_rect);
                if(p2d_obb_verts_intersects_obb_verts(sel_obb, world_obb)){
                    add_selection(ent);
                }
            }
        }
        itr = itr->next;
    }
}

void editor_selection_handler(SDL_Event event){
    // check if mouse left window
    if(event.type & SDL_EVENT_WINDOW_MOUSE_LEAVE){
        is_dragging = false; editor_draw_drag_rect = false;
    }

    // if we arent hovering editor ignore
    float mx, my; SDL_GetMouseState(&mx, &my);
    if(!is_hovering_editor(mx, my) || lock_viewport_interaction) {
        is_dragging = false; editor_draw_drag_rect = false;
        return;
    }

    // update mx and my to be world positions
    my = my - 35; // account for the menu bar

    float scaleX = (float)YE_STATE.engine.screen_width / (float)YE_STATE.engine.target_camera->camera->view_field.w;
    float scaleY = (float)YE_STATE.engine.screen_height / (float)YE_STATE.engine.target_camera->camera->view_field.h;
    struct ye_rectf campos = ye_get_position(YE_STATE.engine.target_camera, YE_COMPONENT_CAMERA);
    mx = ((mx / scaleX) + campos.x);
    my = ((my / scaleY) + campos.y);

    if(is_dragging)
        editor_draw_drag_rect = !(fabs(mx - drag_start.x) < PREFS.min_select_px && fabs(my - drag_start.y) < PREFS.min_select_px);
    

    switch (event.type) {
        case SDL_EVENT_MOUSE_BUTTON_DOWN :
            if (event.button.button == SDL_BUTTON_LEFT) {
                // if we clicked at all and werent holding ctrl, clear selections
                if (!(SDL_GetModState() & SDL_KMOD_CTRL)) {
                    editor_deselect_all();
                }
                
                /*
                    Detect the item clicked on and add it to the selected list

                    We check for:
                    - clicked within renderer comp bounds
                    - clicked within collider bounds
                    - clicked within audiosource bounds
                    - clicked within camera viewport
                    // (not now) - clicked within 10px of transform position
                */
                struct ye_entity_node *itr = entity_list_head;
                while(itr != NULL){
                    struct ye_entity * ent = itr->entity;

                    // if ent is editor_camera or origin skip
                    if(ent == editor_camera || ent == origin){
                        itr = itr->next;
                        continue;
                    }

                    if(!ent->active) {
                        itr = itr->next;
                        continue;
                    }

                    struct ye_pointf mp = {.x=mx, .y=my};
                    struct ye_point_rectf pos;
                    bool prev_sel = already_selected(ent);
                    bool selected = false;
                    
                    if(ye_component_exists(ent, YE_COMPONENT_RENDERER)){
                        pos = ye_get_position2(ent, YE_COMPONENT_RENDERER);
                        if(ye_pointf_in_point_rectf(mp, pos)) {
                            add_selection(ent);
                            selected = true;
                        }
                    }
                    // if(ye_component_exists(ent, YE_COMPONENT_COLLIDER)){
                    //     pos = ye_get_position2(ent, YE_COMPONENT_COLLIDER);
                    //     if(ye_pointf_in_point_rectf(mp, pos)) {
                    //         add_selection(ent);
                    //         selected = true;
                    //     }
                    // } //TODO: ye_component rigidbody
                    if(ye_component_exists(ent, YE_COMPONENT_AUDIOSOURCE)){
                        pos = ye_get_position2(ent, YE_COMPONENT_AUDIOSOURCE);

                        float w = ent->audiosource->range.w;

                        struct ye_rectf new = {pos.verticies[0].x - w, pos.verticies[0].y - w, w*2, w};
                        pos = ye_rect_to_point_rectf(new);

                        if(ye_pointf_in_point_rectf(mp, pos)) {
                            add_selection(ent);
                            selected = true;
                        }
                    }
                    // just ignore cameras because it's too annoying
                    // if(ye_component_exists(ent, YE_COMPONENT_CAMERA)){
                    //     pos = ye_get_position2(ent, YE_COMPONENT_CAMERA);
                    //     if(ye_pointf_in_point_rectf(mp, pos)) {
                    //         add_selection(ent);
                    //         selected = true;
                    //     }
                    // }
                    if(ye_component_exists(ent, YE_COMPONENT_BUTTON)){
                        pos = ye_get_position2(ent, YE_COMPONENT_BUTTON);
                        if(ye_pointf_in_point_rectf(mp, pos)) {
                            add_selection(ent);
                            selected = true;
                        }
                    }

                    /*
                        If holding ctrl and the entity we selected is already selected, deselect it
                    */
                    if(prev_sel && selected && (SDL_GetModState() & SDL_KMOD_CTRL)){
                        editor_deselect(ent);
                    }

                    // skip trans because it has no area
                    itr = itr->next;
                    continue;
                }
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP :
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (is_dragging) {
                    is_dragging = false;
                    editor_draw_drag_rect = false;

                    // attempt to select all items within the drag rectangle
                    select_within(editor_selecting_rect);
                }
            }
            break;
        case SDL_EVENT_MOUSE_MOTION :
            if (event.motion.state & SDL_BUTTON_LMASK) {
                if (!is_dragging) {
                    // Start dragging if not already dragging
                    is_dragging = true;

                    // Set the start point of the drag rectangle
                    drag_start = (SDL_Point){mx, my};
                }

                editor_selecting_rect = (SDL_Rect){drag_start.x, drag_start.y, mx - drag_start.x, my - drag_start.y};
            }
            break;
        default:
            break;
    }
}

SDL_Color purple = (SDL_Color){255, 0, 255, 225};
SDL_Color red = (SDL_Color){255, 0, 0, 225};
SDL_Color green = (SDL_Color){0, 255, 0, 225};
SDL_Color blue = (SDL_Color){0, 0, 255, 225};
SDL_Color orange = (SDL_Color){255, 165, 0, 225};
SDL_Color yellow = (SDL_Color){255, 255, 0, 225};
SDL_Color fade_yellow = (SDL_Color){255, 255, 0, 100};
SDL_Color pink = (SDL_Color){255, 105, 180, 225};

/*
    TODO: honestly, for selected entities we should always display their
    collider and audiosource bounds, and a box around renderer area if invisible.
    The actual rect that surrounds them all could show the minimum rect area to encompass
    all of them.
*/
void editor_render_selection_rects(){
    struct editor_selection_node * itr = editor_selections;
    while(itr != NULL){
        /*
            Do some checks to show the "area" of selected entities. We can only show
            entities that have some kind of "bound" ie: renderer, collider, audio source.
            The shape we draw when selected takes precedence in this order (high to low):
            - renderer bounds
            - collider bounds
            - audio source bounds
            - camera viewport bounds
            - if none of these components exist, draw a dot at the center of the entity transform.
            - if a transform doesnt exist, skip the entity
        */
        struct ye_entity * ent = itr->ent;

        // SDL_Color select_color = (SDL_Color){255, 0, 255, 255};

        if(ye_component_exists(ent, YE_COMPONENT_RENDERER)){
            // aligned
            struct ye_point_rectf pos = ye_get_position2(ent, YE_COMPONENT_RENDERER);
            pos = ye_world_prectf_to_screen(pos);
            ye_debug_render_prect(pos, green, 8);
            
            // not aligned
            ye_debug_render_prect(ent->renderer->_paintbounds_full_verts, pink, 8);

            // render cached center point
            ye_debug_render_rect(ent->renderer->_world_center.x - 5, ent->renderer->_world_center.y - 5, 10, 10, pink, 8);
        }
        if(ye_component_exists(ent, YE_COMPONENT_RIGIDBODY)){
            struct ye_point_rectf pos = ye_get_position2(ent, YE_COMPONENT_RIGIDBODY);
            
            if(ent->rigidbody->p2d_object.type == P2D_OBJECT_RECTANGLE){
                struct ye_rectf rect = {pos.verticies[0].x, pos.verticies[0].y, pos.verticies[1].x - pos.verticies[0].x, pos.verticies[2].y - pos.verticies[1].y};
                ye_debug_render_rect(rect.x, rect.y, ent->rigidbody->p2d_object.rectangle.width, ent->rigidbody->p2d_object.rectangle.height, red, 8);
            }
            else if(ent->rigidbody->p2d_object.type == P2D_OBJECT_CIRCLE){
                ye_debug_render_circle(pos.verticies[0].x, pos.verticies[0].y, ent->rigidbody->p2d_object.circle.radius, red, 8);
            }
        }
        if(ye_component_exists(ent, YE_COMPONENT_AUDIOSOURCE)){
            struct ye_point_rectf pos = ye_get_position2(ent, YE_COMPONENT_AUDIOSOURCE);
            // pos = ye_world_prectf_to_screen(pos);

            // calculate the center of the audio range
            // struct ye_pointf center = ye_point_rectf_center(pos);

            float max_range = ent->audiosource->range.w;
            float min_range = ent->audiosource->range.h;

            float x = pos.verticies[0].x;
            float y = pos.verticies[0].y;

            // ye_debug_render_circle(center.x, center.y, max_range, yellow, 8);
            // ye_debug_render_circle(center.x, center.y, min_range, fade_yellow, 8);
            ye_debug_render_circle(x, y, max_range, yellow, 8);
            ye_debug_render_circle(x, y, min_range, fade_yellow, 8);

            // ye_debug_render_circle(center.x - (max_range), center.y - (max_range), max_range, yellow, 8);
            // ye_debug_render_circle(center.x - (min_range), center.y - (min_range), min_range, fade_yellow, 8);
        }
        if(ye_component_exists(ent, YE_COMPONENT_CAMERA)){
            struct ye_point_rectf pos = ye_get_position2(ent, YE_COMPONENT_CAMERA);
            pos = ye_world_prectf_to_screen(pos);
            ye_debug_render_prect(pos, purple, 8);
        }
        if(ye_component_exists(ent, YE_COMPONENT_BUTTON)){
            struct ye_point_rectf pos = ye_get_position2(ent, YE_COMPONENT_BUTTON);
            pos = ye_world_prectf_to_screen(pos);
            ye_debug_render_prect(pos, blue, 8);
        }
        if(ye_component_exists(ent, YE_COMPONENT_TRANSFORM)){
            vec2_t center = (vec2_t){.data={ent->transform->x, ent->transform->y}};
            // center = lla_mat3_mult_vec2(YE_STATE.runtime.world2cam, center);

            ye_debug_render_rect(center.data[0] - 5, center.data[1] - 5, 10, 10, orange, 8);
        }

        itr = itr->next;
    }
}

bool editor_is_selected(struct ye_entity * ent){
    struct editor_selection_node * itr = editor_selections;
    while(itr != NULL){
        if(itr->ent == ent) return true;
        itr = itr->next;
    }
    return false;
}

void editor_deselect(struct ye_entity * ent){
    ye_reset_editor_selection_group();
    
    struct editor_selection_node * itr = editor_selections;
    struct editor_selection_node * prev = NULL;
    while(itr != NULL){
        if(itr->ent == ent){
            if(prev == NULL){
                editor_selections = itr->next;
            }
            else{
                prev->next = itr->next;
            }
            free(itr);
            num_editor_selections--;
            return;
        }
        prev = itr;
        itr = itr->next;
    }
}

void editor_select(struct ye_entity * ent){
    ye_reset_editor_selection_group();

    // check if keyboard is currently pressing ctrl
    if(!(SDL_GetModState() & SDL_KMOD_CTRL)){
        editor_deselect_all();
    }
    add_selection(ent);
}