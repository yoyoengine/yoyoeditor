/*
    This file is a part of yoyoengine. (https://github.com/zoogies/yoyoengine)
    Copyright (C) 2023-2025  Ryan Zmuda

    Licensed under the MIT license. See LICENSE file in the project root for details.
*/

// sucess, this file is now gross and #NOTCROSSPLATFORM

#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
    #include <unistd.h>
#else
    #include <platform/windows/unistd.h>
#endif
#include <stdbool.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "editor.h"

#include <yoyoengine/yoyoengine.h>

void editor_build_packs(bool force){
    // engine resources base dir
    char *_engine_resources = ye_get_engine_resource_static("");
    char *engine_resources = malloc(strlen(_engine_resources) + 1);
    strcpy(engine_resources, _engine_resources);

    // resources base dir
    char *_resources = ye_path("resources/");
    char *resources = malloc(strlen(_resources) + 1);
    strcpy(resources, _resources);

    // engine.yep path
    char *_engine_yep = ye_path("engine.yep");
    char *engine_yep = malloc(strlen(_engine_yep) + 1);
    strcpy(engine_yep, _engine_yep);

    // resources.yep path
    char *_resources_yep = ye_path("resources.yep");
    char *resources_yep = malloc(strlen(_resources_yep) + 1);
    strcpy(resources_yep, _resources_yep);

    if(force){
        yep_force_pack_directory(engine_resources, engine_yep);
        yep_force_pack_directory(resources, resources_yep);
    }
    else{
        // pack the /engine_resources into a .yep file
        yep_pack_directory(engine_resources, engine_yep);

        // pack the /resources into a .yep file
        yep_pack_directory(resources, resources_yep);
    }

    // free the memory
    free(engine_resources);
    free(resources);
    free(engine_yep);
    free(resources_yep);
}

// TODO: NOTCROSSPLATFORM

pid_t _configure() {
    pid_t pid = fork();

    if(pid == 0){
        execlp("cmake", "..", NULL);
        exit(0);
    }

    return pid;
}

// -u is for unbuffered output btw

