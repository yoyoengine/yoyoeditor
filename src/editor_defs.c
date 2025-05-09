/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include <yoyoengine/yoyoengine.h>

const SDL_DialogFileFilter editor_image_filters[] = {
    { "PNG images",  "png" },
    { "JPEG images", "jpg;jpeg" },
    { "All images",  "png;jpg;jpeg" }
};
int editor_num_image_filters = sizeof(editor_image_filters) / sizeof(SDL_DialogFileFilter);

const SDL_DialogFileFilter editor_audio_filters[] = {
    { "WAV audio",   "wav" },
    { "MP3 audio",   "mp3" },
    { "All audio",   "wav;mp3" }
};
int editor_num_audio_filters = sizeof(editor_audio_filters) / sizeof(SDL_DialogFileFilter);

const SDL_DialogFileFilter editor_script_filters[] = {
    { "Lua script",   "lua" }
};
int editor_num_script_filters = sizeof(editor_script_filters) / sizeof(SDL_DialogFileFilter);

const SDL_DialogFileFilter editor_yoyo_filters[] = {
    { "yoyo config",   "yoyo" }
};
int editor_num_yoyo_filters = sizeof(editor_yoyo_filters) / sizeof(SDL_DialogFileFilter);

const SDL_DialogFileFilter editor_font_filters[] = {
    { "TrueType fonts",   "ttf" }
};
int editor_num_font_filters = sizeof(editor_font_filters) / sizeof(SDL_DialogFileFilter);

const SDL_DialogFileFilter editor_icon_filters[] = {
    { "Icon",   "rc" }
};
int editor_num_icon_filters = sizeof(editor_icon_filters) / sizeof(SDL_DialogFileFilter);

const SDL_DialogFileFilter editor_any_filters[] = {
    { "All files",   "*" }
};
int editor_num_any_filters = sizeof(editor_any_filters) / sizeof(SDL_DialogFileFilter);
