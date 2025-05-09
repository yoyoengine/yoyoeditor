/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#ifndef YE_EDITOR_FILE_PICKER_H
#define YE_EDITOR_FILE_PICKER_H

#include "editor_defs.h"

/*
    File picker response wrapper modes.
*/
enum editor_picker_response_mode {
    EDITOR_PICKER_WRITE_CHAR_PTR,
    EDITOR_PICKER_WRITE_CHAR_BUF,
    EDITOR_PICKER_FWD_CB
};

/*
    Wrap SDL_FilePicker parameters with some custom
    response modes.
*/
struct editor_picker_data {
    SDL_DialogFileFilter *filter;
    int *num_filters;
    const char *default_location;
    void *userdata;

    // custom extra
    enum editor_picker_response_mode response_mode;
    union {
        char **output_ptr;
        struct {
            char *buffer;
            size_t size;
        } output_buf;
        SDL_DialogFileCallback callback;
    } dest;

    // private (internal)
    bool _truncate_resource_path;
};

/*
    Pick a single file, use struct args to set parameters.
*/
void editor_pick_file(struct editor_picker_data data);

/*
    Pick a single file, use struct args to set parameters, but do not set
    default_location beforehand.

    Returns the truncated file path relative to the resource folder.
*/
void editor_pick_resource_file(struct editor_picker_data data);

/*
    Pick a folder, use struct args to set parameters.
*/
void editor_pick_folder(struct editor_picker_data data);

#endif