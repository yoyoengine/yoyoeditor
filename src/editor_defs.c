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
    { "OGG audio",   "ogg" },
    { "MP3 audio",   "mp3" },
    { "All audio",   "wav;ogg;mp3" }
};
int editor_num_audio_filters = sizeof(editor_audio_filters) / sizeof(SDL_DialogFileFilter);