// -DGAME_NAME
// -DGAME_RC_PATH
// -DCMAKE_C_FLAGS
// -DYOYO_ENGINE_SOURCE_DIR
// -DYOYO_ENGINE_BUILD_RELEASE  - NOT IMPLEMENTED
// -DGAME_BUILD_DESTINATION     - NOT IMPLEMENTED
// -DCMAKE_TOOLCHAIN_FILE
char **retrieve_build_args() {
    int num_args = 8;
    char **args = malloc((num_args + 1) * sizeof(char *));
    if (args == NULL) {
        perror("Failed to allocate memory for args");
        return NULL;
    }

    // predeclare for the goto cleanup
    json_t *SETTINGS_FILE = NULL;
    json_t *BUILD_FILE = NULL;

    SETTINGS_FILE = json_load_file(ye_path("settings.yoyo"), 0, NULL);
    if (SETTINGS_FILE == NULL) {
        ye_logf(error, "Failed to read settings file.\n");
        goto error;
    }

    BUILD_FILE = json_load_file(ye_path("build.yoyo"), 0, NULL);
    if (BUILD_FILE == NULL) {
        ye_logf(error, "Failed to read build file.\n");
        goto error;
    }

    const char *game_name = json_string_value(json_object_get(SETTINGS_FILE, "name"));
    const char *game_rc_path = json_string_value(json_object_get(BUILD_FILE, "rc_path"));
    const char *cflags = json_string_value(json_object_get(BUILD_FILE, "cflags"));
    const char *platform = json_string_value(json_object_get(BUILD_FILE, "platform"));
    const char *core_tag = json_string_value(json_object_get(BUILD_FILE, "core_tag"));
    bool use_local_engine = json_boolean_value(json_object_get(BUILD_FILE, "use_local_engine"));
    const char *local_engine_path = json_string_value(json_object_get(BUILD_FILE, "local_engine_path"));

    if (!game_name || !game_rc_path || !cflags || !platform || !core_tag || !local_engine_path) {
        ye_logf(error, "Failed to get required values from JSON files.\n");
        goto error;
    }

    args[0] = malloc(strlen(game_name) + strlen("-DGAME_NAME=") + 1);

    // check if the rcpath is empty, if so dont include this arg
    if(strlen(game_rc_path) == 0) {
        args[1] = malloc(1);
    }
    else {
        args[1] = malloc(strlen(game_rc_path) + strlen("-DGAME_RC_PATH=") + 1);
    }
    args[2] = malloc(strlen(cflags) + strlen("-DCMAKE_C_FLAGS=") + 1);
    
    /*
        args[3] is either the engine source dir if we
        want to use a local copy, or its the tag to pull from
        with fetchcontent
    */
    if(use_local_engine) {
        args[3] = malloc(strlen(local_engine_path) + strlen("-DYOYO_ENGINE_SOURCE_DIR=\"\"") + 1);
    }
    else {
        args[3] = malloc(strlen(core_tag) + strlen("-DYOYO_ENGINE_BUILD_TAG=\"\"") + 1); // the engine tag for the game to build against. TODO: expose this?
    }
    args[4] = malloc(1); // Placeholder for future use
    args[5] = malloc(1); // Placeholder for future use
    args[6] = malloc(strlen(EDITOR_STATE.opened_project_path) + strlen("toolchains/") + strlen("-DCMAKE_TOOLCHAIN_FILE=") + 256);

    if (!args[0] || !args[1] || !args[2] || !args[3] || !args[4] || !args[5] || !args[6]) {
        perror("Failed to allocate memory for argument strings");
        goto error;
    }

    snprintf(args[0], strlen(game_name) + strlen("-DGAME_NAME=") + 1, "-DGAME_NAME=%s", game_name);

    // check if the rcpath is empty, if so dont include this arg
    if(strlen(game_rc_path) == 0) {
        args[1][0] = '\0';
    }
    else {
        snprintf(args[1], strlen(game_rc_path) + strlen("-DGAME_RC_PATH=") + 1, "-DGAME_RC_PATH=%s", game_rc_path);
    }
    snprintf(args[2], strlen(cflags) + strlen("-DCMAKE_C_FLAGS=") + 1, "-DCMAKE_C_FLAGS=%s", cflags);
    if(use_local_engine) {
        snprintf(args[3], strlen(local_engine_path) + strlen("-DYOYO_ENGINE_SOURCE_DIR=\"\"") + 1, "-DYOYO_ENGINE_SOURCE_DIR=\"%s\"", local_engine_path);
    }
    else {
        snprintf(args[3], strlen(core_tag) + strlen("-DYOYO_ENGINE_BUILD_TAG=\"\"") + 1, "-DYOYO_ENGINE_BUILD_TAG=\"%s\"", core_tag);
    }
    args[4][0] = '\0';
    args[5][0] = '\0';

    if (strcmp(platform, "windows") != 0 && strcmp(platform, "emscripten") != 0) {
        args[6][0] = '\0';
    } else {
        snprintf(args[6], strlen(EDITOR_STATE.opened_project_path) + strlen("toolchains/") + strlen("-DCMAKE_TOOLCHAIN_FILE=") + 256, "-DCMAKE_TOOLCHAIN_FILE=%s/toolchains/%s.cmake", EDITOR_STATE.opened_project_path, platform);
        printf("toolchain file: %s\n", args[6]);
    }

    // delimeter
    args[num_args - 1] = strdup("..");
    args[num_args] = NULL;

    json_decref(SETTINGS_FILE);
    json_decref(BUILD_FILE);

    return args;

error:
    if (SETTINGS_FILE) json_decref(SETTINGS_FILE);
    if (BUILD_FILE) json_decref(BUILD_FILE);
    for (int i = 0; i < num_args; i++) {
        if (args[i]) free(args[i]);
    }
    free(args);
    return NULL;
}

void editor_run(){

    // TODO: do we want pack rebuild on run only? its pretty cheap if we dont change any files
    // editor_build_packs(false);

    // create fork for running game
    // if(EDITOR_STATE.is_running) {
        // stop the forked proc by pid
        // kill(EDITOR_STATE.running_thread, SIGKILL);
        // EDITOR_STATE.is_running = false;
    // }

    pid_t pid = fork();
    if(pid == 0){
        // chdir to build dir
        chdir(ye_path("build"));

        // run the game
        execlp("make", "make", "-j8", "run", NULL);
        exit(0);
    }
    // else {
        // EDITOR_STATE.running_thread = pid;
        // EDITOR_STATE.is_running = true;
    // }
}

