cmake_minimum_required(VERSION 3.20)
project(engineSound)

add_subdirectory(libwave)

add_executable(engineSound "pacat-simple.c" "libEngineSound.c")

target_link_libraries(engineSound pulse pulse-simple m wave)