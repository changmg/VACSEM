# Set the minimum version of CMake that can be used
# To find the cmake version run
# $ cmake --version
cmake_minimum_required(VERSION 3.16)

# Set the project name
project(VECSAM)

# Set top directory
set(TOP_DIR ${PROJECT_SOURCE_DIR})

# Add sub directories
add_subdirectory(Circuit2Cnf)
add_subdirectory(SimSharpSat)
