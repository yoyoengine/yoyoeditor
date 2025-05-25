/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/
#include <stdbool.h>

#ifndef YE_EDITOR_FS_OPS_H
#define YE_EDITOR_FS_OPS_H

/*
    v2 SDL3 API
*/

/**
 * @brief Make a directory.
 * 
 * @param path The path to the directory to create 
 * @return true on success, false on failure
 */
bool editor_mkdir(const char *path);

/**
 * @brief Check if a file exists.
 * 
 * @param file_path The path to the file
 * @return true if the file exists, false otherwise
 */
bool editor_file_exists(const char *file_path);

/**
 * @brief Rename a file or directory.
 * 
 * @param src The source path
 * @param dst The destination path
 * @return true on success, false on failure
 */
bool editor_rename_path(const char *src, const char *dst);

/**
 * @brief Delete a file or directory.
 * 
 * @param path The path to the file or directory to delete
 * @return true on success, false on failure
 */
bool editor_delete_file(const char *path);

/**
 * @brief Copy a file or directory.
 * 
 * @param src The source path
 * @param dst The destination path
 * @return true on success, false on failure
 */
bool editor_copy_file(const char *src, const char *dst);

/**
 * @brief Recursively copy a directory.
 * 
 * @param src The source directory
 * @param dst The destination directory
 * @return true on success, false on failure
 */
bool editor_recurse_copy_directory(const char *src, const char *dst);

/**
 * @brief Touches a file with the given content.
 * 
 * @param file_path The path to the file (the folder will be created if it doesn't exist)
 * @param content The content to write to the file, if any
 */
void editor_touch_file(const char *file_path, const char *content);

/**
 * @brief Sets the access and modification times of a file or folder.
 * 
 * @param path The path to the file or folder
 * @param access_time The access time to set (0 to keep the current)
 * @param modification_time The modification time to set (0 to keep the current)
 * @return int 0 on success, -1 on error
 */
int editor_set_fs_times(const char *path, time_t access_time, time_t modification_time);

/**
 * @brief Change the current working directory to the specified path.
 * 
 * @param path The path to change to
 * @return true on success, false on failure
 */
bool editor_chdir(const char *path);

/**
 * @brief Recursively delete a directory and its contents.
 * 
 * @param path The path to the directory to delete
 * @return true on success, false on failure
 */
bool editor_recurse_delete_directory(const char *path);

#endif