/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include "editor.h"
#include "editor_file_picker.h"


static void SDLCALL _picker_wrapper(void* userdata, const char* const* filelist, int filter){
    struct editor_picker_data *picker_data = (struct editor_picker_data*)userdata;
    if(!picker_data) {
        ye_logf(YE_LL_ERROR, "Couldn't convert picker data.\n");
        return;
    }

    if(!filelist) {
        ye_logf(YE_LL_ERROR, "An error occured: %s\n", SDL_GetError());
        return;
    }
    else if (!*filelist) {
        ye_logf(YE_LL_DEBUG, "No file selected in engine selector dialog.\n");
        return;
    }
    ye_logf(YE_LL_DEBUG, "Picker selected: %s\n", *filelist);

    /*
        PTR output mode
    */
    if(picker_data->response_mode == EDITOR_PICKER_WRITE_CHAR_PTR) {
        // if output pointer is valid
        if(picker_data->dest.output_ptr){
            // free the an existing ptr value
            if(*picker_data->dest.output_ptr){
                free(*picker_data->dest.output_ptr);
            }
            // write the selected path to the output pointer
            if(picker_data->_truncate_resource_path){
                // truncate the path to the resource folder
                const char *resources_subpath = strstr(*filelist, "resources/");
                if (resources_subpath) {
                    *picker_data->dest.output_ptr = strdup(resources_subpath + strlen("resources/"));
                } else {
                    *picker_data->dest.output_ptr = strdup(*filelist);
                }
            }
            else{
                *picker_data->dest.output_ptr = strdup(*filelist);
            }
        }
        else {
            ye_logf(YE_LL_ERROR, "File browser return write: output pointer is NULL.\n");
        }
    }

    /*
        BUF output mode
    */
    else if(picker_data->response_mode == EDITOR_PICKER_WRITE_CHAR_BUF) {
        if(picker_data->dest.output_buf.buffer && picker_data->dest.output_buf.size > 0){
            // write the selected path directly to the output buffer
            if(picker_data->_truncate_resource_path){
                // truncate the path to the resource folder
                const char *resources_subpath = strstr(*filelist, "resources/");
                if (resources_subpath) {
                    strncpy(picker_data->dest.output_buf.buffer, resources_subpath + strlen("resources/"), 
                            picker_data->dest.output_buf.size - 1);
                    picker_data->dest.output_buf.buffer[picker_data->dest.output_buf.size - 1] = '\0';
                } else {
                    strncpy(picker_data->dest.output_buf.buffer, *filelist, picker_data->dest.output_buf.size - 1);
                    picker_data->dest.output_buf.buffer[picker_data->dest.output_buf.size - 1] = '\0';
                }
            }
            else{
                strncpy(picker_data->dest.output_buf.buffer, *filelist, picker_data->dest.output_buf.size - 1);
                picker_data->dest.output_buf.buffer[picker_data->dest.output_buf.size - 1] = '\0';
            }
        }
        else {
            ye_logf(YE_LL_ERROR, "File browser return write: output buffer is NULL.\n");
        }
    }

    /*
        CB output mode
    */
    else if(picker_data->response_mode == EDITOR_PICKER_FWD_CB) {
        // call the callback function
        picker_data->dest.callback(picker_data->userdata, filelist, filter);
    }
    else {
        ye_logf(YE_LL_ERROR, "File browser return write: invalid response mode.\n");
    }
    
    (void)filter; // unused

    // cleanup the heap copy of data
    free(picker_data);
}

void editor_pick_file(struct editor_picker_data data) {
    // create a heap copy of data
    struct editor_picker_data *picker_data = malloc(sizeof(struct editor_picker_data));
    if (!picker_data) {
        ye_logf(YE_LL_ERROR, "Failed to allocate memory for picker data\n");
        return;
    }
    memcpy(picker_data, &data, sizeof(struct editor_picker_data));

    SDL_ShowOpenFileDialog(
        _picker_wrapper,
        (void*)picker_data,
        YE_STATE.runtime.window,
        picker_data->filter,
        *picker_data->num_filters,
        picker_data->default_location,
        false
    );
}

void editor_pick_resource_file(struct editor_picker_data data) {
    // create a heap copy of data
    struct editor_picker_data *picker_data = malloc(sizeof(struct editor_picker_data));
    if (!picker_data) {
        ye_logf(YE_LL_ERROR, "Failed to allocate memory for picker data\n");
        return;
    }
    memcpy(picker_data, &data, sizeof(struct editor_picker_data));

    // set the truncate resource path flag
    picker_data->_truncate_resource_path = true;

    // default location should be EDITOR_STATE.opened_project_resources_path
    picker_data->default_location = EDITOR_STATE.opened_project_resources_path;

    SDL_ShowOpenFileDialog(
        _picker_wrapper,
        (void*)picker_data,
        YE_STATE.runtime.window,
        picker_data->filter,
        *picker_data->num_filters,
        picker_data->default_location,
        false
    );
}

void editor_pick_folder(struct editor_picker_data data){
    // create a heap copy of data
    struct editor_picker_data *picker_data = malloc(sizeof(struct editor_picker_data));
    if (!picker_data) {
        ye_logf(YE_LL_ERROR, "Failed to allocate memory for picker data\n");
        return;
    }
    memcpy(picker_data, &data, sizeof(struct editor_picker_data));

    SDL_ShowOpenFolderDialog(
        _picker_wrapper,
        (void*)picker_data,
        YE_STATE.runtime.window,
        picker_data->default_location,
        false
    );
}