// NOTCROSSPLATFORM
void editor_build(bool force_configure, bool should_run){
    editor_build_packs(false);    

    EDITOR_STATE.is_building = true;

    if(pipe(EDITOR_STATE.pipefd) == -1){
        ye_logf(error, "Failed to create pipe for build process.\n");
        return;
    }

    fcntl(EDITOR_STATE.pipefd[0], F_SETFL, O_NONBLOCK);

    char **args = retrieve_build_args();
    if(args == NULL){
        ye_logf(error, "Failed to retrieve build args.\n");
        return;
    }

    // print out build args
    // for(int i = 0; args[i] != NULL; i++){
        // printf("BUILD ARGS:\n");
        // printf("\targ %d: %s\n", i, args[i]);
    // }

    json_t *BUILD_FILE = json_load_file(ye_path("build.yoyo"), 0, NULL);
    if (BUILD_FILE == NULL) {
        ye_logf(error, "Failed to read build file.\n");
        return;
    }

    if(force_configure || json_boolean_value(json_object_get(BUILD_FILE, "delete_cache"))) {
        // absolutely NUKING the cache HELL YEA!!!!
        char buff[512];
        snprintf(buff, sizeof(buff), "rm -rf %s", ye_path("build/CMakeCache.txt"));
        system(buff);

        // set delete_cache to false
        json_object_set_new(BUILD_FILE, "delete_cache", json_false());
    }

    // serialize
    json_dump_file(BUILD_FILE, ye_path("build.yoyo"), JSON_INDENT(4));

    json_decref(BUILD_FILE);
    BUILD_FILE = NULL;

    // create a system argument string
    char invoke[512];
    snprintf(invoke, sizeof(invoke), "cmake ");
    for(int i = 0; args[i] != NULL; i++){
        // skip empty args
        if(strlen(args[i]) == 0) continue;

        // wrap any multi word args in quotes
        char *equal_sign = strchr(args[i], '=');
        if (equal_sign != NULL && strchr(equal_sign + 1, ' ')) {
            // Split the argument at the equal sign
            *equal_sign = '\0';
            strcat(invoke, args[i]);
            strcat(invoke, "=");
            strcat(invoke, "\"");
            strcat(invoke, equal_sign + 1);
            strcat(invoke, "\" ");
        } else {
            strcat(invoke, args[i]);
            strcat(invoke, " ");
        }

    }
    ye_logf(debug, "build cmake invokation: %s\n", invoke);
    
    pid_t pid = fork();
    if(pid == 0){
        close(EDITOR_STATE.pipefd[0]);

        char status[256];

        // create build dir (handle gracefully if it already exists)
        // snprintf(status, sizeof(status), "Creating build directory ...");
        // write(EDITOR_STATE.pipefd[1], status, strlen(status));
        // dont send beacuse its so short it messes up loading refresh timing

        // create ye_path("build") directory
        mkdir(ye_path("build"), 0777);

        // chdir to ye_path("build")
        chdir(ye_path("build"));

        // printf("%s\n", ye_path("build"));
        // printf("%s\n", ye_path("build/CMakeCache.txt"));

        // if CMakeCache.txt exists, we are going to skip running cmake explicitly
        if(force_configure || access(ye_path("build/CMakeCache.txt"), F_OK) == -1){
            // run cmake
            snprintf(status, sizeof(status), "Running CMake ...");
            write(EDITOR_STATE.pipefd[1], status, strlen(status));

            if(system(invoke) != 0){
                snprintf(status, sizeof(status), "error");
                write(EDITOR_STATE.pipefd[1], status, strlen(status));
                exit(0);
            }
        }

        // run make
        snprintf(status, sizeof(status), "Running Make ...");
        write(EDITOR_STATE.pipefd[1], status, strlen(status));

        if(system("make -j8") != 0){
            snprintf(status, sizeof(status), "error");
            write(EDITOR_STATE.pipefd[1], status, strlen(status));
            exit(0);
        }

        snprintf(status, sizeof(status), "done");
        write(EDITOR_STATE.pipefd[1], status, strlen(status));

        close(EDITOR_STATE.pipefd[1]);

        // cleanup arg memory
        for(int i = 0; args[i] != NULL; i++){
            free(args[i]);
            args[i] = NULL; // Avoid dangling pointer
        }

        if(should_run){
            editor_run();
        }

        exit(0);
    } else {
        EDITOR_STATE.building_thread = pid;
        EDITOR_STATE.is_building = true;
    }
}

void editor_build_and_run(){
    editor_build(false, true);
}

void editor_build_reconfigure(){

    editor_build(true, false);

}