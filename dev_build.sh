#!/usr/bin/env bash

# Create a new build directory if it doesn't exist
mkdir -p "./build"

# Navigate into the build directory
cd "./build"

# we assume engine path is ../engine/engine, but if we receive a parameter, we use it
if [ -z "$1" ]; then
    ENGINE_PATH="../engine/engine"
else
    ENGINE_PATH="$1"
fi

# Check if CMakeCache.txt exists or if reconfigure is forced
if [ ! -f "CMakeCache.txt" ]; then
    echo "Running cmake..."
    cmake -DCMAKE_BUILD_TYPE=Debug -DZOOGIES_DEVELOPMENT_BUILD=TRUE -DYOYO_ENGINE_PATH=$ENGINE_PATH ..
    if [ $? -eq 0 ]; then
        echo "cmake configuration succeeded."
    else
        echo "cmake configuration failed."
        exit 1
    fi
fi

# Run make
echo "Running make with -j$(nproc)..."
cmake --build . --parallel
if [ $? -eq 0 ]; then
    echo "make build succeeded."
else
    echo "make build failed."
    exit 1
fi

# Run the editor
echo "Running the editor..."
./Debug/yoyoeditor