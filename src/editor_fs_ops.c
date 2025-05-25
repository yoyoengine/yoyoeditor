/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include "editor.h"
#include "editor_fs_ops.h"

#ifdef _WIN32
    #include <windows.h>
    #include <sys/utime.h>
    #include <io.h>
#else
    #include <utime.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

#include <yoyoengine/yoyoengine.h>

bool editor_mkdir(const char *path) {
    bool res = SDL_CreateDirectory(path);
    
    if(res) ye_logf(info, "EDITOR Created directory: %s\n", path);
    else    ye_logf(error, "EDITOR Failed to create directory: %s. %s\n", path, SDL_GetError());

    return res;
}

bool editor_file_exists(const char *path) {
    bool res = SDL_GetPathInfo(path, NULL);

    if(res) ye_logf(info, "EDITOR File exists: %s\n", path);
    else    ye_logf(error, "EDITOR File does not exist: %s. %s\n", path, SDL_GetError());

    return res;
}

bool editor_rename_path(const char *src, const char *dst) {
    bool res = SDL_RenamePath(src, dst);

    if(res) ye_logf(info, "EDITOR Renamed path from %s to %s\n", src, dst);
    else    ye_logf(error, "EDITOR Failed to rename path from %s to %s. %s\n", src, dst, SDL_GetError());

    return res;
}

bool editor_delete_file(const char *path) {
    bool res = SDL_RemovePath(path);

    if(res) ye_logf(info, "EDITOR Deleted path: %s\n", path);
    else    ye_logf(error, "EDITOR Failed to delete path: %s. %s\n", path, SDL_GetError());

    return res;
}

bool editor_copy_file(const char *src, const char *dst) {
    bool res = SDL_CopyFile(src, dst);

    if(res) ye_logf(info, "EDITOR Copied file from %s to %s\n", src, dst);
    else    ye_logf(error, "EDITOR Failed to copy file from %s to %s. %s\n", src, dst, SDL_GetError());

    return res;
}

static SDL_EnumerationResult SDLCALL _recurse_copy_callback(void *userdata, const char *dirname, const char *fname) {
    char src[512];
    char dst[512];

    snprintf(src, sizeof(src), "%s/%s", dirname, fname);
    snprintf(dst, sizeof(dst), "%s/%s", (const char *)userdata, fname);

    if(SDL_CopyFile(src, dst)) {
        ye_logf(info, "EDITOR Copied file from %s to %s\n", src, dst);
    } else {
        ye_logf(error, "EDITOR Failed to copy file from %s to %s. %s\n", src, dst, SDL_GetError());
        return SDL_ENUM_FAILURE;
    }

    return SDL_ENUM_CONTINUE;
}

bool editor_recurse_copy_directory(const char *src, const char *dst) {
    bool res = SDL_EnumerateDirectory(src, _recurse_copy_callback, NULL);

    if(res) ye_logf(info, "EDITOR Recursively copied directory from %s to %s\n", src, dst);
    else    ye_logf(error, "EDITOR Failed to recursively copy directory from %s to %s. %s\n", src, dst, SDL_GetError());

    return res;
}

void editor_touch_file(const char *file_path, const char *content) {
    // ensure the directory exists
    char *dir = strdup(file_path);
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        editor_mkdir(dir);
    }
    free(dir);

    FILE *file = fopen(file_path, "w");
    if (file) {
        if(content != NULL)
            fprintf(file, "%s", content);
        fclose(file);
        ye_logf(debug, "EDITOR Touched file: %s\n", file_path);
    }
    else {
        ye_logf(error, "EDITOR Failed to touch file: %s\n", file_path);
    }
}

int editor_set_fs_times(const char *path, time_t access_time, time_t modification_time) {
#ifdef _WIN32
    struct _utimbuf times;
    struct _stat st;

    if (_stat(path, &st) != 0) {
        perror("stat failed");
        return -1;
    }

    times.actime = (access_time > 0) ? access_time : st.st_atime;
    times.modtime = (modification_time > 0) ? modification_time : st.st_mtime;

    return _utime(path, &times);
#else
    struct utimbuf times;
    struct stat st;

    if (stat(path, &st) != 0) {
        perror("stat failed");
        return -1;
    }

    times.actime = (access_time > 0) ? access_time : st.st_atime;
    times.modtime = (modification_time > 0) ? modification_time : st.st_mtime;

    return utime(path, &times);
#endif
}

#ifdef _WIN32
    #include <direct.h>
#else
    #include <unistd.h>
#endif

bool editor_chdir(const char *path) {
    #ifdef _WIN32
        if (_chdir(path) == 0) {
            ye_logf(info, "EDITOR Changed directory to: %s\n", path);
            return true;
        } else {
            ye_logf(error, "EDITOR Failed to change directory to: %s. %s\n", path, SDL_GetError());
            return false;
        }
    #else
        if (chdir(path) == 0) {
            ye_logf(info, "EDITOR Changed directory to: %s\n", path);
            return true;
        } else {
            ye_logf(error, "EDITOR Failed to change directory to: %s. %s\n", path, SDL_GetError());
            return false;
        }
    #endif
}

static SDL_EnumerationResult SDLCALL _recurse_delete_callback(void *userdata, const char *dirname, const char *fname) {
    (void)userdata; // unused
    
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dirname, fname);

    if (SDL_RemovePath(path)) {
        ye_logf(info, "EDITOR Deleted path: %s\n", path);
    } else {
        ye_logf(error, "EDITOR Failed to delete path: %s. %s\n", path, SDL_GetError());
        return SDL_ENUM_FAILURE;
    }

    return SDL_ENUM_CONTINUE;
}

bool editor_recurse_delete_directory(const char *path) {
    bool res = SDL_EnumerateDirectory(path, _recurse_delete_callback, NULL);

    if (res) {
        if (SDL_RemovePath(path)) {
            ye_logf(info, "EDITOR Recursively deleted directory: %s\n", path);
        } else {
            ye_logf(error, "EDITOR Failed to delete directory: %s. %s\n", path, SDL_GetError());
            return false;
        }
    } else {
        ye_logf(error, "EDITOR Failed to recursively delete directory: %s. %s\n", path, SDL_GetError());
        return false;
    }

    return true;
}
