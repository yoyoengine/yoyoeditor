/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#ifndef EDITOR_UTILS_H
#define EDITOR_UTILS_H

#include <stdbool.h>
#include <stddef.h>

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

/**
 * @brief Check if a path is absolute on the current platform.
 *
 * @param path The path to check
 * @return true if the path is absolute, false otherwise
 */
bool editor_path_is_absolute(const char *path);

/**
 * @brief Make target_path relative to base_dir when both paths share a root.
 *
 * Writes the normalized target path unchanged when a relative path cannot be
 * made. Backslashes are normalized to forward slashes.
 *
 * @param base_dir The directory the relative path should start from
 * @param target_path The path to make relative
 * @param output Destination buffer
 * @param output_size Destination buffer size
 * @return true if output is relative, false if output is normalized target_path
 */
bool editor_make_relative_path(const char *base_dir, const char *target_path, char *output, size_t output_size);

/**
 * @brief Resolve target_path against base_dir when target_path is relative.
 *
 * Writes a normalized absolute path when base_dir is absolute. Absolute
 * target_path values are normalized and copied unchanged.
 *
 * @param base_dir The directory relative paths should start from
 * @param target_path The absolute or relative path to resolve
 * @param output Destination buffer
 * @param output_size Destination buffer size
 */
void editor_make_absolute_path(const char *base_dir, const char *target_path, char *output, size_t output_size);

#endif // EDITOR_UTILS_H
