/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include <stdio.h>
#include <stdarg.h>

#include <yoyoengine/yoyoengine.h>

void editor_open_in_system(const char *url_or_file_path) {
    char command[512];

    #ifdef _WIN32
        snprintf(command, sizeof(command), "start %s", url_or_file_path);
    #elif defined(__APPLE__)
        snprintf(command, sizeof(command), "open %s", url_or_file_path);
    #else
        snprintf(command, sizeof(command), "xdg-open %s", url_or_file_path);
    #endif

    system(command);
}

bool editor_update_window_title(const char *format, ...) {
    char title[256];
    va_list args;
    va_start(args, format);
    vsnprintf(title, sizeof(title), format, args);
    va_end(args);
    
    if (SDL_SetWindowTitle(YE_STATE.runtime.window, title)) {
        ye_logf(info, "EDITOR Updated window title to: %s\n", title);
        return true;
    }
    else{
        ye_logf(error, "EDITOR Failed to update window title: %s\n", SDL_GetError());
        return false;
    }
}
