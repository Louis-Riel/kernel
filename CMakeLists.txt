cmake_minimum_required(VERSION 3.16.0)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

execute_process(COMMAND bash -c "cd ${CMAKE_CURRENT_SOURCE_DIR}; ./minify.sh ")

project(usefuluseless)