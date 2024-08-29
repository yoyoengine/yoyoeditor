/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2024  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

/*
    Some platform specific implementations of managing projects
*/

#include <yoyoengine/yep.h>
#include <yoyoengine/yoyoengine.h>

#include "editor_build.h"

void editor_create_new_project(const char *target_dir) {

    #ifdef __unix__ || __linux__

        // create path/to/proj/proj_name (aka target_dir)
        if(!editor_create_directory(target_dir)){
            ye_logf(error, "Failed to create project at %s\n", target_dir);
            return;
        }
        
        // get the tag we want to merge into
        char *desired_tag;
        ye_version_tagify(YOYO_ENGINE_VERSION_STRING, &desired_tag);
        desired_tag = "main";
        
        // git clone with depth 1
        char git_clone_cmd[512];
        snprintf(git_clone_cmd, sizeof(git_clone_cmd), "git clone --depth 1 --branch %s https://github.com/yoyoengine/template.git \"%s\"", desired_tag, target_dir);
        if(system(git_clone_cmd) != 0){
            ye_logf(error, "Failed to clone template repository into %s\n", target_dir);
            return;
        }
        
        // Remove the .git directory to clean up
        char git_remove_git_dir_cmd[512];
        snprintf(git_remove_git_dir_cmd, sizeof(git_remove_git_dir_cmd), "rm -rf \"%s/.git\"", target_dir);
        if (system(git_remove_git_dir_cmd) != 0) {
            ye_logf(error, "Failed to remove .git directory in %s\n", target_dir);
            return;
        }
        
        // Initialize a new git repository
        char git_init_cmd[512];
        snprintf(git_init_cmd, sizeof(git_init_cmd), "cd \"%s\" && git init", target_dir);
        if (system(git_init_cmd) != 0) {
            ye_logf(error, "Failed to initialize a new git repository in %s\n", target_dir);
            return;
        }
        
        // build the initial packs
        ye_update_base_path(target_dir);
        editor_build_packs(true);

        ye_logf(info, "Successfully created project at %s\n", target_dir);

    #else
        ye_logf(error, "Unsupported platform for creating new projects!\n");
    #endif
}

void editor_open_project() {}