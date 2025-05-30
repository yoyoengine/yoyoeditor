/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#ifndef EDITOR_UTILS_H
#define EDITOR_UTILS_H

#include <stdbool.h>

/**
 * @brief Opens a file or URL in the system's default application.
 * 
 * @param url_or_file_path The URL or file path to open
 */
void editor_open_in_system(const char *url_or_file_path);

/**
 * @brief Updates the window title (takes a format string).
 * 
 * @param format The format string for the title
 * @param ... The arguments for the format string
 * 
 * @return true if the title was updated successfully, false otherwise
 */
bool editor_update_window_title(const char *format, ...);

#endif // EDITOR_UTILS_H