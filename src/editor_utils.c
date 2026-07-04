/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

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

bool editor_path_is_absolute(const char *path) {
    if (!path || path[0] == '\0') return false;

    if (path[0] == '/' || path[0] == '\\') return true;

#ifdef _WIN32
    return strlen(path) >= 2 && isalpha((unsigned char)path[0]) && path[1] == ':';
#else
    return false;
#endif
}

static void editor_copy_normalized_path(const char *input, char *output, size_t output_size) {
    if (!output || output_size == 0) return;

    if (!input) {
        output[0] = '\0';
        return;
    }

    size_t i = 0;
    for (; i + 1 < output_size && input[i] != '\0'; i++) {
        output[i] = input[i] == '\\' ? '/' : input[i];
    }
    output[i] = '\0';

    while (i > 1 && output[i - 1] == '/') {
#ifdef _WIN32
        if (i == 3 && isalpha((unsigned char)output[0]) && output[1] == ':') break;
#endif
        output[--i] = '\0';
    }
}

static bool editor_path_chars_equal(char a, char b) {
#ifdef _WIN32
    return tolower((unsigned char)a) == tolower((unsigned char)b);
#else
    return a == b;
#endif
}

static int editor_count_path_segments(const char *path) {
    int segments = 0;
    bool in_segment = false;

    for (const char *p = path; *p; p++) {
        if (*p == '/') {
            if (in_segment) {
                segments++;
                in_segment = false;
            }
        } else {
            in_segment = true;
        }
    }

    if (in_segment) segments++;
    return segments;
}

bool editor_make_relative_path(const char *base_dir, const char *target_path, char *output, size_t output_size) {
    char base[1024];
    char target[1024];

    editor_copy_normalized_path(base_dir, base, sizeof(base));
    editor_copy_normalized_path(target_path, target, sizeof(target));

    if (!editor_path_is_absolute(base) || !editor_path_is_absolute(target)) {
        editor_copy_normalized_path(target_path, output, output_size);
        return false;
    }

    size_t common_len = 0;
    size_t i = 0;
    while (base[i] && target[i] && editor_path_chars_equal(base[i], target[i])) {
        if (base[i] == '/') common_len = i + 1;
        i++;
    }

    if (base[i] == '\0' && (target[i] == '\0' || target[i] == '/')) common_len = i;
    if (target[i] == '\0' && (base[i] == '\0' || base[i] == '/')) common_len = i;

    if (common_len == 0) {
        editor_copy_normalized_path(target_path, output, output_size);
        return false;
    }

    const char *base_tail = base + common_len;
    const char *target_tail = target + common_len;
    if (*base_tail == '/') base_tail++;
    if (*target_tail == '/') target_tail++;

    int up_segments = editor_count_path_segments(base_tail);
    size_t used = 0;

    if (up_segments == 0 && target_tail[0] == '\0') {
        snprintf(output, output_size, ".");
        return true;
    }

    output[0] = '\0';
    for (int segment = 0; segment < up_segments; segment++) {
        int written = snprintf(output + used, output_size - used, "../");
        if (written < 0 || (size_t)written >= output_size - used) return false;
        used += (size_t)written;
    }

    if (target_tail[0] != '\0') {
        snprintf(output + used, output_size - used, "%s", target_tail);
    } else if (used > 0 && output[used - 1] == '/') {
        output[used - 1] = '\0';
    }

    return true;
}

void editor_make_absolute_path(const char *base_dir, const char *target_path, char *output, size_t output_size) {
    if (!output || output_size == 0) return;

    if (editor_path_is_absolute(target_path)) {
        editor_copy_normalized_path(target_path, output, output_size);
        return;
    }

    char normalized_base[1024];
    editor_copy_normalized_path(base_dir, normalized_base, sizeof(normalized_base));

    if (!editor_path_is_absolute(normalized_base)) {
        editor_copy_normalized_path(target_path, output, output_size);
        return;
    }

    snprintf(output, output_size, "%s/%s", normalized_base, target_path ? target_path : "");
    editor_copy_normalized_path(output, output, output_size);
}
