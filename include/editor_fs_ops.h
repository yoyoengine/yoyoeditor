/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/
#include <stdbool.h>

#ifndef YE_EDITOR_FS_OPS_H
#define YE_EDITOR_FS_OPS_H

enum dialog_type{
    FILE_DIALOG,
    FOLDER_DIALOG
};

// recurse copy a directory
bool editor_copy_directory(const char *src, const char *dst);

// rename a directory or file
bool editor_rename(const char *src, const char *dst);

// only works one level deep of new directories
bool editor_create_directory(const char *path);

// prompts the user to select a folder and returns a malloced string
char * editor_file_dialog_select_folder();

// prompts the user to select a file and returns a malloced string
char *editor_file_dialog_select_file(const char *filter);

char *editor_file_dialog_select_resource(const char *filter);

#endif