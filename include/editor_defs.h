/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#ifndef YE_EDITOR_DEFS_H
#define YE_EDITOR_DEFS_H

#include <yoyoengine/yoyoengine.h>

extern SDL_DialogFileFilter editor_image_filters[];
extern int editor_num_image_filters;

extern SDL_DialogFileFilter editor_audio_filters[];
extern int editor_num_audio_filters;

extern SDL_DialogFileFilter editor_script_filters[];
extern int editor_num_script_filters; 

extern SDL_DialogFileFilter editor_yoyo_filters[];
extern int editor_num_yoyo_filters; 

extern SDL_DialogFileFilter editor_font_filters[];
extern int editor_num_font_filters; 

extern SDL_DialogFileFilter editor_icon_filters[];
extern int editor_num_icon_filters; 

extern SDL_DialogFileFilter editor_any_filters[];
extern int editor_num_any_filters;

#endif