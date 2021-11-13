cmake_minimum_required(VERSION 3.16)
set(APPNAME hide-volume-osd-c)
project(${APPNAME})

add_executable(${APPNAME} WIN32 main.c resource.rc)
target_compile_definitions(${APPNAME} PUBLIC _UNICODE UNICODE)
target_compile_options(${APPNAME} PRIVATE -Wall -Wextra -pedantic -Werror)
target_link_options(${APPNAME} PRIVATE -municode)
target_link_libraries(${APPNAME} comctl32)
